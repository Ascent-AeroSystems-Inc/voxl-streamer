/*******************************************************************************
 * Copyright 2023 ModalAI Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * 4. The Software is used solely in conjunction with devices provided by
 *    ModalAI Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> // for exit()
#include <getopt.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <glib-object.h>
#include <modal_pipe.h>
#include <modal_journal.h>
#include "camera_metadata.h"
#include "context.h"
#include "pipeline.h"
#include "configuration.h"
#include "gst/rtsp/rtsp.h"

#define PROCESS_NAME "voxl-streamer"
#define PIPE_CH 0

// This is the main data structure for the application. It is passed / shared
// with other modules as needed.
static context_data context;
static int first_client = 0;
static int first_run = 0;
static int is_standalone = 0;
static int closing_pipe_intentionally = 0;
static int source_pipe_disconnected = 0;

// called whenever we connect or reconnect to the server
static void _cam_connect_cb(__attribute__((unused)) int ch, __attribute__((unused)) void* context)
{
    M_PRINT("Camera server Connected\n");
}


// called whenever we disconnect from the server
static void _cam_disconnect_cb(__attribute__((unused)) int ch, __attribute__((unused)) void* context)
{
    if(!closing_pipe_intentionally){
        M_PRINT("Camera server Disconnected Unintentionally\n");
        source_pipe_disconnected = 1;
    }
    else{
        M_PRINT("Camera server Disconnected Intentionally\n");
        closing_pipe_intentionally = 0;
    }
}

// camera helper callback whenever a frame arrives
static void _cam_helper_cb(int ch,
                           camera_image_metadata_t meta,
                           char* frame,
                           void* context)
{
    static int dump_meta_data = 1;
    static int pipe_size = 0;
    context_data *ctx = (context_data*) context;
    GstMapInfo info;
    GstFlowReturn status;


    if (dump_meta_data) {
        M_DEBUG("Meta data from incoming frame:\n");
        M_DEBUG("\tmagic_number 0x%X \n", meta.magic_number);
        M_DEBUG("\ttimestamp_ns: %" PRIu64 "\n", meta.timestamp_ns);
        M_DEBUG("\tframe_id: %d\n", meta.frame_id);
        M_DEBUG("\twidth: %d\n", meta.width);
        M_DEBUG("\theight: %d\n", meta.height);
        M_DEBUG("\tsize_bytes: %d\n", meta.size_bytes);
        M_DEBUG("\tstride: %d\n", meta.stride);
        M_DEBUG("\texposure_ns: %d\n", meta.exposure_ns);
        M_DEBUG("\tgain: %d\n", meta.gain);
        M_DEBUG("\tformat: %d\n", meta.format);
        dump_meta_data = 0;
    }


    if(first_run == 0){
        ctx->last_timestamp = (guint64) meta.timestamp_ns;

        if (meta.format == IMAGE_FORMAT_H264) {
            M_DEBUG("Saving h264 SPS\n");
            ctx->h264_sps_nal = gst_buffer_new_and_alloc(meta.size_bytes);
            gst_buffer_map(ctx->h264_sps_nal, &ctx->sps_info, GST_MAP_WRITE);
            memcpy(ctx->sps_info.data, frame, meta.size_bytes);
        } else if (meta.format == IMAGE_FORMAT_H265){
            M_DEBUG("Saving h265 SPS\n");
            ctx->h265_sps_nal = gst_buffer_new_and_alloc(meta.size_bytes);
            gst_buffer_map(ctx->h265_sps_nal, &ctx->sps_info, GST_MAP_WRITE);
            memcpy(ctx->sps_info.data, frame, meta.size_bytes); 
        }
        if ( ! main_running) return;

        // Encoded frames can change size dynamically
        if (meta.format != IMAGE_FORMAT_H264 && meta.format != IMAGE_FORMAT_H265) {
            if (ctx->input_frame_size != (uint32_t) meta.size_bytes) {
                M_ERROR("Frame size mismatch: got %d bytes from pipe, expected %d\n",
                        meta.size_bytes,
                        ctx->input_frame_size);
                M_ERROR("Most Likely the publisher of the camera data made a mistake\n");
                M_ERROR("Shutting down voxl-streamer\n");
                main_running = 0;
                return;
            }
        }

        // fetch the pipe size on first run to compare with later
        pipe_size = pipe_client_get_pipe_size(ch);
    }
    first_run = 1;


    // The need_data flag is set by the pipeline callback asking for
    // more data.
    if (! ctx->need_data) return;

    ctx->input_frame_number++;

    if (ctx->input_frame_number % ctx->output_frame_decimator) return;

    // Allocate a gstreamer buffer to hold the frame data
    // TODO can we ditch the alloc and memcpy?
    GstBuffer *gst_buffer = gst_buffer_new_and_alloc(meta.size_bytes);
    gst_buffer_map(gst_buffer, &info, GST_MAP_WRITE);

    if (info.size < (uint32_t) meta.size_bytes) {
        M_ERROR("Not enough memory for the frame buffer\n");
        main_running = 0;
        return;
    }

    memcpy(info.data, frame, meta.size_bytes);

    if ((ctx->input_frame_number == 1) && (meta.format == IMAGE_FORMAT_H264)) {
        // Signal that the header
        g_signal_emit_by_name(ctx->app_source, "push-buffer", ctx->h264_sps_nal, &status);
        if (status == GST_FLOW_OK) {
            M_DEBUG("SPS accepted\n", ctx->output_frame_number);
        } else {
            M_ERROR("SPS rejected\n");
            raise(2);
        }
    } else if ((ctx->input_frame_number == 1) && (meta.format == IMAGE_FORMAT_H265)) {
        // Signal that the header
        g_signal_emit_by_name(ctx->app_source, "push-buffer", ctx->h265_sps_nal, &status);
        if (status == GST_FLOW_OK) {
            M_DEBUG("SPS accepted\n", ctx->output_frame_number);
        } else {
            M_ERROR("SPS rejected\n");
            raise(2);
        }
    }
    GstBuffer *output_buffer = gst_buffer;
    pthread_mutex_lock(&ctx->lock);

    // To get minimal latency make sure to set this to the timestamp of
    // the very first frame that we will be sending through the pipeline.
    if (ctx->initial_timestamp == 0) ctx->initial_timestamp = (guint64) meta.timestamp_ns;

    // Do the timestamp calculations.
    // It is very important to set these up accurately.
    // Otherwise, the stream can look bad or just not work at all.
    // TODO: Experiment with taking some time off of pts???
    GST_BUFFER_TIMESTAMP(output_buffer) = ((guint64) meta.timestamp_ns - ctx->initial_timestamp);
    GST_BUFFER_DURATION(output_buffer) = ((guint64) meta.timestamp_ns) - ctx->last_timestamp;
    ctx->last_timestamp = (guint64) meta.timestamp_ns;

    pthread_mutex_unlock(&ctx->lock);

    ctx->output_frame_number++;

    // Signal that the frame is ready for use
    g_signal_emit_by_name(ctx->app_source, "push-buffer", output_buffer, &status);
    if (status == GST_FLOW_OK) {
        M_VERBOSE("Frame %d accepted\n", ctx->output_frame_number);
    } else {
        M_ERROR("New frame rejected, status = %d\n", status);
    }

    // Release the buffer so that we don't have a memory leak
    gst_buffer_unmap(gst_buffer, &info);
    gst_buffer_unref(gst_buffer);


    // check if we are filling up and flush the pipe if so
    if(pipe_size>0){
        if(pipe_client_bytes_in_pipe(ch)>(pipe_size/2)){
            M_WARN("source pipe getting backed up, flushing\n");
            pipe_client_flush(ch);
        }
    }

    return;
}


// This callback lets us know when an RTSP client has disconnected so that
// we can stop trying to feed video frames to the pipeline and reset everything
// for the next connection.
static void rtsp_client_disconnected(GstRTSPClient* self, context_data *data)
{
    pthread_mutex_lock(&data->lock);
    context_data* ctx = (context_data*)data;
    ctx->num_rtsp_clients--;
    if(ctx->num_rtsp_clients<0) ctx->num_rtsp_clients=0;

    if(ctx->num_rtsp_clients==0) {
        ctx->input_frame_number = 0;
        ctx->output_frame_number = 0;
        ctx->need_data = 0;
        ctx->initial_timestamp = 0;
        ctx->last_timestamp = 0;
    }

    M_PRINT("rtsp client disconnected, total clients: %d\n", ctx->num_rtsp_clients);

    pthread_mutex_unlock(&data->lock);


    if(ctx->num_rtsp_clients == 0){
        // Wait for the buffer processing thread to exit
        closing_pipe_intentionally = 1;
        M_PRINT("no more rtsp clients, closing source pipe intentionally\n");
        pipe_client_close(PIPE_CH);
        first_client = 0;
    }


}

// Gts client information 
static void print_client_info(GstRTSPClient* object)
{
    GstRTSPConnection *connection = gst_rtsp_client_get_connection(object);
    if (connection == NULL)
    {
        M_PRINT("Could not get RTSP connection\n");
        // DEBUG_EXIT;
        return;
    }

    GstRTSPUrl *url = gst_rtsp_connection_get_url(connection);
    if (url == NULL)
    {
        M_PRINT("Could not get RTSP connection URL\n");
        // DEBUG_EXIT;
        return;
    }
    gchar* uri = gst_rtsp_url_get_request_uri(url);
    M_PRINT("A new client %s has connected, total clients: %d\n",uri, context.num_rtsp_clients);
    g_free(uri);
}


// This is called by the RTSP server when a client has connected.
static void rtsp_client_connected(GstRTSPServer* self, GstRTSPClient* object,
                                  context_data *data)
{
    if(first_client==0)
    {
        closing_pipe_intentionally = 0;
        first_client = 1;
        first_run = 0;
        pipe_client_set_connect_cb(PIPE_CH, _cam_connect_cb, NULL);
        pipe_client_set_disconnect_cb(PIPE_CH, _cam_disconnect_cb, NULL);
        pipe_client_set_camera_helper_cb(PIPE_CH, _cam_helper_cb, &context);
        pipe_client_open(PIPE_CH, context.input_pipe_name, PROCESS_NAME, EN_PIPE_CLIENT_CAMERA_HELPER, 0);
    }

    pthread_mutex_lock(&data->lock);
     data->num_rtsp_clients++;
    print_client_info(object);

    // Install the disconnect callback with the client.
    g_signal_connect(object, "closed", G_CALLBACK(rtsp_client_disconnected), data);
    pthread_mutex_unlock(&data->lock);
    return;
}



/* this timeout is periodically run to clean up the expired sessions from the
 * pool. This needs to be run explicitly currently but might be done
 * automatically as part of the mainloop. */
static gboolean timeout(gpointer data)
{
  context_data* ctx = (context_data*)data;

  GstRTSPSessionPool *pool;
  pool = gst_rtsp_server_get_session_pool(ctx->rtsp_server);
  guint removed = gst_rtsp_session_pool_cleanup(pool);
  g_object_unref(pool);

  if (removed > 0){
    M_PRINT("Removed %d sessions\n", removed);
    pthread_mutex_lock(&ctx->lock);
    ctx->num_rtsp_clients--;
    if(ctx->num_rtsp_clients<0) ctx->num_rtsp_clients=0;

    if(ctx->num_rtsp_clients == 0){
        ctx->input_frame_number = 0;
        ctx->output_frame_number = 0;
        ctx->need_data = 0;
        ctx->initial_timestamp = 0;
        ctx->last_timestamp = 0;
    }

    pthread_mutex_unlock(&ctx->lock);
  }

  return TRUE;
}
// This callback is setup to happen at 1 second intervals so that we can
// monitor when the program is ending and exit the main loop.
gboolean loop_callback(gpointer data) {
    if(! main_running){
        M_PRINT("Trying to quit g main loop due to program shutdown\n");
        g_main_loop_quit((GMainLoop*) data);
    }
    if(source_pipe_disconnected){
        M_PRINT("Trying to quit g main loop due to source pipe disconnect\n");
        g_main_loop_quit((GMainLoop*) data);
        source_pipe_disconnected = 0;
    }
    return TRUE;
}

// This will cause all remaining RTSP clients to be removed
GstRTSPFilterResult stop_rtsp_clients(GstRTSPServer* server,
                                      GstRTSPClient* client,
                                      gpointer user_data) {
    return  GST_RTSP_FILTER_REMOVE;
}

static void PrintHelpMessage()
{
    M_PRINT("\nCommand line arguments are as follows:\n\n");
    M_PRINT("-b --bitrate    <#>     | Override bitrate specified in the config file\n");
    M_PRINT("-c --config             | Load config file only and quit (used for scripted setup)\n");
    M_PRINT("-d --decimator  <#>     | Override the decimator specified in the config file\n");
    M_PRINT("-h --help               | Print this help message\n");
    M_PRINT("-i --input-pipe <name>  | Override the input pipe specified in the config file\n");
    M_PRINT("-p --port       <#>     | Override the RTSP port number specified in the config file\n");
    M_PRINT("-s --standalone         | Use this to launch a new instance alongside the default service\n");
    M_PRINT("-v --verbosity  <#>     | Log verbosity level (Default 2)\n");
    M_PRINT("                      0 | Print verbose logs\n");
    M_PRINT("                      1 | Print >= info logs\n");
    M_PRINT("                      2 | Print >= warning logs\n");
    M_PRINT("                      3 | Print only fatal logs\n");
    M_PRINT("\n");
    M_PRINT("Typical useage of voxl-streamer is to let systemd start the voxl-streamer\n");
    M_PRINT("service in the background on boot. In this case, systemd starts the voxl-streamer\n");
    M_PRINT("binary with no arguments and only the configure file is used to set up parameters.\n");
    M_PRINT("\n");
    M_PRINT("If you wish to launch a second instance of voxl-streamer without interfering with\n");
    M_PRINT("the default background process, you must launch it with the --standalone argument\n");
    M_PRINT("and provide a new unique port number with the --port argument.\n");
    M_PRINT("\n");
    M_PRINT("Example of standalone mode:\n");
    M_PRINT("Lets say voxl-streamer is running in the background with default configuration\n");
    M_PRINT("streaming the hires_small_encoded h265 stream via RTSP to port 8900\n");
    M_PRINT("You could launch a second stream yourself for the tracking camera like this:\n");
    M_PRINT("\n");
    M_PRINT("voxl2:/$ voxl-streamer --standalone --port 8901 -i tracking\n");
    M_PRINT("\n");
}

static int ParseArgs(int         argc,                 ///< Number of arguments
                     char* const argv[])               ///< Argument list
{
    static struct option LongOptions[] =
    {
        {"bitrate",          required_argument,  0, 'b'},
        {"config",           no_argument,        0, 'c'},
        {"decimator",        required_argument,  0, 'd'},
        {"help",             no_argument,        0, 'h'},
        {"input-pipe",       required_argument,  0, 'i'},
        {"port",             required_argument,  0, 'p'},
        {"standalone",       no_argument,        0, 's'},
        {"verbosity",        required_argument,  0, 'v'},
    };

    int optionIndex = 0;
    int option;

    while ((option = getopt_long (argc, argv, ":b:cd:hi:p:sv:", &LongOptions[0], &optionIndex)) != -1)
    {
        switch (option) {
            case 'v':{
                int verbosity;
                if (sscanf(optarg, "%d", &verbosity) != 1)
                {
                    M_ERROR("Failed to parse debug level specified after -d flag\n");
                    return -1;
                }

                if (verbosity >= PRINT || verbosity < VERBOSE)
                {
                    M_ERROR("Invalid debug level specified: %d\n", verbosity);
                    return -1;
                }

                M_JournalSetLevel((M_JournalLevel) verbosity);
                break;
            }
            case 'b':
                if(sscanf(optarg, "%u", &context.output_stream_bitrate) != 1){
                    M_ERROR("Failed to get valid integer for bitrate from: %s\n", optarg);
                    return -1;
                }
                break;
            case 'c':
                M_PRINT("parsed config file\n");
                exit(0);
                break;
            case 'd':
                if(sscanf(optarg, "%u", &context.output_frame_decimator) != 1){
                    M_ERROR("Failed to get valid integer for decimator from: %s\n", optarg);
                    return -1;
                }
                break;
                break;
            case 'i':
                strncpy(context.input_pipe_name, optarg, MODAL_PIPE_MAX_PATH_LEN);
                break;
            case 'p':
                strncpy(context.rtsp_server_port, optarg, MAX_RTSP_PORT_SIZE);
                break;
            case 'h':
                PrintHelpMessage();
                exit(0);
            case 's':
                is_standalone = 1;
                break;
            case ':':
                M_ERROR("Option %c needs a value\n\n", optopt);
                PrintHelpMessage();
                return -1;
            case '?':
                M_ERROR("Unknown option: %c\n\n", optopt);
                PrintHelpMessage();
                return -1;
        }
    }

    // optind is for the extra arguments which are not parsed
    if(optind < argc){
        M_WARN("extra arguments:\n");
        for (; optind < argc; optind++) {
            M_WARN("\t%s\n", argv[optind]);
        }
    }

    return 0;
}





int _setup_context(void)
{
    // Wait for pipe to appear
    M_PRINT("Waiting for pipe %s to appear\n", context.input_pipe_name);
    while(main_running){
        if(pipe_exists(context.input_pipe_name)){
            M_PRINT("Found Pipe\n");
            break;
        }
        usleep(100000);
    }

    // make sure it's the right type
    if(!pipe_is_type(context.input_pipe_name, "camera_image_metadata_t")){
        M_ERROR("Pipe type mismatch for metadata\n");
        return -1;
    }

    // we just did some waiting, make sure the user didn't want to quit
    if(!main_running) return 0;

    // wait for the server to finish setting up the pipe and fetch its info
    usleep(200000);
    cJSON* json = pipe_get_info_json(context.input_pipe_name);
    if( json == NULL || \
        json_fetch_int(json, "width", &context.input_frame_width)   || \
        json_fetch_int(json, "height", &context.input_frame_height) || \
        json_fetch_int(json, "int_format", &context.input_format)   || \
        json_fetch_int(json, "framerate", &context.input_frame_rate)|| \
        context.input_frame_width  < 1                              || \
        context.input_frame_height < 1)
    {
        M_WARN("Failed to fetch one or more of width, height, into_format, framerate from pipe info file\n");
        M_WARN("going to connect to the pipe for 1 frame to inspect it now\n");
        if(metadataGrabber(PROCESS_NAME, &context)) return -1;
        M_WARN("grabbed the data from the actual pipe and closed it\n");
    }

    M_PRINT("detected following stats from pipe:\n");
    M_PRINT("w: %d h: %d fps: %d format: %s\n", \
                context.input_frame_width,\
                context.input_frame_height,\
                context.input_frame_rate,\
                pipe_image_format_to_string(context.input_format));


    // final check that the data from either json or pipe metadata was good
    // the metadata grabber didn't do this check
    if( context.input_frame_width  < 1 || \
        context.input_frame_height < 1)
    {
        M_ERROR("invalid width, height, or framerate\n");
        return -1;
    }

    // Cannot decimate encoded frames
    if((context.input_format == IMAGE_FORMAT_H264 ||
        context.input_format == IMAGE_FORMAT_H265) &&
        context.output_frame_decimator != 1) {
        M_WARN("Streaming pre-encoded frames, will not be able to apply decimator\n");
        context.output_frame_decimator = 1;
    } else {
        context.input_frame_rate = context.input_frame_rate / context.output_frame_decimator;
        context.output_frame_rate = context.input_frame_rate;
        M_DEBUG("Frame rate is: %u\n", context.input_frame_rate);
    }

    // set output resolution based on input resolution and rotation
    if(context.output_stream_rotation == 90 || context.output_stream_rotation == 270){
        context.output_stream_height = context.input_frame_width;
        context.output_stream_width = context.input_frame_height;
    } else {
        context.output_stream_height = context.input_frame_height;
        context.output_stream_width = context.input_frame_width;
    }


    return configure_frame_format(context.input_format, &context);
}





int _run_gstreamer(void)
{
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    GMainContext* loop_context;
    GMainLoop *loop;
    GSource *loop_source;
    GSource *time_loop_source;

    context.num_rtsp_clients = 0;

    // Initialize Gstreamer
    gst_init(NULL, NULL);

    // Create the RTSP server
    context.rtsp_server = gst_rtsp_server_new();
    if (context.rtsp_server) {
        M_DEBUG("Made rtsp_server\n");
    } else {
        M_ERROR("couldn't make rtsp_server\n");
        return -1;
    }

    // Configure the RTSP server port
    g_object_set(context.rtsp_server, "service", context.rtsp_server_port, NULL);

    // Setup the callback to alert when a new connection has come in
    g_signal_connect(context.rtsp_server, "client-connected",
                     G_CALLBACK(rtsp_client_connected), &context);

    // Setup the glib loop for the RTSP server to use
    loop_context = g_main_context_new();
    if (loop_context) {
        M_DEBUG("Created RTSP server main loop context\n");
    } else {
        M_ERROR("Couldn't create loop context\n");
        return -1;
    }
    loop = g_main_loop_new(loop_context, FALSE);
    if (loop) {
        M_DEBUG("Created RTSP server main loop\n");
    } else {
        M_ERROR("Couldn't create loop\n");
        return -1;
    }

    // Add an event source to the loop context. This will generate a callback
    // at 1 second intervals. We can use this to detect when the program is
    // ending and exit the loop
    loop_source = g_timeout_source_new_seconds(1);
    if (loop_source) {
        M_DEBUG("Created RTSP server main loop source for callback\n");
    } else {
        M_ERROR("Couldn't create loop source for callback\n");
        return -1;
    }
    g_source_set_callback(loop_source, loop_callback, loop, NULL);
    g_source_attach(loop_source, loop_context);

      // attach timeout to out loop
    time_loop_source = g_timeout_source_new_seconds(2);
    if (time_loop_source) {
        M_DEBUG("Created timeout loop source for callback\n");
    } else {
        M_ERROR("Couldn't create timeout loop source for callback\n");
        return -1;
    }
    g_source_set_callback(time_loop_source, timeout, &context, NULL);
    g_source_attach(time_loop_source, loop_context);

    g_main_context_unref(loop_context);
    g_source_unref(loop_source);
    g_source_unref(time_loop_source);

    // get the mount points for the RTSP server, every server has a default object
    // that will be used to map uri mount points to media factories
    mounts = gst_rtsp_server_get_mount_points(context.rtsp_server);
    if ( ! mounts) {
        M_ERROR("Couldn't get mount points\n");
        return -1;
    }

    // We override the create element function with our own so that we can use
    // our custom pipeline instead of a launch line.
    factory = gst_rtsp_media_factory_new();
    if ( ! factory) {
        M_ERROR("Couldn't create new media factory\n");
        return -1;
    }
    // Set as shared to consider video disconnects
    gst_rtsp_media_factory_set_shared (factory, TRUE);
    GstRTSPMediaFactoryClass *memberFunctions = GST_RTSP_MEDIA_FACTORY_GET_CLASS(factory);
    if ( ! memberFunctions) {
        M_ERROR("Couldn't get media factory class pointer\n");
        return -1;
    }
    memberFunctions->create_element = create_custom_element;
    char *link_name = "/live";
    gst_rtsp_mount_points_add_factory(mounts, link_name, factory);
    g_object_unref(mounts);

    // Attach the RTSP server to our loop
    int source_id = gst_rtsp_server_attach(context.rtsp_server, loop_context);
    M_DEBUG("Got %d from gst_rtsp_server_attach\n", source_id);
    if ( ! source_id) {
        M_ERROR("gst_rtsp_server_attach failed\n");
        return -1;
    }



    // Indicate how to connect to the stream
    M_PRINT("Stream available at rtsp://127.0.0.1:%s%s\n", context.rtsp_server_port,
           link_name);

    // Start the main loop that the RTSP Server is attached to. This will not
    // exit until it is stopped.
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    // Main loop has exited, time to clean up and exit the program
    M_DEBUG("g_main_loop exited\n");

    // Stop any remaining RTSP clients
    (void) gst_rtsp_server_client_filter(context.rtsp_server, stop_rtsp_clients, NULL);

    return 0;
}









//--------
//  Main
//--------
int main(int argc, char *argv[])
{

    strncpy(context.rtsp_server_port, DEFAULT_RTSP_PORT, MAX_RTSP_PORT_SIZE);

    // Have the configuration module fill in the context data structure
    // with all of the required parameters to support the given configuration
    // in the given configuration file.
    if (config_file_read(&context)) {
        M_ERROR("Could not parse the configuration data\n");
        return -1;
    }

    if(ParseArgs(argc, argv)){
        M_ERROR("Failed to parse args\n");
        return -1;
    }

    // make sure another instance isn't running
    // if return value is -3 then a background process is running with
    // higher privaledges and we couldn't kill it, in which case we should
    // not continue or there may be hardware conflicts. If it returned -4
    // then there was an invalid argument that needs to be fixed.
    if(!is_standalone){
        if(kill_existing_process(PROCESS_NAME, 2.0)<-2) return -1;
    }

    // start signal manager so we can exit cleanly
    if(enable_signal_handler()==-1){
        fprintf(stderr,"ERROR: failed to start signal manager\n");
        return -1;
    }

    // make PID file to indicate your project is running
    // due to the check made on the call to rc_kill_existing_process() above
    // we can be fairly confident there is no PID file already and we can
    // make our own safely.
    if(!is_standalone) make_pid_file(PROCESS_NAME);
    main_running = 1;

    M_DEBUG("Using input:     %s\n", context.input_pipe_name);
    M_DEBUG("Using RTSP port: %s\n", context.rtsp_server_port);
    M_DEBUG("Using bitrate:   %d\n", context.output_stream_bitrate);
    M_DEBUG("Using decimator: %d\n", context.output_frame_decimator);


    // Pass a pointer to the context to the pipeline module
    pipeline_init(&context);


    // keep trying to run the streamer
    // a pipe disconnect will
    while(main_running)
    {
        // try to get pipe info and set up context, retry if failed
        if(_setup_context()){
            M_WARN("failure setting up context based on requested pipe\n");
            M_WARN("waiting and trying again\n");
            usleep(500000);
            continue;
        }

        // _setup_context might have taken a while, check if we should shutdown
        if(!main_running) break;

        // this is blocking until gstreamer is told to stop by disconnect callback
        // or sigint handler
        _run_gstreamer();

        // if still running, sleep and go start the cycle again
        if(main_running) usleep(500000);
    }

    // Clean up gstreamer
    M_PRINT("cleaning up gstreamer\n");
    gst_deinit();


    pipe_client_close_all();
    if(!is_standalone) remove_pid_file(PROCESS_NAME);
    M_PRINT("Exited Cleanly\n");

    return 0;
}

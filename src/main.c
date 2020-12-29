/*******************************************************************************
 * Copyright 2020 ModalAI Inc.
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <glib-object.h>
#include "context.h"
#include "input.h"
#include "pipeline.h"
#include "configuration.h"

// This is the main data structure for the application. It is passed / shared
// with other modules as needed.
static context_data context;

// Some configuration information that will be passed to the configuration
// module for further processing.
#define MAX_CONFIG_NAME_LENGTH 256
#define MAX_CONFIG_FILE_NAME_LENGTH 256

static char config_name[MAX_CONFIG_NAME_LENGTH];
static char config_file_name[MAX_CONFIG_FILE_NAME_LENGTH];
static char uvc_device_name[MAX_UVC_DEVICE_STRING_LENGTH];

// Definition of the default port used by the RTSP server
#define MAX_RTSP_PORT_SIZE 8
#define DEFAULT_RTSP_PORT "8900"

// Used to capture ctrl-c signal to allow graceful exit
void intHandler(int dummy) {
    printf("Got SIGINT, exiting\n");
    context.running = 0;
}

// This callback lets us know when an RTSP client has disconnected so that
// we can stop trying to feed video frames to the pipeline and reset everything
// for the next connection.
static void rtsp_client_disconnected(GstRTSPClient* self, context_data *data) {
    printf("An existing client has disconnected from the RTSP server\n");

    pthread_mutex_lock(&data->lock);
    data->num_rtsp_clients--;

    if ( ! data->num_rtsp_clients) {
        data->input_frame_number = 0;
        data->output_frame_number = 0;
        data->need_data = 0;
        data->initial_timestamp = 0;
        data->last_timestamp = 0;
    }
    pthread_mutex_unlock(&data->lock);
}

// This is called by the RTSP server when a client has connected.
static void rtsp_client_connected(GstRTSPServer* self, GstRTSPClient* object,
                                  context_data *data) {
    printf("A new client has connected to the RTSP server\n");

    pthread_mutex_lock(&data->lock);
    data->num_rtsp_clients++;

    // Install the disconnect callback with the client.
    g_signal_connect(object, "closed", G_CALLBACK(rtsp_client_disconnected), data);
    pthread_mutex_unlock(&data->lock);
}

// This callback is setup to happen at 1 second intervals so that we can
// monitor when the program is ending and exit the main loop.
gboolean loop_callback(gpointer data) {
    if ( ! context.running) {
        g_print("Trying to stop loop\n");
        g_main_loop_quit((GMainLoop*) data);
    }
    return TRUE;
}

// This will cause all remaining RTSP clients to be removed
GstRTSPFilterResult stop_rtsp_clients(GstRTSPServer* server,
                                      GstRTSPClient* client,
                                      gpointer user_data) {
    return  GST_RTSP_FILTER_REMOVE;
}

void help() {
    printf("Usage: voxl-streamer <options>\n");
    printf("Options:\n");
    printf("-d                Show extra debug messages.\n");
    printf("-v                Show extra frame level debug messages.\n");
    printf("-u <uvc device>   UVC device to use (to override what is in the configuration file).\n");
    printf("-c <name>         Configuration name (to override what is in the configuration file).\n");
    printf("-f <filename>     Configuration file name (default is /etc/modalai/voxl-streamer.conf).\n");
    printf("-h                Show help.\n");
}

//--------
//  Main
//--------
int main(int argc, char *argv[]) {
    int opt = 0;
    GstStateChangeReturn state_change_status;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    GMainContext* loop_context;
    GMainLoop *loop;
    GSource *loop_source;
    pthread_t input_thread_id;
    char rtsp_server_port[MAX_RTSP_PORT_SIZE];

    strncpy(rtsp_server_port, DEFAULT_RTSP_PORT, MAX_RTSP_PORT_SIZE);

    // Setup the default configuration file name
    strncpy(config_file_name, "/etc/modalai/voxl-streamer.conf", MAX_CONFIG_FILE_NAME_LENGTH);

    // Parse all command line options
    while ((opt = getopt(argc, argv, "dvc:f:p:u:h")) != -1) {
        switch (opt) {
        case 'd':
            printf("Enabling debug messages\n");
            context.debug = 1;
            break;
        case 'v':
            printf("Enabling frame debug messages\n");
            context.frame_debug = 1;
            break;
        case 'c':
            strncpy(config_name, optarg, MAX_CONFIG_NAME_LENGTH);
            printf("Using configuration %s\n", config_name);
            break;
        case 'f':
            strncpy(config_file_name, optarg, MAX_CONFIG_FILE_NAME_LENGTH);
            printf("Using configuration file %s\n", config_file_name);
            break;
        case 'u':
            strncpy(uvc_device_name, optarg, MAX_UVC_DEVICE_STRING_LENGTH);
            printf("Using UVC device name %s\n", uvc_device_name);
            break;
        case 'p':
            strncpy(rtsp_server_port, optarg, MAX_RTSP_PORT_SIZE);
            printf("Using RTSP port %s\n", rtsp_server_port);
            break;
        case 'h':
            help();
            return -1;
        case ':':
            fprintf(stderr, "Error - option %c needs a value\n\n", optopt);
            help();
            return -1;
        case '?':
            fprintf(stderr, "Error - unknown option: %c\n\n", optopt);
            help();
            return -1;
        }
    }

    // optind is for the extra arguments which are not parsed
    for (; optind < argc; optind++) {
        fprintf(stderr, "extra arguments: %s\n", argv[optind]);
        help();
        return -1;
    }

    // Have the configuration module fill in the context data structure
    // with all of the required parameters to support the given configuration
    // in the given configuration file.
    if (prepare_configuration(config_file_name, config_name, &context)) {
        fprintf(stderr, "ERROR: Could not parse the configuration data\n");
        return -1;
    }

    // If we got a UVC device name on the command line then use it instead of
    // whatever was found in the configuration file
    if (strlen(uvc_device_name)) {
        strncpy(context.uvc_device_name, uvc_device_name, MAX_UVC_DEVICE_STRING_LENGTH);
    }

    // Pass a pointer to the context to the pipeline module
    pipeline_init(&context);

    // Initialize Gstreamer
    gst_init(NULL, NULL);

    // Setup our signal handler to catch ctrl-c
    signal(SIGINT, intHandler);

    // All systems are go...
    context.running = 1;

    // Start the frame input thread for MPA sources
    // If we are in test mode then we generate our own frames and don't need
    // the input thread to receive frames from external sources. UVC will
    // get frames directly from the camera and also doesn't need this.
    if (context.interface == MPA_INTERFACE) {
        // Start our input frame processing thread
        pthread_create(&input_thread_id, NULL,
                       input_thread, (void*) &context);
    }

    // Wait until the input parameters have been intialized. For MPA interfaces
    // this happens when the first frame meta data is received.
    int timeout = 5;
    while (timeout) {
        if (context.input_parameters_initialized) break;
        usleep(500000);
        timeout--;
    }
    if (context.input_parameters_initialized) {
        if (context.debug) printf("Input parameters initialized\n");
    } else {
        fprintf(stderr, "ERROR: Timeout on input parameter initialization\n");
        return -1;
    }

    // Create the RTSP server
    context.rtsp_server = gst_rtsp_server_new();
    if (context.rtsp_server) {
        if (context.debug) printf("Made rtsp_server\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make rtsp_server\n");
        return -1;
    }

    // Configure the RTSP server port
    g_object_set(context.rtsp_server, "service", rtsp_server_port, NULL);

    // Setup the callback to alert when a new connection has come in
    g_signal_connect(context.rtsp_server, "client-connected",
                     G_CALLBACK(rtsp_client_connected), &context);

    // Setup the glib loop for the RTSP server to use
    loop_context = g_main_context_new();
    if (loop_context) {
        if (context.debug) printf("Created RTSP server main loop context\n");
    } else {
        fprintf(stderr, "ERROR: Couldn't create loop context\n");
        return -1;
    }
    loop = g_main_loop_new(loop_context, FALSE);
    if (loop) {
        if (context.debug) printf("Created RTSP server main loop\n");
    } else {
        fprintf(stderr, "Couldn't create loop\n");
        return -1;
    }

    // Add an event source to the loop context. This will generate a callback
    // at 1 second intervals. We can use this to detect when the program is
    // ending and exit the loop
    loop_source = g_timeout_source_new_seconds(1);
    if (loop_source) {
        if (context.debug) printf("Created RTSP server main loop source for callback\n");
    } else {
        fprintf(stderr, "Couldn't create loop source for callback\n");
        return -1;
    }
    g_source_set_callback(loop_source, loop_callback, loop, NULL);
    g_source_attach(loop_source, loop_context);

    g_main_context_unref(loop_context);
    g_source_unref(loop_source);

    // get the mount points for the RTSP server, every server has a default object
    // that will be used to map uri mount points to media factories
    mounts = gst_rtsp_server_get_mount_points(context.rtsp_server);
    if ( ! mounts) {
        fprintf(stderr, "ERROR: Couldn't get mount points\n");
        return -1;
    }

    // We override the create element function with our own so that we can use
    // our custom pipeline instead of a launch line.
    factory = gst_rtsp_media_factory_new();
    if ( ! factory) {
        fprintf(stderr, "ERROR: Couldn't create new media factory\n");
        return -1;
    }
    GstRTSPMediaFactoryClass *memberFunctions = GST_RTSP_MEDIA_FACTORY_GET_CLASS(factory);
    if ( ! memberFunctions) {
        fprintf(stderr, "ERROR: Couldn't get media factory class pointer\n");
        return -1;
    }
    memberFunctions->create_element = create_custom_element;
    char *link_name = "/live";
    gst_rtsp_mount_points_add_factory(mounts, link_name, factory);
    g_object_unref(mounts);

    // Attach the RTSP server to our loop
    int source_id = gst_rtsp_server_attach(context.rtsp_server, loop_context);
    if (context.debug) printf("Got %d from gst_rtsp_server_attach\n", source_id);
    if ( ! source_id) {
        fprintf(stderr, "ERROR: gst_rtsp_server_attach failed\n");
        return -1;
    }

    // Indicate how to connect to the stream
    printf("Stream available at rtsp://127.0.0.1:%s%s\n", rtsp_server_port,
           link_name);

    // Start the main loop that the RTSP Server is attached to. This will not
    // exit until it is stopped.
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    // Main loop has exited, time to clean up and exit the program
    if (context.debug) printf("g_main_loop exited\n");
    context.running = 0;

    // Wait for the buffer processing thread to exit
    if (context.interface == MPA_INTERFACE) {
        pthread_join(input_thread_id, NULL);
        if (context.debug) printf("input_thread exited\n");
    }

    // Stop any remaining RTSP clients
    (void) gst_rtsp_server_client_filter(context.rtsp_server, stop_rtsp_clients, NULL);

    // Clean up gstreamer
    gst_deinit();

    printf("voxl-streamer ending\n");

    return 0;
}

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
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <modal_journal.h>

#include "context.h"

#define TODO_NEED_ENCODER 0
static context_data *context;
static GstElement* pipeline;

// Simple initialization. Just save a copy of the context pointer.
void pipeline_init(context_data *ctx)
{
    context = ctx;
}

void pipeline_deinit(void)
{
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

static void create_elements(context_data *context) {
    // Just create everything. We'll decide which ones to use later
    context->test_source = gst_element_factory_make("videotestsrc", "frame_source_test");
    context->test_caps_filter = gst_element_factory_make("capsfilter", "test_caps_filter");
    context->app_source = gst_element_factory_make("appsrc", "frame_source_mpa");
    context->app_source_filter = gst_element_factory_make("capsfilter", "appsrc_filter");
    context->overlay_queue = gst_element_factory_make("queue", "overlay_queue");
    context->image_overlay = gst_element_factory_make("gdkpixbufoverlay", "image_overlay");
    context->scaler_queue = gst_element_factory_make("queue", "scaler_queue");
    context->scaler = gst_element_factory_make("videoscale", "scaler");
    context->converter_queue = gst_element_factory_make("queue", "converter_queue");
    context->video_converter = gst_element_factory_make("videoconvert", "converter");
    context->rotator_queue = gst_element_factory_make("queue", "rotator_queue");
    context->video_rotate = gst_element_factory_make("videoflip", "video_rotate");
    context->video_rotate_filter = gst_element_factory_make("capsfilter", "video_rotate_filter");
    context->encoder_queue = gst_element_factory_make("queue", "encoder_queue");
    context->omx_encoder = gst_element_factory_make("omxh264enc", "omx_encoder");
    context->omx_h265_encoder = gst_element_factory_make("omxh265enc", "omx_h265_encoder");
    context->h264_parser = gst_element_factory_make("h264parse", "h264_parser");
    context->h265_parser = gst_element_factory_make("h265parse", "h265_parser");
    context->rtp_filter = gst_element_factory_make("capsfilter", "rtp_filter");
    context->rtp_queue = gst_element_factory_make("queue", "rtp_queue");
    context->rtp_payload = gst_element_factory_make("rtph264pay", "rtp_payload");
    context->rtp_h265_payload = gst_element_factory_make("rtph265pay", "rtp_h265_payload");
}

static int verify_element_creation(context_data *context) {
    if (context->test_source) {
        M_DEBUG("Made test_source\n");
    } else {
        M_ERROR("Couldn't make test_source\n");
        return -1;
    }
    if (context->test_caps_filter) {
        M_DEBUG("Made test_caps_filter\n");
    } else {
        M_ERROR("Couldn't make test_caps_filter\n");
        return -1;
    }
    if (context->overlay_queue) {
        M_DEBUG("Made overlay_queue\n");
    } else {
        M_ERROR("Couldn't make overlay_queue\n");
        return -1;
    }
    if (context->image_overlay) {
        M_DEBUG("Made image_overlay\n");
    } else {
        M_ERROR("Couldn't make image_overlay\n");
        return -1;
    }
    if (context->scaler_queue) {
        M_DEBUG("Made scaler_queue\n");
    } else {
        M_ERROR("Couldn't make scaler_queue\n");
        return -1;
    }
    if (context->scaler) {
        M_DEBUG("Made scaler\n");
    } else {
        M_ERROR("Couldn't make scaler\n");
        return -1;
    }
    if (context->converter_queue) {
        M_DEBUG("Made converter_queue\n");
    } else {
        M_ERROR("Couldn't make converter_queue\n");
        return -1;
    }
    if (context->video_converter) {
        M_DEBUG("Made video_converter\n");
    } else {
        M_ERROR("Couldn't make video_converter\n");
        return -1;
    }
    if (context->rotator_queue) {
        M_DEBUG("Made rotator_queue\n");
    } else {
        M_ERROR("Couldn't make rotator_queue\n");
        return -1;
    }
    if (context->video_rotate) {
        M_DEBUG("Made video_rotate\n");
    } else {
        M_ERROR("Couldn't make video_rotate\n");
        return -1;
    }
    if (context->video_rotate_filter) {
        M_DEBUG("Made video_rotate_filter\n");
    } else {
        M_ERROR("Couldn't make video_rotate_filter\n");
        return -1;
    }
    if (context->app_source) {
        M_DEBUG("Made app_source\n");
    } else {
        M_ERROR("Couldn't make app_source\n");
        return -1;
    }
    if (context->app_source_filter) {
        M_DEBUG("Made app_source_filter\n");
    } else {
        M_ERROR("Couldn't make app_source_filter\n");
        return -1;
    }
    if (context->encoder_queue) {
        M_DEBUG("Made encoder_queue\n");
    } else {
        M_ERROR("Couldn't make encoder_queue\n");
        return -1;
    }
    if (context->omx_encoder) {
        M_DEBUG("Made omx_encoder\n");
    } else {
        M_ERROR("Couldn't make omx_encoder\n");
        return -1;
    }
    if (context->omx_h265_encoder) {
        M_DEBUG("Made omx_h265_encoder\n");
    } else {
        M_ERROR("Couldn't make omx_h265_encoder\n");
        return -1;
    }
    if (context->h264_parser) {
        M_DEBUG("Made h264_parser\n");
    } else {
        M_ERROR("Couldn't make h264_parser\n");
        return -1;
    }
    if (context->h265_parser) {
        M_DEBUG("Made h265_parser\n");
    } else {
        M_ERROR("Couldn't make h265_parser\n");
        return -1;
    }
    if (context->rtp_filter) {
        M_DEBUG("Made rtp_filter\n");
    } else {
        M_ERROR("Couldn't make rtp_filter\n");
        return -1;
    }
    if (context->rtp_queue) {
        M_DEBUG("Made rtp_queue\n");
    } else {
        M_ERROR("Couldn't make rtp_queue\n");
        return -1;
    }
    if (context->rtp_payload) {
        M_DEBUG("Made rtp_payload\n");
    } else {
        M_ERROR("Couldn't make rtp_payload\n");
        return -1;
    }
    if (context->rtp_h265_payload) {
        M_DEBUG("Made rtp_h265_payload\n");
    } else {
        M_ERROR("Couldn't make rtp_h265_payload\n");
        return -1;
    }
    return 0;
}

// This is a callback to indicate when the pipeline needs data
static void start_feed(GstElement *source, guint size, context_data *data) {
    M_VERBOSE("*** Start feeding ***\n");
    data->need_data = 1;
}

// This is a callback to indicate when the pipeline no longer needs data.
// This isn't called with our pipeline because we feed frames at the correct rate.
static void stop_feed(GstElement *source, context_data *data) {
    M_VERBOSE("*** Stop feeding ***\n");
    data->need_data = 0;
}

// These are the callbacks to let us know of bus messages
static void warn_cb(GstBus *bus, GstMessage *msg, context_data *data) {
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error(msg, &err, &debug_info);
    M_WARN("Received from element %s: %s\n\tDebugging information: %s\n",
                GST_OBJECT_NAME(msg->src),
                err->message,
                debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);
}

static void error_cb(GstBus *bus, GstMessage *msg, context_data *data) {
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error(msg, &err, &debug_info);
    M_ERROR("Received from element %s: %s\n\tDebugging information: %s\n",
                GST_OBJECT_NAME(msg->src),
                err->message,
                debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);
}



// This is used to override the standard element creator that relies on having
// a gstreamer launch line. This allows us to use our custom pipeline.
GstElement *create_custom_element(GstRTSPMediaFactory *factory, const GstRTSPUrl *url)
{

    M_DEBUG("Creating media pipeline for RTSP client\n");


    GstVideoInfo* video_info;
    GstCaps* video_caps;
    GstBus* bus;

    // TODO: Why aren't these factory lock macros available in the header?!?
    // GST_RTSP_MEDIA_FACTORY_LOCK (factory);

    // Create an empty pipeline
    pipeline = gst_pipeline_new(NULL);
    if (pipeline) {
        M_DEBUG("Made empty pipeline\n");
    } else {
        M_ERROR("couldn't make empty pipeline\n");
        pipe_client_close_all();
        exit(-1);
    }

    // Figure out what kinds of transformations are required to get the
    // video ready for the encoder
    int rotation_method = 0;
    if (context->output_stream_rotation == 90) {
        rotation_method = 1;
    } else if (context->output_stream_rotation == 180) {
        rotation_method = 2;
    } else if (context->output_stream_rotation == 270) {
        rotation_method = 3;
    } else if (context->output_stream_rotation) {
        M_ERROR("Rotation can only be 0, 90, 180, or 270, not %u\n",
                context->output_stream_rotation);
        return NULL;
    }

    // Create all of the needed elements
    create_elements(context);

    // Verify that all needed elements were created
    if (verify_element_creation(context)) return NULL;

    if(context->input_format == IMAGE_FORMAT_H264){
        video_caps = gst_caps_new_simple("video/x-h264",
                                        "width", G_TYPE_INT, context->output_stream_width,
                                        "height", G_TYPE_INT, context->output_stream_height,
                                        "profile", G_TYPE_STRING, "baseline",
                                        "stream-format", G_TYPE_STRING, "byte-stream",
                                        "alignment", G_TYPE_STRING, "nal",
                                        NULL);
    } else if(context->input_format == IMAGE_FORMAT_H265){
        video_caps = gst_caps_new_simple("video/x-h265",
                                        "width", G_TYPE_INT, context->output_stream_width,
                                        "height", G_TYPE_INT, context->output_stream_height,
                                        "profile", G_TYPE_STRING, "baseline",
                                        "stream-format", G_TYPE_STRING, "byte-stream",
                                        "alignment", G_TYPE_STRING, "nal",
                                        NULL);
    } else {
        // Configure the application source
        video_info = gst_video_info_new();
        if (video_info) {
            M_DEBUG("Made video_info\n");
        } else {
            M_ERROR("Couldn't make video_info\n");
            return NULL;
        }
        gst_video_info_set_format(video_info,
                                  context->input_frame_gst_format,
                                  context->input_frame_width,
                                  context->input_frame_height);
        video_info->size = context->input_frame_size;
        video_info->par_n = context->input_frame_width;
        video_info->par_d = context->input_frame_height;
        video_info->fps_n = context->input_frame_rate;
        video_info->fps_d = 1;
        video_info->chroma_site = GST_VIDEO_CHROMA_SITE_UNKNOWN;

        video_caps = gst_video_info_to_caps(video_info);
        gst_video_info_free(video_info);
        M_DEBUG("Finished setting up video capture\n");
    }

    if ( ! video_caps) {
        M_ERROR("Failed to create video_caps object\n");
        return NULL;
    }

    g_object_set(context->app_source, "caps", video_caps, NULL);
    g_object_set(context->app_source, "format", GST_FORMAT_TIME, NULL);
    g_object_set(context->app_source, "is-live", 1, NULL);

    g_signal_connect(context->app_source, "need-data", G_CALLBACK(start_feed), context);
    g_signal_connect(context->app_source, "enough-data", G_CALLBACK(stop_feed), context);

    g_object_set(context->app_source_filter, "caps", video_caps, NULL);
    gst_caps_unref(video_caps);

    // Configure the image overlay input queue
    g_object_set(context->overlay_queue, "leaky", 1, NULL);
    g_object_set(context->overlay_queue, "max-size-buffers", 100, NULL);

    // Configure the video scaler input queue
    g_object_set(context->scaler_queue, "leaky", 1, NULL);
    g_object_set(context->scaler_queue, "max-size-buffers", 100, NULL);

    // Configure the video converter input queue
    g_object_set(context->converter_queue, "leaky", 1, NULL);
    g_object_set(context->converter_queue, "max-size-buffers", 100, NULL);

    // Configure the video rotator input queue
    g_object_set(context->rotator_queue, "leaky", 1, NULL);
    g_object_set(context->rotator_queue, "max-size-buffers", 100, NULL);

    // Configure the rotator
    g_object_set(context->video_rotate, "method", rotation_method, NULL);

    // Configure the encoder queue
    g_object_set(context->encoder_queue, "leaky", 1, NULL);
    g_object_set(context->encoder_queue, "max-size-buffers", 100, NULL);

    // Configure the h264 parser
    // stream-format: { (string)avc, (string)avc3, (string)byte-stream }
    // alignment: { (string)au, (string)nal }
    // g_object_set(context->h264_parser, "stream-format", "byte-stream", NULL);
    // g_object_set(context->h264_parser, "alignment", "nal", NULL);

    // Configure the OMX encoder
    g_object_set(context->omx_encoder, "control-rate", 1, NULL);
    g_object_set(context->omx_encoder, "target-bitrate",
                 context->output_stream_bitrate, NULL);

    // Configure the RTP input queue
    g_object_set(context->rtp_queue, "leaky", 1, NULL);
    g_object_set(context->rtp_queue, "max-size-buffers", 100, NULL);

    // Configure the RTP payload
    g_object_set(context->rtp_payload, "name", "pay0", NULL);
    g_object_set(context->rtp_payload, "pt", 96, NULL);
    g_object_set(context->rtp_h265_payload, "name", "pay0", NULL);
    g_object_set(context->rtp_h265_payload, "pt", 96, NULL);

    // Configure the caps filter to reflect the output of the OMX encoder
    GstCaps *filtercaps;
    if(context->input_format == IMAGE_FORMAT_H264){
        filtercaps = gst_caps_new_simple("video/x-h264",
                                                "width", G_TYPE_INT, context->output_stream_width,
                                                "height", G_TYPE_INT, context->output_stream_height,
                                                "profile", G_TYPE_STRING, "baseline",
                                                NULL);
    } else if(context->input_format == IMAGE_FORMAT_H265){
        filtercaps = gst_caps_new_simple("video/x-h265",
                                                "width", G_TYPE_INT, context->output_stream_width,
                                                "height", G_TYPE_INT, context->output_stream_height,
                                                "profile", G_TYPE_STRING, "baseline",
                                                NULL);
    } else {
        filtercaps = gst_caps_new_simple("video/x-raw",
                                                "format", G_TYPE_STRING, "NV12",
                                                "width", G_TYPE_INT, context->output_stream_width,
                                                "height", G_TYPE_INT, context->output_stream_height,
                                                "framerate", GST_TYPE_FRACTION,
                                                context->input_frame_rate, 1,
                                                NULL);
        if ( ! filtercaps) {
            M_ERROR("Failed to create filtercaps object\n");
            return NULL;
        }
        g_object_set(context->video_rotate_filter, "caps", filtercaps, NULL);
        gst_caps_unref(filtercaps);

        // Configure the caps filter to reflect the output of the OMX encoder
        filtercaps = gst_caps_new_simple("video/x-h264",
                                        "width", G_TYPE_INT, context->output_stream_width,
                                        "height", G_TYPE_INT, context->output_stream_height,
                                        "profile", G_TYPE_STRING, "baseline",
                                        NULL);
    }
    if ( ! filtercaps) {
        M_ERROR("Failed to create filtercaps object\n");
        return NULL;
    }
    g_object_set(context->rtp_filter, "caps", filtercaps, NULL);
    gst_caps_unref(filtercaps);

    if(context->input_format == IMAGE_FORMAT_H264){
        gst_bin_add_many(GST_BIN(pipeline),
                        context->app_source,
                        context->h264_parser,
                        context->rtp_payload,
                        NULL);
    } else if(context->input_format == IMAGE_FORMAT_H265){
        gst_bin_add_many(GST_BIN(pipeline),
                        context->app_source,
                        context->h265_parser,
                        context->rtp_h265_payload,
                        NULL);
    } else {
        gst_bin_add_many(GST_BIN(pipeline),
                        context->app_source,
                        context->app_source_filter,
                        NULL);
        gst_bin_add_many(GST_BIN(pipeline),
                        context->scaler_queue,
                        context->scaler,
                        context->converter_queue,
                        context->video_converter,
                        context->rotator_queue,
                        context->video_rotate,
                        context->video_rotate_filter,
                        context->encoder_queue,
                        context->omx_encoder,
                        context->rtp_filter,
                        context->rtp_queue,
                        context->rtp_payload,
                        NULL);
    }

    if(context->input_format == IMAGE_FORMAT_H264){
        if ( ! gst_element_link_many(context->app_source,
                                    context->h264_parser,
                                    // context->rtp_filter,
                                    // context->rtp_queue,
                                    context->rtp_payload,
                                    NULL)) {
            M_ERROR("Couldn't finish pipeline linking part 2\n");
            return NULL;
        }
    } else if(context->input_format == IMAGE_FORMAT_H265){
        if ( ! gst_element_link_many(context->app_source,
                                    context->h265_parser,
                                    // context->rtp_filter,
                                    // context->rtp_queue,
                                    context->rtp_h265_payload,
                                    NULL)) {
            M_ERROR("Couldn't finish pipeline linking part 3\n");
            return NULL;
        }
    } else {
        GstElement *last_element = NULL;

        // Link all elements in the pipeline
        if ( ! gst_element_link(context->app_source,
                                context->app_source_filter)) {
            M_ERROR("Couldn't link app_source and app_source_filter\n");
            return NULL;
        }
        M_DEBUG("Linked app source and filter\n");
        last_element = context->app_source_filter;

        if ( ! gst_element_link_many(last_element,
                                    context->scaler_queue,
                                    context->scaler,
                                    context->converter_queue,
                                    context->video_converter,
                                    context->rotator_queue,
                                    context->video_rotate,
                                    context->video_rotate_filter,
                                    NULL)) {
            M_ERROR("Couldn't finish pipeline linking part 1\n");
            return NULL;
        }
        last_element = context->video_rotate_filter;
        if ( ! gst_element_link_many(last_element,
                                     context->encoder_queue,
                                     context->omx_encoder,
                                     context->rtp_filter,
                                     context->rtp_queue,
                                     context->rtp_payload,
                                     NULL)) {
            M_ERROR("Couldn't finish pipeline linking part 4\n");
            return NULL;
        }
    }

    // Set up our bus and callback for messages
    bus = gst_element_get_bus(pipeline);
    if (bus) {
        gst_bus_add_signal_watch(bus);
        g_signal_connect(G_OBJECT(bus), "message::error", (GCallback) error_cb, &context);
        gst_object_unref(bus);
    } else {
        M_ERROR("Could not attach error callback to pipeline\n");
    }

    return pipeline;
}

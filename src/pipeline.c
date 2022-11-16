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

// Simple initialization. Just save a copy of the context pointer.
void pipeline_init(context_data *ctx) {
    context = ctx;
}

static void create_elements(context_data *context) {
    // Just create everything. We'll decide which ones to use later
    context->app_source = gst_element_factory_make("appsrc", "frame_source_mpa");
    context->app_source_filter = gst_element_factory_make("capsfilter", "appsrc_filter");
    context->encoder_queue = gst_element_factory_make("queue", "encoder_queue");
    context->omx_encoder = gst_element_factory_make("omxh264enc", "omx_encoder");
    context->h264_parser = gst_element_factory_make("h264parse", "h264_parser");
    context->rtp_filter = gst_element_factory_make("capsfilter", "rtp_filter");
    context->rtp_queue = gst_element_factory_make("queue", "rtp_queue");
    context->rtp_payload = gst_element_factory_make("rtph264pay", "rtp_payload");
}

static int verify_element_creation(context_data *context) {
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
    if (context->h264_parser) {
        M_DEBUG("Made h264_parser\n");
    } else {
        M_ERROR("Couldn't make h264_parser\n");
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
GstElement *create_custom_element(GstRTSPMediaFactory *factory, const GstRTSPUrl *url) {

    M_DEBUG("Creating media pipeline for RTSP client\n");

    GstElement* new_bin;
    // GstVideoInfo* video_info;
    GstCaps* video_caps;
    GstBus* bus;

    // TODO: Why aren't these factory lock macros available in the header?!?
    // GST_RTSP_MEDIA_FACTORY_LOCK (factory);

    // Create an empty pipeline
    new_bin = gst_pipeline_new(NULL);
    if (new_bin) {
        M_DEBUG("Made empty pipeline\n");
    } else {
        M_ERROR("couldn't make empty pipeline\n");
        return NULL;
    }

    // Create all of the needed elements
    create_elements(context);

    // Verify that all needed elements were created
    if (verify_element_creation(context)) return NULL;

    // Configure the elements

    // Configure the application source
    // video_info = gst_video_info_new();
    // if (video_info) {
    //     M_DEBUG("Made video_info\n");
    // } else {
    //     M_ERROR("couldn't make video_info\n");
    //     return NULL;
    // }
    // gst_video_info_set_format(video_info,
    //                           context->input_frame_gst_format,
    //                           context->input_frame_width,
    //                           context->input_frame_height);
    // video_info->size = context->input_frame_size;
    // video_info->par_n = context->input_frame_width;
    // video_info->par_d = context->input_frame_height;
    // video_info->fps_n = context->input_frame_rate;
    // video_info->fps_d = 1;
    // video_info->chroma_site = GST_VIDEO_CHROMA_SITE_UNKNOWN;
    //
    // video_caps = gst_video_info_to_caps(video_info);
    // gst_video_info_free(video_info);
    // if (video_caps) {
    //     M_DEBUG("Made video_caps\n");
    // } else {
    //     M_ERROR("couldn't make video_caps\n");
    //     return NULL;
    // }

    video_caps = gst_caps_new_simple("video/x-h264",
                                     "width", G_TYPE_INT, context->output_stream_width,
                                     "height", G_TYPE_INT, context->output_stream_height,
                                     "profile", G_TYPE_STRING, "baseline",
                                     "stream-format", G_TYPE_STRING, "byte-stream",
                                     "alignment", G_TYPE_STRING, "nal",
                                     NULL);
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

    // Configure the encoder queue
    g_object_set(context->encoder_queue, "leaky", 1, NULL);
    g_object_set(context->encoder_queue, "max-size-buffers", 100, NULL);

    // Configure the OMX encoder
    g_object_set(context->omx_encoder, "control-rate", 1, NULL);
    g_object_set(context->omx_encoder, "target-bitrate",
                 context->output_stream_bitrate, NULL);

    // Configure the h264 parser
    // stream-format: { (string)avc, (string)avc3, (string)byte-stream }
    // alignment: { (string)au, (string)nal }
    // g_object_set(context->h264_parser, "stream-format", "byte-stream", NULL);
    // g_object_set(context->h264_parser, "alignment", "nal", NULL);

    // Configure the RTP input queue
    g_object_set(context->rtp_queue, "leaky", 1, NULL);
    g_object_set(context->rtp_queue, "max-size-buffers", 100, NULL);

    // Configure the RTP payload
    g_object_set(context->rtp_payload, "name", "pay0", NULL);
    g_object_set(context->rtp_payload, "pt", 96, NULL);

    // Configure the caps filter to reflect the output of the OMX encoder
    GstCaps *filtercaps = gst_caps_new_simple("video/x-h264",
                                              "width", G_TYPE_INT, context->output_stream_width,
                                              "height", G_TYPE_INT, context->output_stream_height,
                                              "profile", G_TYPE_STRING, "baseline",
                                              NULL);
    if ( ! filtercaps) {
        M_ERROR("Failed to create filtercaps object\n");
        return NULL;
    }
    g_object_set(context->rtp_filter, "caps", filtercaps, NULL);
    gst_caps_unref(filtercaps);

    // Put all needed elements into the bin (our pipeline)
    // gst_bin_add_many(GST_BIN(new_bin),
    //                  context->app_source,
    //                  context->app_source_filter,
    //                  NULL);
    //
    // gst_bin_add_many(GST_BIN(new_bin),
    //                  context->encoder_queue,
    //                  context->omx_encoder,
    //                  context->h264_parser,
    //                  context->rtp_filter,
    //                  context->rtp_queue,
    //                  context->rtp_payload,
    //                  NULL);

    gst_bin_add_many(GST_BIN(new_bin),
                     context->app_source,
                     context->h264_parser,
                     context->rtp_payload,
                     NULL);

    // GstElement *last_element = NULL;
    //
    // // Link all elements in the pipeline
    // if ( ! gst_element_link(context->app_source,
    //                         context->app_source_filter)) {
    //     M_ERROR("Couldn't link app_source and app_source_filter\n");
    //     return NULL;
    // }
    // last_element = context->app_source_filter;

    // if (TODO_NEED_ENCODER) {
    //     if ( ! gst_element_link_many(last_element,
    //                                  context->encoder_queue,
    //                                  context->omx_encoder,
    //                                  NULL)) {
    //         M_ERROR("Couldn't finish pipeline linking part 2\n");
    //         return NULL;
    //     }
    //     last_element = context->omx_encoder;
    // }

    if ( ! gst_element_link_many(context->app_source,
                                 context->h264_parser,
                                 // context->rtp_filter,
                                 // context->rtp_queue,
                                 context->rtp_payload,
                                 NULL)) {
        M_ERROR("Couldn't finish pipeline linking part 3\n");
        return NULL;
    }

    // Set up our bus and callback for messages
    bus = gst_element_get_bus(new_bin);
    if (bus) {
        gst_bus_add_signal_watch(bus);
        g_signal_connect(G_OBJECT(bus), "message::error", (GCallback) error_cb, &context);
        gst_object_unref(bus);
    } else {
        M_ERROR("Could not attach error callback to pipeline\n");
    }

    // GST_RTSP_MEDIA_FACTORY_UNLOCK (factory);

    return new_bin;
}

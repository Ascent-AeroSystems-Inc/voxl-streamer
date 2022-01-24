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
#include "context.h"

static context_data *context;

// Simple initialization. Just save a copy of the context pointer.
void pipeline_init(context_data *ctx) {
    context = ctx;
}

static void create_elements(context_data *context) {
    // Just create everything. We'll decide which ones to use later
    context->test_source = gst_element_factory_make("videotestsrc", "frame_source_test");
    context->test_caps_filter = gst_element_factory_make("capsfilter", "test_caps_filter");
    context->app_source = gst_element_factory_make("appsrc", "frame_source_mpa");
    context->app_source_filter = gst_element_factory_make("capsfilter", "appsrc_filter");
    context->uvc_source = gst_element_factory_make("v4l2src", "frame_source_v4l2");
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
    context->rtp_filter = gst_element_factory_make("capsfilter", "rtp_filter");
    context->rtp_queue = gst_element_factory_make("queue", "rtp_queue");
    context->rtp_payload = gst_element_factory_make("rtph264pay", "rtp_payload");

    // We can only create a SW h264 encoder if it is supported in the system image.
    if (context->use_sw_h264) {
        context->h264_encoder = gst_element_factory_make("x264enc", "h264_encoder");
    }
}

static int verify_element_creation(context_data *context) {
    if (context->test_source) {
        if (context->debug) printf("Made test_source\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make test_source\n");
        return -1;
    }
    if (context->test_caps_filter) {
        if (context->debug) printf("Made test_caps_filter\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make test_caps_filter\n");
        return -1;
    }
    if (context->app_source) {
        if (context->debug) printf("Made app_source\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make app_source\n");
        return -1;
    }
    if (context->app_source_filter) {
        if (context->debug) printf("Made app_source_filter\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make app_source_filter\n");
        return -1;
    }
    if (context->uvc_source) {
        if (context->debug) printf("Made uvc_source\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make uvc_source\n");
        return -1;
    }
    if (context->overlay_queue) {
        if (context->debug) printf("Made overlay_queue\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make overlay_queue\n");
        return -1;
    }
    if (context->image_overlay) {
        if (context->debug) printf("Made image_overlay\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make image_overlay\n");
        return -1;
    }
    if (context->scaler_queue) {
        if (context->debug) printf("Made scaler_queue\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make scaler_queue\n");
        return -1;
    }
    if (context->scaler) {
        if (context->debug) printf("Made scaler\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make scaler\n");
        return -1;
    }
    if (context->converter_queue) {
        if (context->debug) printf("Made converter_queue\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make converter_queue\n");
        return -1;
    }
    if (context->video_converter) {
        if (context->debug) printf("Made video_converter\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make video_converter\n");
        return -1;
    }
    if (context->rotator_queue) {
        if (context->debug) printf("Made rotator_queue\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make rotator_queue\n");
        return -1;
    }
    if (context->video_rotate) {
        if (context->debug) printf("Made video_rotate\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make video_rotate\n");
        return -1;
    }
    if (context->video_rotate_filter) {
        if (context->debug) printf("Made video_rotate_filter\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make video_rotate_filter\n");
        return -1;
    }
    if (context->encoder_queue) {
        if (context->debug) printf("Made encoder_queue\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make encoder_queue\n");
        return -1;
    }
    if (context->omx_encoder) {
        if (context->debug) printf("Made omx_encoder\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make omx_encoder\n");
        return -1;
    }
    if (context->rtp_filter) {
        if (context->debug) printf("Made rtp_filter\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make rtp_filter\n");
        return -1;
    }
    if (context->rtp_queue) {
        if (context->debug) printf("Made rtp_queue\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make rtp_queue\n");
        return -1;
    }
    if (context->rtp_payload) {
        if (context->debug) printf("Made rtp_payload\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make rtp_payload\n");
        return -1;
    }

    // We can only create a SW h264 encoder if it is supported in the system image.
    if (context->use_sw_h264) {
        if (context->h264_encoder) {
            if (context->debug) printf("Made h264_encoder\n");
        } else {
            fprintf(stderr, "ERROR: couldn't make h264_encoder\n");
            return -1;
        }
    }

    return 0;
}

// This is a callback to indicate when the pipeline needs data
static void start_feed(GstElement *source, guint size, context_data *data) {
    if (data->frame_debug) printf("*** Start feeding ***\n");
    data->need_data = 1;
}

// This is a callback to indicate when the pipeline no longer needs data.
// This isn't called with our pipeline because we feed frames at the correct rate.
static void stop_feed(GstElement *source, context_data *data) {
    if (data->frame_debug) printf("*** Stop feeding ***\n");
    data->need_data = 0;
}

// These are the callbacks to let us know of bus messages
static void warn_cb(GstBus *bus, GstMessage *msg, context_data *data) {
    GError *err;
    gchar *debug_info;

    printf("*** Got gstreamer warning callback ***\n");

    /* Print error details on the screen */
    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Warning received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);
}

static void error_cb(GstBus *bus, GstMessage *msg, context_data *data) {
    GError *err;
    gchar *debug_info;

    printf("*** Got gstreamer error callback ***\n");

    /* Print error details on the screen */
    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);
}

// This is used to override the standard element creator that relies on having
// a gstreamer launch line. This allows us to use our custom pipeline.
GstElement *create_custom_element(GstRTSPMediaFactory *factory, const GstRTSPUrl *url) {

    if (context->debug) printf("Creating media pipeline for RTSP client\n");

    int rc = 0;
    GstElement* new_bin;
    GstVideoInfo* video_info;
    GstCaps* video_caps;
    gboolean success;
    GstBus* bus;

    // TODO: Why aren't these factory lock macros available in the header?!?
    // GST_RTSP_MEDIA_FACTORY_LOCK (factory);

    // Create an empty pipeline
    new_bin = gst_pipeline_new(NULL);
    if (new_bin) {
        if (context->debug) printf("Made empty pipeline\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make empty pipeline\n");
        return NULL;
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
        fprintf(stderr, "ERROR: Rotation can only be 0, 90, 180, or 270, not %u\n",
                context->output_stream_rotation);
        return NULL;
    }

    // Create all of the needed elements
    create_elements(context);

    // Verify that all needed elements were created
    if (verify_element_creation(context)) return NULL;

    // Configure the elements

    // Make the video test source act as if it was a live feed like our
    // camera
    g_object_set(context->test_source, "is-live", 1, NULL);

    // Setup the proper caps for the videotestsrc
    GstCaps *test_filtercaps = gst_caps_new_simple("video/x-raw",
                                                   "format", G_TYPE_STRING,
                                                   context->input_frame_caps_format,
                                                   "width", G_TYPE_INT, context->input_frame_width,
                                                   "height", G_TYPE_INT, context->input_frame_height,
                                                   "framerate", GST_TYPE_FRACTION,
                                                   context->output_frame_rate, 1,
                                                   NULL);
    if ( ! test_filtercaps) {
        fprintf(stderr, "ERROR: Failed to make test_filtercaps\n");
        return NULL;
    }
    g_object_set(context->test_caps_filter, "caps", test_filtercaps, NULL);
    gst_caps_unref(test_filtercaps);

    // Configure the application source
    video_info = gst_video_info_new();
    if (video_info) {
        if (context->debug) printf("Made video_info\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make video_info\n");
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
    if (video_caps) {
        if (context->debug) printf("Made video_caps\n");
    } else {
        fprintf(stderr, "ERROR: couldn't make video_caps\n");
        return NULL;
    }
    g_object_set(context->app_source, "caps", video_caps, NULL);
    g_object_set(context->app_source, "format", GST_FORMAT_TIME, NULL);
    g_object_set(context->app_source, "is-live", 1, NULL);

    g_signal_connect(context->app_source, "need-data", G_CALLBACK(start_feed), context);
    g_signal_connect(context->app_source, "enough-data", G_CALLBACK(stop_feed), context);

    g_object_set(context->app_source_filter, "caps", video_caps, NULL);
    gst_caps_unref(video_caps);

    // Configure UVC source.
    // TODO: This is deprecated and should be removed. voxl-uvc-server is now
    //       the supported way to use UVC cameras.
    if (context->interface == UVC_INTERFACE) {
        g_object_set(context->uvc_source, "device", context->uvc_device_name, NULL);
    }

    // Configure the image overlay input queue
    g_object_set(context->overlay_queue, "leaky", 1, NULL);
    g_object_set(context->overlay_queue, "max-size-buffers", 100, NULL);

    // Configure the image overlay element
    if (context->overlay_flag) {
        g_object_set(context->image_overlay, "location",
                     context->overlay_frame_location, NULL);
        g_object_set(context->image_overlay, "offset-x",
                     context->overlay_offset_x, NULL);
        g_object_set(context->image_overlay, "offset-y",
                     context->overlay_offset_y, NULL);
    }

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

    // Configure the software h264 encoder
    // Check out this: https://stackoverflow.com/questions/30730082/realtime-zero-latency-video-stream-what-codec-parameters-to-use
    // for more tips on tuning for low latency
    if (context->use_sw_h264) {
        g_object_set(context->h264_encoder, "bitrate",
                     (context->output_stream_bitrate / 1000), NULL);
        g_object_set(context->h264_encoder, "key-int-max", 10, NULL);
        g_object_set(context->h264_encoder, "speed-preset", 1, NULL); // ultrafast
        g_object_set(context->h264_encoder, "tune", 4, NULL); // zerolatency
    }

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

    // Configure the caps filter to rotate the image
    if (context->debug) printf("Output %ux%u, %u fps\n", context->output_stream_width, context->output_stream_height, context->output_frame_rate);
    GstCaps *filtercaps = gst_caps_new_simple("video/x-raw",
                                              "format", G_TYPE_STRING, "NV12",
                                              "width", G_TYPE_INT, context->output_stream_width,
                                              "height", G_TYPE_INT, context->output_stream_height,
                                              "framerate", GST_TYPE_FRACTION,
                                              context->input_frame_rate, 1,
                                              NULL);
    if ( ! filtercaps) {
        fprintf(stderr, "Failed to create filtercaps object\n");
        return NULL;
    }
    g_object_set(context->video_rotate_filter, "caps", filtercaps, NULL);
    gst_caps_unref(filtercaps);

    // Configure the caps filter to reflect the output of the OMX encoder
    filtercaps = gst_caps_new_simple("video/x-h264",
                                     "width", G_TYPE_INT, context->output_stream_width,
                                     "height", G_TYPE_INT, context->output_stream_height,
                                     "profile", G_TYPE_STRING, "high",
                                     NULL);
    if ( ! filtercaps) {
        fprintf(stderr, "Failed to create filtercaps object\n");
        return NULL;
    }
    g_object_set(context->rtp_filter, "caps", filtercaps, NULL);
    gst_caps_unref(filtercaps);

    // Put all needed elements into the bin (our pipeline)
    if (context->interface == TEST_INTERFACE) {
        gst_bin_add_many(GST_BIN(new_bin),
                         context->test_source,
                         context->test_caps_filter,
                         NULL);
    } else if (context->interface == MPA_INTERFACE) {
        gst_bin_add_many(GST_BIN(new_bin),
                         context->app_source,
                         context->app_source_filter,
                         NULL);
    } else if (context->interface == UVC_INTERFACE) {
        gst_bin_add_many(GST_BIN(new_bin),
                         context->uvc_source,
                         NULL);
    }

    gst_bin_add_many(GST_BIN(new_bin),
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

    if (context->use_sw_h264) {
        gst_bin_add_many(GST_BIN(new_bin),
                         context->h264_encoder,
                         NULL);
    }

    if (context->overlay_flag) {
        gst_bin_add_many(GST_BIN(new_bin),
                         context->overlay_queue,
                         context->image_overlay,
                         NULL);
    }

    GstElement *last_element = NULL;

    // Link all elements in the pipeline
    if (context->interface == TEST_INTERFACE) {
        success = gst_element_link(context->test_source,
                                   context->test_caps_filter);
        if ( ! success) {
            fprintf(stderr, "ERROR: couldn't link test_source and test_caps_filter\n");
            return NULL;
        }
        last_element = context->test_caps_filter;
    } else  if (context->interface == MPA_INTERFACE) {
        success = gst_element_link(context->app_source,
                                   context->app_source_filter);
        if ( ! success) {
            fprintf(stderr, "ERROR: couldn't link app_source and app_source_filter\n");
            return NULL;
        }
        last_element = context->app_source_filter;
    } else if (context->interface == UVC_INTERFACE) {
        last_element = context->uvc_source;
    }

    success = gst_element_link_many(last_element,
                                    context->scaler_queue,
                                    context->scaler,
                                    context->converter_queue,
                                    context->video_converter,
                                    context->rotator_queue,
                                    context->video_rotate,
                                    context->video_rotate_filter,
                                    NULL);
    if ( ! success) {
        fprintf(stderr, "ERROR: couldn't finish pipeline linking part 1\n");
        return NULL;
    }
    last_element = context->video_rotate_filter;

    // Link in optional image overlay element
    if (context->overlay_flag) {
        printf("Linking in overlay\n");
        success = gst_element_link_many(last_element,
                                        context->overlay_queue,
                                        context->image_overlay,
                                        NULL);
        if ( ! success) {
            fprintf(stderr, "ERROR: couldn't link in overlay elements\n");
            return NULL;
        }
        last_element = context->image_overlay;
    }

    if (context->use_sw_h264) {
        success = gst_element_link_many(last_element,
                                        context->encoder_queue,
                                        context->h264_encoder,
                                        context->rtp_filter,
                                        // TODO: Why does the rtp_queue cause latency issues???
                                        // context->rtp_queue,
                                        context->rtp_payload,
                                        NULL);
    } else {
        success = gst_element_link_many(last_element,
                                        context->encoder_queue,
                                        context->omx_encoder,
                                        context->rtp_filter,
                                        context->rtp_queue,
                                        context->rtp_payload,
                                        NULL);
    }
    if ( ! success) {
        fprintf(stderr, "ERROR: couldn't finish pipeline linking part 2\n");
        return NULL;
    }

    // Set up our bus and callback for messages
    bus = gst_element_get_bus(new_bin);
    if (bus) {
        gst_bus_add_signal_watch(bus);
        g_signal_connect(G_OBJECT(bus), "message::error", (GCallback) error_cb, &context);
        gst_object_unref(bus);
    } else {
        if (context->debug) printf("WARNING: Could not attach error callback to pipeline\n");
    }

    // GST_RTSP_MEDIA_FACTORY_UNLOCK (factory);

    return new_bin;
}

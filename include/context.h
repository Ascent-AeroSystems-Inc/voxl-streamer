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

/**
 * @file context.h
 *
 * This file contains the definition of the main data structure for the
 * application.
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <modal_pipe.h>
#include <pthread.h>
#include <gst/rtsp-server/rtsp-server.h>

// Definition of the default port used by the RTSP server
#define MAX_RTSP_PORT_SIZE 8
#define DEFAULT_RTSP_PORT "8900"

#define MAX_OVERLAY_FILE_NAME_STRING_LENGTH 64
#define MAX_IMAGE_FORMAT_STRING_LENGTH 16

// Structure to contain all needed information, so we can pass it to callbacks
typedef struct _context_data {

    GstElement *test_source;
    GstElement *test_caps_filter;
    GstElement *overlay_queue;
    GstElement *image_overlay;
    GstElement *app_source;
    GstElement *app_source_filter;
    GstElement *scaler_queue;
    GstElement *scaler;
    GstElement *converter_queue;
    GstElement *video_converter;
    GstElement *rotator_queue;
    GstElement *video_rotate;
    GstElement *video_rotate_filter;
    GstElement *encoder_queue;
    GstElement *h264_encoder;
    GstElement *omx_encoder;
    GstElement *rtp_filter;
    GstElement *rtp_queue;
    GstElement *rtp_payload;

    GstRTSPServer *rtsp_server;
    uint32_t num_rtsp_clients;

    uint32_t input_parameters_initialized;
    uint32_t input_frame_width;
    uint32_t input_frame_height;
    uint32_t input_frame_size;
    uint32_t input_frame_rate;

    char input_frame_format[MAX_IMAGE_FORMAT_STRING_LENGTH];
    char input_frame_caps_format[MAX_IMAGE_FORMAT_STRING_LENGTH];
    GstVideoFormat input_frame_gst_format;

    char input_pipe_name[MODAL_PIPE_MAX_PATH_LEN];
    char rtsp_server_port[MAX_RTSP_PORT_SIZE];

    uint32_t output_stream_width;
    uint32_t output_stream_height;
    uint32_t output_stream_bitrate;
    uint32_t output_stream_rotation;
    uint32_t output_frame_rate;
    uint32_t output_frame_decimator;

    uint32_t input_frame_number;
    uint32_t output_frame_number;
    guint64 initial_timestamp;
    guint64 last_timestamp;

    FILE *output_fp;

    int use_sw_h264;

    int overlay_flag;
    int overlay_offset_x;
    int overlay_offset_y;
    char overlay_frame_location[MAX_OVERLAY_FILE_NAME_STRING_LENGTH];

    volatile int need_data;

    pthread_mutex_t lock;

} context_data;

#endif // CONTEXT_H

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

#include <pthread.h>
#include <gst/rtsp-server/rtsp-server.h>

// These are the supported frame interfaces.
// MPA is the ModalAI Pipe Architecture.
// PIPE is just a Linux named pipe.
// UVC is for webcams, etc.
enum interface_type {
    MPA_INTERFACE,
//    UVC_INTERFACE,
    TEST_INTERFACE,
    MAX_INTERFACES
};

#define MAX_IMAGE_FORMAT_STRING_LENGTH 16
#define MAX_INPUT_PIPE_NAME_STRING_LENGTH 256

// Structure to contain all needed information, so we can pass it to callbacks
typedef struct _context_data {

    enum interface_type interface;

    GstElement *test_source;
    GstElement *test_caps_filter;
    GstElement *app_source;
    GstElement *parser_queue;
    GstElement *raw_video_parser;
    GstElement *scaler_queue;
    GstElement *scaler;
    GstElement *caps_filter_1;
    GstElement *converter_queue;
    GstElement *video_converter;
    GstElement *caps_filter_2;
    GstElement *rotator_queue;
    GstElement *video_rotate;
    GstElement *video_rotate_filter;
    GstElement *encoder_queue;
    GstElement *omx_encoder;
    GstElement *caps_filter_3;
    GstElement *rtp_queue;
    GstElement *rtp_payload;

    GstRTSPServer *rtsp_server;
    uint32_t num_rtsp_clients;

    uint32_t input_parameters_initialized;
    uint32_t input_frame_width;
    uint32_t input_frame_height;
    uint32_t input_frame_size;

    char input_frame_format[MAX_IMAGE_FORMAT_STRING_LENGTH];
    char input_frame_caps_format[MAX_IMAGE_FORMAT_STRING_LENGTH];
    GstVideoFormat input_frame_gst_format;

    char input_pipe_name[MAX_INPUT_PIPE_NAME_STRING_LENGTH];

    uint32_t output_stream_width;
    uint32_t output_stream_height;
    uint32_t output_stream_bitrate;
    uint32_t output_stream_rotation;
    uint32_t output_frame_rate;
    uint32_t output_frame_decimator;

    uint32_t input_frame_number;
    uint32_t output_frame_number;

    FILE *output_fp;

    int debug;
    int print_pad_caps;

    volatile int need_data;
    volatile int running;

    pthread_mutex_t lock;

} context_data;

#endif // CONTEXT_H

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
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <modal_json.h>
#include <modal_journal.h>
#include <modal_pipe_client.h>
#include <gst/video/video.h>
#include "configuration.h"

#define CONFIG_FILENAME "/etc/modalai/voxl-streamer.conf"

int configure_frame_format(const char *format, context_data *ctx) {
    // Prepare configuration based on input frame format
    if ( ! strcmp(format, "yuyv")) {
        strcpy(ctx->input_frame_caps_format, "YUY2");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_YUY2;
        ctx->input_frame_size = ctx->input_frame_width * \
                                   ctx->input_frame_height * 2;
    } else if ( ! strcmp(format, "uyvy")) {
        strcpy(ctx->input_frame_caps_format, "UYVY");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_UYVY;
        ctx->input_frame_size = ctx->input_frame_width * \
                                   ctx->input_frame_height * 2;
    } else if ( ! strcmp(format, "nv12")) {
        strcpy(ctx->input_frame_caps_format, "NV12");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_NV12;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                    ctx->input_frame_height) + \
                                   (ctx->input_frame_width * \
                                    ctx->input_frame_height) / 2;
    } else if ( ! strcmp(format, "nv21")) {
        strcpy(ctx->input_frame_caps_format, "NV21");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_NV21;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                    ctx->input_frame_height) + \
                                   (ctx->input_frame_width * \
                                    ctx->input_frame_height) / 2;
    } else if ( ! strcmp(format, "gray8")) {
        strcpy(ctx->input_frame_caps_format, "GRAY8");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_GRAY8;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                 ctx->input_frame_height);
    } else if ( ! strcmp(format, "yuv420")) {
        strcpy(ctx->input_frame_caps_format, "I420");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_I420;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                    ctx->input_frame_height) + \
                                   (ctx->input_frame_width * \
                                    ctx->input_frame_height) / 2;
    } else if ( ! strcmp(format, "rgb")) {
        strcpy(ctx->input_frame_caps_format, "RGB");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_RGB;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                    ctx->input_frame_height * 3);
    } else if ( ! strcmp(format, "gray16")) {
        strcpy(ctx->input_frame_caps_format, "GRAY16");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_GRAY16_BE;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                 ctx->input_frame_height *2);
    } else {
        M_ERROR("Unsupported input file format %s\n",
                ctx->input_frame_format);
        return -1;
    }

    return 0;
}

int prepare_configuration(context_data *ctx) {

    cJSON* config;

    // Try to open the configuration file for further processing
    if ( ! (config = json_read_file(CONFIG_FILENAME))) {
        M_ERROR("Failed to open configuration file: %s\n", CONFIG_FILENAME);
        return -1;
    }

    // We are using MPA, just need to know what pipe name to use.
    // The input frame parameters will come over the pipe as meta data.
    if (json_fetch_string(config, "input-pipe", ctx->input_pipe_name, MODAL_PIPE_MAX_PATH_LEN)) {
        M_WARN("Failed to get default pipe name from configuration file\n");
    }

    if (json_fetch_int(config, "bitrate", (int*) &ctx->output_stream_bitrate)) {
        M_WARN("Failed to get default bitrate from configuration file\n");
    }

    if (json_fetch_int_with_default(config, "rotation", (int*) &ctx->output_stream_rotation, 0)) {
        // Rotation is an optional parameter
        ctx->output_stream_rotation = 0;
    }

    if (json_fetch_int_with_default(config, "decimator", (int*) &ctx->output_frame_decimator, 1)) {
        // Frame decimation is an optional parameter
        ctx->output_frame_decimator = 1;
    }

    cJSON_Delete(config);

    // MPA will configure the input parameters based on meta data
    ctx->input_parameters_initialized = 0;

    return 0;
}

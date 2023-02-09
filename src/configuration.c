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

int configure_frame_format(const int format, context_data *ctx) {
    // Prepare configuration based on input frame format
    switch(format) {
        case IMAGE_FORMAT_RAW8:
        case IMAGE_FORMAT_STEREO_RAW8:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_GRAY8;
            ctx->input_frame_size = (ctx->input_frame_width *
                                     ctx->input_frame_height);
            break;
        case IMAGE_FORMAT_NV12:
        case IMAGE_FORMAT_STEREO_NV12:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_NV12;
            ctx->input_frame_size = (ctx->input_frame_width *
                                        ctx->input_frame_height) +
                                       (ctx->input_frame_width *
                                        ctx->input_frame_height) / 2;
            break;
        case IMAGE_FORMAT_H264:
            // Special case. Don't need to set up the context for it.
            break;
        case IMAGE_FORMAT_RAW16:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_GRAY16_BE;
            ctx->input_frame_size = (ctx->input_frame_width *
                                     ctx->input_frame_height *2);
            break;
        case IMAGE_FORMAT_NV21:
        case IMAGE_FORMAT_STEREO_NV21:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_NV21;
            ctx->input_frame_size = (ctx->input_frame_width *
                                        ctx->input_frame_height) +
                                       (ctx->input_frame_width *
                                        ctx->input_frame_height) / 2;
            break;
        case IMAGE_FORMAT_YUV422:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_YUY2;
            ctx->input_frame_size = ctx->input_frame_width *
                                       ctx->input_frame_height * 2;
            break;
        case IMAGE_FORMAT_YUV420:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_I420;
            ctx->input_frame_size = (ctx->input_frame_width *
                                        ctx->input_frame_height) +
                                       (ctx->input_frame_width *
                                        ctx->input_frame_height) / 2;
            break;
        case IMAGE_FORMAT_RGB:
        case IMAGE_FORMAT_STEREO_RGB:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_RGB;
            ctx->input_frame_size = (ctx->input_frame_width *
                                        ctx->input_frame_height * 3);
            break;
        case IMAGE_FORMAT_YUV422_UYVY:
            ctx->input_frame_gst_format = GST_VIDEO_FORMAT_UYVY;
            ctx->input_frame_size = ctx->input_frame_width *
                                       ctx->input_frame_height * 2;
            break;
        // case IMAGE_FORMAT_H265:
        // case IMAGE_FORMAT_JPG:
        // case IMAGE_FORMAT_FLOAT32:
        default:
            M_ERROR("Unsupported input frame format: %s\n", pipe_image_format_to_string(format));
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

    int tmp;
    if (json_fetch_int(config, "port", &tmp)) {
        M_WARN("Failed to get port from configuration file\n");
    } else {
        snprintf(ctx->rtsp_server_port, 7 , "%u", tmp);
    }


    // Optional
    json_fetch_int_with_default(config, "decimator", (int*) &ctx->output_frame_decimator, 1);

    cJSON_Delete(config);

    // MPA will configure the input parameters based on meta data
    ctx->input_parameters_initialized = 0;

    return 0;
}

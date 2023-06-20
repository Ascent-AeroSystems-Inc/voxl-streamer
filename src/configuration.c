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

#define CONF_FILE "/etc/modalai/voxl-streamer.conf"
#define DEFAULT_INPUT_PIPE "hires_small_encoded"

#define CONFIG_FILE_HEADER "\
/**\n\
 * This file contains configuration parameters for voxl-streamer.\n\
 *\n\
 * input-pipe:\n\
 *    This is the MPA pipe to subscribe to. Ideally this is a pipe\n\
 *    with an H264 stream such as the default: hires-stream\n\
 *    However, you can point voxl-streamer to a RAW8 uncompressed stream such as\n\
 *    tracking or qvio_overlay. In this case voxl-streamer will encode the stream\n\
 *    at the bitrate provided in the birtate field.\n\
 *    if input-pipe is already H264 then the bitrate config here is ignored and you\n\
 *    should set the bitrate in voxl-camera-server.conf!!!!!\n\
 *\n\
 * bitrate:\n\
 *    Bitrate to compress raw MPA streams to.\n\
 *    Ignored for H264 streams like hires_stream\n\
 *\n\
 * decimator:\n\
 *    Decimate frames to drop framerate of RAW streams.\n\
 *    Ignored for H264 streams like hires_stream\n\
 *\n\
 * port:\n\
 *    port to serve rtsp stream on, default is 8900\n\
 *\n\
 */\n"



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
        case IMAGE_FORMAT_H265:
            // Special case. Don't need to set up the context for it.
            break;
        // case IMAGE_FORMAT_JPG:
        // case IMAGE_FORMAT_FLOAT32:
        default:
            M_ERROR("Unsupported input frame format: %s\n", pipe_image_format_to_string(format));
            return -1;
    }

    return 0;
}

int config_file_read(context_data *ctx) {

    int ret = json_make_empty_file_with_header_if_missing(CONF_FILE, CONFIG_FILE_HEADER);
    if(ret < 0) return -1;
    else if(ret>0) fprintf(stderr, "Creating new config file: %s\n", CONF_FILE);

    cJSON* parent = json_read_file(CONF_FILE);
    if(parent==NULL) return -1;

    json_fetch_string_with_default(parent, "input-pipe", ctx->input_pipe_name, MODAL_PIPE_MAX_PATH_LEN, DEFAULT_INPUT_PIPE);
    json_fetch_int_with_default(parent, "bitrate", (int*) &ctx->output_stream_bitrate, 1000000);
    json_fetch_int_with_default(parent, "rotation", (int*) &ctx->output_stream_rotation, 0);
    json_fetch_int_with_default(parent, "decimator", (int*) &ctx->output_frame_decimator, 1);

    int tmp;
    json_fetch_int_with_default(parent, "port", &tmp, 8900);
    snprintf(ctx->rtsp_server_port, 7 , "%u", tmp);


    if(json_get_parse_error_flag()){
        fprintf(stderr, "failed to parse config file %s\n", CONF_FILE);
        cJSON_Delete(parent);
        return -1;
    }

    // write modified data to disk if neccessary
    if(json_get_modified_flag()){
        M_PRINT("writing %s to disk\n", CONF_FILE);
        json_write_to_file_with_header(CONF_FILE, parent, CONFIG_FILE_HEADER);
    }
    cJSON_Delete(parent);

    ctx->input_parameters_initialized = 0;

    return 0;
}

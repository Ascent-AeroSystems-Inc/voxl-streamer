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
#include <voxl_camera_server.h>
#include <modal_json.h>
#include <gst/video/video.h>
#include "configuration.h"

int prepare_configuration(const char* config_file_name, const char* config_name,
                          context_data *ctx) {

    cJSON* config_file;
    cJSON* config_object;
    char config_object_name[MAX_CONFIG_OBJECT_STRING_LENGTH];
    char interface_type_name[MAX_INTERFACE_NAME_STRING_LENGTH];
    cJSON* input_config;
    cJSON* input_frame_config;
    cJSON* output_config;
    cJSON* output_stream_config;
    int rc = 0;

    // Validate the configuration file name
    if (( ! config_file_name) || ( ! strlen(config_file_name))) {
        fprintf(stderr, "No valid configuration file name provided\n");
        return -1;
    }

    // Try to open the configuration file for further processing
    config_file = json_read_file(config_file_name);
    if ( ! config_file) {
        fprintf(stderr, "Failed to open configuration file %s\n", config_file_name);
        return -1;
    } else {
        printf("Using configuration file %s\n", config_file_name);
    }

    // Use the configuration name that was passed in. If one was not provided
    // then try to read it from the configuration file.
    if ((config_name) && (strlen(config_name))) {
        strncpy(config_object_name, config_name, MAX_CONFIG_OBJECT_STRING_LENGTH);
    } else {
        rc = json_fetch_string(config_file, "configuration",
                               config_object_name,
                               MAX_CONFIG_OBJECT_STRING_LENGTH);
        if (rc) {
            fprintf(stderr, "Failed to get configuration name from configuration file\n");
            cJSON_Delete(config_file);
            return -1;
        }
    }

    config_object = json_fetch_object(config_file, config_object_name);
    if ( ! config_object) {
        fprintf(stderr,
                "Failed to get configuration object %s from configuration file\n",
                config_object_name);
        cJSON_Delete(config_file);
        return -1;
    }

    input_config = json_fetch_object(config_object, "input");
    if ( ! input_config) {
        fprintf(stderr, "Failed to get input object from configuration object\n");
        cJSON_Delete(config_file);
        return -1;
    }
    input_frame_config = json_fetch_object(input_config, "frame");
    if ( ! input_frame_config) {
        fprintf(stderr, "Failed to get input frame object from configuration file\n");
        cJSON_Delete(config_file);
        return -1;
    }
    output_config = json_fetch_object(config_object, "output");
    if ( ! output_config) {
        fprintf(stderr, "Failed to get output object from configuration file\n");
        cJSON_Delete(config_file);
        return -1;
    }
    output_stream_config = json_fetch_object(output_config, "stream");
    if ( ! output_stream_config) {
        fprintf(stderr, "Failed to get output stream object from configuration file\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_string(input_config, "interface",
                           interface_type_name,
                           MAX_INTERFACE_NAME_STRING_LENGTH);
    if (rc) {
        fprintf(stderr, "Failed to get interface type from configuration file\n");
        cJSON_Delete(config_file);
        return -1;
    }

    if ( ! strcmp(interface_type_name, "mpa")) {
        ctx->interface = MPA_INTERFACE;
    } else if ( ! strcmp(interface_type_name, "pipe")) {
        ctx->interface = PIPE_INTERFACE;
    } else {
        fprintf(stderr, "Invalid interface type in configuration file: %s\n",
                interface_type_name);
        cJSON_Delete(config_file);
        return -1;
    }

    if (ctx->interface == MPA_INTERFACE) {
        char mpa_camera_name[MAX_INPUT_PIPE_NAME_STRING_LENGTH];
        rc = json_fetch_string(input_config, "mpa-camera",
                               mpa_camera_name,
                               MAX_INPUT_PIPE_NAME_STRING_LENGTH);
        if (rc) {
            fprintf(stderr, "Failed to get pipe name from configuration file\n");
            cJSON_Delete(config_file);
            return -1;
        }

        if ( ! strcmp(mpa_camera_name, "hires")) {
            strcpy(ctx->input_pipe_name, HIRES_PREVIEW_CHANNEL_DIR);
        } else if ( ! strcmp(mpa_camera_name, "tracking")) {
            strcpy(ctx->input_pipe_name, MONO_CHANNEL_DIR);
        } else if ( ! strcmp(mpa_camera_name, "stereo")) {
            strcpy(ctx->input_pipe_name, STEREO_CHANNEL_DIR);
        } else {
            fprintf(stderr, "Invalid MPA camera in configuration file: %s\n",
                    mpa_camera_name);
            cJSON_Delete(config_file);
            return -1;
        }
    } else if (ctx->interface == PIPE_INTERFACE) {
        rc = json_fetch_string(input_config, "pipe-name",
                               ctx->input_pipe_name,
                               MAX_INPUT_PIPE_NAME_STRING_LENGTH);
        if (rc) {
            fprintf(stderr, "Failed to get pipe name from configuration file\n");
            cJSON_Delete(config_file);
            return -1;
        }
    }

    rc = json_fetch_int(input_frame_config, "width",
                         (int*) &ctx->input_frame_width);
    if (rc) {
        fprintf(stderr, "Failed to get width of the input frame\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_int(input_frame_config, "height",
                         (int*) &ctx->input_frame_height);
    if (rc) {
        fprintf(stderr, "Failed to get height of the input frame\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_int(input_frame_config, "rate",
                         (int*) &ctx->input_frame_rate);
    if (rc) {
        fprintf(stderr, "Failed to get frame rate of the input\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_string(input_frame_config, "format",
                             ctx->input_frame_format,
                             MAX_IMAGE_FORMAT_STRING_LENGTH);
    if (rc) {
        fprintf(stderr, "Failed to get format of the input frames\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_int(output_stream_config, "width",
                         (int*) &ctx->output_stream_width);
    if (rc) {
        fprintf(stderr, "Failed to get width of the output frame\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_int(output_stream_config, "height",
                         (int*) &ctx->output_stream_height);
    if (rc) {
        fprintf(stderr, "Failed to get height of the output frame\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_int(output_stream_config, "bitrate",
                         (int*) &ctx->output_stream_bitrate);
    if (rc) {
        fprintf(stderr, "Failed to get bitrate of the output stream\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_int(output_stream_config, "rotation",
                         (int*) &ctx->output_stream_rotation);
    if (rc) {
        fprintf(stderr, "Failed to get rotation of the output stream\n");
        cJSON_Delete(config_file);
        return -1;
    }

    rc = json_fetch_int(output_stream_config, "rate",
                         (int*) &ctx->output_frame_rate);
    if (rc) {
        fprintf(stderr, "Failed to get frame rate for the output stream\n");
        cJSON_Delete(config_file);
        return -1;
    }

    cJSON_Delete(config_file);

    if (ctx->debug) {
        printf("Input frame width %u\n", ctx->input_frame_width);
        printf("Input frame height %u\n", ctx->input_frame_height);
        printf("Input frame rate %u\n", ctx->input_frame_rate);
        printf("Input frame format %s\n", ctx->input_frame_format);
        printf("Input pipe name %s\n", ctx->input_pipe_name);
        printf("Output stream width %u\n", ctx->output_stream_width);
        printf("Output stream height %u\n", ctx->output_stream_height);
        printf("Output stream bitrate %u\n", ctx->output_stream_bitrate);
        printf("Output stream rotation %u\n", ctx->output_stream_rotation);
        printf("Output stream frame rate %u\n", ctx->output_frame_rate);
    }

    // Prepare configuration based on input frame format
    if ( ! strcmp(ctx->input_frame_format, "uyvy")) {
        strcpy(ctx->input_frame_caps_format, "UYVY");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_UYVY;
        ctx->input_frame_size = ctx->input_frame_width * \
                                   ctx->input_frame_height * 2;
    } else if ( ! strcmp(ctx->input_frame_format, "nv12")) {
        strcpy(ctx->input_frame_caps_format, "NV12");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_NV12;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                    ctx->input_frame_height) + \
                                   (ctx->input_frame_width * \
                                    ctx->input_frame_height) / 2;
    } else if ( ! strcmp(ctx->input_frame_format, "nv21")) {
        strcpy(ctx->input_frame_caps_format, "NV21");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_NV21;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                    ctx->input_frame_height) + \
                                   (ctx->input_frame_width * \
                                    ctx->input_frame_height) / 2;
    } else if ( ! strcmp(ctx->input_frame_format, "gray8")) {
        strcpy(ctx->input_frame_caps_format, "GRAY8");
        ctx->input_frame_gst_format = GST_VIDEO_FORMAT_GRAY8;
        ctx->input_frame_size = (ctx->input_frame_width * \
                                 ctx->input_frame_height);
    } else {
        fprintf(stderr, "Unsupported input file format %s\n",
                ctx->input_frame_format);
        return -1;
    }

    // Output frame rate cannot be higher than input frame rate.
    if (ctx->output_frame_rate > ctx->input_frame_rate) {
        ctx->output_frame_rate = ctx->input_frame_rate;
    }

    // Calculate decimator used to drop frames if the output frame rate is
    // lower than the input frame rate.
    ctx->output_frame_decimator = ctx->input_frame_rate / ctx->output_frame_rate;

    return 0;
}

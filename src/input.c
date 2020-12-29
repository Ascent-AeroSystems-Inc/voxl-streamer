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
#include <sys/stat.h>
#include <fcntl.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <modal_pipe_client.h>
#include <modal_camera_server_interface.h>
#include "context.h"
#include "configuration.h"


// This is the main thread that reads frame data from the pipe and submits
// it into the pipeline for processing.
void *input_thread(void *vargp) {

    context_data *ctx = (context_data*) vargp;

    int fifo_fd = 0;
    int bytes_read = 0;
    int rc = 0;

    if (ctx->debug) printf("buffer processing thread starting\n");

    // Setup the correct input interface based on the configuration
    if (ctx->interface == MPA_INTERFACE) {
        rc = pipe_client_init_channel(0, ctx->input_pipe_name, "voxl-streamer",
                                      EN_PIPE_CLIENT_DEBUG_PRINTS, 0);
        if (rc) {
            fprintf(stderr, "ERROR: Couldn't open MPA pipe %s\n",
                    ctx->input_pipe_name);
            ctx->running = 0;
            return NULL;
        }
        fifo_fd = pipe_client_get_fd(0);
    } else {
        fprintf(stderr, "ERROR: Unsupported interface type: %d\n",
                ctx->interface);
        ctx->running = 0;
        return NULL;
    }

    if (fifo_fd == -1) {
        fprintf(stderr, "Error: could not open the fifo\n");
        ctx->running = 0;
        return NULL;
    } else {
        if (ctx->debug) printf("Opened the video frame fifo for reading\n");
    }

    camera_image_metadata_t frame_meta_data;
    GstBuffer *gst_buffer;
    GstMapInfo info;
    int dump_meta_data = 1;

    // This is the main processing loop
    while (ctx->running) {
        // In MPA the first thing sent over the pipe is the metadata.
        // This precedes every frame.
        bytes_read = read(fifo_fd,
                          &frame_meta_data,
                          sizeof(camera_image_metadata_t));
        if (((uint32_t) bytes_read) != sizeof(camera_image_metadata_t)) {
            fprintf(stderr, "ERROR: Got %d bytes. Expecting %d\n", bytes_read,
                    sizeof(camera_image_metadata_t));
            ctx->running = 0;
            break;
        } else {
            if ((ctx->debug) && (dump_meta_data)) {
                printf("Meta data from incoming frame:\n");
                printf("\tmagic_number 0x%X \n", frame_meta_data.magic_number);
                printf("\ttimestamp_ns: %lld\n", frame_meta_data.timestamp_ns);
                printf("\tframe_id: %d\n", frame_meta_data.frame_id);
                printf("\twidth: %d\n", frame_meta_data.width);
                printf("\theight: %d\n", frame_meta_data.height);
                printf("\tsize_bytes: %d\n", frame_meta_data.size_bytes);
                printf("\tstride: %d\n", frame_meta_data.stride);
                printf("\texposure_ns: %d\n", frame_meta_data.exposure_ns);
                printf("\tgain: %d\n", frame_meta_data.gain);
                printf("\tformat: %d\n", frame_meta_data.format);
                dump_meta_data = 0;
            }

            // Initialize the input parameters based on the incoming meta data
            if ( ! ctx->input_parameters_initialized) {
                rc = pipe_client_set_pipe_size(0, frame_meta_data.size_bytes);
                if (rc == -1) {
                    fprintf(stderr, "ERROR: Couldn't set MPA pipe size\n");
                    ctx->running = 0;
                    break;
                }

                switch (frame_meta_data.format) {
                case IMAGE_FORMAT_RAW8:
                case IMAGE_FORMAT_STEREO_RAW8:
                    strncpy(ctx->input_frame_format, "gray8",
                            MAX_IMAGE_FORMAT_STRING_LENGTH);
                    break;
                case IMAGE_FORMAT_NV12:
                    strncpy(ctx->input_frame_format, "nv12",
                            MAX_IMAGE_FORMAT_STRING_LENGTH);
                    break;
                case IMAGE_FORMAT_NV21:
                    strncpy(ctx->input_frame_format, "nv21",
                            MAX_IMAGE_FORMAT_STRING_LENGTH);
                    break;
                case IMAGE_FORMAT_YUV422:
                    strncpy(ctx->input_frame_format, "uyvy",
                            MAX_IMAGE_FORMAT_STRING_LENGTH);
                    break;
                case IMAGE_FORMAT_YUV420:
                    strncpy(ctx->input_frame_format, "yuv420",
                            MAX_IMAGE_FORMAT_STRING_LENGTH);
                    break;
                default:
                    fprintf(stderr, "ERROR: Unsupported frame format %d\n",
                            frame_meta_data.format);
                    ctx->running = 0;
                    break;
                }
                if ( ! ctx->running) break;

                ctx->input_frame_width = frame_meta_data.width;
                if (frame_meta_data.format == IMAGE_FORMAT_STEREO_RAW8) {
                    // The 2 stereo images are stacked on top of each other
                    // so have to double the height to account for both of them
                    if (ctx->debug) printf("Got stereo raw8 as input format\n");
                    ctx->input_frame_height = frame_meta_data.height * 2;
                } else {
                    ctx->input_frame_height = frame_meta_data.height;
                }

                rc = configure_frame_format(ctx->input_frame_format, ctx);

                if (ctx->input_frame_size != (uint32_t) frame_meta_data.size_bytes) {
                    fprintf(stderr, "ERROR: Frame size mismatch %d %d\n",
                            frame_meta_data.size_bytes,
                            ctx->input_frame_size);
                    ctx->running = 0;
                    break;
                }

                ctx->input_parameters_initialized = 1;
            }
        }

        // Allocate a gstreamer buffer to hold the frame data
        gst_buffer = gst_buffer_new_and_alloc(ctx->input_frame_size);
        gst_buffer_map(gst_buffer, &info, GST_MAP_WRITE);

        if (info.size < ctx->input_frame_size) {
            fprintf(stderr, "ERROR: not enough memory for the frame buffer\n");
            ctx->running = 0;
            break;
        }

        // Pull frame data from the input pipe. This will block until enough
        // data is ready.
        bytes_read = read(fifo_fd,
                          info.data,
                          ctx->input_frame_size);
        if (((uint32_t) bytes_read) != ctx->input_frame_size) {
            fprintf(stderr, "WARNING: Got %d bytes. Expecting %d. skipping frame.\n",
                    bytes_read, ctx->input_frame_size);
        } else {
            GstFlowReturn status;

            // The need_data flag is set by the pipeline callback asking for
            // more data.
            if (ctx->need_data) {
                // If the input frame rate is higher than the output frame rate then
                // we ignore some of the frames.
                if ( ! (ctx->input_frame_number % ctx->output_frame_decimator)) {

                    pthread_mutex_lock(&ctx->lock);

                    if (ctx->last_timestamp == 0) {
                        ctx->last_timestamp = (guint64) frame_meta_data.timestamp_ns;
                        pthread_mutex_unlock(&ctx->lock);
                    } else {
                        if (ctx->initial_timestamp == 0) {
                            ctx->initial_timestamp = (guint64) frame_meta_data.timestamp_ns;
                        }

                        // Setup the frame number and frame duration. It is very important
                        // to set this up accurately. Otherwise, the stream can look bad
                        // or just not work at all.
                        GST_BUFFER_TIMESTAMP(gst_buffer) = (guint64) frame_meta_data.timestamp_ns - ctx->initial_timestamp;
                        GST_BUFFER_DURATION(gst_buffer) = ((guint64) frame_meta_data.timestamp_ns) - ctx->last_timestamp;

                        ctx->last_timestamp = (guint64) frame_meta_data.timestamp_ns;

                        pthread_mutex_unlock(&ctx->lock);

                        if (ctx->frame_debug) {
                            printf("Output frame %d %llu %llu\n", ctx->output_frame_number, GST_BUFFER_TIMESTAMP(gst_buffer),
                                   GST_BUFFER_DURATION(gst_buffer));
                        }

                        ctx->output_frame_number++;

                        // Signal that the frame is ready for use
                        g_signal_emit_by_name(ctx->app_source, "push-buffer", gst_buffer, &status);
                        if (status == GST_FLOW_OK) {
                            if (ctx->frame_debug) printf("Frame %d accepted\n", ctx->output_frame_number);
                        } else {
                            fprintf(stderr, "ERROR: New frame rejected\n");
                        }
                    }
                }

                ctx->input_frame_number++;

            } else {
                if (ctx->frame_debug) printf("*** Skipping buffer ***\n");
            }

            // Release the buffer so that we don't have a memory leak
            gst_buffer_unmap(gst_buffer, &info);
            gst_buffer_unref(gst_buffer);
        }
    }

    // Close the pipe
    pipe_client_close_channel(0);

    if (ctx->debug) printf("buffer processing thread ending\n");

    return NULL;
}

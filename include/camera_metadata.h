#include <modal_pipe.h>
#include <modal_journal.h>
#include "context.h"

static int successful_grab = 0;

#define GRABBER_PIPE_CH 1

// camera helper callback whenever a frame arrives
static void _cam_metadata_helper_cb( int ch,camera_image_metadata_t meta,
                                    __attribute__((unused)) char* frame,
                                    __attribute__((unused)) void* context)
{
    context_data* ctx = (context_data*)context;
    ctx->input_format = meta.format;
    ctx->input_frame_width = meta.width;
    ctx->input_frame_height = meta.height;
    ctx->input_frame_rate = meta.framerate;
    successful_grab = 1;
    pipe_client_close(ch);
}

// called to fetch one camera frame to get its metadata
static int metadataGrabber(const char* process_name, context_data* context)
{
    pipe_client_set_camera_helper_cb(GRABBER_PIPE_CH, _cam_metadata_helper_cb, &context);

    if(pipe_client_open(GRABBER_PIPE_CH, context->input_pipe_name, process_name, \
        EN_PIPE_CLIENT_CAMERA_HELPER | CLIENT_FLAG_DISABLE_AUTO_RECONNECT, 0))
    {
        M_ERROR("metadata grabber failed to open pipe %s\n", context->input_pipe_name);
        return -1;
    }
    
    // grab required metadata and sleep while that happens
    time_t current_time = time(NULL);
    while(time(NULL) - current_time < 2){
        if(successful_grab == 1){
            pipe_client_close(GRABBER_PIPE_CH);
            return 0;
        }
        usleep(50000);
    }
    pipe_client_close(GRABBER_PIPE_CH);
    M_ERROR("TIMEOUT Failed to grab metadata from pipe %s\n", context->input_pipe_name);
    return -1;
}

#include <modal_pipe.h>
#include <modal_journal.h>
#include "context.h"

static int successful_grab = 0;

// called whenever we connect or reconnect to the server
static void _cam_metadata_connect_cb(__attribute__((unused)) int ch, __attribute__((unused)) void* context)
{
    M_PRINT("Camera server Connected\n");
}


// called whenever we disconnect from the server
static void _cam_metadata_disconnect_cb(__attribute__((unused)) int ch, __attribute__((unused)) void* context)
{
    M_PRINT("Camera server Disconnected\n");
}

// camera helper callback whenever a frame arrives
static void _cam_metadata_helper_cb(
    __attribute__((unused))int ch,
                           camera_image_metadata_t meta,
                           char* frame,
                           void* context)
{
    M_PRINT("CAM HELPER ");
    context_data *ctx = (context_data*) context;
    ctx->input_format = meta.format;
    ctx->input_frame_width = meta.width;
    ctx->input_frame_height = meta.height;
    ctx->input_frame_rate = meta.framerate;
    successful_grab = 1;
}

// called to instante camera helper for one instance
static int metadataGrabber(int pipe_channel, const char* process_name, context_data* context){
    pipe_client_set_connect_cb(pipe_channel, _cam_metadata_connect_cb, NULL);
    pipe_client_set_disconnect_cb(pipe_channel, _cam_metadata_disconnect_cb, NULL);
    pipe_client_set_camera_helper_cb(pipe_channel, _cam_metadata_helper_cb, &context);
    pipe_client_open(pipe_channel, context->input_pipe_name, process_name, EN_PIPE_CLIENT_CAMERA_HELPER, 0);
    
    // grab required metadata and sleep while that happens
    time_t current_time = time(NULL);
    while(time(NULL) - current_time < 2){
        if(successful_grab == 1){
            pipe_client_close(pipe_channel);
            return 1;
        } else {
            usleep(5000000);
        }
    }
    pipe_client_close(pipe_channel);
    return 0;
}
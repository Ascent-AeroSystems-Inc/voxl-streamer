/*******************************************************************************
 * Copyright 2021 ModalAI Inc.
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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <modal_json.h>
#include <string.h>

#define CONFIG_FILE_NAME "/etc/modalai/voxl-streamer.conf"

char pipe_name[128];
unsigned int bitrate, decimator, port, rotation;

static void PrintHelpMessage()
{
    printf("\nCommand line arguments are as follows:\n\n");
    printf("-b --bitrate    <#>     | Specify bitrate     (Default 1000000)\n");
    printf("-d --decimator  <#>     | Specify decimator   (Default 2)\n");
    printf("-h --help               | Print this help message\n");
    printf("-i --input-pipe <name>  | Specify input pipe  (Default hires)\n");
    printf("-p --port       <#>     | Specify Port number (Default 8900)\n");
    printf("-r --rotation   <#>     | Specify rotation    (Default 0)\n");
}

static int ParseArgs(int         argc,                 ///< Number of arguments
                     char* const argv[])               ///< Argument list
{
    static struct option LongOptions[] =
    {
        {"bitrate",          required_argument,  0, 'b'},
        {"decimator",        required_argument,  0, 'd'},
        {"help",             no_argument,        0, 'h'},
        {"input-pipe",       required_argument,  0, 'i'},
        {"port",             required_argument,  0, 'p'},
        {"rotation",         required_argument,  0, 'r'}
    };

    int optionIndex = 0;
    int option;

    while ((option = getopt_long (argc, argv, ":b:d:hi:p:r:", &LongOptions[0], &optionIndex)) != -1)
    {
        switch (option) {
            case 'd':
                if(sscanf(optarg, "%u", &decimator) != 1){
                    fprintf(stderr, "Failed to get valid integer for decimator from: %s\n", optarg);
                    return -1;
                }
                break;
            case 'b':
                if(sscanf(optarg, "%u", &bitrate) != 1){
                    fprintf(stderr, "Failed to get valid integer for bitrate from: %s\n", optarg);
                    return -1;
                }
                break;
            case 'r':
                if(sscanf(optarg, "%u", &rotation) != 1){
                    fprintf(stderr, "Failed to get valid integer for bitrate from: %s\n", optarg);
                    return -1;
                }
                if( rotation != 0   &&
                    rotation != 90  &&
                    rotation != 180 &&
                    rotation != 270 ) {
                    fprintf(stderr, "Invalid rotation: %u, must be 0, 90, 180, or 270\n", rotation);
                    return -1;
                }
                break;
            case 'i':
                strncpy(pipe_name, optarg, 128);
                break;
            case 'p':
                if(sscanf(optarg, "%u", &port) != 1){
                    fprintf(stderr, "Failed to get valid integer for port from: %s\n", optarg);
                    return -1;
                }
                break;
            case 'h':
                PrintHelpMessage();
                return -1;
            case ':':
                fprintf(stderr, "Option %c needs a value\n\n", optopt);
                PrintHelpMessage();
                return -1;
            case '?':
                fprintf(stderr, "Unknown option: %c\n\n", optopt);
                PrintHelpMessage();
                return -1;
        }
    }

    return 0;
}


// -----------------------------------------------------------------------------------------------------------------------------
// Main camera server configuration tool, recommend that this tool is not called directly, but through
// the voxl-configure-cameras script due to specific supported camera layouts
// -----------------------------------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{

    // Defaults
    strcpy(pipe_name, "hires");
    bitrate   = 1000000;
    decimator = 2;
    port      = 8900;
    rotation  = 0;

    if(ParseArgs(argc, argv)) return -1;

    printf("Using config:\n");
    printf("\tName:      %s\n", pipe_name);
    printf("\tBitrate:   %u\n", bitrate);
    printf("\tDecimator: %u\n", decimator);
    printf("\tPort:      %u\n", port);
    printf("\tRotation:  %u\n", rotation);

    cJSON* head = cJSON_CreateObject();

    cJSON_AddStringToObject(head, "input-pipe",  pipe_name);
    cJSON_AddNumberToObject(head, "bitrate",     bitrate);
    cJSON_AddNumberToObject(head, "decimator",   decimator);
    cJSON_AddNumberToObject(head, "port",        port);
    cJSON_AddNumberToObject(head, "rotation",    rotation);

    FILE *file = fopen(CONFIG_FILE_NAME, "w");
    if(file == NULL){

        fprintf(stderr, "Error opening config file: %s to write to\n", CONFIG_FILE_NAME);

    }else{
        char *jsonString = cJSON_Print(head);

        printf("Writing new configuration to %s\n",CONFIG_FILE_NAME);
        fwrite(jsonString, 1, strlen(jsonString), file);

        fclose(file);
        free(jsonString);
    }

    return 0;

}

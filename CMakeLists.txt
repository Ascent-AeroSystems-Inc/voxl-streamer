cmake_minimum_required(VERSION 2.8.11)
project(voxl-streamer)

add_definitions(-D_GNU_SOURCE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -std=c11 -Wall -pedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=500 -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections -fno-zero-initialized-in-bss")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function -Wno-unused-variable")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-cast-align")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")


include_directories( ../include )

include_directories( SYSTEM /usr/include/gstreamer-1.0 )
include_directories( SYSTEM /usr/include/glib-2.0 )
include_directories( SYSTEM /home/root/include )

add_executable( voxl-streamer
                src/util.c
                src/input.c
                src/pipeline.c
                src/configuration.c
                src/main.c)

target_link_libraries(voxl-streamer
                      pthread
                      gstreamer-1.0
                      gstvideo-1.0
                      gstrtspserver-1.0
                      gobject-2.0
                      gstapp-1.0
                      glib-2.0
                      modal_pipe
                      modal_json)

install(
    TARGETS voxl-streamer
    DESTINATION /usr/bin
)

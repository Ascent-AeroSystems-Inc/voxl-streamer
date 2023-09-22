cmake_minimum_required(VERSION 3.3)
project(voxl-streamer)

add_definitions(-D_GNU_SOURCE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -std=c11 -Wall -pedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=500 -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections -fno-zero-initialized-in-bss")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-cast-align")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

include_directories( SYSTEM /usr/include/gstreamer-1.0 )
include_directories( SYSTEM /usr/include/glib-2.0 )

include_directories( include/ )

option(PLATFORM "platform APQ8096 or QRB5165" ERROR)

if(PLATFORM MATCHES APQ8096)
    message(STATUS "Building for platform APQ8096")
    add_definitions(-DPLATFORM_APQ8096 )
    include_directories( SYSTEM /usr/lib/glib-2.0/include )
    find_library(MODAL_JSON    modal_json    HINTS /usr/lib)
    find_library(MODAL_PIPE    modal_pipe    HINTS /usr/lib)
    find_library(MODAL_JOURNAL modal_journal HINTS /usr/lib)
elseif(PLATFORM MATCHES QRB5165)
    message(STATUS "Building for platform QRB5165")
    add_definitions(-DPLATFORM_QRB5165 )
    include_directories( SYSTEM /usr/lib/aarch64-linux-gnu/glib-2.0/include )
    find_library(MODAL_JSON    modal_json    HINTS /usr/lib64)
    find_library(MODAL_PIPE    modal_pipe    HINTS /usr/lib64)
    find_library(MODAL_JOURNAL modal_journal HINTS /usr/lib64)
else()
    message(FATAL_ERROR "Platform type not specified")
endif()



add_executable( voxl-streamer
    src/util.c
    src/pipeline.c
    src/configuration.c
    src/main.c
)

target_link_libraries(voxl-streamer
    pthread
    gstreamer-1.0
    gstvideo-1.0
    gstrtspserver-1.0
    gobject-2.0
    gstapp-1.0
    glib-2.0
    gstrtsp-1.0
    ${MODAL_JSON}
    ${MODAL_PIPE}
    ${MODAL_JOURNAL}
)


install(
    TARGETS voxl-streamer
    DESTINATION /usr/bin
)

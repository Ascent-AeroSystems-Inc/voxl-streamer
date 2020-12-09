# voxl-streamer

Application based on GStreamer to stream video from Voxl connected cameras using h.264 / h.265 with RTSP.

## Usage

The voxl-streamer user manual is located [here](https://docs.modalai.com/voxl-streamer/)

## Building

On build host:
```bash
$ ./get_dependencies.sh
$ voxl-docker -i voxl-emulator -p
```
In the voxl-emulator:
```bash
# ./install-dependencies.sh
# mkdir build
# cd build
# cmake ..
# make
# cd ..
# ./make_package.sh
# exit
```

# voxl-streamer

Application based on GStreamer to stream video from Voxl connected cameras using h.264 / h.265 with RTSP.

dependencies:
* libmodal_pipe
* libmodal_json

This README covers building this package. The voxl-streamer user manual is located [here](https://docs.modalai.com/voxl-streamer/)


## Build Instructions

1) prerequisite: voxl-emulator docker image

Follow the instructions here:

https://gitlab.com/voxl-public/voxl-docker


2) Launch Docker and make sure this project directory is mounted inside the Docker.

```bash
~/git/voxl-streamer$ voxl-docker -i voxl-emulator
bash-4.3$ ls
README.md   build.sh  install_deps_in_docker.sh  ipk       src
build-deps  clean.sh  install_on_voxl.sh         make_package.sh
```

3) Install dependencies inside the docker.

```bash
./install_build_deps.sh
```

4) Compile inside the docker.

```bash
./build.sh
```

5) Make an ipk package inside the docker.

```bash
./make_ipk.sh

Package Name:  voxl-streamer
version Number:  x.x.x
ar: creating voxl-streamer_x.x.x.ipk

DONE
```

This will make a new voxl-streamer_x.x.x.ipk file in your working directory. The name and version number came from the ipk/control/control file. If you are updating the package version, edit it there.


## Deploy to VOXL

You can now push the ipk package to the VOXL and install with opkg however you like. To do this over ADB, you may use the included helper script: install_on_voxl.sh.

Do this OUTSIDE of docker as your docker image probably doesn't have usb permissions for ADB.

```bash
~/git/voxl-streamer$ ./install_on_voxl.sh
pushing voxl-streamer_x.x.x.ipk to target
searching for ADB device
adb device found
voxl-streamer_x.x.x.ipk: 1 file pushed. 2.1 MB/s (51392 bytes in 0.023s)
Removing package voxl-streamer from root...
Installing voxl-streamer (x.x.x) on root.
Configuring voxl-streamer

Done installing voxl-streamer
```


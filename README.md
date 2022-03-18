# voxl-streamer

Application based on GStreamer to stream video from Voxl connected cameras using h.264 with RTSP.

dependencies:
* libmodal_pipe
* libmodal_json

This README covers building this package. The voxl-streamer user manual is located [here](https://docs.modalai.com/voxl-streamer/)


## Build Instructions

#### Prerequisites

* voxl-emulator for voxl builds: https://gitlab.com/voxl-public/voxl-docker
* qrb5165-emulator for qrb5165 builds: https://gitlab.com/voxl-public/utilities/voxl-docker/-/tree/dev


#### Clean out all old artifacts

```bash
~/git/voxl-streamer$ ./clean.sh
```

#### Build for Voxl

1) Launch Docker

```bash
~/git/voxl-streamer$ voxl-docker -i voxl-emulator
voxl-emulator:~$
```

2) Compile inside the docker.

```bash
voxl-emulator:~$ ./build.sh
```

3) Make an ipk package inside the docker.

```bash
voxl-emulator:~$ ./make_package.sh
Making Normal Package

Package Name:  voxl-streamer
version Number:  0.2.8
Creating an ipk for voxl
starting building voxl-streamer_0.2.8.ipk
/usr/bin/ar: creating voxl-streamer_0.2.8.ipk

DONE
```

This will make a new voxl-streamer_x.x.x.ipk file in your working directory. The name and version number came from the pkg/control/control file. If you are updating the package version, edit it there.

#### Build for QRB5165

1) Launch Docker

```bash
~/git/voxl-streamer$ voxl-docker -i qrb5165-emulator
sh-4.4#
```

2) Compile inside the docker.

```bash
sh-4.4# ./build.sh
```

3) Make a deb package inside the docker.

```bash
sh-4.4# ./make_package.sh
Making Normal Package

Package Name:  voxl-streamer
version Number:  0.2.8
Creating a debian package for qrb5165
starting building Debian Package
dpkg-deb: building package 'voxl-streamer' in 'voxl-streamer_0.2.8.deb'.

DONE  
```

This will make a new voxl-streamer_x.x.x.deb file in your working directory. The name and version number came from the pkg/control/control file. If you are updating the package version, edit it there.


## Deploy to VOXL / QRB5165

You can now push the package to the board and install with the package manager however you like.
To do this over ADB, you may use the included helper script: install_on_voxl.sh.

Do this OUTSIDE of docker as your docker image probably doesn't have usb permissions for ADB.

```bash
$ ./install_on_voxl.sh
searching for ADB device
adb device found
dpkg detected
voxl-streamer_0.2.8.deb: 1 file pushed. 4.9 MB/s (49584 bytes in 0.010s)
Selecting previously unselected package voxl-streamer.
(Reading database ... 79035 files and directories currently installed.)
Preparing to unpack /data/voxl-streamer_0.2.8.deb ...
Unpacking voxl-streamer (0.2.8) ...
Setting up voxl-streamer (0.2.8) ...
/

Done installing voxl-streamer

DONE
```

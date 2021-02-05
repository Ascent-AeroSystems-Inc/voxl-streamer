#!/bin/bash

# This script will look through the v4l2 devices and find the first
# non-Qualcomm device. This device is chosen as the UVC camera for the
# voxl-streamer application.
# Note: The Qualcomm v4l2 devices are NOT UVC!

device="/sys/class/video4linux/video"
devicenumber=0
devicename=$device$devicenumber/name

while [ -f $devicename ]; do
    name=`cat $device$devicenumber/name`

    if [[ $name != "msm"* ]]; then
        set -x
        voxl-streamer -d -c uvc-video -u /dev/video$devicenumber
        set +x
        break
    fi

    ((devicenumber=devicenumber+1))
    devicename=$device$devicenumber/name
done

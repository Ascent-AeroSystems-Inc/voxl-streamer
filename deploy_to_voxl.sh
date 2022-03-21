#!/bin/bash
################################################################################
# Copyright (c) 2022 ModalAI, Inc. All rights reserved.
#
# Installs the package on target.
# Requires the package to be built and an adb connection.
################################################################################
set -e

PACKAGE=$(cat pkg/control/control | grep "Package" | cut -d' ' -f 2)

# count package files in current directory
NUM_IPK=$(ls -1q $PACKAGE*.ipk | wc -l)
NUM_DEB=$(ls -1q $PACKAGE*.deb | wc -l)

if [[ $NUM_IPK -eq "0" && $NUM_DEB -eq "0" ]]; then
	echo "ERROR: no ipk or deb file found"
	echo "run build.sh and make_package.sh first"
	exit 1
elif [[ $NUM_IPK -gt "1" || $NUM_DEB -gt "1" ]]; then
	echo "ERROR: more than 1 ipk or deb file found"
	echo "make sure there is at most one of each in the current directory"
	exit 1
fi

echo "searching for ADB device"
adb wait-for-device
echo "adb device found"

RESULT=`adb shell 'ls /usr/bin/dpkg 2> /dev/null | grep pkg -q; echo $?'`
RESULT="${RESULT//[$'\t\r\n ']}" ## remove the newline from adb
if [ "$RESULT" == "0" ] ; then
	echo "dpkg detected";
	MODE="dpkg"
else
	RESULT=`adb shell 'ls /usr/bin/opkg 2> /dev/null | grep pkg -q; echo $?'`
	RESULT="${RESULT//[$'\t\r\n ']}" ## remove the newline from adb
	if [[ ${RESULT} == "0" ]] ; then
		echo "opkg detected";
		MODE="opkg"
	else
		echo "ERROR neither dpkg nor opkg found on VOXL"
		exit 1
	fi
fi

if [ $MODE == dpkg ]; then
	FILE=$(ls -1q $PACKAGE*.deb)
	adb push $FILE /data/$FILE
	adb shell "dpkg -i --force-downgrade --force-depends /data/$FILE"
else
	FILE=$(ls -1q $PACKAGE*.ipk)
	adb push $FILE /data/$FILE
	adb shell "opkg install --force-reinstall --force-downgrade --force-depends /data/$FILE"
fi

echo "DONE"

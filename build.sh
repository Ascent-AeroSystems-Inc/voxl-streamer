#!/bin/bash
################################################################################
# Copyright (c) 2022 ModalAI, Inc. All rights reserved.
################################################################################

BUILDSIZE=32
QRB5165IDFILE=/etc/modalai/qrb5165-emulator.id

if test -f "$QRB5165IDFILE"; then
    echo "Building for qrb5165"
	BUILDSIZE=64
else
	echo "Building for voxl"
fi

BUILDDIR=build$BUILDSIZE
mkdir -p $BUILDDIR
cd $BUILDDIR
cmake -DBUILDSIZE=$BUILDSIZE ../
make -j4
cd ../

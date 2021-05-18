#!/bin/bash
################################################################################
# Copyright 2020 ModalAI Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# 4. The Software is used solely in conjunction with devices provided by
#    ModalAI Inc.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
################################################################################

set -e # exit on error to prevent bad ipk from being generated

################################################################################
# variables
################################################################################
VERSION=$(cat ipk/control/control | grep "Version" | cut -d' ' -f 2)
PACKAGE=$(cat ipk/control/control | grep "Package" | cut -d' ' -f 2)
IPK_NAME=${PACKAGE}_${VERSION}.ipk

DATA_DIR=ipk/data
CONTROL_DIR=ipk/control

echo ""
echo "Package Name: " $PACKAGE
echo "version Number: " $VERSION

################################################################################
# start with a little cleanup to remove old files
################################################################################
sudo rm -rf $DATA_DIR
mkdir -p $DATA_DIR
rm -rf ipk/control.tar.gz
rm -rf ipk/data.tar.gz
rm -rf $IPK_NAME

################################################################################
## copy useful files into data directory
################################################################################

# must run as root so files in ipk have correct permissions
cd build && sudo make DESTDIR=../ipk/data PREFIX=/usr install && cd -

sudo mkdir -p $DATA_DIR/share/modalai/
sudo cp config/voxl-streamer.conf $DATA_DIR/share/modalai/

sudo mkdir -p $DATA_DIR/etc/modalai/
sudo cp media/modalai.png $DATA_DIR/etc/modalai/
sudo cp script/start-uvc-stream.sh $DATA_DIR/usr/bin/

sudo mkdir -p $DATA_DIR/etc/systemd/system/
sudo cp service/* $DATA_DIR/etc/systemd/system/

mkdir -p $DATA_DIR/usr/share/bash-completion/completions
cp bash_completions/* $DATA_DIR/usr/share/bash-completion/completions

################################################################################
# pack the control, data, and final ipk archives
################################################################################

cd $CONTROL_DIR/
tar --create --gzip -f ../control.tar.gz *
cd ../../

cd $DATA_DIR/
tar --create --gzip -f ../data.tar.gz *
cd ../../

ar -r $IPK_NAME ipk/control.tar.gz ipk/data.tar.gz ipk/debian-binary

echo ""
echo DONE

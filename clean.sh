#!/bin/bash
################################################################################
# Copyright (c) 2022 ModalAI, Inc. All rights reserved.
################################################################################

PACKAGE=$(cat pkg/control/control | grep "Package" | cut -d' ' -f 2)

sudo rm -rf build32/
sudo rm -rf $PACKAGE*.ipk
sudo rm -rf build64/
sudo rm -rf $PACKAGE*.deb

sudo rm -rf pkg/control.tar.gz
sudo rm -rf pkg/data/
sudo rm -rf pkg/data.tar.gz
sudo rm -rf pkg/DEB/
sudo rm -rf pkg/IPK/
sudo rm -rf .bash_history

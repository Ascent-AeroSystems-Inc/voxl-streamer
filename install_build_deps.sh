#!/bin/bash
################################################################################
# Copyright (c) 2020 ModalAI, Inc. All rights reserved.
################################################################################

# list all your dependencies here
DEPS="libmodal_json libmodal_pipe"

# make sure opkg config file exists
OPKG_CONF=/etc/opkg/opkg.conf
if [ ! -f ${OPKG_CONF} ]; then
	echo "ERROR: missing ${OPKG_CONF}"
	echo "are you not running in Yocto?"
	exit 1
fi

# make sure voxl-packages is in the opkg config
if ! grep -q "voxl-packages" ${OPKG_CONF}; then
	# echo "opkg not configured for voxl-packages repository yet"
	# echo "adding repository now"

	sudo echo "src/gz voxl-packages http://voxl-packages.modalai.com/dev" >> ${OPKG_CONF}
fi

## make sure we have the latest package index
sudo opkg update
echo ""

# install/update each dependency
for i in ${DEPS}; do
	echo "Installing $i"

	# this will also update if already installed!
	sudo opkg install $i
done

# This needs to be added to the system include directory. This file exists
# on target but not in the SDK for some reason.
cp include/glibconfig.h /usr/include

echo ""
echo "Done installing dependencies"
echo ""
exit 0

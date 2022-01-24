#!/bin/bash
################################################################################
# Copyright (c) 2022 ModalAI, Inc. All rights reserved.
#
# Semi-universal script for making a deb and ipk package. This is shared
# between the vast majority of VOXL-SDK packages
#
# Add the 'timestamp' argument to add a date-timestamp suffix to the deb package
# version. This is used by CI for making nightly and development builds.
#
# authors: james@modalai.com eric.katzfey@modalai.com
################################################################################

set -e # exit on error to prevent bad package from being generated

################################################################################
# Check arguments
################################################################################

USETIMESTAMP=false

## convert argument to lower case for robustness
arg=$(echo "$1" | tr '[:upper:]' '[:lower:]')
case ${arg} in
	"")
		echo "Making Normal Package"
		;;
	"-t"|"timestamp"|"--timestamp")
		echo "using timestamp suffix"
		USETIMESTAMP=true
		;;
	*)
		echo "invalid option"
		exit 1
esac


################################################################################
# variables
################################################################################
VERSION=$(cat pkg/control/control | grep "Version" | cut -d' ' -f 2)
PACKAGE=$(cat pkg/control/control | grep "Package" | cut -d' ' -f 2)
IPK_NAME=${PACKAGE}_${VERSION}.ipk
DEB_NAME=${PACKAGE}_${VERSION}.deb

DATA_DIR=pkg/data
CONTROL_DIR=pkg/control
IPK_DIR=pkg/IPK
DEB_DIR=pkg/DEB

echo ""
echo "Package Name: " $PACKAGE
echo "version Number: " $VERSION

# Check whether we are making a package for voxl or qrb5165
BUILD_TYPE=build32
QRB5165IDFILE=/etc/modalai/qrb5165-emulator.id

if test -f "$QRB5165IDFILE"; then
    echo "Creating a debian package for qrb5165"
	BUILD_TYPE=build64
else
	echo "Creating an ipk for voxl"
fi


################################################################################
# start with a little cleanup to remove old files
################################################################################
# remove data directory where 'make install' installed to
sudo rm -rf $DATA_DIR
mkdir $DATA_DIR

# remove ipk and deb packaging folders
rm -rf $IPK_DIR
rm -rf $DEB_DIR

################################################################################
## copy useful files into data directory
################################################################################

# must run as root so files in ipk have correct permissions
cd $BUILD_TYPE && sudo make DESTDIR=../pkg/data PREFIX=/usr install && cd -

sudo cp script/show-video-device-info.sh $DATA_DIR/usr/bin/

################################################################################
# make the desired package
################################################################################
if [[ $BUILD_TYPE = build32 ]]; then
	# make an IPK
	echo "starting building $IPK_NAME"

	# Remove any old packages
	rm -f *.ipk

	## make a folder dedicated to IPK building and make the required version file
	mkdir $IPK_DIR
	echo "2.0" > $IPK_DIR/debian-binary

	## add tar archives of data and control for the IPK package
	cd $CONTROL_DIR/
	tar --create --gzip -f ../../$IPK_DIR/control.tar.gz *
	cd ../../
	cd $DATA_DIR/
	tar --create --gzip -f ../../$IPK_DIR/data.tar.gz *
	cd ../../

	## use ar to make the final .ipk and place it in the repository root
	ar -r $IPK_NAME $IPK_DIR/control.tar.gz $IPK_DIR/data.tar.gz $IPK_DIR/debian-binary
else
	# make a DEB package
	echo "starting building Debian Package"

	# Remove any old packages
	rm -f *.deb

	## make a folder dedicated to Debian building and copy the requires debian-binary file in
	mkdir $DEB_DIR

	## copy the control stuff in
	cp -rf $CONTROL_DIR/ $DEB_DIR/DEBIAN
	cp -rf $DATA_DIR/*   $DEB_DIR

	## update version with timestamp if enabled
	if $USETIMESTAMP; then
		dts=$(date +"%Y%m%d%H%M")
		sed -E -i "s/Version.*/&-$dts/" $DEB_DIR/DEBIAN/control
		VERSION="${VERSION}-${dts}"
		DEB_NAME=${PACKAGE}_${VERSION}.deb
		echo "new version with timestamp: $VERSION"
	fi

	dpkg-deb --build ${DEB_DIR} ${DEB_NAME}
fi


echo ""
echo "DONE"

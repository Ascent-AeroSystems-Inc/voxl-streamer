#!/bin/bash
################################################################################
# Copyright (c) 2022 ModalAI, Inc. All rights reserved.
################################################################################

BUILDSIZE=32

# placeholder in case more cmake opts need to be added later
EXTRA_OPTS=""

## this list is just for tab-completion
AVAILABLE_PLATFORMS="qrb5165 apq8096 native"


print_usage(){
	echo ""
	echo " Build the current project based on platform target."
	echo ""
	echo " Usage:"
	echo ""
	echo "  ./build.sh apq8096"
	echo "        Build 32-bit binaries for apq8096"
	echo ""
	echo "  ./build.sh qrb5165"
	echo "        Build 64-bit binaries for qrb5165"
	echo ""
	echo ""
}


case "$1" in
	apq8096)
		BUILDSIZE=32
		;;
	qrb5165)
		BUILDSIZE=64
		;;
	*)
		print_usage
		exit 1
		;;
esac

BUILDDIR=build$BUILDSIZE
mkdir -p $BUILDDIR
cd $BUILDDIR
cmake -DBUILDSIZE=$BUILDSIZE ../
make -j4
cd ../

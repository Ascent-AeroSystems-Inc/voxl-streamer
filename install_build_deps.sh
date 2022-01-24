#!/bin/bash

# list all your dependencies here. libuvc is available in Ubuntu but
# we want our custom version. Luckily it is called libuvc0 in Ubuntu.
DEPS="libmodal-pipe libmodal-json"

# variables
OPKG_CONF=/etc/opkg/opkg.conf
DPKG_FILE="/etc/apt/sources.list.d/modalai.list"
STABLE=http://voxl-packages.modalai.com/stable
DEV=http://voxl-packages.modalai.com/dev

PKG_TYPE=ipk
QRB5165IDFILE=/etc/modalai/qrb5165-emulator.id

if test -f "$QRB5165IDFILE"; then
    echo "Installing build dependencies for qrb5165"
	PKG_TYPE=deb
else
	echo "Installing build dependencies for voxl"
	# make sure opkg config file exists
	if [ ! -f ${OPKG_CONF} ]; then
		echo "ERROR: missing ${OPKG_CONF}"
		echo "are you not running in voxl-emulator or voxl-cross?"
		exit 1
	fi
fi

# parse dev or stable option
if [ "$1" == "stable" ]; then
	echo "using stable repository"
	LINE="deb [trusted=yes] http://voxl-packages.modalai.com/sdk-latest-stable/ ./"
	PKG_STRING="src/gz stable ${STABLE}"

elif [ "$1" == "dev" ]; then
	echo "using development repository"
	LINE="deb [trusted=yes] http://voxl-packages.modalai.com/dev-deb/ ./"
	PKG_STRING="src/gz dev ${DEV}"

else
	echo ""
	echo "Please specify if the build dependencies should be pulled from"
	echo "the stable or development modalai package repos."
	echo "If building the master branch you should specify stable."
	echo "For development branches please specify dev."
	echo ""
	echo "./install_build_deps.sh stable"
	echo "./install_build_deps.sh dev"
	echo ""
	exit 1
fi

if [ $PKG_TYPE = "deb" ]; then
	# write in the new entry
	sudo echo "${LINE}" > ${DPKG_FILE}

	## make sure we have the latest package index
	## only pull from voxl-packages to save time
	sudo apt-get update -o Dir::Etc::sourcelist="sources.list.d/modalai.list" -o Dir::Etc::sourceparts="-" -o APT::Get::List-Cleanup="0"

	## install the user's list of dependencies
	if [ -n "$DEPS" ]; then
		echo "installing: "
		echo $DEPS
		sudo apt-get install -y $DEPS
	fi
else
	# delete any existing repository entries
	sudo sed -i '/voxl-packages.modalai.com/d' ${OPKG_CONF}

	# write in the new entry
	sudo echo ${PKG_STRING} >> ${OPKG_CONF}
	sudo echo "" >> ${OPKG_CONF}

	## make sure we have the latest package index
	sudo opkg update


	# install/update each dependency
	for i in ${DEPS}; do
		# this will also update if already installed!
		sudo opkg install $i
	done
fi


echo ""
echo "Done installing dependencies"
echo ""

exit 0

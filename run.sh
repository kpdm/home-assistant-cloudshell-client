#!/bin/bash

MESON_BUILDDIR=builddir

USER=MQTT_USERNAME
PASS=MQTT_PASSWORD
HOST=MQTT_HOST
PORT=MQTT_PORT        
TOPIC=MQTT_TOPIC      # topic to listen for sensor data
NET_DEVICE=ETH_DEVICE # prints ip address 

if [ ! -f ${MESON_BUILDDIR} ]; then
	echo "Setting up meson builddir";
	if ! meson setup ${MESON_BUILDDIR}; then
		echo "Meson setup failed";
		exit -1;
	fi
fi

cd ${MESON_BUILDDIR}
if ! meson compile; then
	echo "Compile failed";
	cd ..
	exit -1
fi
cd ..
echo "Starting..."

# Disable LCD Slepp mode
echo -e '\033[9;0]' > /dev/tty1
# console font
# More fonts on: /usr/share/consolefonts
export TERM="linux"
#export CONSOLE_FONT="Lat7-Fixed18"
export CONSOLE_FONT="Lat15-Terminus20x10"
# Output Console (ttyX)
export OUTPUT_CONSOLE="1"
oc="/dev/tty$OUTPUT_CONSOLE"

# font setup
setfont $CONSOLE_FONT > $oc

# Ensure that we are in the right TTY
chvt $OUTPUT_CONSOLE
./builddir/hacli-x -u ${USER} -p ${PASS} -h ${HOST} -P ${PORT} -t ${TOPIC} -n ${NET_DEVICE} > $oc 

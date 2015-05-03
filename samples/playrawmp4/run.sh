#!/bin/sh
#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

set -x
IP=SERVERIP
DEBUG=CONFIG_DEBUG
PRG=PROGRAM
LOCLIBS=USELOCALLIBS
LOCLIBLLAD=USELOCALLIBLLAD
LOCLIBRUA=USELOCALLIBRUA
LOCLIBDCC=USELOCALLIBDCC
VIDNAME=VIDRAW
AUDNAME=AUDRAW
FILESYSTEM="/usb/usb0"
FILEPATH="$FILESYSTEM/video"
VIDEOFILE="$FILEPATH/$VIDNAME"
AUDIOFILE="$FILEPATH/$AUDNAME"

cd /tmp || exit 1

rm -f "$PRG" || exit 1
wget "http://$IP/dma-2500/$PRG" || exit 1
chmod +x "$PRG" || exit 1

rm -f libllad.so || exit 1
rm -f librua.so || exit 1
rm -f libdcc.so || exit 1
if [ "$LOCLIBS" != "yes" ]; then
	if [ "$LOCLIBLLAD" != "yes" ]; then
		wget "http://$IP/dma-2500/libllad.so" || exit 1
		chmod +x "libllad.so" || exit 1
	fi
	
	if [ "$LOCLIBRUA" != "yes" ]; then
		wget "http://$IP/dma-2500/librua.so" || exit 1
		chmod +x "librua.so" || exit 1
	fi

	if [ "$LOCLIBDCC" != "yes" ]; then
		wget "http://$IP/dma-2500/libdcc.so" || exit 1
		chmod +x "libdcc.so" || exit 1
	fi
fi

if [ ! -e gdbserver ]; then
	wget http://$IP/dma-2500/gdbserver || exit 1
	chmod +x gdbserver || exit 1
fi

mount | grep -e usb0
if [ $? -ne 0 ]; then
	# The test requires an USB storage device to store the video data.
	mount /dev/sda1 /usb/usb0 || exit 1
fi
mkdir -p "$FILEPATH" || exit 1
if [ ! -e "$AUDIOFILE" ]; then
	cd "$FILEPATH" || exit 1
	wget "http://$IP/dma-2500/$AUDNAME" || exit 1
	cd /tmp || exit 1
fi
if [ ! -e "$VIDEOFILE" ]; then
	cd "$FILEPATH" || exit 1
	wget "http://$IP/dma-2500/$VIDNAME" || exit 1
	cd /tmp || exit 1
fi

if [ "$LOCLIBS" != "yes" ]; then
	export LD_LIBRARY_PATH="/tmp:$LD_LIBRARY_PATH"
fi

if [ "$DEBUG" = "y" ]; then
	./gdbserver CLIENTIP:1234 "./$PRG" "$VIDEOFILE" "$AUDIOFILE"
else
	#rm nohup.out
	#touch nohup.out || exit 1
	#nohup "./$PRG" "$VIDEOFILE" "$AUDIOFILE"
	"./$PRG" "$VIDEOFILE" "$AUDIOFILE"
fi

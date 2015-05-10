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
URL=VIDEOURL

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

if [ "$LOCLIBS" != "yes" ]; then
	export LD_LIBRARY_PATH="/tmp:$LD_LIBRARY_PATH"
fi

if [ "$DEBUG" = "y" ]; then
	./gdbserver CLIENTIP:1234 "./$PRG" "$URL"
else
	"./$PRG" "$URL"
fi

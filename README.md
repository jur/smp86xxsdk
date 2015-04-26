# SDK for SMP86XX
This is the Software Development Kit (SDK) for the SMP8634/SMP8635.
It supports the Zyxel DMA-2500.

# Purpose
I tried to be compatible on source code and binary level with the official
SDK. As I don't have the official Sigma SMP86XX SDK for the Zyxel DMA-2500,
I can't verify whether this is really compatible. I verified that it is
compatible to examples which I found.

# Status
It is possible to use OSD (On Screen Display). Video and sound is not yet
supported in the libraries. But when you use the local libraries from the
DMA-2500, it is possible to play video and audio streams from a mp4 file.

# Toolchain
You need a toolchain for building it, e.g.:
* git clone https://github.com/jur/smp86xx.git
* cd smp86xx
* . DMATOOLCHAIN.sh
* cd ..

# Build
To build the SDK you need first to source the DMATOOLCHAIN.sh from the toolchain.

* git clone https://github.com/jur/smp86xxsdk.git
* cd smp86xxsdk
* make
* cd ..

youtube-dl and ffmpeg with mp4 support is required, because it will download
a video for testing and convert it to the raw video and audio data.

# Test Setup
The test is configured in config.mk.

You can configure CLIENTIP and SERVERIP in this file. The default is that it
tries to auto-detect the IP of the server (the host computer where you run
make). It assumes that dma-2500 will be resolved by a name server to the ip
address of the Zyxel DMA-2500.

The OSD test works with the following configuration:
* TESTPRG = smptest
* USELOCALLIBS = no

The video playback test needs the following configuration (default):
* TESTPRG = playrawmp4
* USELOCALLIBS = yes

As there is no full support for video and audio in the libaries, you need to
use the libraries which are installed on the DMA-2500.

# Run Test
You need first to boot the Zyxel DMA-2500 and connect it to the network. The
test will login into the device and start the example program.
It is recommended to use firmware 1.00.07b1 which is the latest one.
You need a webserver running on your host system. The files are copied to
/var/www/dma-2500/. You can change this in the config.mk file. You can also
change the IP address of the Zyxel DMA-2500.
To run the test you need first to source the DMATOOLCHAIN.sh from the toolchain.
* cd smp86xxsdk
* make TESTPRG=smptest USELOCALLIBS=no run
* cd ..

For testing video playback, you need to connect a USB storage device. The test
will download a youtube video on the host and extracts the raw video and audio
data. These will be copied ot the local web server /var/www and dowanloaded from
the DMA-2500 to /usb/usb0/video. It can be tested with:
* cd smp86xxsdk
* make TESTPRG=playrawmp4 USELOCALLIBS=yes YOUTUBEID=HXOaeE6IMWA run
* cd ..

The sound is sometime not working. To ensure that will work you should play a
mp4 file in the DMA-2500 using the offical software before running the test.

# Licence
The library and the header files are GNU LGPL.
The sample code is BSD.

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
supported.

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

# Run Test
You need first to boot the Zyxel DMA-2500 and connect it to the network. The
test will login into the device and start the example program.
It is recommended to use firmware 1.00.07b1 which is the latest one.
You need a webserver running on your host system. The files are copied to
/var/www/dma-2500/. You can change this in the config.mk file. You can also
change the IP address of the Zyxel DMA-2500.
To run the test you need first to source the DMATOOLCHAIN.sh from the toolchain.
* cd smp86xxsdk
* make run
* cd ..

# Licence
The library and the header files are GNU LGPL.
The sample code is BSD.

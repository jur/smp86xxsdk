#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

# Name of test to run:
# Simple OSD test
#TESTPRG = smptest
# Test needs a USB storage device at the DMA 2500.
TESTPRG = playrawmp4

# Install prefix:
PREFIX ?= /usr/local

# yes for using libraries installed on target, no for using the compiled libraries
USELOCALLIBS = no
#USELOCALLIBS = yes
USELOCALLIBLLAD = yes
USELOCALLIBRUA = no
USELOCALLIBDCC = no

# IP address of the dma-2500
CLIENTIP=$(shell dig +short dma-2500 | awk '{ print ; exit }')

# IP address of the computer where this Makefile is located
SERVERIP=$(shell LANG=C ifconfig | awk -F':' '/inet addr/&&!/127.0.0.1/{split($$2,_," ");print _[1]}')

# Install binaries to local web server:
WEBDIR = /var/www/dma-2500/

# Video ID of youtube video which will be downloaded for the test:
YOUTUBEID = HXOaeE6IMWA
#YOUTUBEID = GQooNIhYVO0

# How much of the video should be extracted:
#DURATION = -t 00:00:30

#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

# Name of test to run:
TESTPRG = smptest

# Install prefix:
PREFIX ?= /usr/local

# yes for using libraries installed on target, no for using the compiled libraries
USELOCALLIBS = no

# IP address of the dma-2500
CLIENTIP=$(shell dig +short dma-2500 | awk '{ print ; exit }')

# IP address of the computer where this Makefile is located
SERVERIP=$(shell LANG=C ifconfig | awk -F':' '/inet addr/&&!/127.0.0.1/{split($$2,_," ");print _[1]}')

# Install binaries to local web server:
WEBDIR = /var/www/dma-2500/

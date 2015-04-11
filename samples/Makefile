#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean

SMPSDKBASE = ..

include $(SMPSDKBASE)/config.mk

all:
	$(MAKE) -C smptest all

install: all
	$(MAKE) -C smptest install

run: all
	$(MAKE) -C $(TESTPRG) run

clean:
	$(MAKE) -C smptest clean

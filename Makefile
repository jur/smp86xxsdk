#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean

all:
	$(MAKE) -C libllad all
	$(MAKE) -C librua all
	$(MAKE) -C libdcc all
	$(MAKE) -C samples all

install:
	$(MAKE) -C libllad install
	$(MAKE) -C librua install
	$(MAKE) -C libdcc install
	$(MAKE) -C samples install

run:
	$(MAKE) -C libllad run
	$(MAKE) -C librua run
	$(MAKE) -C libdcc run
	$(MAKE) -C samples run

clean:
	$(MAKE) -C libllad clean
	$(MAKE) -C librua clean
	$(MAKE) -C libdcc clean
	$(MAKE) -C samples clean

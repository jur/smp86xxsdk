#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean install-header libraries samples install-libaries install-samples

HEADERFILES += include/dcc.h include/llad.h include/rua_common.h include/rua.h
HEADERFILES += include/zyxel_dma2500.h

all: libraries samples

libraries:
	$(MAKE) -C libllad all
	$(MAKE) -C librua all
	$(MAKE) -C libdcc all
	$(MAKE) -C liboslayer all

samples:
	$(MAKE) -C samples all

install: install-header install-libaries install-samples

install-libaries:
	$(MAKE) -C libllad install
	$(MAKE) -C librua install
	$(MAKE) -C libdcc install
	$(MAKE) -C liboslayer install

install-samples:
	$(MAKE) -C samples install

install-header: $(HEADERFILES)
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp $(HEADERFILES) $(DESTDIR)$(PREFIX)/include

run:
	$(MAKE) -C libllad run
	$(MAKE) -C librua run
	$(MAKE) -C libdcc run
	$(MAKE) -C liboslayer run
	$(MAKE) -C samples run

gdb:
	$(MAKE) -C libllad run
	$(MAKE) -C librua run
	$(MAKE) -C libdcc run
	$(MAKE) -C liboslayer run
	$(MAKE) -C samples gdb

debug:
	$(MAKE) -C libllad run
	$(MAKE) -C librua run
	$(MAKE) -C libdcc run
	$(MAKE) -C liboslayer run
	$(MAKE) -C samples debug

clean:
	$(MAKE) -C libllad clean
	$(MAKE) -C librua clean
	$(MAKE) -C libdcc clean
	$(MAKE) -C liboslayer clean
	$(MAKE) -C samples clean

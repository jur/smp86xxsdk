#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean

SMPSDKBASE = ..

include $(SMPSDKBASE)/config.mk

SAMPLES = smptest playrawmp4

all:
	for TEST in $(SAMPLES); do \
		$(MAKE) -C $$TEST all || exit 1; \
	done

install: all
	for TEST in $(SAMPLES); do \
		$(MAKE) -C $$TEST install || exit 1; \
	done

run: all
	$(MAKE) -C $(TESTPRG) run || exit 1; \

clean:
	for TEST in $(SAMPLES); do \
		$(MAKE) -C $$TEST clean || exit 1; \
	done

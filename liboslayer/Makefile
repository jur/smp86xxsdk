#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean

SMPSDKBASE = ..

MODS += oslayer
OBJS = $(addsuffix .o,$(MODS))

include $(SMPSDKBASE)/cross.mk
include $(SMPSDKBASE)/config.mk

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration
CPPFLAGS += -g
CPPFLAGS += -fPIC
CPPFLAGS += -I$(SMPSDKBASE)/include

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp $(OBJS) $(DESTDIR)$(PREFIX)/lib

run: all

all: $(OBJS)

clean:
	rm -f $(OBJS)

.PHONY: install all clean

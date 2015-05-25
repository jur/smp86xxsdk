#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean

SMPSDKBASE = ..

LIB = $(SMPSDKBASE)/librcc/librcc.a

MODS += rcc
OBJS = $(addsuffix .o,$(MODS))

include $(SMPSDKBASE)/cross.mk
include $(SMPSDKBASE)/config.mk

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration
CPPFLAGS += -g
CPPFLAGS += -fPIC
CPPFLAGS += -I$(SMPSDKBASE)/include

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp $(LIB) $(DESTDIR)$(PREFIX)/lib

run: all

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

clean:
	rm -f $(LIB) $(OBJS)

.PHONY: install all clean

#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean

SMPSDKBASE = ..

LIB = $(SMPSDKBASE)/librua/librua.so

MODS += rua
OBJS = $(addsuffix .o,$(MODS))

include $(SMPSDKBASE)/cross.mk
include $(SMPSDKBASE)/config.mk

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration
CPPFLAGS += -g
CPPFLAGS += -fPIC
CPPFLAGS += -I$(SMPSDKBASE)/include

install: all
	mkdir -p $(PREFIX)/lib
	cp "$(LIB)" "$(PREFIX)/lib"

run: all
	mkdir -p "$(WEBDIR)"
	cp "$(LIB)" "$(WEBDIR)"

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) -shared -o $@ $^

clean:
	rm -f $(LIB) $(OBJS)

.PHONY: install all clean

#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

.PHONY: install all clean

SMPSDKBASE = ../..

PROGRAM = smptest

SHELL = bash -x

MODS += smptest
MODS += oslayer
LDLIBS += -ldcc
LDLIBS += -lrua
LDLIBS += -lllad

OBJS = $(addsuffix .o,$(MODS))

include $(SMPSDKBASE)/cross.mk
include $(SMPSDKBASE)/config.mk

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration
CPPFLAGS += -g
CPPFLAGS += -I$(SMPSDKBASE)/include
LDFLAGS += -L$(SMPSDKBASE)/lib

install: all
	mkdir -p "$(WEBDIR)"
	cp "$(PROGRAM)" "$(WEBDIR)"
	cp "$(SMP86XXBASE)/smp86xx_rootfs_2.8.2.0/build_mipsel/gdbserver-6.5/gdbserver" "$(WEBDIR)"

run: install
	sed \
		-e "s/CONFIG_DEBUG/n/g" \
		-e "s/SERVERIP/$(SERVERIP)/g" \
		-e "s/CLIENTIP/CLIENTIP/g" \
		-e "s/PROGRAM/$(PROGRAM)/g" \
		-e "s/USELOCALLIBS/$(USELOCALLIBS)/g" \
		<"run.sh" \
		>"$(WEBDIR)/run-$(PROGRAM).sh"
	chmod +x "$(WEBDIR)/run-$(PROGRAM).sh"
	./dmarun.exp "http://$(SERVERIP)/dma-2500/run-$(PROGRAM).sh" "$(CLIENTIP)"

debug: install
	sed \
		-e "s/CONFIG_DEBUG/y/g" \
		-e "s/SERVERIP/$(SERVERIP)/g" \
		-e "s/CLIENTIP/CLIENTIP/g" \
		-e "s/PROGRAM/$(PROGRAM)/g" \
		<"run.sh" \
		>"$(WEBDIR)/run-$(PROGRAM).sh"
	chmod +x "$(WEBDIR)/run-$(PROGRAM).sh"
	dmarun.exp "http://$(SERVERIP)/dma-2500/run-$(PROGRAM).sh" "$(CLIENTIP)"

gdb: debug.gdb
	$(CROSS_COMPILE)gdb -nx -x debug.gdb $(PROGRAM)

all: $(PROGRAM)

%.gdb: %.gdb.base
	sed >$@ <$< \
		-e "s#\$$PREFIX#$(PREFIX)#g" \
		-e "s#\$$CLIENTIP#$(CLIENTIP)#g" \
		-e "s#\$$DMABASE#$(DMABASE)#g"

$(PROGRAM): $(OBJS)

clean:
	rm -f $(PROGRAM) $(OBJS)

.PHONY: install all run clean debug gdb

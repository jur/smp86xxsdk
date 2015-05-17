#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

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

ifneq ($(USELOCALLIBLLAD),yes)
SMPSDKLIBDIRS += :$(SMPSDKBASE)/libllad
endif
ifneq ($(USELOCALLIBRUA),yes)
SMPSDKLIBDIRS += :$(SMPSDKBASE)/librua
endif
ifneq ($(USELOCALLIBDCC),yes)
SMPSDKLIBDIRS += :$(SMPSDKBASE)/libdcc
endif

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration
CPPFLAGS += -g
CPPFLAGS += -I$(SMPSDKBASE)/include
LDFLAGS += -L$(SMPSDKBASE)/libllad
LDFLAGS += -L$(SMPSDKBASE)/librua
LDFLAGS += -L$(SMPSDKBASE)/libdcc

all: $(PROGRAM)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROGRAM) $(DESTDIR)$(BINDIR)
	$(STRIP) $(DESTDIR)$(BINDIR)/$(PROGRAM)

install-web: all
	mkdir -p "$(WEBDIR)"
	cp "$(PROGRAM)" "$(WEBDIR)"
	cp "$(SMP86XXBASE)/smp86xx_rootfs_2.8.2.0/build_mipsel/gdbserver-6.5/gdbserver" "$(WEBDIR)"

run: install-web
	sed \
		-e "s/CONFIG_DEBUG/n/g" \
		-e "s/SERVERIP/$(SERVERIP)/g" \
		-e "s/CLIENTIP/CLIENTIP/g" \
		-e "s/PROGRAM/$(PROGRAM)/g" \
		-e "s/USELOCALLIBS/$(USELOCALLIBS)/g" \
		-e "s/USELOCALLIBLLAD/$(USELOCALLIBLLAD)/g" \
		-e "s/USELOCALLIBRUA/$(USELOCALLIBRUA)/g" \
		-e "s/USELOCALLIBDCC/$(USELOCALLIBDCC)/g" \
		<"run.sh" \
		>"$(WEBDIR)/run-$(PROGRAM).sh"
	chmod +x "$(WEBDIR)/run-$(PROGRAM).sh"
	./dmarun.exp "http://$(SERVERIP)/dma-2500/run-$(PROGRAM).sh" "$(CLIENTIP)"

debug: install-web
	sed \
		-e "s/CONFIG_DEBUG/y/g" \
		-e "s/SERVERIP/$(SERVERIP)/g" \
		-e "s/CLIENTIP/CLIENTIP/g" \
		-e "s/PROGRAM/$(PROGRAM)/g" \
		-e "s/USELOCALLIBS/$(USELOCALLIBS)/g" \
		-e "s/USELOCALLIBLLAD/$(USELOCALLIBLLAD)/g" \
		-e "s/USELOCALLIBRUA/$(USELOCALLIBRUA)/g" \
		-e "s/USELOCALLIBDCC/$(USELOCALLIBDCC)/g" \
		<"run.sh" \
		>"$(WEBDIR)/run-$(PROGRAM).sh"
	chmod +x "$(WEBDIR)/run-$(PROGRAM).sh"
	./dmarun.exp "http://$(SERVERIP)/dma-2500/run-$(PROGRAM).sh" "$(CLIENTIP)"

gdb: debug.gdb
	$(CROSS_COMPILE)gdb -nx -x debug.gdb $(PROGRAM)

%.gdb: %.gdb.base
	sed >$@ <$< \
		-e "s#\$$PREFIX#$(DESTDIR)$(PREFIX)#g" \
		-e "s#\$$CLIENTIP#$(CLIENTIP)#g" \
		-e "s#\$$DMABASE#$(DMABASE)#g" \
		-e "s#\$$SMPSDKLIBDIRS#$(SMPSDKLIBDIRS)#g"

$(PROGRAM): $(OBJS)

clean:
	rm -f $(PROGRAM) $(OBJS) debug.gdb

.PHONY: install all run clean debug gdb install-web

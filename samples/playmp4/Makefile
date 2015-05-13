#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

SMPSDKBASE = ../..

PROGRAM = playmp4

YOUTUBEDL = youtube-dl

VIDEOFILE = youtubevideo-$(YOUTUBEID).mp4
# Format 18 is 640x360 mp4
YOUTUBEFORMAT = 18
YOUTUBELINK = https://www.youtube.com/watch?v=$(YOUTUBEID)
VIDEOURL = $(WEBURLBASE)/$(VIDEOFILE)

SHELL = bash -x

MODS += playmp4
MODS += oslayer
LDLIBS += -ldcc
LDLIBS += -lrua
LDLIBS += -lllad
LDLIBS += -lavformat
LDLIBS += -lavcodec
LDLIBS += -lavutil
LDLIBS += -lpthread
LDLIBS += -lm
OBJS = $(addsuffix .o,$(MODS))

include $(SMPSDKBASE)/cross.mk
include $(SMPSDKBASE)/config.mk

ifneq ($(USELOCALLIBLLAD),yes)
SMPSDKLIBDIRS += $(SMPSDKBASE)/libllad
endif
ifneq ($(USELOCALLIBRUA),yes)
SMPSDKLIBDIRS += $(SMPSDKBASE)/librua
endif
ifneq ($(USELOCALLIBDCC),yes)
SMPSDKLIBDIRS += $(SMPSDKBASE)/libdcc
endif

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration
CPPFLAGS += -g
CPPFLAGS += -I$(SMPSDKBASE)/include
LDFLAGS += -L$(SMPSDKBASE)/libllad
LDFLAGS += -L$(SMPSDKBASE)/librua
LDFLAGS += -L$(SMPSDKBASE)/libdcc

all: $(PROGRAM)

install: all
	mkdir -p $(BINDIR)
	cp $(PROGRAM) $(BINDIR)

install-web: $(WEBDIR)/$(VIDEOFILE) $(WEBDIR)/$(PROGRAM) $(WEBDIR)/gdbserver

$(WEBDIR)/$(VIDEOFILE): $(VIDEOFILE)
	mkdir -p "$(WEBDIR)"
	cp --preserve=timestamps $^ $@

$(WEBDIR)/$(PROGRAM): $(PROGRAM)
	mkdir -p "$(WEBDIR)"
	cp --preserve=timestamps $^ $@

$(WEBDIR)/gdbserver: $(SMP86XXBASE)/smp86xx_rootfs_2.8.2.0/build_mipsel/gdbserver-6.5/gdbserver
	mkdir -p "$(WEBDIR)"
	cp --preserve=timestamps $^ $@

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
		-e "s'VIDEOURL'$(VIDEOURL)'g" \
		<"run.sh" \
		>"$(WEBDIR)/run-$(PROGRAM).sh"
	chmod +x "$(WEBDIR)/run-$(PROGRAM).sh"
	./dmarun.exp "http://$(SERVERIP)/dma-2500/run-$(PROGRAM).sh" "$(CLIENTIP)"
VIDEOURL = "http://$(SERVERIP)/dma-2500/$(VIDEOFILE)"

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
		-e "s'VIDEOURL'$(VIDEOURL)'g" \
		<"run.sh" \
		>"$(WEBDIR)/run-$(PROGRAM).sh"
	chmod +x "$(WEBDIR)/run-$(PROGRAM).sh"
	./dmarun.exp "http://$(SERVERIP)/dma-2500/run-$(PROGRAM).sh" "$(CLIENTIP)"

gdb: debug.gdb
	$(CROSS_COMPILE)gdb -nx -x debug.gdb $(PROGRAM)

%.gdb: %.gdb.base
	sed >$@ <$< \
		-e "s#\$$PREFIX#$(PREFIX)#g" \
		-e "s#\$$CLIENTIP#$(CLIENTIP)#g" \
		-e "s#\$$DMABASE#$(DMABASE)#g" \
		-e "s#\$$SMPSDKLIBDIRS#$$(echo $(SMPSDKLIBDIRS) | tr ' ' ':')#g"

$(PROGRAM): $(OBJS)

$(VIDEOFILE):
	$(YOUTUBEDL) -o "$@" -f $(YOUTUBEFORMAT) "$(YOUTUBELINK)"

clean:
	rm -f $(PROGRAM) $(OBJS) debug.gdb

.PHONY: install all run clean debug gdb install-web

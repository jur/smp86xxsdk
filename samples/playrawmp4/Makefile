#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

SMPSDKBASE = ../..

PROGRAM = playrawmp4

FFMPEG = ffmpeg
YOUTUBEDL = youtube-dl

VIDRAW = video-$(YOUTUBEID).raw
AUDRAW = audio-$(YOUTUBEID).raw
VIDEOFILE = youtubevideo-$(YOUTUBEID).mp4
# Format 18 is 640x360 mp4
YOUTUBEFORMAT = 18
YOUTUBELINK = https://www.youtube.com/watch?v=$(YOUTUBEID)

SHELL = bash -x

MODS += playrawmp4
MODS += oslayer
LDLIBS += -ldcc
LDLIBS += -lrua
LDLIBS += -lllad
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

install-web: all $(AUDRAW) $(VIDRAW)
	mkdir -p "$(WEBDIR)"
	cp "$(PROGRAM)" "$(WEBDIR)"
	cp "$(VIDRAW)" "$(WEBDIR)"
	cp "$(AUDRAW)" "$(WEBDIR)"
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
		-e "s/VIDRAW/$(VIDRAW)/g" \
		-e "s/AUDRAW/$(AUDRAW)/g" \
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
		-e "s/VIDRAW/$(VIDRAW)/g" \
		-e "s/AUDRAW/$(AUDRAW)/g" \
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

$(VIDRAW): $(VIDEOFILE)
	$(FFMPEG) -i $^ -vbsf h264_mp4toannexb -vcodec copy -an -f rawvideo $(DURATION) $@

$(AUDRAW): $(VIDEOFILE)
	$(FFMPEG) -i $^ -vn -sn -f adts -acodec copy $(DURATION) $@

clean:
	rm -f $(PROGRAM) $(OBJS) $(AUDRAW) $(VIDRAW) debug.gdb

.PHONY: install all run clean debug gdb install-web

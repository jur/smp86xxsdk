#
# Copyright (c) 2015, Juergen Urban
# All rights reserved.
#

ifndef CROSS_COMPILE
ifneq ($(shell which mipsel-linux-gnu-gcc),)
CROSS_COMPILE=mipsel-linux-gnu-
endif
endif

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
RANLIB=$(CROSS_COMPILE)ranlib
AS=$(CROSS_COMPILE)as
OBJDUMP=$(CROSS_COMPILE)objdump
OBJCOPY=$(CROSS_COMPILE)objcopy
STRIP=$(CROSS_COMPILE)strip

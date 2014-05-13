#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= zrtp
$(MOD)_SRCS	+= zrtp.c
$(MOD)_LFLAGS	+= -lzrtp -lbn
CFLAGS          += -I$(SYSROOT)/local/include/libzrtp

include mk/mod.mk

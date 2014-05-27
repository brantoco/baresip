#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= snapshot
$(MOD)_SRCS	+= snapshot.c jpg_vf.c
$(MOD)_LFLAGS	+= -ljpeg

include mk/mod.mk

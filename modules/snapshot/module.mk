#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= snapshot
$(MOD)_SRCS	+= snapshot.c png_vf.c jpg_vf.c
$(MOD)_LFLAGS	+= -lpng -ljpeg

include mk/mod.mk

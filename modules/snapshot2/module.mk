#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= snapshot2
$(MOD)_SRCS	+= snapshot2.c png_vf.c
$(MOD)_LFLAGS	+= -lpng

include mk/mod.mk

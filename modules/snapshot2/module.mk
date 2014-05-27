#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= snapshot2
$(MOD)_SRCS	+= snapshot2.c jpg_vf.c
$(MOD)_LFLAGS	+= -ljpeg

include mk/mod.mk

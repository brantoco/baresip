#
# module.mk
#
# Copyright (C) 2014 Mazko Oleg, Fadeev Alexander
#

MOD		:= server
$(MOD)_SRCS	+= server.c
$(MOD)_LFLAGS	+=

include mk/mod.mk

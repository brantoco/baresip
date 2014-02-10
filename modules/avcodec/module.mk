#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

USE_X264 := $(shell [ -f $(SYSROOT)/include/x264.h ] || \
	[ -f $(SYSROOT)/local/include/x264.h ] || \
	[ -f $(SYSROOT_ALT)/include/x264.h ] && echo "yes")

USE_GST_VIDEO := $(shell [ "$(USE_GST)" = "yes" ] && echo "yes")

MOD		:= avcodec
$(MOD)_SRCS	+= avcodec.c h263.c h264.c encode.c decode.c
$(MOD)_LFLAGS	+= -lavcodec -lavutil
CFLAGS          += -I/usr/include/ffmpeg
ifneq ($(USE_GST_VIDEO),)
$(MOD)_SRCS	+= gst_video.c
$(MOD)_LFLAGS	+= `pkg-config --libs gstreamer-0.10 gstreamer-app-0.10`
CFLAGS          += -DUSE_GST_VIDEO `pkg-config --cflags gstreamer-0.10 gstreamer-app-0.10`
endif
ifneq ($(USE_X264),)
CFLAGS	+= -DUSE_X264
$(MOD)_LFLAGS	+= -lx264
endif

include mk/mod.mk

/**
 * @file gst.h  Gstreamer video pipeline
 *
 * Copyright (C) 2014 Fadeev Alexander
 */

typedef struct { int dummy; } gst_video_t;

gst_video_t *gst_video_alloc(int width, int height, int framerate, int bitrate);
void gst_video_free(gst_video_t *ctx);

int gst_video_push(gst_video_t *ctx, const void *src, int size);
int gst_video_set_pull_callback(gst_video_t *ctx, void (*f)(void *dst, int size, void *arg), void *arg);

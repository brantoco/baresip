/**
 * @file vidrec.h  Video container
 *
 * Copyright (C) 2014 Fadeev Alexander
 */

typedef struct { int dummy; } vidcont_t;

vidcont_t *vidcont_alloc(const char *filename, int width, int height, int framerate, int bitrate, enum CodecID codec_id, struct mbuf *sps, struct mbuf *pps);
void vidcont_free(vidcont_t *ctx);

int vidcont_video_write(vidcont_t *ctx, void *buf, int size);

/**
 * @file vidrec.h  Video recording Video-Filter
 *
 * Copyright (C) 2014 Fadeev Alexander
 */

void vidrec_init_once(int width, int height, int framerate, int bitrate, enum CodecID codec_id, struct mbuf *sps, struct mbuf *pps);
void vidrec_deinit(void);
void vidrec_video_write(void *src, size_t size, bool is_key);

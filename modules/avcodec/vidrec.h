/**
 * @file vidrec.h  Video recording Video-Filter
 *
 * Copyright (C) 2014 Fadeev Alexander
 */

void vidrec_init(int width, int height, int framerate, int bitrate, enum CodecID codec_id);
void vidrec_deinit(void);
int vidrec_write(void *src, size_t size);

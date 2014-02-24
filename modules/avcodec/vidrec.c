/**
 * @file vidrec.c  Video recording Video-Filter
 *
 * Copyright (C) 2014 Fadeev Alexander
 */
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <pthread.h>
#include <libavcodec/avcodec.h>
#include "vidrec.h"
#include "vidcont.h"


static pthread_mutex_t flock = PTHREAD_MUTEX_INITIALIZER;

static vidcont_t *f;
static int f_width;
static int f_height;
static int f_framerate;
static int f_bitrate;
static enum CodecID f_codec_id;


static int cmd_video_start(struct re_printf *pf, void *arg)
{
	char *path = (char *)pf->arg;

	(void)arg;

	if (!pf || !pf->arg) {
		return EINVAL;
	}

	pthread_mutex_lock(&flock);

	if (f) {
		pthread_mutex_unlock(&flock);
		return 0;
	}

	
	f = vidcont_alloc(path, f_width, f_height, f_framerate, f_bitrate, f_codec_id);

	pthread_mutex_unlock(&flock);

	debug("Start video recording: %s\n", path);

	return 0;
}


static int cmd_video_stop(struct re_printf *pf, void *arg)
{
	(void)pf;
	(void)arg;

	pthread_mutex_lock(&flock);

	if (f) {
		vidcont_free(f);
		f = NULL;
	}

	pthread_mutex_unlock(&flock);

	debug("Stop video recording...\n");

	return 0;
}


static const struct cmd cmdv[] = {
	{'z', 0, "Start video recording", cmd_video_start },
	{'Z', 0, "Stop video recording", cmd_video_stop }
};


void vidrec_init(int width, int height, int framerate, int bitrate, enum CodecID codec_id)
{
	f_width = width;
	f_height = height;
	f_framerate = framerate;
	f_bitrate = bitrate;
	f_codec_id = codec_id;

	cmd_register(cmdv, ARRAY_SIZE(cmdv));
}


void vidrec_deinit(void)
{
	cmd_unregister(cmdv);
}

int vidrec_write(void *src, size_t size)
{
	if (!src)
		return 0;

	pthread_mutex_lock(&flock);

	if (f) {
		vidcont_write(f, src, size);
	}

	pthread_mutex_unlock(&flock);

	return 0;
}

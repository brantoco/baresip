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


static pthread_mutex_t int_lock = PTHREAD_MUTEX_INITIALIZER;

static int int_initialized = 0;

static int int_width;
static int int_height;
static int int_framerate;
static int int_bitrate;
static enum AVCodecID int_codec_id;
static struct mbuf *int_sps;
static struct mbuf *int_pps;
static bool int_was_key_frame;

static vidcont_t *f;


static void internal_video_start(const char *path)
{
	pthread_mutex_lock(&int_lock);

	if (!f) {
		f = vidcont_alloc(path, int_width, int_height, int_framerate, int_bitrate, int_codec_id, int_sps, int_pps);
	}

	pthread_mutex_unlock(&int_lock);
}


static void internal_video_stop(void)
{
	pthread_mutex_lock(&int_lock);

	if (f) {
		vidcont_free(f);
		f = NULL;
		int_was_key_frame = false;
	}

	pthread_mutex_unlock(&int_lock);
}


static void internal_video_write(void *src, size_t size, bool is_key)
{
	pthread_mutex_lock(&int_lock);

	/* Wait until key frame. */
	if (is_key) {
		int_was_key_frame = true;
	}

	if (f && int_was_key_frame) {
		vidcont_video_write(f, src, size, is_key);
	}

	pthread_mutex_unlock(&int_lock);
}


static int cmd_video_start(struct re_printf *pf, void *arg)
{
	char *video_path;
	char *preview_path;

	struct re_printf pf_preview;

	(void)arg;

	if (!pf || !pf->arg) {
		return EINVAL;
	}

	/* Parse incoming arguments. */
	video_path = strtok((char *)pf->arg, " \r\n");
	preview_path = strtok(NULL, " \r\n");

	if (!video_path) {
		video_path = (char *)pf->arg;
	}

	pf_preview.arg = preview_path;

	/* Make preview. */
	cmd_process(NULL, 'o', &pf_preview);

	/* Start video recording. */
	internal_video_start(video_path);

	debug("Start video recording: %s\n", video_path);
	debug("Video preview: %s\n", preview_path);

	return 0;
}


static int cmd_video_stop(struct re_printf *pf, void *arg)
{
	(void)pf;
	(void)arg;

	internal_video_stop();

	debug("Stop video recording...\n");

	return 0;
}


static const struct cmd cmdv[] = {
	{'z', 0, "Start video recording", cmd_video_start },
	{'Z', 0, "Stop video recording", cmd_video_stop }
};


void vidrec_init_once(int width, int height, int framerate, int bitrate, enum AVCodecID codec_id, struct mbuf *sps, struct mbuf *pps)
{
	if (int_initialized) {
		return;
	}

	int_width = width;
	int_height = height;
	int_framerate = framerate;
	int_bitrate = bitrate;
	int_codec_id = codec_id;
	int_sps = mbuf_alloc_ref(sps);
	int_pps = mbuf_alloc_ref(pps);
	int_was_key_frame = false;

	cmd_register(cmdv, ARRAY_SIZE(cmdv));

	int_initialized = 1;

	debug("Video recording is initialized\n");
}


void vidrec_deinit(void)
{
	if (!int_initialized) {
		return;
	}

	cmd_unregister(cmdv);

	internal_video_stop();

	int_sps = mem_deref(int_sps);
	int_pps = mem_deref(int_pps);

	int_initialized = 0;

	debug("Video recording is deinitialized.\n");
}


void vidrec_video_write(void *src, size_t size, bool is_key)
{
	if (!int_initialized) {
		return;
	}

	if (!src) {
		return;
	}

	internal_video_write(src, size, is_key);
}

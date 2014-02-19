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
#include "vidrec.h"


static pthread_mutex_t flock = PTHREAD_MUTEX_INITIALIZER;
static FILE *f;


int vidrec_encode(const char *src, size_t size)
{
	if (!src)
		return 0;

	pthread_mutex_lock(&flock);

	if (f) {
		fwrite(src, 1, size, f);
	}

	pthread_mutex_unlock(&flock);

	return 0;
}


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

	f = fopen(path, "w+");

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
		fclose(f);
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


int vidrec_init(void)
{
	return cmd_register(cmdv, ARRAY_SIZE(cmdv));
}


int vidrec_deinit(void)
{
	cmd_unregister(cmdv);
	return 0;
}

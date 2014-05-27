/**
 * @file snapshot.c  Snapshot Video-Filter
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <pthread.h>
#include "jpg_vf.h"


static pthread_mutex_t png_create_lock =  PTHREAD_MUTEX_INITIALIZER;
static bool flag_enc;
static char *image_path;
static char *preview_path;


static void *thread_create_png(void *arg)
{
	struct vidframe *frame = arg;

	pthread_mutex_lock(&png_create_lock);
	jpg_save_vidframe(frame, image_path, preview_path);
	mem_deref(frame);
	pthread_mutex_unlock(&png_create_lock);

	return NULL;
}


static int encode(struct vidfilt_enc_st *st, struct vidframe *frame)
{
	(void)st;

	if (!frame)
		return 0;

	if (flag_enc) {
		pthread_t tid;

		if (image_path) {
			debug("Snapshot2: %s\n", image_path);
		}

		if (preview_path) {
			debug("Snapshot2 preview: %s\n", preview_path);
		} else {
			debug("Snapshot2 preview is not set\n");
		}

		flag_enc = false;

		mem_ref(frame);

		pthread_create(&tid, NULL, thread_create_png, frame);
	}

	return 0;
}


static int cmd_snapshot(struct re_printf *pf, void *arg)
{
	(void)arg;

	if (!pf || !pf->arg) {
		return EINVAL;
	}

	pthread_mutex_lock(&png_create_lock);

	image_path = strtok((char *)pf->arg, " \r\n");
	preview_path = strtok(NULL, " \r\n");

	if (!image_path) {
		image_path = (char *)pf->arg;
	}

	if (!preview_path) {
		preview_path = image_path;
	}

	flag_enc = true;

	pthread_mutex_unlock(&png_create_lock);

	return 0;
}


static struct vidfilt snapshot2 = {
	LE_INIT, "snapshot2", NULL, encode, NULL, NULL,
};


static const struct cmd cmdv[] = {
	{'o', 0, "Take video snapshot", cmd_snapshot },
};


static int module_init(void)
{
	vidfilt_register(&snapshot2);
	return cmd_register(cmdv, ARRAY_SIZE(cmdv));
}


static int module_close(void)
{
	vidfilt_unregister(&snapshot2);
	cmd_unregister(cmdv);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(snapshot2) = {
	"snapshot2",
	"vidfilt",
	module_init,
	module_close
};

/**
 * @file snapshot.c  Snapshot Video-Filter
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "png_vf.h"


static bool flag_enc;
static char *filename;


static int encode(struct vidfilt_enc_st *st, struct vidframe *frame)
{
	(void)st;

	if (!frame)
		return 0;

	if (flag_enc) {
		debug("%s\n", filename);
		flag_enc = false;
		png_save_vidframe(frame, filename);
	}

	return 0;
}


static int cmd_snapshot(struct re_printf *pf, void *arg)
{
	(void)arg;

	if (!pf || !pf->arg) {
		return EINVAL;
	}

	/* NOTE: not re-entrant */
	filename = (char *)pf->arg;
	flag_enc = true;

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
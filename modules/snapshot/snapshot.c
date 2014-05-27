/**
 * @file snapshot.c  Snapshot Video-Filter
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "jpg_vf.h"

static bool flag_enc_j, flag_dec_j;


static int encode(struct vidfilt_enc_st *st, struct vidframe *frame)
{
	(void)st;

	if (!frame)
		return 0;

	if (flag_enc_j) {
		flag_enc_j = false;
		jpg_save_vidframe(frame, "snapshot-send");
	}

	return 0;
}


static int decode(struct vidfilt_dec_st *st, struct vidframe *frame)
{
	(void)st;

	if (!frame)
		return 0;

	if (flag_dec_j) {
		flag_dec_j = false;
		jpg_save_vidframe(frame, "snapshot-recv");
	}

	return 0;
}

static int do_snapshot_j(struct re_printf *pf, void *arg)
{
	(void)pf;
	(void)arg;

	/* NOTE: not re-entrant */
	flag_enc_j = flag_dec_j = true;

	return 0;
}

static struct vidfilt snapshot = {
	LE_INIT, "snapshot", NULL, encode, NULL, decode,
};

static const struct cmd cmdv[] = {
	{'p', 0, "Take jpg video snapshot", do_snapshot_j },
};


static int module_init(void)
{
	vidfilt_register(&snapshot);
	info("Snapshot\n");
	return cmd_register(cmdv, ARRAY_SIZE(cmdv));
}


static int module_close(void)
{
	vidfilt_unregister(&snapshot);
	cmd_unregister(cmdv);
	return 0;
}

EXPORT_SYM const struct mod_export DECL_EXPORTS(snapshot) = {
	"snapshot",
	"vidfilt",
	module_init,
	module_close
};

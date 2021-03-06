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
#include "jpg_vf.h"

static bool flag_enc, flag_dec;
static bool flag_enc_j, flag_dec_j;


static int encode(struct vidfilt_enc_st *st, struct vidframe *frame)
{
	(void)st;

	if (!frame)
		return 0;

	if (flag_enc) {
		flag_enc = false;
		png_save_vidframe(frame, "snapshot-send");
	}
	
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

	if (flag_dec) {
		flag_dec = false;
		png_save_vidframe(frame, "snapshot-recv");
	}

	if (flag_dec_j) {
		flag_dec_j = false;
		jpg_save_vidframe(frame, "snapshot-recv");
	}

	return 0;
}

static int do_snapshot(struct re_printf *pf, void *arg)
{
	(void)pf;
	(void)arg;

	/* NOTE: not re-entrant */
	flag_enc = flag_dec = true;

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
	{'o', 0, "Take png video snapshot", do_snapshot },
	{'p', 0, "Take jpg video snapshot", do_snapshot_j },
};


static int module_init(void)
{
	vidfilt_register(&snapshot);
	info("Snapshot: PNG JPG\n");
	return  cmd_register(cmdv, ARRAY_SIZE(cmdv));
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

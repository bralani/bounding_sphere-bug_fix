/*                      F A C E P L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/size.c
 *
 * Controls for view elements (center dot, model axes, view axes,
 * etc.) that are built into BRL-CAD views.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "bview.h"

#include "../ged_private.h"
#include "./ged_view.h"

#ifndef COMMA
#  define COMMA ','
#endif

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_fp_info {
    struct ged *gedp;
    long verbosity;
    const struct bu_cmdtab *cmds;
    struct bu_opt_desc *gopts;
};

static int
_fp_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_fp_info *gd = (struct _ged_fp_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

int
_fp_cmd_list(void *ds, int argc, const char **argv)
{
    const char *usage_string = "fp [options] list";
    const char *purpose_string = "list elements which can be controlled.";
    if (_fp_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;

    bu_vls_printf(gedp->ged_result_str, "TODO");

    return GED_OK;
}


const struct bu_cmdtab _fp_cmds[] = {
    { "list",            _fp_cmd_list},
    { (char *)NULL,      NULL}
};

int
_fp_cmd_help(void *ds, int argc, const char **argv)
{
    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    if (!argc || !argv || BU_STR_EQUAL(argv[0], "help")) {
	bu_vls_printf(gd->gedp->ged_result_str, "fp [options] subcommand [args]\n");
	if (gd->gopts) {
	    char *option_help = bu_opt_describe(gd->gopts, NULL);
	    if (option_help) {
		bu_vls_printf(gd->gedp->ged_result_str, "Options:\n%s\n", option_help);
		bu_free(option_help, "help str");
	    }
	}
	bu_vls_printf(gd->gedp->ged_result_str, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	size_t maxcmdlen = 0;
	for (ctp = gd->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    maxcmdlen = (maxcmdlen > strlen(ctp->ct_name)) ? maxcmdlen : strlen(ctp->ct_name);
	}
	for (ctp = gd->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gd->gedp->ged_result_str, "  %s%*s", ctp->ct_name, (int)(maxcmdlen - strlen(ctp->ct_name)) +   2, " ");
	    if (!BU_STR_EQUAL(ctp->ct_name, "help")) {
		helpflag[0] = ctp->ct_name;
		bu_cmd(gd->cmds, 2, helpflag, 0, (void *)gd, &ret);
	    } else {
		bu_vls_printf(gd->gedp->ged_result_str, "print help and exit\n");
	    }
	}
    } else {
	int ret;
	const char **helpargv = (const char **)bu_calloc(argc+1, sizeof(char *), "help argv");
	helpargv[0] = argv[0];
	helpargv[1] = HELPFLAG;
	for (int i = 1; i < argc; i++) {
	    helpargv[i+1] = argv[i];
	}
	bu_cmd(gd->cmds, argc+1, helpargv, 0, (void *)gd, &ret);
	bu_free(helpargv, "help argv");
	return ret;
    }

    return GED_OK;
}


int
ged_faceplate_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_fp_info gd;
    gd.gedp = gedp;
    gd.cmds = _fp_cmds;
    gd.verbosity = 0;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return GED_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the faceplate command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",  NULL,               &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",  &bu_opt_incr_long,  &gd.verbosity, "Verbose output");
    BU_OPT_NULL(d[2]);

    gd.gopts = d;

    int ac = bu_opt_parse(NULL, argc, argv, d);

    if (!ac || help) {
	_fp_cmd_help(&gd, 0, NULL);
	return GED_OK;
    }

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no current view set");
	return GED_ERROR;
    }

    int ret;
    if (bu_cmd(_fp_cmds, ac, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    return GED_ERROR;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

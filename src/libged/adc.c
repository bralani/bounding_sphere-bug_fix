/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file adc.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "ged.h"
#include "dm.h"

static void ged_adc_model_To_adc_view(struct ged_view *gvp);
static void ged_adc_grid_To_adc_view(struct ged_view *gvp);
static void ged_adc_view_To_adc_grid(struct ged_view *gvp);
static void ged_adc_reset(struct ged_view *gvp);
static void ged_adc_vls_print(struct ged_view *gvp, fastf_t base2local, struct bu_vls *out_vp);

static char ged_adc_syntax[] = "\
 adc vname			toggle display of angle/distance cursor\n\
 adc vname vars			print a list of all variables (i.e. var = val)\n\
 adc vname draw [0|1]		set or get the draw parameter\n\
 adc vname a1 [#]		set or get angle1\n\
 adc vname a2 [#]		set or get angle2\n\
 adc vname dst [#]		set or get radius (distance) of tick\n\
 adc vname odst [#]		set or get radius (distance) of tick (+-2047)\n\
 adc vname hv [# #]		set or get position (grid coordinates)\n\
 adc vname xyz [# # #]		set or get position (model coordinates)\n\
 adc vname x [#]		set or get horizontal position (+-2047)\n\
 adc vname y [#]		set or get vertical position (+-2047)\n\
 adc vname dh #			add to horizontal position (grid coordinates)\n\
 adc vname dv #			add to vertical position (grid coordinates)\n\
 adc vname dx #			add to X position (model coordinates)\n\
 adc vname dy #			add to Y position (model coordinates)\n\
 adc vname dz #			add to Z position (model coordinates)\n\
 adc vname anchor_pos		[0|1]	anchor ADC to current position in model coordinates\n\
 adc vname anchor_a1		[0|1]	anchor angle1 to go through anchorpoint_a1\n\
 adc vname anchor_a2		[0|1]	anchor angle2 to go through anchorpoint_a2\n\
 adc vname anchor_dst		[0|1]	anchor tick distance to go through anchorpoint_dst\n\
 adc vname anchorpoint_a1 	[# # #]	set or get anchor point for angle1\n\
 adc vname anchorpoint_a2 	[# # #]	set or get anchor point for angle2\n\
 adc vname anchorpoint_dst 	[# # #]	set or get anchor point for tick distance\n\
 adc vname -i			any of the above appropriate commands will interpret parameters as increments\n\
 adc vname reset		reset angles, location, and tick distance\n\
 adc vname help			prints this help message\n\
";


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 *        that multiple attributes can be set with a single command call.
 */
int
ged_adc(struct ged	*gedp,
	int		argc,
	const char	*argv[])
{
    char *command;
    char *parameter;
    char **argp = (char **)argv;
    point_t user_pt;		/* Value(s) provided by user */
    point_t scaled_pos;
    int incr_flag;
    int i;
    static const char *usage = ged_adc_syntax;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc < 2 || 6 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    command = (char *)argv[0];

    if (strcmp(argv[1], "-i") == 0) {
	if (argc < 5) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: -i option specified without an op-val pair", command);
	    return GED_ERROR;
	}

	incr_flag = 1;
	parameter = (char *)argv[2];
	argc -= 3;
	argp += 3;
    } else {
	incr_flag = 0;
	parameter = (char *)argv[1];
	argc -= 2;
	argp += 2;
    }

    for (i = 0; i < argc; ++i)
	if (sscanf(argp[i], "%lf", &user_pt[i]) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

    if (strcmp(parameter, "draw") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_draw);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_adc.gas_draw = 1;
	    else
		gedp->ged_gvp->gv_adc.gas_draw = 0;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "a1") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g", gedp->ged_gvp->gv_adc.gas_a1);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_a1) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_a1 += user_pt[0];
		else
		    gedp->ged_gvp->gv_adc.gas_a1 = user_pt[0];

		gedp->ged_gvp->gv_adc.gas_dv_a1 = (1.0 - (gedp->ged_gvp->gv_adc.gas_a1 / 45.0)) * GED_MAX;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s a1' command accepts only 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "a2") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g", gedp->ged_gvp->gv_adc.gas_a2);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_a2) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_a2 += user_pt[0];
		else
		    gedp->ged_gvp->gv_adc.gas_a2 = user_pt[0];

		gedp->ged_gvp->gv_adc.gas_dv_a2 = (1.0 - (gedp->ged_gvp->gv_adc.gas_a2 / 45.0)) * GED_MAX;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s a2' command accepts only 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "dst") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g", gedp->ged_gvp->gv_adc.gas_dst * gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_dst) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_dst += user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		else
		    gedp->ged_gvp->gv_adc.gas_dst = user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);

		gedp->ged_gvp->gv_adc.gas_dv_dist = (gedp->ged_gvp->gv_adc.gas_dst / M_SQRT1_2 - 1.0) * GED_MAX;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dst' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "odst") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_dv_dist);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_dst) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_dv_dist += user_pt[0];
		else
		    gedp->ged_gvp->gv_adc.gas_dv_dist = user_pt[0];

		gedp->ged_gvp->gv_adc.gas_dst = (gedp->ged_gvp->gv_adc.gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s odst' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "dh") == 0) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_grid[X] += user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		ged_adc_grid_To_adc_view(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dh' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "dv") == 0) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_grid[Y] += user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		ged_adc_grid_To_adc_view(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dv' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "hv") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g %g",
			  gedp->ged_gvp->gv_adc.gas_pos_grid[X] * gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local,
			  gedp->ged_gvp->gv_adc.gas_pos_grid[Y] * gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
	    return GED_OK;
	} else if (argc == 2) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_adc.gas_pos_grid[X] += user_pt[X] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		    gedp->ged_gvp->gv_adc.gas_pos_grid[Y] += user_pt[Y] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		} else {
		    gedp->ged_gvp->gv_adc.gas_pos_grid[X] = user_pt[X] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		    gedp->ged_gvp->gv_adc.gas_pos_grid[Y] = user_pt[Y] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		}

		gedp->ged_gvp->gv_adc.gas_pos_grid[Z] = 0.0;
		ged_adc_grid_To_adc_view(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_model);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s hv' command requires 0 or 2 arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "dx") == 0) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_model[X] += user_pt[0] * gedp->ged_wdbp->dbip->dbi_local2base;
		ged_adc_model_To_adc_view(gedp->ged_gvp);
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dx' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "dy") == 0) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_model[Y] += user_pt[0] * gedp->ged_wdbp->dbip->dbi_local2base;
		ged_adc_model_To_adc_view(gedp->ged_gvp);
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dy' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "dz") == 0) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_model[Z] += user_pt[0] * gedp->ged_wdbp->dbip->dbi_local2base;
		ged_adc_model_To_adc_view(gedp->ged_gvp);
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dz' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "xyz") == 0) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_wdbp->dbip->dbi_base2local);
	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));
	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_adc.gas_pos_model, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_pos_model, user_pt);
	    }

	    ged_adc_model_To_adc_view(gedp->ged_gvp);
	    ged_adc_view_To_adc_grid(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s xyz' command requires 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "x") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_dv_x);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_adc.gas_dv_x += user_pt[0];
		} else {
		    gedp->ged_gvp->gv_adc.gas_dv_x = user_pt[0];
		}

		gedp->ged_gvp->gv_adc.gas_pos_view[X] = gedp->ged_gvp->gv_adc.gas_dv_x * INV_GED;
		gedp->ged_gvp->gv_adc.gas_pos_view[Y] = gedp->ged_gvp->gv_adc.gas_dv_y * INV_GED;
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s x' command requires 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "y") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_dv_y);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_adc.gas_dv_y += user_pt[0];
		} else {
		    gedp->ged_gvp->gv_adc.gas_dv_y = user_pt[0];
		}

		gedp->ged_gvp->gv_adc.gas_pos_view[X] = gedp->ged_gvp->gv_adc.gas_dv_x * INV_GED;
		gedp->ged_gvp->gv_adc.gas_pos_view[Y] = gedp->ged_gvp->gv_adc.gas_dv_y * INV_GED;
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s y' command requires 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchor_pos") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_pos);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i < 0 || 2 < i) {
		bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_pos' parameter accepts values of 0, 1, or 2.");
		return GED_ERROR;
	    }

	    gedp->ged_gvp->gv_adc.gas_anchor_pos = i;
	    ged_calc_adc_pos(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_pos' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchor_a1") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_a1);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_adc.gas_anchor_a1 = 1;
	    else
		gedp->ged_gvp->gv_adc.gas_anchor_a1 = 0;

	    ged_calc_adc_a1(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_a1' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchorpoint_a1") == 0) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, gedp->ged_wdbp->dbip->dbi_base2local);
	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, user_pt);
	    }

	    ged_calc_adc_a1(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchorpoint_a1' command accepts 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchor_a2") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_a2);

	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_adc.gas_anchor_a2 = 1;
	    else
		gedp->ged_gvp->gv_adc.gas_anchor_a2 = 0;

	    ged_calc_adc_a2(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_a2' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchorpoint_a2") == 0) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, gedp->ged_wdbp->dbip->dbi_base2local);

	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, user_pt);
	    }

	    ged_calc_adc_a2(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchorpoint_a2' command accepts 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchor_dst") == 0) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_dst);

	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i) {
		gedp->ged_gvp->gv_adc.gas_anchor_dst = 1;
	    } else
		gedp->ged_gvp->gv_adc.gas_anchor_dst = 0;

	    ged_calc_adc_dst(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_dst' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "anchorpoint_dst") == 0) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, gedp->ged_wdbp->dbip->dbi_base2local);
	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, user_pt);
	    }

	    ged_calc_adc_dst(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchorpoint_dst' command accepts 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "reset") == 0) {
	if (argc == 0) {
	    ged_adc_reset(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s reset' command accepts no arguments\n", command);
	return GED_ERROR;
    }

    if (strcmp(parameter, "vars") == 0) {
	ged_adc_vls_print(gedp->ged_gvp, gedp->ged_wdbp->dbip->dbi_base2local, &gedp->ged_result_str);
	return GED_OK;
    }

    if (strcmp(parameter, "help") == 0) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", command, usage);
	return GED_HELP;
    }

    bu_vls_printf(&gedp->ged_result_str, "%s: unrecognized command '%s'\nUsage: %s %s\n",
		  command, parameter, command, usage);
    return GED_ERROR;
}

static void
ged_adc_model_To_adc_view(struct ged_view *gvp)
{
    MAT4X3PNT(gvp->gv_adc.gas_pos_view, gvp->gv_model2view, gvp->gv_adc.gas_pos_model);
    gvp->gv_adc.gas_dv_x = gvp->gv_adc.gas_pos_view[X] * GED_MAX;
    gvp->gv_adc.gas_dv_y = gvp->gv_adc.gas_pos_view[Y] * GED_MAX;
}

static void
ged_adc_grid_To_adc_view(struct ged_view *gvp)
{
    point_t model_pt;
    point_t view_pt;

    VSETALL(model_pt, 0.0);
    MAT4X3PNT(view_pt, gvp->gv_model2view, model_pt);
    VADD2(gvp->gv_adc.gas_pos_view, view_pt, gvp->gv_adc.gas_pos_grid);
    gvp->gv_adc.gas_dv_x = gvp->gv_adc.gas_pos_view[X] * GED_MAX;
    gvp->gv_adc.gas_dv_y = gvp->gv_adc.gas_pos_view[Y] * GED_MAX;
}

static void
ged_adc_view_To_adc_grid(struct ged_view *gvp)
{
    point_t model_pt;
    point_t view_pt;

    VSETALL(model_pt, 0.0);
    MAT4X3PNT(view_pt, gvp->gv_model2view, model_pt);
    VSUB2(gvp->gv_adc.gas_pos_grid, gvp->gv_adc.gas_pos_view, view_pt);
}

void
ged_calc_adc_pos(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_pos == 1) {
	ged_adc_model_To_adc_view(gvp);
	ged_adc_view_To_adc_grid(gvp);
    } else if (gvp->gv_adc.gas_anchor_pos == 2) {
	ged_adc_grid_To_adc_view(gvp);
	MAT4X3PNT(gvp->gv_adc.gas_pos_model, gvp->gv_view2model, gvp->gv_adc.gas_pos_view);
    } else {
	ged_adc_view_To_adc_grid(gvp);
	MAT4X3PNT(gvp->gv_adc.gas_pos_model, gvp->gv_view2model, gvp->gv_adc.gas_pos_view);
    }
}

void
ged_calc_adc_a1(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_a1) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_adc.gas_anchor_pt_a1);
	dx = view_pt[X] * GED_MAX - gvp->gv_adc.gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gvp->gv_adc.gas_dv_y;

	if (dx != 0.0 || dy != 0.0) {
	    gvp->gv_adc.gas_a1 = RAD2DEG*atan2(dy, dx);
	    gvp->gv_adc.gas_dv_a1 = (1.0 - (gvp->gv_adc.gas_a1 / 45.0)) * GED_MAX;
	}
    }
}

void
ged_calc_adc_a2(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_a2) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_adc.gas_anchor_pt_a2);
	dx = view_pt[X] * GED_MAX - gvp->gv_adc.gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gvp->gv_adc.gas_dv_y;

	if (dx != 0.0 || dy != 0.0) {
	    gvp->gv_adc.gas_a2 = RAD2DEG*atan2(dy, dx);
	    gvp->gv_adc.gas_dv_a2 = (1.0 - (gvp->gv_adc.gas_a2 / 45.0)) * GED_MAX;
	}
    }
}

void
ged_calc_adc_dst(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_dst) {
	fastf_t dist;
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_adc.gas_anchor_pt_dst);

	dx = view_pt[X] * GED_MAX - gvp->gv_adc.gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gvp->gv_adc.gas_dv_y;
	dist = sqrt(dx * dx + dy * dy);
	gvp->gv_adc.gas_dst = dist * INV_GED;
	gvp->gv_adc.gas_dv_dist = (dist / M_SQRT1_2) - GED_MAX;
    } else
	gvp->gv_adc.gas_dst = (gvp->gv_adc.gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
}

static void
ged_adc_reset(struct ged_view *gvp)
{
    gvp->gv_adc.gas_dv_x = gvp->gv_adc.gas_dv_y = 0;
    gvp->gv_adc.gas_dv_a1 = gvp->gv_adc.gas_dv_a2 = 0;
    gvp->gv_adc.gas_dv_dist = 0;

    VSETALL(gvp->gv_adc.gas_pos_view, 0.0);
    MAT4X3PNT(gvp->gv_adc.gas_pos_model, gvp->gv_view2model, gvp->gv_adc.gas_pos_view);
    gvp->gv_adc.gas_dst = (gvp->gv_adc.gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
    gvp->gv_adc.gas_a1 = gvp->gv_adc.gas_a2 = 45.0;
    ged_adc_view_To_adc_grid(gvp);

    VSETALL(gvp->gv_adc.gas_anchor_pt_a1, 0.0);
    VSETALL(gvp->gv_adc.gas_anchor_pt_a2, 0.0);
    VSETALL(gvp->gv_adc.gas_anchor_pt_dst, 0.0);

    gvp->gv_adc.gas_anchor_pos = 0;
    gvp->gv_adc.gas_anchor_a1 = 0;
    gvp->gv_adc.gas_anchor_a2 = 0;
    gvp->gv_adc.gas_anchor_dst = 0;
}

static void
ged_adc_vls_print(struct ged_view *gvp, fastf_t base2local, struct bu_vls *out_vp)
{
    bu_vls_printf(out_vp, "draw = %d\n", gvp->gv_adc.gas_draw);
    bu_vls_printf(out_vp, "a1 = %.15e\n", gvp->gv_adc.gas_a1);
    bu_vls_printf(out_vp, "a2 = %.15e\n", gvp->gv_adc.gas_a2);
    bu_vls_printf(out_vp, "dst = %.15e\n", gvp->gv_adc.gas_dst * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "odst = %d\n", gvp->gv_adc.gas_dv_dist);
    bu_vls_printf(out_vp, "hv = %.15e %.15e\n",
		  gvp->gv_adc.gas_pos_grid[X] * gvp->gv_scale * base2local,
		  gvp->gv_adc.gas_pos_grid[Y] * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "xyz = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_pos_model[X] * base2local,
		  gvp->gv_adc.gas_pos_model[Y] * base2local,
		  gvp->gv_adc.gas_pos_model[Z] * base2local);
    bu_vls_printf(out_vp, "x = %d\n", gvp->gv_adc.gas_dv_x);
    bu_vls_printf(out_vp, "y = %d\n", gvp->gv_adc.gas_dv_y);
    bu_vls_printf(out_vp, "anchor_pos = %d\n", gvp->gv_adc.gas_anchor_pos);
    bu_vls_printf(out_vp, "anchor_a1 = %d\n", gvp->gv_adc.gas_anchor_a1);
    bu_vls_printf(out_vp, "anchor_a2 = %d\n", gvp->gv_adc.gas_anchor_a2);
    bu_vls_printf(out_vp, "anchor_dst = %d\n", gvp->gv_adc.gas_anchor_dst);
    bu_vls_printf(out_vp, "anchorpoint_a1 = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_anchor_pt_a1[X] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a1[Y] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a1[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_a2 = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_anchor_pt_a2[X] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a2[Y] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a2[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_dst = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_anchor_pt_dst[X] * base2local,
		  gvp->gv_adc.gas_anchor_pt_dst[Y] * base2local,
		  gvp->gv_adc.gas_anchor_pt_dst[Z] * base2local);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

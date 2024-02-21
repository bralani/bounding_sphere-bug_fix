/*                N M G _ B O O L E V A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize/nmg_booleval.cpp
 *
 * Classic NMG boolean evaluation path.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"

#include "bu/hook.h"
#include "bu/vls.h"
#include "bu/log.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

extern "C" {
static int
_facetize_bomb_hook(void *cdata, void *str)
{
    struct _ged_facetize_state *o = (struct _ged_facetize_state *)cdata;
    if (o->log_s->nmg_log_print_header) {
       bu_vls_printf(o->log_s->nmg_log, "%s\n", bu_vls_addr(o->log_s->nmg_log_header));
       o->log_s->nmg_log_print_header = 0;
    }
    bu_vls_printf(o->log_s->nmg_log, "%s\n", (const char *)str);
    return 0;
}

static int
_facetize_nmg_logging_hook(void *data, void *str)
{
    struct _ged_facetize_state *o = (struct _ged_facetize_state *)data;
    if (o->log_s->nmg_log_print_header) {
       bu_vls_printf(o->log_s->nmg_log, "%s\n", bu_vls_addr(o->log_s->nmg_log_header));
       o->log_s->nmg_log_print_header = 0;
    }
    bu_vls_printf(o->log_s->nmg_log, "%s\n", (const char *)str);
    return 0;
}

static void
_facetize_log_nmg(struct _ged_facetize_state *o)
{
    if (fileno(stderr) < 0)
       return;

    /* Seriously, bu_bomb, we don't want you blathering
     * to stderr... shut down stderr temporarily, assuming
     * we can find /dev/null or something similar */
    o->log_s->fnull = open("/dev/null", O_WRONLY);
    if (o->log_s->fnull == -1) {
       /* https://gcc.gnu.org/ml/gcc-patches/2005-05/msg01793.html */
       o->log_s->fnull = open("nul", O_WRONLY);
    }
    if (o->log_s->fnull != -1) {
       o->log_s->serr = fileno(stderr);
       o->log_s->stderr_stashed = dup(o->log_s->serr);
       dup2(o->log_s->fnull, o->log_s->serr);
       close(o->log_s->fnull);
    }

    /* Set bu_log logging to capture in nmg_log, rather than the
     * application defaults */
    bu_log_hook_delete_all();
    bu_log_add_hook(_facetize_nmg_logging_hook, (void *)o);

    /* Also engage the nmg bomb hooks */
    bu_bomb_delete_all_hooks();
    bu_bomb_add_hook(_facetize_bomb_hook, (void *)o);
}

static void
_facetize_log_default(struct _ged_facetize_state *o)
{
    if (fileno(stderr) < 0 || !o || !o->log_s)
       return;

    /* Put stderr back */
    if (o->log_s->fnull != -1) {
       fflush(stderr);
       dup2(o->log_s->stderr_stashed, o->log_s->serr);
       close(o->log_s->stderr_stashed);
       o->log_s->fnull = -1;
    }

    /* Restore bu_bomb hooks to the application defaults */
    bu_bomb_delete_all_hooks();
    bu_bomb_restore_hooks(o->log_s->saved_bomb_hooks);

    /* Restore bu_log hooks to the application defaults */
    bu_log_hook_delete_all();
    bu_log_hook_restore_all(o->log_s->saved_log_hooks);
}

}

static union tree *
facetize_region_end(struct db_tree_state *tsp,
		    const struct db_full_path *pathp,
		    union tree *curtree,
		    void *client_data)
{
    union tree **facetize_tree;

    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);

    facetize_tree = (union tree **)client_data;

    if (curtree->tr_op == OP_NOP) return curtree;

    if (*facetize_tree) {
	union tree *tr;
	BU_ALLOC(tr, union tree);
	RT_TREE_INIT(tr);
	tr->tr_op = OP_UNION;
	tr->tr_b.tb_regionp = REGION_NULL;
	tr->tr_b.tb_left = *facetize_tree;
	tr->tr_b.tb_right = curtree;
	*facetize_tree = tr;
    } else {
	*facetize_tree = curtree;
    }

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}


static struct model *
_try_nmg_facetize(struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int i;
    int failed = 0;
    struct db_tree_state init_state;
    union tree *facetize_tree;
    struct model *nmg_model;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    _facetize_log_nmg(s);

    db_init_db_tree_state(&init_state, gedp->dbip, wdbp->wdb_resp);

    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;

    facetize_tree = (union tree *)0;
    nmg_model = nmg_mm();
    init_state.ts_m = &nmg_model;

    if (!BU_SETJUMP) {
	/* try */
	i = db_walk_tree(gedp->dbip, argc, (const char **)argv,
			 1,
			&init_state,
			 0,			/* take all regions */
			 facetize_region_end,
			 rt_booltree_leaf_tess,
			 (void *)&facetize_tree
			);
    } else {
	/* catch */
	BU_UNSETJUMP;
	_facetize_log_default(s);
	return NULL;
    } BU_UNSETJUMP;

    if (i < 0) {
	/* Destroy NMG */
	_facetize_log_default(s);
	return NULL;
    }

    if (facetize_tree) {
	if (!BU_SETJUMP) {
	    /* try */
	    failed = nmg_boolean(facetize_tree, nmg_model, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    _facetize_log_default(s);
	    return NULL;
	} BU_UNSETJUMP;

    } else {
	failed = 1;
    }

    if (!failed && facetize_tree) {
	NMG_CK_REGION(facetize_tree->tr_d.td_r);
	facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;
    }

    if (facetize_tree) {
	db_free_tree(facetize_tree, &rt_uniresource);
    }

    _facetize_log_default(s);
    return (failed) ? NULL : nmg_model;
}

static int
_write_nmg(struct _ged_facetize_state *s, struct model *nmg_model, const char *name)
{
    struct ged *gedp = s->gedp;
    struct rt_db_internal intern;
    struct directory *dp;
    struct db_i *dbip = gedp->dbip;

    /* Export NMG as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)nmg_model;

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (s->verbosity) {
	    bu_log("Cannot add %s to directory\n", name);
	}
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (s->verbosity) {
	    bu_log("Failed to write %s to database\n", name);
	}
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}
int
_ged_facetize_nmgeval(struct _ged_facetize_state *s, int argc, const char **argv, const char *newname)
{
    int ret = FACETIZE_SUCCESS;
    struct model *nmg_model = NULL;

    nmg_model = _try_nmg_facetize(s, argc, argv);
    if (nmg_model == NULL) {
	if (s->verbosity > 1) {
	    bu_log("NMG(%s):  no resulting region, aborting\n", newname);
	}
	ret = FACETIZE_FAILURE;
	goto ged_nmg_obj_memfree;
    }

    /* Write the NMG */
    ret = _write_nmg(s, nmg_model, newname);

ged_nmg_obj_memfree:
    if (!s->quiet && ret != BRLCAD_OK) {
	bu_log("NMG: failed to generate %s\n", newname);
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


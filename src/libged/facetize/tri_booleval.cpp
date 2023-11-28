/*               T R I _ B O O L E V A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize/tri_booleval.cpp
 *
 * The core evaluation logic of the facetize command when targeting
 * triangle output.
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>

#include <string.h>

#include "manifold/manifold.h"
#ifdef USE_ASSETIMPORT
#include "manifold/meshIO.h"
#endif

#include "bu/app.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

// Customized version of rt_booltree_leaf_tess for Manifold processing
static union tree *
_booltree_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *data)
{
    int ts_status = 0;
    union tree *curtree;
    struct directory *dp;

    if (!tsp || !pathp || !ip)
	return TREE_NULL;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (tsp->ts_m)
	NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_RESOURCE(tsp->ts_resp);


    void *odata = NULL;
    ts_status = manifold_tessellate(&odata, tsp, pathp, ip, data);
    if (ts_status < 0) {
	// If nothing worked, return TREE_NULL
	return TREE_NULL;
    }

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = NULL;
    curtree->tr_d.td_d = odata;

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK)
	bu_log("_booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
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

    struct _ged_facetize_state *s = (struct _ged_facetize_state *)client_data;
    facetize_tree = &s->facetize_tree;

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

int
_ged_manifold_do_bool(
        union tree *tp, union tree *tl, union tree *tr,
        int op, struct bu_list *UNUSED(vlfree), const struct bn_tol *UNUSED(tol), void *data)
{
    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    if (!s)
	return -1;

    // Uncomment to see debugging output
    //_ged_facetize_log_default(s);

    // Translate op for MANIFOLD
    manifold::OpType manifold_op = manifold::OpType::Add;
    switch (op) {
	case OP_UNION:
	    manifold_op = manifold::OpType::Add;
	    break;
	case OP_INTERSECT:
	    manifold_op = manifold::OpType::Intersect;
	    break;
	case OP_SUBTRACT:
	    manifold_op = manifold::OpType::Subtract;
	    break;
	default:
	    manifold_op = manifold::OpType::Add;
    };

    // manifold_tessellate should have prepared our Manifold inputs - now
    // it's a question of doing the evaluation

    // We're either working with the results of CSG NMG tessellations,
    // or we have prior manifold_mesh results - figure out which.
    // If it's the NMG results, we need to make manifold_meshes for
    // processing.
    manifold::Manifold *lm = (manifold::Manifold *)tl->tr_d.td_d;
    manifold::Manifold *rm = (manifold::Manifold *)tr->tr_d.td_d;
    manifold::Manifold *result = NULL;
    int failed = 0;
    if (!lm || lm->Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - left manifold invalid\n");
	lm = NULL;
	failed = 1;
    }
    if (!rm || rm->Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - right manifold invalid\n");
	rm = NULL;
	failed = 1;
    }
    if (!failed) {
	manifold::Manifold bool_out;
	try {
	    bool_out = lm->Boolean(*rm, manifold_op);
	    if (bool_out.Status() != manifold::Manifold::Error::NoError) {
		bu_log("Error - bool result invalid\n");
		failed = 1;
	    }
	} catch (...) {
	    bu_log("Manifold boolean library threw failure\n");
#ifdef USE_ASSETIMPORT
	    const char *evar = getenv("GED_MANIFOLD_DEBUG");
	    // write out the failing inputs to files to aid in debugging
	    if (evar && strlen(evar)) {
		std::cerr << "Manifold op: " << (int)manifold_op << "\n";
		manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::string(".glb"), lm->GetMesh(), {});
		manifold::ExportMesh(std::string(tr->tr_d.td_name)+std::string(".glb"), rm->GetMesh(), {});
		bu_exit(EXIT_FAILURE, "Exiting to avoid overwriting debug outputs from Manifold boolean failure.");
	    }
#endif
	    failed = 1;
	}

#if 0
	// If we're debugging and need to capture glb for a "successful" case, these can be uncommented
	manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::string(".glb"), lm->GetMesh(), {});
	manifold::ExportMesh(std::string(tr->tr_d.td_name)+std::string(".glb"), rm->GetMesh(), {});
	manifold::ExportMesh(std::string(tl->tr_d.td_name)+std::to_string(op)+std::string(tr->tr_d.td_name)+std::string(".glb"), bool_out.GetMesh(), {});
#endif

	if (!failed)
	    result = new manifold::Manifold(bool_out);
    }

    if (failed) {
	// TODO - we may want to try an NMG boolean op (or possibly
	// even geogram) here as a fallback
#if 0
	manifold::Mesh lmesh = lm->GetMesh();
	Mesh -> NMG
	manifold::Mesh rmesh = rm->GetMesh();
	Mesh -> NMG
	int nmg_op = NMG_BOOL_ADD;
	switch (op) {
	    case OP_UNION:
		nmg_op = NMG_BOOL_ADD;
		break;
	    case OP_INTERSECT:
		nmg_op = NMG_BOOL_ISECT;
		break;
	    case OP_SUBTRACT:
		nmg_op = NMG_BOOL_SUB;
		break;
	    default:
		nmg_op = NMG_BOOL_ADD;
	}
	struct nmgregion *reg;
	if (!BU_SETJUMP) {
	    /* try */

	    /* move operands into the same model */
	    if (td_l->m_p != td_r->m_p)
		nmg_merge_models(td_l->m_p, td_r->m_p);
	    reg = nmg_do_bool(td_l, td_r, nmg_op, &RTG.rtg_vlfree, &wdbp->wdb_tol);
	    if (reg) {
		nmg_r_radial_check(reg, &RTG.rtg_vlfree, &wdbp->wdb_tol);
	    }
	    result = nmg_to_manifold(reg);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    return 1;
	} BU_UNSETJUMP;
#endif
    }

    // Memory cleanup
    if (tl->tr_d.td_d) {
	manifold::Manifold *m = (manifold::Manifold *)tl->tr_d.td_d;
	delete m;
	tl->tr_d.td_d = NULL;
    }
    if (tr->tr_d.td_d) {
	manifold::Manifold *m = (manifold::Manifold *)tr->tr_d.td_d;
	delete m;
	tr->tr_d.td_d = NULL;
    }

    if (failed) {
	tp->tr_d.td_d = NULL;
	return -1;
    }

    tp->tr_op = OP_TESS;
    tp->tr_d.td_d = (void *)result;
    return 0;
}

int
_ged_facetize_booleval(struct _ged_facetize_state *s, int argc, const char **argv, const char *newname)
{
    if (!s)
	return BRLCAD_ERROR;

    if (!argc || !argv)
	return BRLCAD_ERROR;

    struct ged *gedp = s->gedp;
    int i;
    union tree *ftree = NULL;

    struct db_tree_state init_state;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    db_init_db_tree_state(&init_state, gedp->dbip, wdbp->wdb_resp);
    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;
    init_state.ts_m = NULL;
    s->facetize_tree = (union tree *)0;

    /* First stage is to process the primitive instances */
    if (!BU_SETJUMP) {
	/* try */
	i = db_walk_tree(gedp->dbip, argc, (const char **)argv,
			 1,
			&init_state,
			 0,			/* take all regions */
			 facetize_region_end,
			 _booltree_leaf_tess,
			 (void *)s
			);
    } else {
	/* catch */
	BU_UNSETJUMP;
	i = -1;
    } BU_UNSETJUMP;

    if (i < 0) {
	_ged_facetize_log_default(s);
	return -1;
    }

    // TODO - We don't have a tree - whether that's an error or not depends
    if (!s->facetize_tree) {
	//_ged_facetize_log_default(s);
	return 0;
    }

    ftree = rt_booltree_evaluate(s->facetize_tree, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource, &_ged_manifold_do_bool, 0, (void *)s);
    if (!ftree) {
	_ged_facetize_log_default(s);
	return -1;
    }
    if (ftree->tr_d.td_d) {
	manifold::Manifold *om = (manifold::Manifold *)ftree->tr_d.td_d;
	manifold::Mesh rmesh = om->GetMesh();
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->bot_flags = 0;
	bot->num_vertices = (int)rmesh.vertPos.size();
	bot->num_faces = (int)rmesh.triVerts.size();
	bot->vertices = (double *)calloc(rmesh.vertPos.size()*3, sizeof(double));;
	bot->faces = (int *)calloc(rmesh.triVerts.size()*3, sizeof(int));
	for (size_t j = 0; j < rmesh.vertPos.size(); j++) {
	    bot->vertices[3*j] = rmesh.vertPos[j].x;
	    bot->vertices[3*j+1] = rmesh.vertPos[j].y;
	    bot->vertices[3*j+2] = rmesh.vertPos[j].z;
	}
	for (size_t j = 0; j < rmesh.triVerts.size(); j++) {
	    bot->faces[3*j] = rmesh.triVerts[j].x;
	    bot->faces[3*j+1] = rmesh.triVerts[j].y;
	    bot->faces[3*j+2] = rmesh.triVerts[j].z;
	}
	delete om;
	ftree->tr_d.td_d = NULL;

	// If we have a manifold_mesh, write it out as a bot
	return _ged_facetize_write_bot(s, bot, newname);
    }

    if (!s->quiet) {
	bu_log("FACETIZE: failed to generate %s\n", newname);
    }

    return -1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


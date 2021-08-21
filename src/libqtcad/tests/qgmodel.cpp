/*                     Q G M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file qgmodel.cpp
 *
 * Brief description
 *
 */

#include <iostream>
#include <unordered_map>
#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "../../libged/alphanum.h"
#include "qtcad/QgModel.h"

struct model_state {
    std::unordered_map<unsigned long long, QgInstance *> instances;
    std::unordered_map<unsigned long long, QgInstance *> tops_instances;
    std::vector<QgItem *> tops_items;
};

db_op_t int_to_op(int bool_op)
{
    switch (bool_op) {
	case OP_UNION:
	    return DB_OP_UNION;
	case OP_INTERSECT:
	    return DB_OP_INTERSECT;
	case OP_SUBTRACT:
	    return DB_OP_SUBTRACT;
	default:
	    return DB_OP_NULL;
    }
}

static void
_get_qg_instances(db_op_t curr_bool, struct db_i *dbip, struct directory *parent_dp, union tree *tp, struct model_state *s)
{
    db_op_t bool_op = curr_bool;
    QgInstance *qg = NULL;
    unsigned long long qg_hash = 0;
    std::string msg;

    if (!tp)
	return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    bool_op = int_to_op(tp->tr_op);
	    _get_qg_instances(bool_op, dbip, parent_dp, tp->tr_b.tb_right, s);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    _get_qg_instances(bool_op, dbip, parent_dp, tp->tr_b.tb_left, s);
	    break;
	case OP_DB_LEAF:
	    qg = new QgInstance;
	    qg->parent = parent_dp;
	    qg->dp = db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET);
	    qg->dp_name = std::string(tp->tr_l.tl_name);
	    qg->op = bool_op;
	    if (tp->tr_l.tl_mat) {
		MAT_COPY(qg->c_m, tp->tr_l.tl_mat);
	    } else {
		MAT_IDN(qg->c_m);
	    }
	    qg_hash = qg->hash();
	    if (s->instances.find(qg_hash) == s->instances.end()) {
		s->instances[qg_hash] = qg;
		msg = qg->print();
		std::cout << msg << "\n";
	    } else {
		delete qg;
		qg = NULL;
		std::cout << "Not creating duplicate\n";
	    }
	    break;

	default:
	    bu_log("unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("qg_instances tree walk\n");
    }
}


int
make_qg_instances(struct db_i *dbip, struct directory *parent_dp, struct rt_comb_internal *comb, struct model_state *s)
{
    int node_count = db_tree_nleaves(comb->tree);
    if (!node_count) return 0;
    _get_qg_instances(int_to_op(OP_UNION), dbip, parent_dp, comb->tree, s);
    return 0;
}

int
make_tops_instances(struct db_i *dbip, struct model_state *s)
{
    if (!s)
	return -1;

    struct directory **tops_paths;
    int tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &tops_paths);

    if (!tops_cnt) {
	bu_log("Error - unable to find tops objects!\n");
	return -1;
    }

    QgInstance *qg = NULL;
    unsigned long long qg_hash = 0;
    for (int i = 0; i < tops_cnt; i++) {
	qg = new QgInstance;
	qg->parent = NULL;
	qg->dp = tops_paths[i];
	qg->dp_name = std::string(qg->dp->d_namep);
	qg->op = DB_OP_UNION;
	MAT_IDN(qg->c_m);
	qg_hash = qg->hash();
	s->tops_instances[qg_hash] = qg;
    }

    // Cleanup
    bu_free(tops_paths, "tops array");

    return tops_cnt;
}

struct QgItem_cmp {
    inline bool operator() (const QgItem *i1, const QgItem *i2)
    {
	if (!i1 && i2)
	    return true;
	if (i1 && !i2)
	    return false;
	if (!i1->inst && i2->inst)
	    return true;
	if (i1->inst && !i2->inst)
	    return false;

	const char *n1 = i1->inst->dp_name.c_str();
	const char *n2 = i2->inst->dp_name.c_str();
	if (alphanum_impl(n1, n2, NULL) < 0)
	    return true;
	return false;
    }
};

void
make_tops_items(struct model_state *s)
{
    std::unordered_map<unsigned long long, QgInstance *>::iterator t_it;
    for (t_it = s->tops_instances.begin(); t_it != s->tops_instances.end(); t_it++) {
	QgInstance *qg = t_it->second;
	QgItem *qi = new QgItem;
	qi->parent = NULL;
	qi->inst = qg;
	s->tops_items.push_back(qi);
    }
    // Sort tops_items according to alphanum
    std::sort(s->tops_items.begin(), s->tops_items.end(), QgItem_cmp());

    for (size_t i = 0; i < s->tops_items.size(); i++) {
	std::cout << s->tops_items[i]->inst->dp_name << "\n";
    }
}


int main(int argc, char *argv[])
{

    bu_setprogname(argv[0]);

    argc--; argv++;

    if (argc != 1)
	bu_exit(-1, "need to specify .g file\n");

    struct db_i *dbip = db_open(argv[0], DB_OPEN_READONLY);
    if (dbip == DBI_NULL)
	bu_exit(-1, "db_open failed on geometry database file %s\n", argv[0]);

    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
    }
    db_update_nref(dbip, &rt_uniresource);

    struct model_state s;

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp = RT_DIR_NULL;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_HIDDEN) continue;
	    if (dp->d_flags & RT_DIR_COMB) {
		bu_log("Comb: %s\n", dp->d_namep);
		struct rt_db_internal intern;
		struct rt_comb_internal *comb;
		if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		    continue;
		}
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		make_qg_instances(dbip, dp, comb, &s);
	    }
	    // TODO - extrusions, other primitives that reference another obj
	}
    }
    bu_log("Hierarchy instance cnt: %zd\n", s.instances.size());

    // The above logic will create hierarchy instances, but it won't create the
    // top level instances (which, by definition, aren't under anything.)  Make
    // a second pass using db_ls to create those instances (this depends on
    // db_update_nref being up to date.)
    make_tops_instances(dbip, &s);
    bu_log("Top instance cnt: %zd\n", s.tops_instances.size());


    // Make items that correspond to the top level instances - those are the only
    // items that will always be present in some form.
    make_tops_items(&s);

    // TODO - so the rough progression of steps here is:
    // 2.  Implement "open" and "close" routines for the items that will exercise
    // the logic to identify, populate, and clear items based on child info.  So far,
    // we're still within the read-only capabilities of the current model, but we want
    // to make sure of this bookkeeping before the next step...
    //
    // 3.  Add callback support for syncing the instance sets after a database
    // operation.  This is the most foundational of the pieces needed for
    // read/write support.  The callback experiments we've been doing have some
    // of this, but we'll need to carefully consider how to handle it.  My
    // current thought is we'll accumulate items to change based on QgInstance
    // changes made, and then do a single Item update pass at the end for
    // performance reasons (we don't want the view updating a whole bunch if
    // thousands of objects are impacted by a single edit...)  We'll probably want
    // to update the QgInstances from the per-object callbacks, and then
    // rebuild the tops list and walk down the Items associated with those instances
    // to perform any needed updates.
    //
    // 4. Figure out how to do the Item update pass in response to #3.  In
    // particular, how to preserve the tree's "opened/closed" state through
    // edit operations.  For each child items vector we'll build a new vector
    // based on the QgInstances tree, comparing it as we go to the Items array
    // that existed previously.  For each old item, if the new item matches the
    // old (qghash comparison?) reuse the old item, otherwise create a new one.
    // This is where set_difference may be useful (not clear yet - if so it may require
    // some fancy tricks with the comparison function...)  We'll need to do this for
    // all active items, but unless the user has tried to expand all trees in
    // all paths this should be a relatively small subset of the .g file
    // structure to verify.
    //

    return s.instances.size() + s.tops_instances.size();
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

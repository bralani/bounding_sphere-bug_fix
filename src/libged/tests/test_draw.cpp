/*                    T E S T _ D R A W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file test_draw.cpp
 *
 * Experiment with approaches for managing drawing and selecting
 *
 */

#include "common.h"

#include <unordered_map>
#include <unordered_set>

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"
}

#include <stdio.h>
#include <bu.h>
#include <ged.h>

struct draw_ctx {
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> p_c;
    std::unordered_map<unsigned long long, unsigned long long> i_map;
};

HIDDEN void
list_children(struct draw_ctx *ctx, unsigned long long phash, struct db_i *dbip, union tree *tp,
    std::unordered_map<unsigned long long, unsigned long long> &i_count)
{
    if (!tp)
	return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    XXH64_state_t h_state;
    unsigned long long chash;

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    list_children(ctx, phash, dbip, tp->tr_b.tb_right, i_count);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    list_children(ctx, phash, dbip, tp->tr_b.tb_left, i_count);
	    break;
	case OP_DB_LEAF:
	    XXH64_reset(&h_state, 0);
	    XXH64_update(&h_state, tp->tr_l.tl_name, strlen(tp->tr_l.tl_name)*sizeof(char));
	    chash = (unsigned long long)XXH64_digest(&h_state);
	    i_count[chash] += 1;
	    if (i_count[chash] > 1) {
		// If we've got multiple instances of the same object in the tree,
		// hash the string labeling the instance and map it to the correct
		// parent comb so we can associate it with the tree contents
		struct bu_vls iname = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&iname, "%s@%llu", tp->tr_l.tl_name, i_count[chash]);
		XXH64_reset(&h_state, 0);
		XXH64_update(&h_state, bu_vls_cstr(&iname), bu_vls_strlen(&iname)*sizeof(char));
		unsigned long long ihash = (unsigned long long)XXH64_digest(&h_state);
		ctx->i_map[ihash] = chash;
		ctx->p_c[phash].insert(ihash);
	    } else {
		ctx->p_c[phash].insert(chash);
	    }
	    break;
	default:
	    bu_log("list_children: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("list_children\n");
    }
}

static void
comb_hash(struct ged *gedp, struct draw_ctx *ctx, struct directory *dp)
{
    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    struct bu_vls hname = BU_VLS_INIT_ZERO;
    XXH64_state_t h_state;

    // First, get parent name hash
    XXH64_reset(&h_state, 0);
    bu_vls_sprintf(&hname, "%s", dp->d_namep);
    XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
    unsigned long long phash = (unsigned long long)XXH64_digest(&h_state);

    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = ctx->p_c.find(phash);
    //pc_it->second.clear();
    if (pc_it == ctx->p_c.end()) {
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree)
	    return;

	std::unordered_map<unsigned long long, unsigned long long> i_count;
	list_children(ctx, phash, gedp->dbip, comb->tree, i_count);
	rt_db_free_internal(&in);
    }

    std::unordered_set<unsigned long long>::iterator cs_it;
    for (pc_it = ctx->p_c.begin(); pc_it != ctx->p_c.end(); pc_it++) {
	bu_log("%llu:\n", pc_it->first);
	for (cs_it = pc_it->second.begin(); cs_it != pc_it->second.end(); cs_it++) {
	    bu_log("	%llu\n", *cs_it);
	}
    }
    bu_log("\n");
}

static void
fp_path_split(std::vector<std::string> &objs, const char *str)
{
    std::string s(str);
    while (s.length() && s.c_str()[0] == '/')
	s.erase(0, 1);  //Remove leading slashes

    std::string nstr;
    bool escaped = false;
    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if (escaped) {
		nstr.push_back(s[i]);
		escaped = false;
		continue;
	    }
	    escaped = true;
	    continue;
	}
	if (s[i] == '/' && !escaped) {
	    if (nstr.length())
		objs.push_back(nstr);
	    nstr.clear();
	    continue;
	}
	nstr.push_back(s[i]);
	escaped = false;
    }
    if (nstr.length())
	objs.push_back(nstr);
}

static std::string
name_deescape(std::string &name)
{
    std::string s(name);
    std::string nstr;

    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if ((i+1) < s.length())
		nstr.push_back(s[i+1]);
	    i++;
	} else {
	    nstr.push_back(s[i]);
	}
    }

    return nstr;
}

static size_t
path_elements(std::vector<std::string> &elements, const char *path)
{
    std::vector<std::string> substrs;
    fp_path_split(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	std::string cleared = name_deescape(substrs[i]);
	elements.push_back(cleared);
    }
    return elements.size();
}

static void
split_test(const char *path)
{
    std::vector<std::string> substrs;
    path_elements(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	bu_log("%s\n", substrs[i].c_str());
    }
    bu_log("\n");
}


static bool
check_elements(struct ged *gedp, struct draw_ctx *ctx, std::vector<std::string> &elements)
{
    if (!gedp || !elements.size())
	return false;

    // If we have only a single entry, make sure it's a valid db entry and return
    if (elements.size() == 1) {
	struct directory *dp = db_lookup(gedp->dbip, elements[0].c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    return false;
	comb_hash(gedp, ctx, dp);
	return true;
    }

    // Deeper paths take more work.  NOTE - if the last element isn't a valid
    // database entry, we have an invalid comb entry specified.  We are more
    // tolerant of such paths and let them be "drawn" to allow views to add
    // wireframes to scenes when objects are renamed to make an existing
    // invalid instance "good".  However, any comb entries above the final
    // entry *must* be valid objects in the database.
    for (size_t i = 0; i < elements.size(); i++) {
	struct directory *dp = db_lookup(gedp->dbip, elements[i].c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL && i < elements.size() - 1) {
	    bu_log("invalid path: %s\n", elements[i].c_str());
	    return false;
	}
	comb_hash(gedp, ctx, dp);
    }

    // parent/child relationship validate
    struct bu_vls hname = BU_VLS_INIT_ZERO;
    XXH64_state_t h_state;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    unsigned long long phash = 0;
    unsigned long long chash = 0;
    XXH64_reset(&h_state, 0);
    bu_vls_sprintf(&hname, "%s", elements[0].c_str());
    bu_log("parent: %s\n", elements[0].c_str());
    XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
    phash = (unsigned long long)XXH64_digest(&h_state);
    for (size_t i = 1; i < elements.size(); i++) {
	pc_it = ctx->p_c.find(phash);
	// The parent comb structure is stored only under its original name's hash - if
	// we have a numbered instance from a comb tree as a parent, we may be able to
	// map it to the correct entry with i_map.  If not, we have an invalid path.
	if (pc_it == ctx->p_c.end()) {
	    std::unordered_map<unsigned long long, unsigned long long>::iterator i_it = ctx->i_map.find(phash);
	    if (i_it == ctx->i_map.end()) {
		return false;
	    }
	    phash = i_it->second;
	}
	XXH64_reset(&h_state, 0);
	bu_vls_sprintf(&hname, "%s", elements[i].c_str());
	XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
	chash = (unsigned long long)XXH64_digest(&h_state);
	bu_log("child: %s\n\n", elements[i].c_str());

	if (pc_it->second.find(chash) == pc_it->second.end()) {
	    bu_log("Invalid element path: %s\n", elements[i].c_str());
	    return false;
	}
	phash = chash;
	bu_log("parent: %s\n", elements[i].c_str());
    }

    return true;
}

int
main(int ac, char *av[]) {
    struct ged *gedp;

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    gedp = ged_open("db", av[1], 1);


    split_test("all.g/cone.r/cone.s");
    split_test("all.g/cone.r/cone.s/");
    split_test("all.g/cone.r/cone.s//");
    split_test("all.g/cone.r/cone.s\\//");
    split_test("all.g\\/cone.r\\//cone.s");
    split_test("all.g\\\\/cone.r\\//cone.s");
    split_test("all.g\\\\\\/cone.r\\//cone.s");
    split_test("all.g\\\\\\\\/cone.r\\//cone.s");
    split_test("all.g\\cone.r\\//cone.s");
    split_test("all.g\\\\cone.r\\//cone.s");
    split_test("all.g\\\\\\cone.r\\//cone.s");
    split_test("all.g\\\\\\\\cone.r\\//cone.s");

    struct draw_ctx ctx;
    {
	std::vector<std::string> pe;
	path_elements(pe, "all.g/cone.r/cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "all.g/cone2.r\\//cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "cone2.r/cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "cone2.r\\//cone.s");
	check_elements(gedp, &ctx, pe);
    }


    ged_close(gedp);

    return 0;
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

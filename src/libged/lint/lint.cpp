/*                         L I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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
/** @file libged/lint.cpp
 *
 * The lint command for finding and reporting problems in .g files.
 */

#include "common.h"

#include <set>
#include <map>
#include <string>
#include <sstream>
#include "json.hpp"

extern "C" {
#include "bu/opt.h"
}
#include "./ged_lint.h"

std::string
lint_data::summary(int verbosity)
{
    if (verbosity < 0)
	return std::string("");

    std::map<std::string, std::set<std::string>> categories;
    std::map<std::string, std::set<std::string>> obj_problems;

    for(nlohmann::json::const_iterator it = j.begin(); it != j.end(); ++it) {
	const nlohmann::json &pdata = *it;
	if (!pdata.contains("problem_type")) {
	    bu_log("Unexpected JSON entry\n");
	    continue;
	}
	std::string ptype(pdata["problem_type"]);

	if (ptype == std::string("cyclic_path")) {
	    if (!pdata.contains("path")) {
		bu_log("Error - malformed cyclic_path JSON data\n");
		continue;
	    }
	    std::string cpath(pdata["path"]);
	    categories[ptype].insert(cpath);
	}
	if (!ptype.compare(0, 7, std::string("missing"))) {
	    if (!pdata.contains("path")) {
		bu_log("Error - malformed missing reference JSON data\n");
		continue;
	    }
	    std::string mpath(pdata["path"]);
	    categories[std::string("missing")].insert(mpath);
	}
	if (!ptype.compare(0, 7, std::string("invalid"))) {
	    if (!pdata.contains("object_name")) {
		bu_log("Error - malformed invalid object reference JSON data\n");
		continue;
	    }
	    std::string oname(pdata["object_name"]);
	    categories[std::string("invalid")].insert(oname);
	    obj_problems[oname].insert(ptype);
	}
    }

    std::string ostr;
    std::set<std::string>::iterator s_it, o_it;
    std::map<std::string, std::set<std::string>>::iterator c_it;
    c_it = categories.find(std::string("cyclic_path"));
    if (c_it != categories.end()) {
	const std::set<std::string> &cpaths = c_it->second;
	ostr.append(std::string("Found cyclic paths:\n"));
	for (s_it = cpaths.begin(); s_it != cpaths.end(); s_it++)
	    ostr.append(std::string("\t") + *s_it + std::string("\n"));
    }
    c_it = categories.find(std::string("missing"));
    if (c_it != categories.end()) {
	const std::set<std::string> &mpaths = c_it->second;
	ostr.append(std::string("Found references to missing objects or files:\n"));
	for (s_it = mpaths.begin(); s_it != mpaths.end(); s_it++)
	    ostr.append(std::string("\t") + *s_it + std::string("\n"));
    }

    c_it = categories.find(std::string("invalid"));
    if (c_it != categories.end()) {
	const std::set<std::string> &invobjs = c_it->second;
	ostr.append(std::string("Found invalid objects:\n"));
	for (s_it = invobjs.begin(); s_it != invobjs.end(); s_it++) {
	    ostr.append(std::string("\t") + *s_it);
	    if (verbosity) {
		ostr.append(std::string(" ["));
		for (o_it = obj_problems[*s_it].begin(); o_it != obj_problems[*s_it].end(); o_it++) {
		    ostr.append(*o_it + std::string(","));
		}
		ostr.pop_back();
		ostr.append(std::string("]"));
	    }
	    ostr.append(std::string("\n"));
	}
    }

    return ostr;
}

extern "C" int
ged_lint_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: lint [-h] [-v[v...]] [ -CMS ] [-F <filter>] [obj1] [obj2] [...]\n";
    int print_help = 0;
    int verbosity = 0;
    int cyclic_check = 0;
    int missing_check = 0;
    int invalid_shape_check = 0;
    struct directory **dpa = NULL;
    struct bu_vls filter = BU_VLS_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    // Primary data container to hold results
    lint_data ldata;
    ldata.gedp = gedp;

    struct bu_opt_desc d[7];
    BU_OPT(d[0],  "h", "help",          "",  NULL,         &print_help,           "Print help and exit");
    BU_OPT(d[1],  "v", "verbose",       "",  &_ged_vopt,   &verbosity,            "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[2],  "C", "cyclic",        "",  NULL,         &cyclic_check,         "Check for cyclic paths (combs whose children reference their parents - potential for infinite looping)");
    BU_OPT(d[3],  "M", "missing",       "",  NULL,         &missing_check,        "Check for objects referenced by other objects that are not in the database");
    BU_OPT(d[4],  "I", "invalid-shape", "",  NULL,         &invalid_shape_check,  "Check for objects that are intended to be valid shapes but do not satisfy validity criteria (examples include non-solid BoTs and twisted arbs)");
    BU_OPT(d[5],  "F", "filter",        "",  &bu_opt_vls,  &filter,               "For checks on existing geometry objects, apply search-style filters to check only the subset of objects that satisfy the filters. Note that these filters do NOT impact cyclic and missing geometry checks.");
    BU_OPT_NULL(d[6]);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	bu_vls_free(&filter);
	return BRLCAD_OK;
    }

    if (argc) {
	dpa = (struct directory **)bu_calloc(argc+1, sizeof(struct directory *), "dp array");
	int nonexist_obj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
	if (nonexist_obj_cnt) {
	    int i;
	    bu_vls_printf(gedp->ged_result_str, "Object argument(s) supplied to lint that do not exist in the database:\n");
	    for (i = argc - nonexist_obj_cnt - 1; i < argc; i++) {
		bu_vls_printf(gedp->ged_result_str, " %s\n", argv[i]);
	    }
	    bu_free(dpa, "dpa");
	    bu_vls_free(&filter);
	    return BRLCAD_ERROR;
	}
    }

    int have_specific_test = cyclic_check+missing_check+invalid_shape_check;

    if (!have_specific_test || cyclic_check) {
	bu_log("Checking for cyclic paths...\n");
	if (_ged_cyclic_check(&ldata, argc, dpa) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    if (!have_specific_test || missing_check) {
	bu_log("Checking for references to non-extant objects...\n");
	if (_ged_missing_check(&ldata, argc, dpa) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    if (!have_specific_test || invalid_shape_check) {
	bu_log("Checking for invalid objects...\n");
	if (_ged_invalid_shape_check(&ldata, argc, dpa, verbosity) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    if (dpa)
	bu_free(dpa, "dp array");

    std::string report = ldata.summary(verbosity);
    bu_vls_printf(gedp->ged_result_str, "%s", report.c_str());

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl lint_cmd_impl = { "lint", ged_lint_core, GED_CMD_DEFAULT };
    const struct ged_cmd lint_cmd = { &lint_cmd_impl };
    const struct ged_cmd *lint_cmds[] = { &lint_cmd,  NULL };

    static const struct ged_plugin pinfo = { GED_API,  lint_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
    {
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


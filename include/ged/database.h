/*                        D A T A B A S E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @addtogroup ged_database
 *
 * Geometry EDiting Library Database Level Functions.
 *
 */
/** @{ */
/** @file ged/database.h */

#ifndef GED_DATABASE_H
#define GED_DATABASE_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS

/** Check if a database is open */
#define GED_CHECK_DATABASE_OPEN(_gedp, _flags) \
    if ((_gedp) == GED_NULL || (_gedp)->ged_wdbp == RT_WDB_NULL || (_gedp)->ged_wdbp->dbip == DBI_NULL) { \
	int ged_check_database_open_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_database_open_quiet) { \
	    if ((_gedp) != GED_NULL) { \
		bu_vls_trunc((_gedp)->ged_result_str, 0); \
		bu_vls_printf((_gedp)->ged_result_str, "A database is not open!"); \
	    } else {\
		bu_log("A database is not open!\n"); \
	    } \
	} \
	return (_flags); \
    }

/**
 * Set/get the database title
 */
GED_EXPORT extern int ged_title(struct ged *gedp, int argc, const char *argv[]);


/* defined in rt.c */
GED_EXPORT extern int ged_build_tops(struct ged	*gedp,
				     char		**start,
				     char		**end);
GED_EXPORT extern size_t ged_count_tops(struct ged *gedp);

/**
 * Find all top level objects
 */
GED_EXPORT extern int ged_tops(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get dbip
 */
GED_EXPORT extern int ged_dbip(struct ged *gedp, int argc, const char *argv[]);


/**
 * Dump a full copy of the database into file.g
 */
GED_EXPORT extern int ged_dump(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find combinations that reference object
 */
GED_EXPORT extern int ged_find(struct ged *gedp, int argc, const char *argv[]);

/**
 * Globs expression against the database
 */
GED_EXPORT extern int ged_glob(struct ged *gedp, int argc, const char *argv[]);

/**
 * Import a database into the current database using an auto-incrementing or custom affix
 */
GED_EXPORT extern int ged_concat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Check for duplicate names in file
 */
GED_EXPORT extern int ged_dup(struct ged *gedp, int argc, const char *argv[]);

/**
 * Text edit the color table
 */
GED_EXPORT extern int ged_edcolor(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit file.
 */
GED_EXPORT extern int ged_editit(struct ged *gedp, int argc, const char *argv[]);

/**
 * List the objects in this database
 */
GED_EXPORT extern int ged_ls(struct ged *gedp, int argc, const char *argv[]);

/**
 * List all paths from name(s) to leaves
 */
GED_EXPORT extern int ged_pathlist(struct ged *gedp, int argc, const char *argv[]);

/**
 * Lists all paths matching the input path
 */
GED_EXPORT extern int ged_pathsum(struct ged *gedp, int argc, const char *argv[]);

/**
 * Checks that each directory in the supplied path actually has the subdirectories
 * that are implied by the path.
 */
GED_EXPORT extern int ged_path_validate(struct ged *gedp, const struct db_full_path * const path);

/**
 * Returns a list of id to region name mappings for the entire database.
 */
GED_EXPORT extern int ged_rmap(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get tessellation and calculation tolerances
 */
GED_EXPORT extern int ged_tol(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the database units
 */
GED_EXPORT extern int ged_units(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns the database version.
 */
GED_EXPORT extern int ged_version(struct ged *gedp, int argc, const char *argv[]);

/**
 * Write region ident codes to filename.
 */
GED_EXPORT extern int ged_wcodes(struct ged *gedp, int argc, const char *argv[]);




__END_DECLS

#endif /* GED_DATABASE_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

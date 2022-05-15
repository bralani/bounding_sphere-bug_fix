/*                         W H I C H . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2022 United States Government as represented by
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

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* common headers */
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"


/* how big should PATH from getenv ever be */
#define MAXPATHENV 32767

/* container for path match results */
static char bu_which_result[MAXPATHLEN] = {0};


static char *
which_path(const char *cmd, const char *path, char *result)
{
    if (!cmd || !path || !result)
	return NULL;

    char test_result[MAXPATHLEN] = {0};

    char *position = NULL;
    char *initial_path = bu_strdup(path);
    char *directory = initial_path;

    /* search for executable iterating over path entries */
    do {
	struct bu_vls vp = BU_VLS_INIT_ZERO;

	position = strchr(directory, BU_PATH_SEPARATOR);
	if (position) {
	    /* 'directory' can't be const because we have to change a character here: */
	    *position = '\0';
	}

	/* empty means use current dir */
	size_t dirlen = strlen(directory);
	if (dirlen == 0) {
	    /* "./cmd" */
	    bu_vls_putc(&vp, '.');
	    bu_vls_putc(&vp, BU_DIR_SEPARATOR);
	    bu_vls_strcat(&vp, cmd);
	    bu_strlcpy(test_result, bu_vls_cstr(&vp), MAXPATHLEN);
	    bu_vls_free(&vp);
	} else if (dirlen <= MAXPATHLEN-2) {
	    /* "dir/cmd" */
	    bu_vls_strcpy(&vp, directory);
	    bu_vls_putc(&vp, BU_DIR_SEPARATOR);
	    bu_vls_strcat(&vp, cmd);
	    bu_strlcpy(test_result, bu_vls_cstr(&vp), MAXPATHLEN);
	    bu_vls_free(&vp);
	} else {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		bu_log("WARNING: PATH dir is too long (%zu > %zu), skipping.\n"
		       "         dir = [%s]\n", dirlen, (size_t)MAXPATHLEN-2, directory);
	    }
	    continue;
	}

	if (bu_file_exists(test_result, NULL)) {
	    if (test_result[0] == '\0')
		continue;
	    bu_free(initial_path, "strdup(path)");
	    bu_strlcpy(result, test_result, MAXPATHLEN);
	    return result;
	}

	if (position) {
	    directory = position + 1;
	} else {
	    directory = NULL;
	}
    } while (directory);

    return NULL;
}


const char *
bu_which(const char *cmd)
{
    static const char *gotpath = NULL;

    char PATH[MAXPATHENV];

    char *position = NULL;

    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("bu_which: [%s]\n", cmd);
    }

    if (UNLIKELY(!cmd || (strlen(cmd) == 0))) {
	return NULL;
    }

    /* start fresh */
    memset(PATH, 0, MAXPATHENV);
    memset(bu_which_result, 0, MAXPATHLEN);

    /* check for full/relative path match */
    bu_strlcpy(bu_which_result, cmd, MAXPATHLEN);
    if (!BU_STR_EQUAL(bu_which_result, cmd)) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("command [%s] is too long\n", cmd);
	}
	return NULL;
    }

    if (bu_file_exists(bu_which_result, NULL) && strchr(bu_which_result, BU_DIR_SEPARATOR)) {
	if (bu_which_result[0] == '\0')
	    return NULL; /* never return empty */
	return bu_which_result;
    }

    /* load up the PATH from the caller's user environment */
    gotpath = getenv("PATH");
    if (gotpath) {
	bu_strlcpy(PATH, gotpath, MAXPATHENV);

	/* make sure it fit, we have a problem if it did not */
	if (!BU_STR_EQUAL(PATH, gotpath)) {
	    position = strrchr(PATH, BU_PATH_SEPARATOR);
	    if (position) {
		position = NULL;
	    } else {
		/* too much and no separator? wtf. */
		if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		    bu_log("path contains invalid data?\n");
		}
		return NULL;
	    }
	}

	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("PATH is %s\n", PATH);
	}
    } else {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("PATH is NULL\n");
	}
	return NULL;
    }

    if (which_path(cmd, PATH, bu_which_result))
	return bu_which_result;

    /* no path or no match */
    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("no %s in %s\n", cmd, gotpath ? gotpath : "(no path)");
    }

    return NULL;
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

/*                     D B 5 _ S I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @addtogroup db5 */
/** @{ */
/** @file librt/db5_size.c
 *
 * Calculate sizes of v5 database objects.
 *
 */

#include "common.h"

#include <set>
#include <queue>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bu/units.h"
#include "rt/db5.h"
#include "raytrace.h"
#include "librt_private.h"

/* flags for size calculations */
#define RT_DIR_SIZE_FINALIZED   0x1
#define RT_DIR_SIZE_ATTR_DONE   0x2
#define RT_DIR_SIZE_COMB_DONE   0x4
#define RT_DIR_SIZE_ACTIVE      0x8

/* sizes in directory pointer arrays */
#define RT_DIR_SIZE_OBJ 0
#define RT_DIR_SIZE_KEEP 1
#define RT_DIR_SIZE_XPUSH 2

#define HSIZE(buf, var) \
    (void)bu_humanize_number(buf, 5, (int64_t)var, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);


HIDDEN int
db_get_external_reuse(register struct bu_external *ep, const struct directory *dp, const struct db_i *dbip)
{
    ep->ext_nbytes = dp->d_len;

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy((char *)ep->ext_buf, dp->d_un.ptr, ep->ext_nbytes);
	return 0;
    }

    if (db_read(dbip, (char *)ep->ext_buf, ep->ext_nbytes, dp->d_addr) < 0) {
	memset(ep->ext_buf, 0, dp->d_len);
	ep->ext_nbytes = 0;
	return -1;      /* VERY BAD */
    }
    return 0;
}

HIDDEN int
rt_db_get_internal_reuse(
	struct bu_external *ext,
	struct rt_db_internal *ip,
	const struct directory *dp,
	const struct db_i *dbip,
	const mat_t mat,
	struct resource *resp)
{
    register int id;
    int ret;
    struct db5_raw_internal raw;
    RT_DB_INTERNAL_INIT(ip);
    if (db_get_external_reuse(ext, dp, dbip) < 0) return -1;
    if (db5_get_raw_internal_ptr(&raw, ext->ext_buf) == NULL) return -1;
    if (!raw.body.ext_buf) return -1;

    /* FIXME: This is a temporary kludge accommodating dumb binunifs
     * that don't export their minor type or have table entries for
     * all their types. (this gets pushed up when a functab wrapper is
     * created)
     */
    switch (raw.major_type) {
	case DB5_MAJORTYPE_BRLCAD:
	    id = raw.minor_type; break;
	case DB5_MAJORTYPE_BINARY_UNIF:
	    id = ID_BINUNIF; break;
	default:
	    return -1;
    }

    if (id == ID_BINUNIF) {
	/* FIXME: binunif export needs to write out minor_type so this isn't
	 * needed, but breaks compatibility.  slate for v6. */
	ret = rt_binunif_import5_minor_type(ip, &raw.body, mat, dbip, resp, raw.minor_type);
    } else if (OBJ[id].ft_import5) {
	ret = OBJ[id].ft_import5(ip, &raw.body, mat, dbip, resp);
    }
    if (ret < 0) {
	rt_db_free_internal(ip);
	return -1;
    }

    ip->idb_major_type = raw.major_type;
    ip->idb_minor_type = raw.minor_type;
    ip->idb_meth = &OBJ[id];

    return id;
}


HIDDEN long
_db5_get_attributes_size(struct bu_external *ext, const struct db_i *dbip, const struct directory *dp)
{
    long attr_size = 0;
    struct db5_raw_internal raw;
    if (dbip->dbi_version < 5) return 0; /* db4 has no attributes */
    if (db_get_external_reuse(ext, dp, dbip) < 0) return 0;
    if (db5_get_raw_internal_ptr(&raw, ext->ext_buf) == NULL) {
	return 0;
    }
    attr_size = raw.attributes.ext_nbytes;
    return attr_size;
}

HIDDEN int
_cmp_dp_states(const void *a, const void *b, void *UNUSED(arg))
{
    struct directory **dpe1 = (struct directory **)a;
    struct directory **dpe2 = (struct directory **)b;
    struct directory *dp1 = *dpe1;
    struct directory *dp2 = *dpe2;

    //bu_log("a: name %s, flags %d\n",  dp1->d_namep, dp1->s_flags);
    //bu_log("b: name %s, flags %d\n",  dp2->d_namep, dp2->s_flags);

    if ((dp1->s_flags & RT_DIR_SIZE_FINALIZED) && !(dp2->s_flags & RT_DIR_SIZE_FINALIZED)) return 1;
    if (!(dp1->s_flags & RT_DIR_SIZE_FINALIZED) && (dp2->s_flags & RT_DIR_SIZE_FINALIZED)) return -1;
    if (dp1->s_flags > dp2->s_flags) return -1;
    if (dp1->s_flags < dp2->s_flags) return 1;
    return 0;
}

long
db5_size(struct db_i *dbip, struct directory *in_dp, int flags)
{
    int local_flags = flags;
    int size_flag_cnt = 0;
    int i, j, finalized, finalized_prev, active_prev;
    int active = 0;
    int dcnt = 0;
    int wcnt = 0;
    int *s_lflags;
    struct directory *dp;
    struct directory **dps;
    //int64_t start, elapsed;
    //int64_t total = 0;
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    unsigned int max_bufsize = 0;

    if (!dbip) return 0;
    if (!in_dp) return 0;
    if (flags & DB_SIZE_OBJ) size_flag_cnt++;
    if (flags & DB_SIZE_KEEP) size_flag_cnt++;
    if (flags & DB_SIZE_XPUSH) size_flag_cnt++;

    if (!size_flag_cnt) {
	local_flags |= DB_SIZE_OBJ;
    }
    BU_EXTERNAL_INIT(&ext);

    /* If we're being asked for a) the isolated size of the object or b) any
     * size for a self-contained solid, take a short cut. DB_SIZE_OBJ is always
     * the d_len, and if the object is not a comb or one of the primitives that
     * references other primitives it's KEEP and XPUSH sizes are also just the
     * object size. In that situation, we don't need the more elaborate iterative
     * logic and can just return the local size. */
    if ((local_flags & DB_SIZE_OBJ) || (!(in_dp->d_flags & RT_DIR_COMB) &&
	    !(in_dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) &&
	    !(in_dp->d_minor_type == DB5_MINORTYPE_BRLCAD_REVOLVE) &&
	    !(in_dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP))) {
	long fsize = 0;
	if (local_flags & DB_SIZE_ATTR) {
	    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, in_dp->d_len, "resize ext_buf");
	    fsize += _db5_get_attributes_size(&ext, dbip, in_dp);
	}
	fsize += in_dp->d_len;
	if (ext.ext_buf) bu_free(ext.ext_buf, "free ext_buf");
	return fsize;
    }

    /* It's not one of the simple cases - we've been asked for a definition that
     * requires awareness of some portion of the hierarchy. */

    /* Get a count of all objects we might care about, and find out what the
     * size of our biggest object is. */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		dcnt++;
		if (dp->d_len > max_bufsize) max_bufsize = dp->d_len;
	    }
	}
    }

    /* Get the flags array and the array to hold usable struct directory pointers */
    s_lflags = (int *)bu_calloc(dcnt+1, sizeof(int), "local size flags");
    dps = (struct directory **)bu_calloc(dcnt+1, sizeof(struct directory *), "sortable directory pointer array *");

    /* Make sure the bu_external buffer has as much memory as it will ever need to handle any one object */
    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, max_bufsize, "resize ext_buf");

    /* Associate the local flag with the directory pointer and put ptr in array */
    j = 0;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		dp->u_data = (void *)(&(s_lflags[j]));
		dps[j] = dp;
		j++;
	    }
	}
    }

    /* If we are asked to force a recalculation, redo everything - otherwise,
     * just deactivate everything. */
    for (i = 0; i < dcnt; i++) {
	dp = dps[i];
	if (flags & DB_SIZE_FORCE_RECALC) {
	    dp->s_flags = 0;
	    if (dp->children) bu_free(dp->children, "free child dp ptr array");
	    dp->children = NULL;
	    for (int dpcind = 0; dpcind != 3; dpcind++) dp->sizes[dpcind] = 0;
	    for (int dpcind = 0; dpcind != 3; dpcind++) dp->sizes_wattr[dpcind] = 0;
	} else {
	    dp->s_flags = dp->s_flags & ~(RT_DIR_SIZE_ACTIVE);
	}
    }

    /* Activate our directory object of interest. */
    in_dp->s_flags |= RT_DIR_SIZE_ACTIVE;
    active++;

    /* Now that we have our initial setup, start calculating sizes.  Each pass should
     * resolve at least one size - if no sizes are resolved, we either are done or
     * there is a cyclic loop in the database.  We can't do all combs at once after
     * processing the solids because of the possibility of cyclic loops - those would
     * trigger an infinite loop when walking the child trees. "finalization" is the
     * indication that a comb is safe to process. */
    finalized = 0;
    finalized_prev = -1;
    active_prev = -1;
    wcnt = dcnt;
    while (finalized != finalized_prev || active != active_prev) {
	finalized_prev = finalized;
	active_prev = active;
#if 0
	bu_sort((void *)dps, wcnt, sizeof(struct directory *), _cmp_dp_states, NULL);
	wcnt = 0;
	while (wcnt < dcnt && !(dps[wcnt]->s_flags & RT_DIR_SIZE_FINALIZED)) wcnt++;
	bu_log("wcnt: %d\n", wcnt);
#endif
	for (i = 0; i < wcnt; i++) {
	    dp = dps[i];
	    if ((dp->s_flags & RT_DIR_SIZE_ACTIVE) && !(dp->s_flags & RT_DIR_SIZE_FINALIZED)) {
		if (!(dp->s_flags & RT_DIR_SIZE_ATTR_DONE)) {
		    dp->sizes_wattr[RT_DIR_SIZE_OBJ] = _db5_get_attributes_size(&ext, dbip, dp);
		    dp->s_flags |= RT_DIR_SIZE_ATTR_DONE;
		}

		if (dp->d_flags & RT_DIR_COMB) {
		    struct directory *cdp;
		    int children_finalized = 1;
		    if (!(dp->s_flags & RT_DIR_SIZE_COMB_DONE)) {
			//start = bu_gettime();
			struct rt_db_internal in;
			struct rt_comb_internal *comb;
			if (rt_db_get_internal_reuse(&ext, &in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			comb = (struct rt_comb_internal *)in.idb_ptr;
			if (dp->children) bu_free(dp->children, "free old dp child list");
			dp->children = db_comb_children(dbip, comb, NULL, NULL);
			dp->s_flags |= RT_DIR_SIZE_COMB_DONE;
			rt_db_free_internal(&in);
			//elapsed = bu_gettime() - start;
			//total += elapsed;
		    }

		    if (dp->children) {
			int cind = 0;
			cdp = dp->children[0];
			while (cdp != RT_DIR_NULL) {
			    if (!(cdp->s_flags & RT_DIR_SIZE_FINALIZED)) children_finalized = 0;
			    if (!(cdp->s_flags & RT_DIR_SIZE_ACTIVE)) active++;
			    cdp->s_flags |= RT_DIR_SIZE_ACTIVE;
			    cind++;
			    cdp = dp->children[cind];
			}
			if (children_finalized) {
			    /* Handle the xpushed size, which is just the sum of all of the
			     * xpushed sizes of the children plus this object. */
			    cind = 0;
			    cdp = dp->children[0];
			    while (cdp) {
				//bu_log("XPUSH %s: adding %s (%ld)\n", dp->d_namep, cdp->d_namep, cdp->sizes[RT_DIR_SIZE_XPUSH]);
				dp->sizes[RT_DIR_SIZE_XPUSH] += cdp->sizes[RT_DIR_SIZE_XPUSH];
				dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += cdp->sizes_wattr[RT_DIR_SIZE_XPUSH];
				cind++;
				cdp = dp->children[cind];
			    }
			    dp->sizes[RT_DIR_SIZE_XPUSH] += dp->d_len;
			    dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += (dp->d_len + dp->sizes_wattr[RT_DIR_SIZE_OBJ]);

			    /* Now that we've handled the hierarchy, finalize the obj numbers */
			    dp->sizes[RT_DIR_SIZE_OBJ] = dp->d_len;
			    dp->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;

			    /* Mark the comb as done */
			    dp->s_flags |= RT_DIR_SIZE_FINALIZED;
			    finalized++;
			}
		    } else {
			/* No children - it's just the comb */
			dp->sizes[RT_DIR_SIZE_OBJ] = dp->d_len;
			dp->sizes[RT_DIR_SIZE_KEEP] = dp->d_len;
			dp->sizes[RT_DIR_SIZE_XPUSH] = dp->d_len;
			dp->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;
			dp->sizes_wattr[RT_DIR_SIZE_KEEP] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
			dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
			dp->s_flags |= RT_DIR_SIZE_FINALIZED;
			finalized++;
		    }
		} else {
		    /* This is not a comb - handle the solids that have other data
		     * associated with them and otherwise use dp->d_len */
		    struct directory *edp = NULL;
		    struct rt_db_internal in;

		    if (!(dp->s_flags & RT_DIR_SIZE_ATTR_DONE)) {
			dp->sizes_wattr[RT_DIR_SIZE_OBJ] = _db5_get_attributes_size(&ext, dbip, dp);
			dp->s_flags |= RT_DIR_SIZE_ATTR_DONE;
			//bu_log("%s attr size: %d\n", dp->d_namep, dp->sizes_wattr[RT_DIR_SIZE_OBJ]);
		    }
		    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
			struct rt_extrude_internal *extr;
			if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			extr = (struct rt_extrude_internal *)in.idb_ptr;
			if (extr->sketch_name) {
			    edp = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET);
			}
			rt_db_free_internal(&in);
		    } else if (dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_REVOLVE) {
			struct rt_revolve_internal *revolve;
			if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			revolve = (struct rt_revolve_internal *)in.idb_ptr;
			if (bu_vls_strlen(&revolve->sketch_name) > 0) {
			    edp = db_lookup(dbip, bu_vls_addr(&revolve->sketch_name), LOOKUP_QUIET);
			}
			rt_db_free_internal(&in);
		    } else if (dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_DSP) {
			struct rt_dsp_internal *dsp;
			if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0) continue;
			dsp = (struct rt_dsp_internal *)in.idb_ptr;
			/* TODO - handle other dsp_datasrc cases */
			if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen(&dsp->dsp_name) > 0) {
			    edp = db_lookup(dbip, bu_vls_addr(&dsp->dsp_name), LOOKUP_QUIET);
			}
			rt_db_free_internal(&in);
		    }
		    if (edp) {
			if (!(edp->s_flags & RT_DIR_SIZE_FINALIZED)) {
			    if (!(edp->s_flags & RT_DIR_SIZE_ACTIVE)) active++;
			    edp->s_flags |= RT_DIR_SIZE_ACTIVE;
			    continue;
			} else {
			    dp->sizes[RT_DIR_SIZE_OBJ] += edp->sizes[RT_DIR_SIZE_OBJ];
			    dp->sizes[RT_DIR_SIZE_KEEP] += edp->sizes[RT_DIR_SIZE_OBJ];
			    dp->sizes[RT_DIR_SIZE_XPUSH] += edp->sizes[RT_DIR_SIZE_OBJ];
			    dp->sizes_wattr[RT_DIR_SIZE_OBJ] += edp->sizes_wattr[RT_DIR_SIZE_OBJ];
			    dp->sizes_wattr[RT_DIR_SIZE_KEEP] += edp->sizes_wattr[RT_DIR_SIZE_OBJ];
			    dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += edp->sizes_wattr[RT_DIR_SIZE_XPUSH];
			}
		    }
		    dp->sizes[RT_DIR_SIZE_OBJ] += dp->d_len;
		    dp->sizes[RT_DIR_SIZE_KEEP] += dp->d_len;
		    dp->sizes[RT_DIR_SIZE_XPUSH] += dp->d_len;
		    dp->sizes_wattr[RT_DIR_SIZE_OBJ] += dp->d_len;
		    dp->sizes_wattr[RT_DIR_SIZE_KEEP] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
		    dp->sizes_wattr[RT_DIR_SIZE_XPUSH] += dp->sizes_wattr[RT_DIR_SIZE_OBJ];
		    dp->s_flags |= RT_DIR_SIZE_FINALIZED;
		    finalized++;
		}
	    }
	}
    }

    //fastf_t seconds = total / 1000000.0;
    //bu_log("comb processing: %f\n", seconds);

    /* Now that we have completed our size calculations, see if there are any active but
     * unfinalized directory objects.  These will be an indication of a cyclic loop and
     * require returning a -1 error code (since the size of geometry definitions containing
     * a cyclic loop is meaningless - arguably the evaluated size is infinite */
    int cycl_finalized = 1;
    for (i = 0; i < dcnt; i++) {
	dp = dps[i];
	if (!(dp->d_flags & RT_DIR_HIDDEN)) {
	    if (dp->d_flags & RT_DIR_SIZE_ACTIVE && !(dp->d_flags & RT_DIR_SIZE_FINALIZED)) {
		bu_log("unfinalized object size: %s\n", dp->d_namep);
		cycl_finalized = 0;
	    }
	}
    }

    if (!cycl_finalized) return 0;

    /* IFF we need it, calculate the keep size for dp.  This is harder than the
     * other calculations, because the size is (possibly) smaller than the sum
     * of the children if the child combs share objects. We must find all
     * unique directory objects and sum over that unique set.  Because of the
     * finalize guard, we do not need to be concerned in this logic with
     * possible cyclic paths. Since the keep size for an object doesn't help in
     * calculating other objects' sizes, we evaluate it only for the specific
     * object of interest rather than mass evaluating it in the loop. */
    if (local_flags & DB_SIZE_KEEP && !in_dp->sizes[RT_DIR_SIZE_KEEP]) {
	std::set<struct directory *> uniq;
	std::queue<struct directory *> q;
	struct directory *cdp;
	int cind = 0;
	q.push(in_dp);
	while (!q.empty()) {
	    struct directory *qdp = q.front();
	    q.pop();
	    if (qdp->u_data) {
		*((int *)(qdp->u_data)) = 1;
		uniq.insert(qdp);
	    }
	    cind = 0;
	    cdp = (qdp->children) ? qdp->children[0] : NULL;
	    while (cdp) {
		if (qdp->u_data && !(*((int *)(cdp->u_data)))) q.push(cdp);
		cind++;
		cdp = qdp->children[cind];
	    }
	}
	for (std::set<struct directory *>::iterator di = uniq.begin(); di != uniq.end(); di++) {
	    in_dp->sizes[RT_DIR_SIZE_KEEP] += (*di)->sizes[RT_DIR_SIZE_OBJ];
	    in_dp->sizes_wattr[RT_DIR_SIZE_KEEP] += (*di)->sizes_wattr[RT_DIR_SIZE_OBJ];
	}
    }

    /* We now have what we need to return the requested size */
    long fsize = 0;
    if (local_flags & DB_SIZE_ATTR) {
	if (local_flags & DB_SIZE_KEEP) {
	    fsize = in_dp->sizes_wattr[RT_DIR_SIZE_KEEP];
	} else if (local_flags & DB_SIZE_XPUSH) {
	    fsize = in_dp->sizes_wattr[RT_DIR_SIZE_XPUSH];
	}
    } else {
	if (local_flags & DB_SIZE_KEEP) {
	    fsize = in_dp->sizes[RT_DIR_SIZE_KEEP];
	} else if (local_flags & DB_SIZE_XPUSH) {
	    fsize = in_dp->sizes[RT_DIR_SIZE_XPUSH];
	}
    }

#if 0
    char hlen[6] = { '\0' };
    char hlen2[6] = { '\0' };
    HSIZE(hlen, dp->sizes[RT_DIR_SIZE_OBJ]);
    HSIZE(hlen2, dp->sizes_wattr[RT_DIR_SIZE_OBJ]);
    bu_log("%s/%s: %s\n", hlen, hlen2, dp->d_namep);
#endif
    if (ext.ext_buf) bu_free(ext.ext_buf, "final free of ext_buf");
    return fsize;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

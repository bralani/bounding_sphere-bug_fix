#include "common.h"

#include <set>
#include <map>
#include <string>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L1_OFFSET 2
#define L2_OFFSET 4

#define WRITE_ISLAND_BREPS 1

// TODO - the topological test by itself is not guaranteed to isolate volumes that
// are uniquely positive or uniquely negative contributions to the overall volume.
// It may be that a candidate subbrep has both positive and
// negative contributions to make to the overall volume.  Hopefully (not sure yet)
// this approach will reduce such situations to planar and general surface volumes,
// but we will need to handle them at those levels.  For example,
//
//                                       *
//                                      * *
//         ---------------------       *   *       ---------------------
//         |                    *     *     *     *                    |
//         |                     * * *       * * *                     |
//         |                                                           |
//         |                                                           |
//         -------------------------------------------------------------
//
// The two faces in the middle contribute both positively and negatively to the
// final surface area, and if this area were extruded would also define both
// positive and negative volume.  Probably convex/concave testing is what
// will be needed to resolve this.

#define TOO_MANY_CSG_OBJS(_obj_cnt, _msgs) \
    if (_obj_cnt > CSG_BREP_MAX_OBJS && _msgs) \
	bu_vls_printf(_msgs, "Error - brep converted to more than %d implicits - not currently a good CSG candidate\n", obj_cnt - 1); \
    if (_obj_cnt > CSG_BREP_MAX_OBJS) \
	goto bail


int
island_faces_characterize(struct subbrep_island_data *sb)
{
    int loops_planar = 1;
    const ON_Brep *brep = sb->brep;
    std::set<int> faces;
    std::set<int> fol;
    std::set<int> fil;
    std::set<int> loops;
    std::set<int>::iterator l_it;

    // unpack loop info
    array_to_set(&loops, sb->island_loops, sb->island_loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(brep->m_L[*l_it]);
	const ON_BrepFace *face = loop->Face();
	bool is_outer = (face->OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	faces.insert(face->m_face_index);
	if (is_outer) {
	    fol.insert(face->m_face_index);
	} else {
	    // TODO - check if fil loops are planar.  If not, it's currently grounds for haulting the conversion
	    // of the island.
	    fil.insert(face->m_face_index);
	    if (!face->SurfaceOf()->IsPlanar(NULL, BREP_PLANAR_TOL)) loops_planar = 0;
	}
    }

    // Pack up results
    set_to_array(&(sb->island_faces), &(sb->island_faces_cnt), &faces);
    set_to_array(&(sb->fol), &(sb->fol_cnt), &fol);
    set_to_array(&(sb->fil), &(sb->fil_cnt), &fil);

    return loops_planar;
}

void
get_edge_set_from_loops(struct subbrep_island_data *sb)
{
    const ON_Brep *brep = sb->brep;
    std::set<int> edges;
    std::set<int> loops;
    std::set<int>::iterator l_it;

    // unpack loop info
    array_to_set(&loops, sb->island_loops, sb->island_loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(brep->m_L[*l_it]);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		edges.insert(trim->m_ei);
	    }
	}
    }

    // Pack up results
    set_to_array(&(sb->island_edges), &(sb->island_edges_cnt), &edges);
}

/*
 * This is the point at which we characterize the relationships between islands.  Assuming our determination
 * of positive and negative shapes is trustworthy, all positive islands are unioned into the toplevel comb.
 * The only question is what needs to be subtracted.  Part of this information is available via the shared
 * loop information from the initial island partitioning, but that by itself is not enough.  It is possible
 * to have protruding subtractions that also remove volume from other shapes.
 *
 * To identify what subtractions may do this, we use the loop connectivity for each island to walk the
 * "child" islands of a union island.  These child islands are where additional subtractions may come from.
 * So the top level union, for example, must be checked against all subtractions.  However, a union nested
 * inside a subtraction should *not* have its "parent" negative island subtracted from it.
 *
 * Currently the intrusion test uses axis aligned bounding boxes, but ideally we should use oriented bounding boxes
 * or even tighter tests.
 */
void
find_hierarchy(struct bu_vls *UNUSED(msgs), struct bu_ptbl *islands)
{
    if (!islands) return;

    // Get the top level islands
    std::set<struct subbrep_island_data *> top_islands;
    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	if (id->fil_cnt == 0) top_islands.insert(id);
    }

    // Map outer loop face inclusions to islands for easy lookup
    std::map<int, long*> fol_to_i;
    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	for (int j = 0; j < id->fol_cnt; j++) {
	    fol_to_i.insert(std::make_pair(id->fol[j], (long *)id));
	}
    }

    // We need a quick and easy way to look up parents and children
    typedef std::multimap<struct subbrep_island_data *, struct subbrep_island_data *> IslandMultiMap;
    IslandMultiMap p2c, c2p;
    IslandMultiMap::iterator p2c_it, c2p_it;
    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	for (int j = 0; j < id->fil_cnt; j++) {
	    struct subbrep_island_data *sid = (struct subbrep_island_data *)fol_to_i.find(id->fil[j])->second;
	    p2c.insert(std::make_pair(sid, id));
	    c2p.insert(std::make_pair(id, sid));
	}
    }

    // Process all islands for subtractions
    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	std::queue<struct subbrep_island_data *> subislands;
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	if (id->nucleus->params->bool_op == '-' || id->local_brep_bool_op == '-') {
	    continue;
	}
	std::pair <IslandMultiMap::iterator, IslandMultiMap::iterator> p2c_ret;
	p2c_ret = p2c.equal_range(id);

	// For this node's inner loops, find the corresponding sub-islands and initialize.
	for (p2c_it = p2c_ret.first; p2c_it != p2c_ret.second; ++p2c_it) {
	    std::pair <IslandMultiMap::iterator, IslandMultiMap::iterator> sp2c_ret;
	    IslandMultiMap::iterator sp2c_it;
	    struct subbrep_island_data *subisland = p2c_it->second;

	    // At the top level for a union, any negative shape is joined by a loop and hence
	    // guaranteed to be a subtraction
	    if (subisland->nucleus->params->bool_op == '-' || subisland->local_brep_bool_op == '-') {
		bu_ptbl_ins_unique(id->subtractions, (long *)subisland);
	    }

	    // Initialize the queue with the next level down, whether or not subisland is a union -
	    // there may be more subtractions below.
	    sp2c_ret = p2c.equal_range(subisland);
	    for (sp2c_it = sp2c_ret.first; sp2c_it != sp2c_ret.second; ++sp2c_it) {
		subislands.push(sp2c_it->second);
	    }
	}

	// Work through the layers of islands, adding any subtractions that overlap with the original
	// island's bbox to the subtraction list.
	while(!subislands.empty()) {

	    struct subbrep_island_data *sid= subislands.front();
	    subislands.pop();

	    std::pair <IslandMultiMap::iterator, IslandMultiMap::iterator> sp2c_ret = p2c.equal_range(sid);
	    for (IslandMultiMap::iterator sp2c_it = sp2c_ret.first; sp2c_it != sp2c_ret.second; ++sp2c_it) {
		if (sp2c_it->second != id) {
		    subislands.push(sp2c_it->second);
		}
	    }

	    if (sid->nucleus->params->bool_op == '-' || sid->local_brep_bool_op == '-') {
		ON_BoundingBox isect;
		bool bbi = isect.Intersection(*sid->bbox, *id->bbox);
		// If we have overlap, we have a possible subtraction operation
		// between this island and the parent.  It's not guaranteed that
		// there is an interaction, but to make sure will require ray testing
		// so for speed we accept that there may be extra subtractions in
		// the tree.
		if (bbi) {
		    bu_ptbl_ins_unique(id->subtractions, (long *)sid);
		}
	    }
	}
    }

    // If we have multiple top level islands, loop over them and check everybody's subtractions
    // to make sure all toplevel islands have all the subtractions they need.
    std::set<struct subbrep_island_data *>::iterator top_it_1, top_it_2;
    if (top_islands.size() > 1) {
	for (top_it_1 = top_islands.begin(); top_it_1 != top_islands.end(); top_it_1++) {
	    struct subbrep_island_data *t1 = *top_it_1;
	    for (top_it_2 = top_islands.begin(); top_it_2 != top_islands.end(); top_it_2++) {
		struct subbrep_island_data *t2 = *top_it_2;
		if (t1->island_id == t2->island_id) continue;
		for (unsigned int i = 0; i < BU_PTBL_LEN(t2->subtractions); i++) {
		    struct subbrep_island_data *sub_candidate = (struct subbrep_island_data *)BU_PTBL_GET(t2->subtractions, i);
		    ON_BoundingBox isect;
		    bool bbi = isect.Intersection(*t1->bbox, *sub_candidate->bbox);
		    if (bbi) {
			bu_ptbl_ins_unique(t1->subtractions, (long *)sub_candidate);
		    }
		}
	    }
	}
    }

    // If a union has a parent that is also a union, some of that parent's
    // subtractions may also be needed in this union. The only unions that will
    // know all of its subtractions with certainty after the above procedures are
    // the top level unions.  So, starting with the toplevels, we iterate over
    // all children of the parent.  If any of the parent subtractions
    // overlap the union child and are not on the direct inheritance line from
    // child to parent, subtract them.  Queue chilren up for processing, but for
    // negative node the only purpose is to look for deeper unions.
    std::queue<struct subbrep_island_data *> union_queue;
    std::set<struct subbrep_island_data *>::iterator top_it;
    for (top_it = top_islands.begin(); top_it != top_islands.end(); top_it++) {
	// If we've got union children, queue 'em up
	std::pair <IslandMultiMap::iterator, IslandMultiMap::iterator> p2c_ret;
	p2c_ret = p2c.equal_range(*top_it);
	for (p2c_it = p2c_ret.first; p2c_it != p2c_ret.second; ++p2c_it) {
	    struct subbrep_island_data *subisland = p2c_it->second;
	    // If the child is a union, we need to check
	    if (subisland->nucleus->params->bool_op == 'u' || subisland->local_brep_bool_op == 'u') {
		union_queue.push(subisland);
	    }
	}
    }

    // Now, see if we need to pull anything in from the parent subtractions.
    while(!union_queue.empty()) {
	struct subbrep_island_data *uid= union_queue.front();
	union_queue.pop();
	if (uid->nucleus->params->bool_op == 'u' || uid->local_brep_bool_op == 'u') {

	    // Get the node's immediate parents
	    std::pair <IslandMultiMap::iterator, IslandMultiMap::iterator> sc2p_ret;
	    IslandMultiMap::iterator sc2p_it;
	    sc2p_ret = c2p.equal_range(uid);

	    // Build a set of any subtractions in the full parent linkage of this node, (for all parents)
	    // so we know not to subtract those.  If negative islands are in the direct ancestoral
	    // connectivity graph, they can't be subtractions of that node.
	    std::set<struct subbrep_island_data *> ignore_islands;
	    std::set<struct subbrep_island_data *> union_parents;
	    std::queue<struct subbrep_island_data *> pqueue;
	    for (sc2p_it = sc2p_ret.first; sc2p_it != sc2p_ret.second; ++sc2p_it) {
		struct subbrep_island_data *pid = sc2p_it->second;
		if (pid != uid) pqueue.push(pid);
		if (pid->nucleus->params->bool_op == '-' || pid->local_brep_bool_op == '-') {
		    ignore_islands.insert(pid);
		} else {
		    union_parents.insert(pid);
		}
	    }
	    while(!pqueue.empty()) {
		struct subbrep_island_data *pid= pqueue.front();
		pqueue.pop();
		std::pair <IslandMultiMap::iterator, IslandMultiMap::iterator> pchain_ret;
		IslandMultiMap::iterator pchain_it;
		pchain_ret = c2p.equal_range(pid);
		for (pchain_it = pchain_ret.first; pchain_it != pchain_ret.second; ++pchain_it) {
		    struct subbrep_island_data *gpid = pchain_it->second;
		    if (gpid != pid) pqueue.push(gpid);
		    if (gpid->nucleus->params->bool_op == '-' || gpid->local_brep_bool_op == '-') {
			ignore_islands.insert(gpid);
		    } else {
			union_parents.insert(pid);
		    }
		}
	    }

	    // Now, check the parents' subtractions against this child
	    std::set<struct subbrep_island_data *>::iterator up_it;
	    for (up_it = union_parents.begin(); up_it != union_parents.end(); up_it++) {
		struct subbrep_island_data *sid = *up_it;
		for (unsigned int i = 0; i < BU_PTBL_LEN(sid->subtractions); i++) {
		    struct subbrep_island_data *sub_candidate = (struct subbrep_island_data *)BU_PTBL_GET(sid->subtractions, i);
		    if (ignore_islands.find(sub_candidate) == ignore_islands.end()) {
			ON_BoundingBox isect;
			bool bbi = isect.Intersection(*uid->bbox, *sub_candidate->bbox);
			if (bbi) {
			    bu_ptbl_ins_unique(uid->subtractions, (long *)sub_candidate);
			}
		    }
		}
	    }
	}

	// If we've got children, union or subtraction, queue 'em up
	std::pair <IslandMultiMap::iterator, IslandMultiMap::iterator> p2c_ret;
	p2c_ret = p2c.equal_range(uid);
	for (p2c_it = p2c_ret.first; p2c_it != p2c_ret.second; ++p2c_it) {
	    struct subbrep_island_data *subisland = p2c_it->second;
	    if (subisland != uid) union_queue.push(subisland);
	}
    }
}


int cyl_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Cylinder corig;
    ON_Surface *csorig = forig->SurfaceOf()->Duplicate();
    csorig->IsCylinder(&corig, BREP_CYLINDRICAL_TOL);
    delete csorig;
    ON_Line lorig(corig.circle.Center(), corig.circle.Center() + corig.Axis());

    ON_Cylinder ccand;
    ON_Surface *cscand = fcand->SurfaceOf()->Duplicate();
    cscand->IsCylinder(&ccand, BREP_CYLINDRICAL_TOL);
    delete cscand;
    double d1 = lorig.DistanceTo(ccand.circle.Center());
    double d2 = lorig.DistanceTo(ccand.circle.Center() + ccand.Axis());

    // Make sure the cylinder axes are colinear
    if (corig.Axis().IsParallelTo(ccand.Axis(), VUNITIZE_TOL) == 0) return 0;
    if (fabs(d1) > BREP_CYLINDRICAL_TOL) return 0;
    if (fabs(d2) > BREP_CYLINDRICAL_TOL) return 0;

    // Make sure the radii are the same
    if (!NEAR_ZERO(corig.circle.Radius() - ccand.circle.Radius(), VUNITIZE_TOL)) return 0;

    // TODO - make sure negative/positive status for both cyl faces is the same.

    return 1;
}


int
shoal_filter_loop(int control_loop, int candidate_loop, struct subbrep_island_data *data)
{
    const ON_Brep *brep = data->brep;
    surface_t *face_surface_types = (surface_t *)data->face_surface_types;
    const ON_BrepFace *forig = brep->m_L[control_loop].Face();
    const ON_BrepFace *fcand = brep->m_L[candidate_loop].Face();
    surface_t otype  = face_surface_types[forig->m_face_index];
    surface_t ctype  = face_surface_types[fcand->m_face_index];

    switch (otype) {
	case SURFACE_CYLINDRICAL_SECTION:
	case SURFACE_CYLINDER:
	    if (ctype != SURFACE_CYLINDRICAL_SECTION && ctype != SURFACE_CYLINDER) return 0;
	    if (!cyl_validate_face(forig, fcand)) return 0;
	    break;
	case SURFACE_SPHERICAL_SECTION:
	case SURFACE_SPHERE:
	    if (ctype != SURFACE_SPHERICAL_SECTION && ctype != SURFACE_SPHERE) return 0;
	    break;
	default:
	    if (otype != ctype) return 0;
	    break;
    }

    return 1;
}

int
shoal_build(int **s_loops, int loop_index, struct subbrep_island_data *data)
{
    int s_loops_cnt = 0;
    const ON_Brep *brep = data->brep;
    std::set<int> processed_loops;
    std::set<int> shoal_loops;
    std::queue<int> todo;

    shoal_loops.insert(loop_index);
    todo.push(loop_index);

    while(!todo.empty()) {
	int lc = todo.front();
	const ON_BrepLoop *loop = &(brep->m_L[lc]);
	processed_loops.insert(lc);
	todo.pop();
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge) {
		for (int j = 0; j < edge->m_ti.Count(); j++) {
		    const ON_BrepTrim *t = &(brep->m_T[edge->m_ti[j]]);
		    int li = t->Loop()->m_loop_index;
		    if (processed_loops.find(li) == processed_loops.end()) {
			if (shoal_filter_loop(lc, li, data)) {
			    shoal_loops.insert(li);
			    todo.push(li);
			} else {
			    processed_loops.insert(li);
			}
		    }
		}
	    }
	}
    }

    set_to_array(s_loops, &(s_loops_cnt), &shoal_loops);
    return s_loops_cnt;
}



/* In order to represent complex shapes, it is necessary to identify
 * subsets of subbreps that can be represented as primitives.  This
 * function will identify such subsets, if possible.  If a subbrep
 * can be successfully decomposed into primitive candidates, the
 * type of the subbrep is set to COMB and the children bu_ptbl is
 * populated with subbrep_island_data sets. */
int
subbrep_split(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    int csg_fail = 0;
    std::set<int> loops;
    std::set<int> active;
    std::set<int> fully_planar;
    std::set<int> partly_planar;
    std::set<int>::iterator l_it, e_it;

    array_to_set(&loops, data->island_loops, data->island_loops_cnt);
    array_to_set(&active, data->island_loops, data->island_loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(data->brep->m_L[*l_it]);
	const ON_BrepFace *face = loop->Face();
	surface_t surface_type = ((surface_t *)data->face_surface_types)[face->m_face_index];
	if (surface_type != SURFACE_PLANE) {
	    // non-planar face - check to see whether it's already part of a shoal.  If not,
	    // create a new one - otherwise, skip.
	    if (active.find(loop->m_loop_index) != active.end()) {
		std::set<int> shoal_loops;
		struct subbrep_shoal_data *sh;
		BU_GET(sh, struct subbrep_shoal_data);
		subbrep_shoal_init(sh, data);
		sh->params->csg_id = (*(data->obj_cnt))++;
		sh->shoal_id = (*(data->obj_cnt))++;
		sh->i = data;
		sh->shoal_loops_cnt = shoal_build(&(sh->shoal_loops), loop->m_loop_index, data);
		for (int i = 0; i < sh->shoal_loops_cnt; i++) {
		    active.erase(sh->shoal_loops[i]);
		}
		int local_fail = 0;
		switch (surface_type) {
		    case SURFACE_CYLINDRICAL_SECTION:
		    case SURFACE_CYLINDER:
			if (!cylinder_csg(msgs, sh, BREP_CYLINDRICAL_TOL)) local_fail++;
			break;
		    case SURFACE_CONE:
			local_fail++;
			break;
		    case SURFACE_SPHERICAL_SECTION:
		    case SURFACE_SPHERE:
			local_fail++;
			break;
		    case SURFACE_TORUS:
			local_fail++;
			break;
		    default:
			break;
		}
		if (!local_fail) {
		    if (BU_PTBL_LEN(sh->shoal_children) > 0) {
			sh->shoal_type = COMB;
		    } else {
			sh->shoal_type = sh->params->csg_type;
			sh->shoal_id = sh->params->csg_id;
		    }
		    bu_ptbl_ins(data->island_children, (long *)sh);
		} else {
		    csg_fail++;
		}
	    }
	}
    }

    if (csg_fail > 0) {
	/* We found something we can't handle, so there will be no CSG
	 * conversion of this subbrep. */
	//bu_log("non-planar splitting subroutines failed: %s\n", bu_vls_addr(data->key));
	for (unsigned int i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	    struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	    subbrep_shoal_free(d);
	}
	bu_ptbl_trunc(data->island_children, 0);
	return 0;
    }
    // We had a successful conversion - now generate a nucleus.  Need to
    // characterize the nucleus as positive or negative volume.  Can also
    // be degenerate if all planar faces are coplanar or if there are no
    // planar faces at all (perfect cylinder, for example) so will need
    // rules for those situations...
    if (!island_nucleus(msgs, data) || !data->nucleus) {
	bu_log("failed to find island nucleus: %s\n", bu_vls_addr(data->key));
	for (unsigned int i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	    struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	    subbrep_shoal_free(d);
	}
	bu_ptbl_trunc(data->island_children, 0);

	return 0;
    }

    // If we've got a single nucleus shape and no children, set the island
    // type to be that of the nucleus type.  Otherwise, it's a comb.
    if (BU_PTBL_LEN(data->island_children) == 0) {
	data->island_type = data->nucleus->shoal_type;
    } else {
	data->island_type = COMB;
    }

    // Note is is possible to have degenerate *edges*, not just vertices -
    // if a shoal shares part of an outer loop (possible when connected
    // only by a vertex - see example) then the edges in the parent loop
    // that are part of that shoal are degenerate for the purposes of the
    // "parent" shape.  May have to limit our inputs to
    // non-self-intersecting loops for now...
    return 1;
}

struct bu_ptbl *
find_subbreps(struct bu_vls *msgs, const ON_Brep *brep)
{
    /* Number of successful conversion operations */
    int successes = 0;
    /* Overall object counter */
    int obj_cnt = 0;
    /* Container to hold island data structures */
    struct bu_ptbl *subbreps;
    std::set<struct subbrep_island_data *> islands;
    std::set<struct subbrep_island_data *>::iterator is_it;

    BU_GET(subbreps, struct bu_ptbl);
    bu_ptbl_init(subbreps, 8, "subbrep table");

    // Characterize all face surface types once
    std::map<int, surface_t> fstypes;
    surface_t *face_surface_types = (surface_t *)bu_calloc(brep->m_F.Count(), sizeof(surface_t), "surface type array");
    for (int i = 0; i < brep->m_F.Count(); i++) {
	face_surface_types[i] = GetSurfaceType(brep->m_F[i].SurfaceOf());
    }

    // Use the loops to identify subbreps
    std::set<int> brep_loops;
    /* Build a set of loops.  All loops will be assigned to a subbrep */
    for (int i = 0; i < brep->m_L.Count(); i++) {
	brep_loops.insert(i);
    }

    while (!brep_loops.empty()) {
	std::set<int> loops;
	std::queue<int> todo;

	/* For each iteration, we have a new island */
	struct subbrep_island_data *sb = NULL;
	BU_GET(sb, struct subbrep_island_data);
	subbrep_island_init(sb, brep);

	// Seed the queue with the first loop in the set
	std::set<int>::iterator top = brep_loops.begin();
	todo.push(*top);

	// Process the queue
	while(!todo.empty()) {
	    const ON_BrepLoop *loop = &(brep->m_L[todo.front()]);
	    loops.insert(todo.front());
	    brep_loops.erase(todo.front());
	    todo.pop();
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
		const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
		if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		    for (int j = 0; j < edge->TrimCount(); j++) {
			int li = edge->Trim(j)->Loop()->m_loop_index;
			if (loops.find(li) == loops.end()) {
			    todo.push(li);
			    loops.insert(li);
			    brep_loops.erase(li);
			}
		    }
		}
	    }
	}

	// If our object count is growing beyond reason, bail
	TOO_MANY_CSG_OBJS(obj_cnt, msgs);

	// Pass the counter pointer through in case sub-objects need
	// to generate IDs as well.
	sb->obj_cnt = &obj_cnt;

	// Pass along shared data
	sb->brep = brep;
	sb->face_surface_types = (void *)face_surface_types;

	// Assign loop set to island
	set_to_array(&(sb->island_loops), &(sb->island_loops_cnt), &loops);

	// Assemble the set of faces, characterize face-from-outer-loop (fol)
	// and face-from-inner-loop (fil) faces in the island.  Test planarity
	// of fil loops - needed for processing.
	int planar_fils = island_faces_characterize(sb);


	// Assemble the set of edges
	get_edge_set_from_loops(sb);

	// Get key based on loop indicies
	set_key(sb->key, sb->island_loops_cnt, sb->island_loops);

	if (!planar_fils) {
	    if (msgs) bu_vls_printf(msgs, "Note - non-planer island mating loop in %s, haulting conversion\n", bu_vls_addr(sb->key));
	    goto bail;
	}

	// The boolean test of the subbrep serves several functions:
	//
	// 1.  Determines what to do boolean wise with B-Rep islands
	// 2.  Identifies self intersecting subbrep shapes, which trigger a conversion hault.
	// 3.  Serve as a sanity check for the CSG routines, which do their own boolean resolution
	//     tests as well.
	int bool_flag = subbrep_brep_boolean(sb);

	// Self intersection is fatal;
	if (bool_flag == -2) {
	    if (msgs) bu_vls_printf(msgs, "Self intersecting island %s, haulting conversion\n", bu_vls_addr(sb->key));
	    goto bail;
	}

	// Check to see if we have a general surface that precludes conversion
	surface_t hof = subbrep_highest_order_face(sb);
	if (hof >= SURFACE_GENERAL) {
	    if (msgs) bu_vls_printf(msgs, "Note - general surface present in island %s - representing as B-Rep\n", bu_vls_addr(sb->key));
	    sb->island_type = BREP;
	    (void)subbrep_make_brep(msgs, sb);
	    sb->local_brep_bool_op = (bool_flag == -1) ? '-' : 'u';
	    if (bool_flag == -1) sb->local_brep->Flip();
	    bu_ptbl_ins(subbreps, (long *)sb);
	    continue;
	}

	int split = subbrep_split(msgs, sb);
	TOO_MANY_CSG_OBJS(obj_cnt, msgs);
	if (!split) {
	    if (msgs) bu_vls_printf(msgs, "Note - split of %s unsuccessful, making brep\n", bu_vls_addr(sb->key));
	    sb->island_type = BREP;
	    (void)subbrep_make_brep(msgs, sb);
	    sb->local_brep_bool_op = (bool_flag == -1) ? '-' : 'u';
	    if (bool_flag == -1) sb->local_brep->Flip();
	} else {
	    successes++;
	    // Make sure the CSG routines and the B-Rep boolean check reached the same conclusion.
	    if ((bool_flag == -1 && sb->nucleus->params->bool_op == 'u') || (bool_flag == 1 && sb->nucleus->params->bool_op == '-'))
		bu_log("Warning - csg and brep boolean determinations do not match: %s\n", bu_vls_addr(sb->key));
#if WRITE_ISLAND_BREPS
	    (void)subbrep_make_brep(msgs, sb);
#endif
	}

	bu_ptbl_ins(subbreps, (long *)sb);
    }

    /* If we didn't do anything to simplify the shape, we're stuck with the original */
    if (!successes) {
	if (msgs) bu_vls_printf(msgs, "%*sNote - no successful simplifications\n", L1_OFFSET, " ");
	goto bail;
    }

    /* Build the bounding boxes for all islands once */
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
	struct subbrep_island_data *island = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	subbrep_bbox(island);
    }

    // Assign island IDs, populate set
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
	struct subbrep_island_data *si = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	si->island_id = obj_cnt++;
	islands.insert(si);
    }

    //bu_log("Characterize subtractions...\n");
    find_hierarchy(msgs, subbreps);

    return subbreps;

bail:
    // Free memory
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_island_data *obj = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	subbrep_island_free(obj);
	BU_PUT(obj, struct subbrep_island_data);
    }
    bu_ptbl_free(subbreps);
    BU_PUT(subbreps, struct bu_ptbl);
    return NULL;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

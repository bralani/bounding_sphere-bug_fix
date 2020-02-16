/*                   P O L Y G O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file polygon.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "brep/pullback.h"
#include "./cdt.h"

long
polygon_t::add_point(mesh_point_t *meshp)
{
    if (!meshp) {
	return -1;
    }

    poly_point_t pp;

    if (loop_id == -1) {
	p_plane.ClosestPointTo(meshp->p, &pp.u, &pp.v);
    } else {
	ON_2dPoint p2d;
	ON_3dPoint p3d;
	if (!m->closest_surf_pt(NULL, p3d, p2d, &(meshp->p), -1)) {
	    std::cerr << "closest point evaluation failed for polygon 2D\n";
	    return -1;
	}
	pp.u = p2d.x;
	pp.v = p2d.y;
    }

    // The current index of p_pnts (before we add the new point)
    // is the vector index that will be assigned to this point.
    pp.vect_ind = p_pnts_vect.size();

    // Teach the polygon point where it came from
    pp.mp = meshp;

    // Add the new polygon point
    p_pnts_vect.push_back(pp);

    // Update the original-to-polygon point map
    o2p[pp.mp->vect_ind] = pp.vect_ind;

    return pp.vect_ind;
}

long
polygon_t::add_point(mesh_point_t *meshp, ON_2dPoint &p2d)
{
    if (!meshp) {
	return -1;
    }
    poly_point_t pp;
    pp.u = p2d.x;
    pp.v = p2d.y;

    // The current index of p_pnts (before we add the new point)
    // is the vector index that will be assigned to this point.
    pp.vect_ind = p_pnts_vect.size();

    // Teach the polygon point where it came from
    pp.mp = meshp;

    // Add the new polygon point
    p_pnts_vect.push_back(pp);

    // Update the original-to-polygon point map.  NOTE: for singular
    // points this mapping is not unique, so the result will be a
    // last-insertion-wins in the o2p map.  Assemblies that need
    // to handle singular points will need to manage the process
    // using information beyond that stored in o2p
    o2p[pp.mp->vect_ind] = pp.vect_ind;

    return pp.vect_ind;
}

poly_edge_t *
polygon_t::add_ordered_edge(long p1, long p2)
{
    if (p1 < 0 || p2 < 0) return NULL;
    if (p1 >= (long)p_pnts_vect.size() || p2 >= (long)p_pnts_vect.size()) return NULL;

    // Look up the polygon points associated with the indices
    poly_point_t &pp1 = p_pnts_vect[p1];
    poly_point_t &pp2 = p_pnts_vect[p2];

    // If either point already has two associated edges, we have a problem.
    if (pp1.pedges.size() > 1 || pp2.pedges.size() > 1) {
	return NULL;
    }

    // Look for prev/next settings
    long prev = -1;
    long next = -1;
    std::set<size_t>::iterator p_it;
    for (p_it = pp1.pedges.begin(); p_it != pp1.pedges.end(); p_it++) {
	poly_edge_t &ce = p_pedges_vect[*p_it];
	if (ce.v[1] == p1) {
	    prev = ce.vect_ind;
	}
	if (ce.v[0] == p2) {
	    next = ce.vect_ind;
	}
    }
    for (p_it = pp2.pedges.begin(); p_it != pp2.pedges.end(); p_it++) {
	poly_edge_t &ce = p_pedges_vect[*p_it];
	if (ce.v[1] == p1) {
	    prev = ce.vect_ind;
	}
	if (ce.v[0] == p2) {
	    next = ce.vect_ind;
	}
    }

    poly_edge_t pe;
    pe.v[0] = p1;
    pe.v[1] = p2;
    pe.polygon = this;
    pe.vect_ind = p_pedges_vect.size();

    if (prev != -1) {
	p_pedges_vect[prev].next = pe.vect_ind;
    }
    pe.prev = prev;

    if (next != -1) {
	p_pedges_vect[next].prev = pe.vect_ind;
    }
    pe.next = next;

    pp1.pedges.insert(pe.vect_ind);
    pp2.pedges.insert(pe.vect_ind);

    p_pedges_vect.push_back(pe);

    // IFF the edge is not degenerate (which can happen if we're initializing
    // closed loops) add it to the RTree
    if (p1 != p2) {
	ON_Line line(ON_2dPoint(pp1.u, pp1.v), ON_2dPoint(pp2.u, pp2.v));
	pe.bb = line.BoundingBox();
	pe.bb.m_max.x = pe.bb.m_max.x + ON_ZERO_TOLERANCE;
	pe.bb.m_max.y = pe.bb.m_max.y + ON_ZERO_TOLERANCE;
	pe.bb.m_min.x = pe.bb.m_min.x - ON_ZERO_TOLERANCE;
	pe.bb.m_min.y = pe.bb.m_min.y - ON_ZERO_TOLERANCE;
	double bp1[2];
	bp1[0] = pe.bb.Min().x;
	bp1[1] = pe.bb.Min().y;
	double bp2[2];
	bp2[0] = pe.bb.Max().x;
	bp2[1] = pe.bb.Max().y;
	p_edges_tree.Insert(bp1, bp2, pe.vect_ind);
    }

    return &p_pedges_vect[pe.vect_ind];
}

void
polygon_t::remove_ordered_edge(poly_edge_t &pe)
{
    std::set<size_t>::iterator p_it;
    for (int i = 0; i < 2; i++) {
	std::set<size_t> &pedges = p_pnts_vect[pe.v[i]].pedges;
	for (p_it = pedges.begin(); p_it != pedges.end(); p_it++) {
	    poly_edge_t &pev = p_pedges_vect[*p_it];
	    if (pev.prev == pe.vect_ind) {
		pev.prev = -1;
	    }
	    if (pev.next == pe.vect_ind) {
		pev.next = -1;
	    }
	    for (int j = 0; j < 2; j++) {
		std::set<size_t> &vedges = p_pnts_vect[pev.v[j]].pedges;
		vedges.erase(pe.vect_ind);
	    } 
	}
    }

    // IFF the edge is not degenerate remove it from the RTree.  (If it is
    // degenerate it should never have been inserted.)
    if (pe.v[0] != pe.v[1]) {
	poly_point_t &pp1 = p_pnts_vect[pe.v[0]];
	poly_point_t &pp2 = p_pnts_vect[pe.v[1]];
	ON_Line line(ON_2dPoint(pp1.u, pp1.v), ON_2dPoint(pp2.u, pp2.v));
	pe.bb = line.BoundingBox();
	pe.bb.m_max.x = pe.bb.m_max.x + 2*ON_ZERO_TOLERANCE;
	pe.bb.m_max.y = pe.bb.m_max.y + 2*ON_ZERO_TOLERANCE;
	pe.bb.m_min.x = pe.bb.m_min.x - 2*ON_ZERO_TOLERANCE;
	pe.bb.m_min.y = pe.bb.m_min.y - 2*ON_ZERO_TOLERANCE;
	double bp1[2];
	bp1[0] = pe.bb.Min().x;
	bp1[1] = pe.bb.Min().y;
	double bp2[2];
	bp2[0] = pe.bb.Max().x;
	bp2[1] = pe.bb.Max().y;
	p_edges_tree.Remove(bp1, bp2, pe.vect_ind);
    }

    // TODO - return pe to queue
}



/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


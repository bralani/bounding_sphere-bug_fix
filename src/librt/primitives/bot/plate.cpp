/*                      P L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2024 United States Government as represented by
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
/** @file plate.cpp
 *
 * Given a plate mode bot, represent its volumetric thickness with
 * individual CSG primitives, then tessellate and combine them to
 * make an evaluated representation of the plate mode volume.
 */

#include "common.h"

#include <set>

// flag to enable writing debugging output in case of boolean failure
#define CHECK_INTERMEDIATES 1

#include "manifold/manifold.h"
#ifdef CHECK_INTERMEDIATES
#  include "manifold/meshIO.h"
#endif

#include "vmath.h"
#include "bu/time.h"
#include "bn/tol.h"
#include "bg/defines.h"
#include "rt/defines.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/functab.h"
#include "rt/global.h"
#include "rt/nmg_conv.h"
#include "rt/primitives/bot.h"

static bool
bot_face_normal(vect_t *n, struct rt_bot_internal *bot, int i)
{
    vect_t a,b;

    /* sanity */
    if (!n || !bot || i < 0 || (size_t)i > bot->num_faces ||
	    bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices) {
	return false;
    }

    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
    VCROSS(*n, a, b);
    VUNITIZE(*n);
    if (bot->orientation == RT_BOT_CW) {
	VREVERSE(*n, *n);
    }

    return true;
}

#define MAX_CYL_STEPS 1000
static struct rt_bot_internal *
edge_cyl(point_t p1, point_t p2, fastf_t r)
{
    int nsegs = 8;
    vect_t h;
    VSUB2(h, p2, p1);

    // Find axis to use for point generation on circles
    vect_t cross1, cross2, xaxis, yaxis;
    bn_vec_ortho(cross1, h);
    VUNITIZE(cross1);
    VCROSS(cross2, cross1, h);
    VSCALE(xaxis, cross1, r);
    VSCALE(yaxis, cross2, r);

    // Figure out the step we take for each ring in the cylinder
    double seg_len = M_PI * r * r / (double)nsegs;
    fastf_t e_len = MAGNITUDE(h);
    int steps = 1;
    fastf_t h_len = (e_len < 2*seg_len) ? e_len : -1.0;
    if (h_len < 0) {
	steps = (int)(e_len / seg_len);
	// Ideally we want the height of the triangles up the cylinder to be on
	// the same order as the segment size, but that's likely to make for a
	// huge number of triangles if we have a long edge.
	if (steps > MAX_CYL_STEPS)
	    steps = MAX_CYL_STEPS;
	h_len = e_len / (double)steps;
    }
    vect_t h_step;
    VMOVE(h_step, h);
    VUNITIZE(h_step);
    VSCALE(h_step, h_step, h_len);

    // Generated the vertices
    point_t *verts = (point_t *)bu_calloc(steps * nsegs + 2, sizeof(point_t), "verts");
    for (int i = 0; i < steps; i++) {
	for (int j = 0; j < nsegs; j++) {
	    double alpha = M_2PI * (double)(2*j+1)/(double)(2*nsegs);
	    double sin_alpha = sin(alpha);
	    double cos_alpha = cos(alpha);
	    /* vertex geometry */
	    VJOIN3(verts[i*nsegs+j], p1, (double)i, h_step, cos_alpha, xaxis, sin_alpha, yaxis);
	}
    }

    // The two center points of the end caps are the last two points
    VMOVE(verts[steps * nsegs], p1);
    VMOVE(verts[steps * nsegs + 1], p2);

    // Next, we define the faces.  The two end caps each have one triangle for each segment.
    // Each step defines 2*nseg triangles.
    int *faces = (int *)bu_calloc(nsegs + nsegs + (steps-1) * 2*nsegs, 3*sizeof(int), "triangles");

    // For the steps, we process in quads - each segment gets two triangles
    for (int i = 0; i < steps - 1; i++) {
	for (int j = 0; j < nsegs; j++) {
	    int pnts[4];
	    pnts[0] = nsegs * i + j;
	    pnts[1] = (j < nsegs - 1) ? nsegs * i + j + 1 : nsegs * i;
	    pnts[2] = nsegs * (i + 1) + j;
	    pnts[3] = (j < nsegs - 1) ? nsegs * (i + 1) + j + 1 : nsegs * (i + 1);
	    int offset = 3 * (i * nsegs * 2 + j * 2);
	    faces[offset + 0] = pnts[0];
	    faces[offset + 1] = pnts[2];
	    faces[offset + 2] = pnts[1];
	    faces[offset + 3] = pnts[2];
	    faces[offset + 4] = pnts[3];
	    faces[offset + 5] = pnts[1];
	}
    }

    // Define the end caps.  The first set of triangles uses the base
    // point (stored at verts[steps*nsegs] and the points of the first
    // circle (stored at the beginning of verts)
    int offset = 3 * ((steps-1) * nsegs * 2);
    for (int j = 0; j < nsegs; j++){
	int pnts[3];
	pnts[0] = steps * nsegs;
	pnts[1] = j;
	pnts[2] = (j < nsegs - 1) ? j + 1 : 0;
	faces[offset + 3*j + 0] = pnts[0];
	faces[offset + 3*j + 1] = pnts[1];
	faces[offset + 3*j + 2] = pnts[2];
    }
    // The second set of cap triangles uses the second edge point
    // point (stored at verts[steps*nsegs+1] and the points of the last
    // circle (stored at the end of verts = (steps-1) * nsegs)
    offset = 3 * (((steps-1) * nsegs * 2) + nsegs);
    for (int j = 0; j < nsegs; j++){
	int pnts[3];
	pnts[0] = steps * nsegs + 1;
	pnts[1] = (steps - 1) * nsegs + j;
	pnts[2] = (j < nsegs - 1) ? (steps - 1) * nsegs + j + 1 : (steps - 1) * nsegs;
	faces[offset + 3*j + 0] = pnts[0];
	faces[offset + 3*j + 1] = pnts[2];
	faces[offset + 3*j + 2] = pnts[1];
    }

    for (int i = 0; i < (steps-1) * nsegs + 2; i++) {
	bu_log("vert[%d]: %g %g %g\n", i, V3ARGS(verts[i]));
    }

    for (int i = 0; i < nsegs + nsegs + (steps-1) * 2*nsegs; i++) {
	bu_log("face[%d]: %d %d %d\n", i, faces[i*3], faces[i*3+1], faces[i*3+2]);
    }


    return NULL;
}

int
rt_bot_plate_to_vol(struct rt_bot_internal **obot, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    if (!obot || !bot || !ttol || !tol)
	return 1;

    if (bot->mode != RT_BOT_PLATE)
	return 1;

    // Check for at least 1 non-zero thickness, or there's no volume to define
    bool have_solid = false;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (bot->thickness[i] > VUNITIZE_TOL) {
	    have_solid = true;
	    break;
	}
    }

    // If there's no volume, there's nothing to produce
    if (!have_solid) {
	*obot = NULL;
	return 0;
    }

    // OK, we have volume.  Now we need to build up the manifold definition
    // using unioned CSG elements
    manifold::Manifold c;

    // Collect the active vertices and edges
    std::set<int> verts;
    std::map<int, double> verts_thickness;
    std::map<int, int> verts_fcnt;
    std::map<std::pair<int, int>, double> edges_thickness;
    std::map<std::pair<int, int>, int> edges_fcnt;
    std::set<std::pair<int, int>> edges;
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t eind;
	double fthickness = (BU_BITTEST(bot->face_mode, i)) ? bot->thickness[i] : 0.5*bot->thickness[i];
	for (int j = 0; j < 3; j++) {
	    verts.insert(bot->faces[i*3+j]);
	    verts_thickness[bot->faces[i*3+j]] += fthickness;
	    verts_fcnt[bot->faces[i*3+j]]++;
	    eind[j] = bot->faces[i*3+j];
	}
	int e11, e12, e21, e22, e31, e32;
	e11 = (eind[0] < eind[1]) ? eind[0] : eind[1];
	e12 = (eind[1] < eind[0]) ? eind[0] : eind[1];
	e21 = (eind[1] < eind[2]) ? eind[1] : eind[2];
	e22 = (eind[2] < eind[1]) ? eind[1] : eind[2];
	e31 = (eind[2] < eind[0]) ? eind[2] : eind[0];
	e32 = (eind[0] < eind[2]) ? eind[2] : eind[0];
	edges.insert(std::make_pair(e11, e12));
	edges.insert(std::make_pair(e21, e22));
	edges.insert(std::make_pair(e31, e32));
	edges_thickness[std::make_pair(e11, e12)] += fthickness;
	edges_thickness[std::make_pair(e21, e22)] += fthickness;
	edges_thickness[std::make_pair(e31, e32)] += fthickness;
	edges_fcnt[std::make_pair(e11, e12)]++;
	edges_fcnt[std::make_pair(e21, e22)]++;
	edges_fcnt[std::make_pair(e31, e32)]++;
    }

    std::set<int>::iterator v_it;
    bu_log("Processing %zd vertices... \n" , verts.size());
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	point_t v;
	double r = ((double)verts_thickness[*v_it]/(double)(verts_fcnt[*v_it]));
	// Make a sph at the vertex point with a radius based on the thickness
	VMOVE(v, &bot->vertices[3**v_it]);

	struct rt_ell_internal ell;
	ell.magic = RT_ELL_INTERNAL_MAGIC;
	VMOVE(ell.v, v);
	VSET(ell.a, r, 0, 0);
	VSET(ell.b, 0, r, 0);
	VSET(ell.c, 0, 0, r);

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_ELL;
	intern.idb_ptr = &ell;
	intern.idb_meth = &OBJ[ID_ELL];

	struct nmgregion *r1 = NULL;
	struct model *m = nmg_mm();
	if (intern.idb_meth->ft_tessellate(&r1, m, &intern, ttol, tol))
	    continue;

	struct rt_bot_internal *sbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tol);
	if (!sbot)
	    continue;

	nmg_km(m);

	manifold::Mesh sph_m;
	for (size_t j = 0; j < sbot->num_vertices ; j++)
	    sph_m.vertPos.push_back(glm::vec3(sbot->vertices[3*j], sbot->vertices[3*j+1], sbot->vertices[3*j+2]));
	for (size_t j = 0; j < sbot->num_faces; j++)
	    sph_m.triVerts.push_back(glm::vec3(sbot->faces[3*j], sbot->faces[3*j+1], sbot->faces[3*j+2]));

	if (sbot->vertices)
	    bu_free(sbot->vertices, "verts");
	if (sbot->faces)
	    bu_free(sbot->faces, "faces");
	BU_FREE(sbot, struct rt_bot_internal);

	manifold::Manifold left = c;
	manifold::Manifold right(sph_m);

	try {
	    c = left.Boolean(right, manifold::OpType::Add);
#if defined(CHECK_INTERMEDIATES)
	    c.GetMesh();
#endif
	} catch (const std::exception &e) {
	    bu_log("Vertices - manifold boolean op failure\n");
	    std::cerr << e.what() << "\n";
#if defined(CHECK_INTERMEDIATES)
	    manifold::ExportMesh(std::string("left.glb"), left.GetMesh(), {});
	    manifold::ExportMesh(std::string("right.glb"), right.GetMesh(), {});
	    bu_exit(EXIT_FAILURE, "halting on boolean failure");
#endif
	    return -1;
	}
    }
    bu_log("Processing %zd vertices... done.\n" , verts.size());

    std::set<std::pair<int, int>>::iterator e_it;
    size_t ecnt = 0;
    int64_t start = bu_gettime();
    int64_t elapsed = 0;
    fastf_t seconds = 0.0;
    bu_log("Processing %zd edges... \n" , edges.size());
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	double r = ((double)edges_thickness[*e_it]/(double)(edges_fcnt[*e_it]));
	// Make an rcc along the edge a radius based on the thickness
	point_t base, v;
	vect_t h;
	VMOVE(base, &bot->vertices[3*(*e_it).first]);
	VMOVE(v, &bot->vertices[3*(*e_it).second]);

	edge_cyl(base, v, r);

	VSUB2(h, v, base);

	vect_t cross1, cross2;
	vect_t a, b;
	if (MAGSQ(h) <= SQRT_SMALL_FASTF)
	    continue;;
	/* Create two mutually perpendicular vectors, perpendicular to H */
	bn_vec_ortho(cross1, h);
	VCROSS(cross2, cross1, h);
	VUNITIZE(cross2);
	VSCALE(a, cross1, r);
	VSCALE(b, cross2, r);
	struct rt_tgc_internal tgc;
	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VMOVE(tgc.v, base);
	VMOVE(tgc.h, h);
	VMOVE(tgc.a, a);
	VMOVE(tgc.b, b);
	VMOVE(tgc.c, a);
	VMOVE(tgc.d, b);

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_TGC;
	intern.idb_ptr = &tgc;
	intern.idb_meth = &OBJ[ID_TGC];

	struct nmgregion *r1 = NULL;
	struct model *m = nmg_mm();
	if (intern.idb_meth->ft_tessellate(&r1, m, &intern, ttol, tol))
	    continue;

	// TODO - cylinders produced by this method sometimes are resulting in
	// very nasty bots.  Visual inspection suggests the NMG form is more
	// properly structured, so unclear what is happening when producing the BoT
	struct rt_bot_internal *cbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tol);
	if (!cbot)
	    continue;

	nmg_km(m);

	manifold::Mesh rcc_m;
	for (size_t j = 0; j < cbot->num_vertices; j++)
	    rcc_m.vertPos.push_back(glm::vec3(cbot->vertices[3*j], cbot->vertices[3*j+1], cbot->vertices[3*j+2]));
	for (size_t j = 0; j < cbot->num_faces; j++)
	    rcc_m.triVerts.push_back(glm::vec3(cbot->faces[3*j], cbot->faces[3*j+1], cbot->faces[3*j+2]));

	if (cbot->vertices)
	    bu_free(cbot->vertices, "verts");
	if (cbot->faces)
	    bu_free(cbot->faces, "faces");
	BU_FREE(cbot, struct rt_bot_internal);

	manifold::Manifold left = c;
	manifold::Manifold right(rcc_m);
	try {
	    c = left.Boolean(right, manifold::OpType::Add);
#if defined(CHECK_INTERMEDIATES)
	    c.GetMesh();
#endif
	} catch (const std::exception &e) {
	    bu_log("Edges - manifold boolean op failure\n");
	    bu_log("v: %g %g %g\n", V3ARGS(tgc.v));
	    bu_log("h: %g %g %g\n", V3ARGS(tgc.h));
	    bu_log("r: %g\n", r);
	    std::cerr << e.what() << "\n";
#if defined(CHECK_INTERMEDIATES)
	    manifold::ExportMesh(std::string("left.glb"), left.GetMesh(), {});
	    manifold::ExportMesh(std::string("right.glb"), right.GetMesh(), {});
	    bu_exit(EXIT_FAILURE, "halting on boolean failure");
#endif
	    return -1;
	}

	ecnt++;
	elapsed = bu_gettime() - start;
	seconds = elapsed / 1000000.0;

	if (seconds > 5) {
	    start = bu_gettime();
	    bu_log("Processed %zd of %zd edges\n", ecnt, edges.size());
	}
    }
    bu_log("Processing %zd edges... done.\n" , edges.size());

    // Now, handle the primary arb faces
    bu_log("Processing %zd faces...\n" , bot->num_faces);
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t pnts[6];
	point_t pf[3];
	vect_t pv1[3], pv2[3];
	vect_t n = VINIT_ZERO;
	bot_face_normal(&n, bot, i);

	for (int j = 0; j < 3; j++) {
	    VMOVE(pf[j], &bot->vertices[bot->faces[i*3+j]*3]);
	    if (BU_BITTEST(bot->face_mode, i)) {
		VSCALE(pv1[j], n, bot->thickness[i]);
		VSCALE(pv2[j], n, -1*bot->thickness[i]);
	    } else {
		VSCALE(pv1[j], n, 0.5*bot->thickness[i]);
		VSCALE(pv2[j], n, -0.5*bot->thickness[i]);
	    }
	}

	for (int j = 0; j < 3; j++) {
	    point_t npnt1;
	    point_t npnt2;
	    VADD2(npnt1, pf[j], pv1[j]);
	    VADD2(npnt2, pf[j], pv2[j]);
	    VMOVE(pnts[j], npnt1);
	    VMOVE(pnts[j+3], npnt2);
	}

	// For arb6 creation, we need a specific point order
	fastf_t pts[3*6];
	/* 1 */ pts[0] = pnts[4][X]; pts[1] = pnts[4][Y]; pts[2] = pnts[4][Z];
	/* 2 */ pts[3] = pnts[3][X]; pts[4] = pnts[3][Y]; pts[5] = pnts[3][Z];
	/* 3 */ pts[6] = pnts[0][X]; pts[7] = pnts[0][Y]; pts[8] = pnts[0][Z];
	/* 4 */ pts[9] = pnts[1][X]; pts[10] = pnts[1][Y]; pts[11] = pnts[1][Z];
	/* 5 */ pts[12] = pnts[5][X]; pts[13] = pnts[5][Y]; pts[14] = pnts[5][Z];
	/* 6 */ pts[15] = pnts[2][X]; pts[16] = pnts[2][Y]; pts[17] = pnts[2][Z];

	point_t pt8[8];
	VMOVE(pt8[0], &pts[0*3]);
	VMOVE(pt8[1], &pts[1*3]);
	VMOVE(pt8[2], &pts[2*3]);
	VMOVE(pt8[3], &pts[3*3]);
	VMOVE(pt8[4], &pts[4*3]);
	VMOVE(pt8[5], &pts[4*3]);
	VMOVE(pt8[6], &pts[5*3]);
	VMOVE(pt8[7], &pts[5*3]);

	struct rt_arb_internal arb;
	arb.magic = RT_ARB_INTERNAL_MAGIC;
	for (int j = 0; j < 8; j++)
	    VMOVE(arb.pt[j], pt8[j]);

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_ARB8;
	intern.idb_ptr = &arb;
	intern.idb_meth = &OBJ[ID_ARB8];

	struct nmgregion *r1 = NULL;
	struct model *m = nmg_mm();
	if (intern.idb_meth->ft_tessellate(&r1, m, &intern, ttol, tol))
	    continue;

	struct rt_bot_internal *abot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tol);
	if (!abot)
	    continue;

	nmg_km(m);

	manifold::Mesh arb_m;
	for (size_t j = 0; j < abot->num_vertices; j++)
	    arb_m.vertPos.push_back(glm::vec3(abot->vertices[3*j], abot->vertices[3*j+1], abot->vertices[3*j+2]));
	for (size_t j = 0; j < abot->num_faces; j++)
	    arb_m.triVerts.push_back(glm::vec3(abot->faces[3*j], abot->faces[3*j+1], abot->faces[3*j+2]));

	if (abot->vertices)
	    bu_free(abot->vertices, "verts");
	if (abot->faces)
	    bu_free(abot->faces, "faces");
	BU_FREE(abot, struct rt_bot_internal);

	manifold::Manifold left = c;
	manifold::Manifold right(arb_m);

	try {
	    c = left.Boolean(right, manifold::OpType::Add);
#if defined(CHECK_INTERMEDIATES)
	    c.GetMesh();
#endif
	} catch (const std::exception &e) {
	    bu_log("Faces - manifold boolean op failure\n");
	    std::cerr << e.what() << "\n";
#if defined(CHECK_INTERMEDIATES)
	    manifold::ExportMesh(std::string("left.glb"), left.GetMesh(), {});
	    manifold::ExportMesh(std::string("right.glb"), right.GetMesh(), {});
	    bu_exit(EXIT_FAILURE, "halting on boolean failure");
#endif
	    return -1;
	}
    }
    bu_log("Processing %zd faces... done.\n" , bot->num_faces);

    manifold::Mesh rmesh = c.GetMesh();
    struct rt_bot_internal *rbot;
    BU_GET(rbot, struct rt_bot_internal);
    rbot->magic = RT_BOT_INTERNAL_MAGIC;
    rbot->mode = RT_BOT_SOLID;
    rbot->orientation = RT_BOT_CCW;
    rbot->thickness = NULL;
    rbot->face_mode = (struct bu_bitv *)NULL;
    rbot->bot_flags = 0;
    rbot->num_vertices = (int)rmesh.vertPos.size();
    rbot->num_faces = (int)rmesh.triVerts.size();
    rbot->vertices = (double *)calloc(rmesh.vertPos.size()*3, sizeof(double));;
    rbot->faces = (int *)calloc(rmesh.triVerts.size()*3, sizeof(int));
    for (size_t j = 0; j < rmesh.vertPos.size(); j++) {
	rbot->vertices[3*j] = rmesh.vertPos[j].x;
	rbot->vertices[3*j+1] = rmesh.vertPos[j].y;
	rbot->vertices[3*j+2] = rmesh.vertPos[j].z;
    }
    for (size_t j = 0; j < rmesh.triVerts.size(); j++) {
	rbot->faces[3*j] = rmesh.triVerts[j].x;
	rbot->faces[3*j+1] = rmesh.triVerts[j].y;
	rbot->faces[3*j+2] = rmesh.triVerts[j].z;
    }
    *obot = rbot;
    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

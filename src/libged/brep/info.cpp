/*                        I N F O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file libged/brep/info.cpp
 *
 * LIBGED brep info subcommand implementation - reports detailed information
 * on the structure of individual brep objects.
 *
 */

#include "common.h"

#include <set>

#include "opennurbs.h"
#include "./ged_brep.h"

static int
brep_surface_info(const ON_Brep *brep, struct bu_vls *vls, int si)
{
    ON_wString wonstr;
    ON_TextLog info_output(wonstr);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((si >= 0) && (si < brep->m_S.Count()))) {
	return GED_ERROR;
    }
    const ON_Surface* srf = brep->m_S[si];
    if (srf) {
	//ON_wString wonstr;
	//ON_TextLog info_output(wonstr);
	ON_Interval udom = srf->Domain(0);
	ON_Interval vdom = srf->Domain(1);
	const char* s = srf->ClassId()->ClassName();
	if (!s)
	    s = "";
	bu_vls_printf(vls, "surface[%2d]: %s u(%g, %g) v(%g, %g)\n",
		si, s,
		udom[0], udom[1],
		vdom[0], vdom[1]
		);
	bu_vls_printf(vls, "NURBS form of Surface:\n");
	ON_NurbsSurface *nsrf = ON_NurbsSurface::New();
	srf->GetNurbForm(*nsrf, 0.0);
	nsrf->Dump(info_output);
	ON_String onstr = ON_String(wonstr);
	const char *infodesc = onstr.Array();
	bu_vls_strcat(vls, infodesc);
	delete nsrf;
    } else {
	bu_vls_printf(vls, "surface[%2d]: NULL\n", si);
    }

    return GED_OK;
}


static int
brep_surface_bezier_info(const ON_Brep *brep, struct bu_vls *vls, int si)
{
    ON_wString wonstr;
    ON_TextLog info_output(wonstr);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((si >= 0) && (si < brep->m_S.Count()))) {
	return GED_ERROR;
    }
    const ON_Surface* srf = brep->m_S[si];
    if (srf) {
	//ON_wString wonstr;
	//ON_TextLog info_output(wonstr);
	ON_Interval udom = srf->Domain(0);
	ON_Interval vdom = srf->Domain(1);
	const char* s = srf->ClassId()->ClassName();
	if (!s)
	    s = "";
	bu_vls_printf(vls, "surface[%2d]: %s u(%g, %g) v(%g, %g)\n",
		si, s,
		udom[0], udom[1],
		vdom[0], vdom[1]
		);
	ON_NurbsSurface *nsrf = ON_NurbsSurface::New();
	srf->GetNurbForm(*nsrf, 0.0);
	int knotlength0 = nsrf->m_order[0] + nsrf->m_cv_count[0] - 2;
	int knotlength1 = nsrf->m_order[1] + nsrf->m_cv_count[1] - 2;
	int order0 = nsrf->m_order[0];
	int order1 = nsrf->m_order[1];
	fastf_t *knot0 = nsrf->m_knot[0];
	fastf_t *knot1 = nsrf->m_knot[1];
	int cnt = 0;
	bu_vls_printf(vls, "bezier patches:\n");
	for (int i = 0; i < knotlength0; ++i) {
	    for (int j = 0; j < knotlength1; ++j) {
		ON_BezierSurface *bezier = new ON_BezierSurface;
		if (nsrf->ConvertSpanToBezier(i, j, *bezier)) {
		    info_output.Print("NO.%d segment\n", ++cnt);
		    info_output.Print("spanindex u from %d to %d\n", i + order0 - 2, i + order0 - 1);
		    info_output.Print("spanindex v from %d to %d\n", j + order1 - 2, j + order1 - 1);
		    info_output.Print("knot u from %.2f to %.2f\n ", knot0[i + order0 - 2], knot0[i + order0 - 1]);
		    info_output.Print("knot v from %.2f to %.2f\n ", knot1[j + order1 - 2], knot1[j + order1 - 1]);
		    info_output.Print("domain u(%g, %g)\n", bezier->Domain(0)[0], bezier->Domain(0)[1]);
		    info_output.Print("domain v(%g, %g)\n", bezier->Domain(1)[0], bezier->Domain(1)[1]);
		    bezier->Dump(info_output);
		    info_output.Print("\n");
		}
		delete bezier;
	    }
	}
	ON_String onstr = ON_String(wonstr);
	const char *infodesc = onstr.Array();
	bu_vls_strcat(vls, infodesc);
	delete nsrf;
    } else {
	bu_vls_printf(vls, "surface[%2d]: NULL\n", si);
    }
    return GED_OK;
}


static int
brep_face_info(const ON_Brep *brep, struct bu_vls *vls, int fi)
{
    ON_wString s;
    ON_TextLog dump(s);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((fi >= 0) && (fi < brep->m_F.Count()))) {
	return GED_ERROR;
    }
    //ON_wString s;
    //ON_TextLog dump(s);
    const ON_BrepFace& face = brep->m_F[fi];
    const ON_Surface* face_srf = face.SurfaceOf();
    dump.Print("face[%2d]: surface(%d) reverse(%d) loops(", fi, face.m_si, face.m_bRev);
    int fli;
    for (fli = 0; fli < face.m_li.Count(); fli++) {
	dump.Print((fli) ? ", %d" : "%d", face.m_li[fli]);
    }
    dump.Print(")\n");
    dump.PushIndent();

    for (fli = 0; fli < face.m_li.Count(); fli++) {
	const int li = face.m_li[fli];
	const ON_BrepLoop& loop = brep->m_L[li];
	const char* sLoopType = 0;
	switch (loop.m_type) {
	    case ON_BrepLoop::unknown:
		sLoopType = "unknown";
		break;
	    case ON_BrepLoop::outer:
		sLoopType = "outer";
		break;
	    case ON_BrepLoop::inner:
		sLoopType = "inner";
		break;
	    case ON_BrepLoop::slit:
		sLoopType = "slit";
		break;
	    case ON_BrepLoop::crvonsrf:
		sLoopType = "crvonsrf";
		break;
	    default:
		sLoopType = "unknown";
		break;
	}
	dump.Print("loop[%2d]: type(%s) %d trims(", li, sLoopType, loop.m_ti.Count());
	int lti;
	for (lti = 0; lti < loop.m_ti.Count(); lti++) {
	    dump.Print((lti) ? ", %d" : "%d", loop.m_ti[lti]);
	}
	dump.Print(")\n");
	dump.PushIndent();
	for (lti = 0; lti < loop.m_ti.Count(); lti++) {
	    const int ti = loop.m_ti[lti];
	    const ON_BrepTrim& trim = brep->m_T[ti];
	    const char* sTrimType = "?";
	    const char* sTrimIso = "-?";
	    const ON_Curve* c2 = trim.TrimCurveOf();
	    ON_NurbsCurve* nc2 = ON_NurbsCurve::New();
	    c2->GetNurbForm(*nc2, 0.0);
	    ON_3dPoint trim_start, trim_end;
	    switch (trim.m_type) {
		case ON_BrepTrim::unknown:
		    sTrimType = "unknown ";
		    break;
		case ON_BrepTrim::boundary:
		    sTrimType = "boundary";
		    break;
		case ON_BrepTrim::mated:
		    sTrimType = "mated   ";
		    break;
		case ON_BrepTrim::seam:
		    sTrimType = "seam    ";
		    break;
		case ON_BrepTrim::singular:
		    sTrimType = "singular";
		    break;
		case ON_BrepTrim::crvonsrf:
		    sTrimType = "crvonsrf";
		    break;
		default:
		    sTrimType = "unknown";
		    break;
	    }
	    switch (trim.m_iso) {
		case ON_Surface::not_iso:
		    sTrimIso = "";
		    break;
		case ON_Surface::x_iso:
		    sTrimIso = "-u iso";
		    break;
		case ON_Surface::W_iso:
		    sTrimIso = "-west side iso";
		    break;
		case ON_Surface::E_iso:
		    sTrimIso = "-east side iso";
		    break;
		case ON_Surface::y_iso:
		    sTrimIso = "-v iso";
		    break;
		case ON_Surface::S_iso:
		    sTrimIso = "-south side iso";
		    break;
		case ON_Surface::N_iso:
		    sTrimIso = "-north side iso";
		    break;
		default:
		    sTrimIso = "-unknown_iso_flag";
		    break;
	    }
	    dump.Print("trim[%2d]: edge(%2d) v0(%2d) v1(%2d) tolerance(%g, %g)\n", ti, trim.m_ei, trim.m_vi[0], trim.m_vi[1], trim.m_tolerance[0], trim.m_tolerance[1]);
	    dump.PushIndent();
	    dump.Print("type(%s%s) rev3d(%d) 2d_curve(%d)\n", sTrimType, sTrimIso, trim.m_bRev3d, trim.m_c2i);
	    if (c2) {
		trim_start = trim.PointAtStart();
		trim_end = trim.PointAtEnd();
		dump.Print("domain(%g, %g) start(%g, %g) end(%g, %g)\n", trim.Domain()[0], trim.Domain()[1], trim_start.x, trim_start.y, trim_end.x, trim_end.y);
		if (0 != face_srf) {
		    ON_3dPoint trim_srfstart = face_srf->PointAt(trim_start.x, trim_start.y);
		    ON_3dPoint trim_srfend = face_srf->PointAt(trim_end.x, trim_end.y);
		    dump.Print("surface points start(%g, %g, %g) end(%g, %g, %g)\n", trim_srfstart.x, trim_srfstart.y, trim_srfstart.z, trim_srfend.x, trim_srfend.y, trim_srfend.z);
		}
	    } else {
		dump.Print("domain(%g, %g) start(?, ?) end(?, ?)\n", trim.Domain()[0], trim.Domain()[1]);
	    }
	    dump.PopIndent();
	}
	dump.PopIndent();
    }
    dump.PopIndent();

    ON_String ss = s;
    bu_vls_printf(vls, "%s\n", ss.Array());

    return GED_OK;
}


static int
brep_trim_info(const ON_Brep *brep, struct bu_vls *vls, int ti)
{
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((ti >= 0) && (ti < brep->m_T.Count()))) {
	return GED_ERROR;
    }
    const ON_BrepTrim &trim = brep->m_T[ti];
    const ON_Surface* trim_srf = trim.SurfaceOf();
    const ON_BrepLoop &loop = brep->m_L[trim.m_li];
    const ON_BrepFace &face = brep->m_F[loop.m_fi];
    const char* sTrimType = "?";
    const char* sTrimIso = "-?";
    const ON_Curve* c2 = trim.TrimCurveOf();
    ON_NurbsCurve* nc2 = ON_NurbsCurve::New();
    c2->GetNurbForm(*nc2, 0.0);
    ON_3dPoint trim_start, trim_end;
    dump.Print("trim[%2d]: surface(%2d) faces(%2d) loops(%2d)\n", ti, face.m_si, face.m_face_index, loop.m_loop_index);
    switch (trim.m_type) {
	case ON_BrepTrim::unknown:
	    sTrimType = "unknown ";
	    break;
	case ON_BrepTrim::boundary:
	    sTrimType = "boundary";
	    break;
	case ON_BrepTrim::mated:
	    sTrimType = "mated   ";
	    break;
	case ON_BrepTrim::seam:
	    sTrimType = "seam    ";
	    break;
	case ON_BrepTrim::singular:
	    sTrimType = "singular";
	    break;
	case ON_BrepTrim::crvonsrf:
	    sTrimType = "crvonsrf";
	    break;
	default:
	    sTrimType = "unknown";
	    break;
    }
    switch (trim.m_iso) {
	case ON_Surface::not_iso:
	    sTrimIso = "";
	    break;
	case ON_Surface::x_iso:
	    sTrimIso = "-u iso";
	    break;
	case ON_Surface::W_iso:
	    sTrimIso = "-west side iso";
	    break;
	case ON_Surface::E_iso:
	    sTrimIso = "-east side iso";
	    break;
	case ON_Surface::y_iso:
	    sTrimIso = "-v iso";
	    break;
	case ON_Surface::S_iso:
	    sTrimIso = "-south side iso";
	    break;
	case ON_Surface::N_iso:
	    sTrimIso = "-north side iso";
	    break;
	default:
	    sTrimIso = "-unknown_iso_flag";
	    break;
    }
    dump.Print("\tedge(%2d) v0(%2d) v1(%2d) tolerance(%g, %g)\n", trim.m_ei, trim.m_vi[0], trim.m_vi[1], trim.m_tolerance[0], trim.m_tolerance[1]);
    dump.PushIndent();
    dump.Print("\ttype(%s%s) rev3d(%d) 2d_curve(%d)\n", sTrimType, sTrimIso, trim.m_bRev3d, trim.m_c2i);
    if (c2) {
	trim_start = trim.PointAtStart();
	trim_end = trim.PointAtEnd();
	dump.Print("\tdomain(%g, %g) start(%g, %g) end(%g, %g)\n", trim.Domain()[0], trim.Domain()[1], trim_start.x, trim_start.y, trim_end.x, trim_end.y);
	if (0 != trim_srf) {
	    ON_3dPoint trim_srfstart = trim_srf->PointAt(trim_start.x, trim_start.y);
	    ON_3dPoint trim_srfend = trim_srf->PointAt(trim_end.x, trim_end.y);
	    dump.Print("\tsurface points start(%g, %g, %g) end(%g, %g, %g)\n", trim_srfstart.x, trim_srfstart.y, trim_srfstart.z, trim_srfend.x, trim_srfend.y, trim_srfend.z);
	}
    } else {
	dump.Print("\tdomain(%g, %g) start(?, ?) end(?, ?)\n", trim.Domain()[0], trim.Domain()[1]);
    }
    dump.PopIndent();
    dump.Print("NURBS form of 2d_curve(trim)\n");
    nc2->Dump(dump);
    delete nc2;

    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return GED_OK;
}


static int
brep_trim_bezier_info(const ON_Brep *brep, struct bu_vls *vls, int ti)
{
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((ti >= 0) && (ti < brep->m_T.Count()))) {
	return GED_ERROR;
    }
    const ON_BrepTrim &trim = brep->m_T[ti];
    const ON_Curve* c2 = trim.TrimCurveOf();
    ON_NurbsCurve* nc2 = ON_NurbsCurve::New();
    c2->GetNurbForm(*nc2, 0.0);
    int knotlength = nc2->m_order + nc2->m_cv_count - 2;
    int order = nc2->m_order;
    fastf_t *knot = nc2->m_knot;
    dump.Print("trim[%2d]: domain(%g, %g)\n", ti, nc2->Domain()[0], nc2->Domain()[1]);
    int cnt = 0;
    dump.Print("NURBS converts to Bezier\n");
    for (int i = 0; i < knotlength - 1; ++i) {
	ON_BezierCurve* bezier = new ON_BezierCurve;
	if (nc2->ConvertSpanToBezier(i, *bezier)) {
	    dump.Print("NO.%d segment\n", ++cnt);
	    dump.Print("spanindex from %d to %d\n", i + order - 2, i + order - 1);
	    dump.Print("knot from %.2f to %.2f\n ", knot[i + order - 2], knot[i + order - 1]);
	    dump.Print("domain(%g, %g)\n", bezier->Domain()[0], bezier->Domain()[1]);
	    bezier->Dump(dump);
	    dump.Print("\n");
	}
	delete bezier;
    }
    delete nc2;
    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return GED_OK;
}


static int
brep_curve_info(const ON_Brep *brep, struct bu_vls *vls, int ci)
{
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((ci >= 0) && (ci < brep->m_C3.Count()))) {
	return GED_ERROR;
    }
    const ON_Curve *curve = brep->m_C3[ci];
    ON_NurbsCurve* nc3 = ON_NurbsCurve::New();
    curve->GetNurbForm(*nc3, 0.0);
    dump.Print("NURBS form of 3d_curve(edge) \n");
    nc3->Dump(dump);
    delete nc3;

    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return GED_OK;
}

static int
brep_loop_info(const ON_Brep *brep, struct bu_vls *vls, int li)
{
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((li >= 0) && (li < brep->m_L.Count()))) {
	return GED_ERROR;
    }
    const ON_BrepLoop &loop = brep->m_L[li];
    dump.Print("loop[%d] on face %d with %d trims\n", li, loop.m_fi, loop.TrimCount());
    if (loop.TrimCount() > 0) {
	dump.Print("trims: ");
	for (int i = 0; i < loop.TrimCount() - 1; ++i) {
	    dump.Print("%d,", loop.m_ti[i]);
	}
	dump.Print("%d\n", loop.m_ti[loop.TrimCount() - 1]);
    }
    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return GED_OK;
}

static int
brep_edge_info(const ON_Brep *brep, struct bu_vls *vls, int ei)
{
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return GED_ERROR;
    }
    if (!((ei >= 0) && (ei < brep->m_E.Count()))) {
	return GED_ERROR;
    }
    const ON_BrepEdge &edge = brep->m_E[ei];
    int trim_cnt = edge.m_ti.Count();
    const ON_Curve* c3 = edge.EdgeCurveOf();
    ON_NurbsCurve* nc3 = ON_NurbsCurve::New();
    c3->GetNurbForm(*nc3, 0.0);
    ON_3dPoint edge_start, edge_end;
    dump.Print("edge[%2d]: for ", ei);
    for (int i = 0; i < trim_cnt; ++i) {
	dump.Print("trim[%2d] ", edge.m_ti[i]);
    }
    dump.Print("\n");
    dump.Print("v0(%2d) v1(%2d) 3d_curve(%2d) tolerance(%g, %g)\n", ei, edge.m_vi[0], edge.m_vi[1], edge.m_c3i, edge.m_tolerance);
    dump.PushIndent();
    if (c3) {
	edge_start = edge.PointAtStart();
	edge_end = edge.PointAtEnd();
	dump.Print("\tdomain(%g, %g) surface points start(%g, %g, %g) end(%g, %g, %g)\n", edge.Domain()[0], edge.Domain()[1], edge_start.x, edge_start.y, edge_start.z, edge_end.x, edge_end.y, edge_end.z);
    } else {
	dump.Print("\tdomain(%g, %g) start(?, ?) end(?, ?)\n", edge.Domain()[0], edge.Domain()[1]);
    }
    dump.PopIndent();
    dump.Print("NURBS form of 3d_curve(edge) \n");
    nc3->Dump(dump);
    delete nc3;

    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return GED_OK;
}

int
brep_info(struct bu_vls *vls, const ON_Brep *brep, int argc, const char **argv)
{
    int ret = GED_ERROR;

    if (!argc) {
	bu_vls_printf(vls, "surfaces:  %d\n", brep->m_S.Count());
	bu_vls_printf(vls, "3d curve:  %d\n", brep->m_C3.Count());
	bu_vls_printf(vls, "2d curves: %d\n", brep->m_C2.Count());
	bu_vls_printf(vls, "vertices:  %d\n", brep->m_V.Count());
	bu_vls_printf(vls, "edges:     %d\n", brep->m_E.Count());
	bu_vls_printf(vls, "trims:     %d\n", brep->m_T.Count());
	bu_vls_printf(vls, "loops:     %d\n", brep->m_L.Count());
	bu_vls_printf(vls, "faces:     %d\n", brep->m_F.Count());
	return GED_OK;
    }
    if (argc == 1) {
	const char *part = argv[0];
	if (BU_STR_EQUAL(part, "S")) {
	    for (int i = 0; i < brep->m_S.Count(); ++i) {
		ret = brep_surface_info(brep, vls, i);
	    }
	} else if (BU_STR_EQUAL(part, "F")) {
	    for (int i = 0; i < brep->m_F.Count(); ++i) {
		ret = brep_face_info(brep, vls, i);
	    }
	} else if (BU_STR_EQUAL(part, "T")) {
	    for (int i = 0; i < brep->m_T.Count(); ++i) {
		ret = brep_trim_info(brep, vls, i);
	    }
	} else if (BU_STR_EQUAL(part, "E")) {
	    for (int i = 0; i < brep->m_E.Count(); ++i) {
		ret = brep_edge_info(brep, vls, i);
	    }
	} else if (BU_STR_EQUAL(part, "SB")) {
	    for (int i = 0; i < brep->m_S.Count(); ++i) {
		ret = brep_surface_bezier_info(brep, vls, i);
	    }
	} else if (BU_STR_EQUAL(part, "TB")) {
	    for (int i = 0; i < brep->m_T.Count(); ++i) {
		ret = brep_trim_bezier_info(brep, vls, i);
	    }
	} else if (BU_STR_EQUAL(part, "C")) {
	    for (int i = 0; i < brep->m_C3.Count(); ++i) {
		ret = brep_curve_info(brep, vls, i);
	    }
	}
    } else if (argc == 2) {
	const char *part = argv[0];
	const char *strindex = argv[1];
	std::set<int> elements;
	std::set<int>::const_iterator e_it;
	if (BU_STR_EQUAL(strindex, "all")) {
	    if (BU_STR_EQUAL(part, "S")) {
		for (int i = 0; i < brep->m_S.Count(); ++i) {
		    ret = brep_surface_info(brep, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "F")) {
		for (int i = 0; i < brep->m_F.Count(); ++i) {
		    ret = brep_face_info(brep, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "T")) {
		for (int i = 0; i < brep->m_T.Count(); ++i) {
		    ret = brep_trim_info(brep, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "E")) {
		for (int i = 0; i < brep->m_E.Count(); ++i) {
		    ret = brep_edge_info(brep, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "L")) {
		for (int i = 0; i < brep->m_L.Count(); ++i) {
		    ret = brep_loop_info(brep, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "SB")) {
		for (int i = 0; i < brep->m_S.Count(); ++i) {
		    ret = brep_surface_bezier_info(brep, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "TB")) {
		for (int i = 0; i < brep->m_T.Count(); ++i) {
		    ret = brep_trim_bezier_info(brep, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "C")) {
		for (int i = 0; i < brep->m_C3.Count(); ++i) {
		    ret = brep_curve_info(brep, vls, i);
		}
	    }
	} else if (BU_STR_EQUAL(strindex, "?")) {
	    //info_usage(vls);
	} else {
	    const char *dash = strchr(strindex, '-');
	    const char *comma = strchr(strindex, ',');
	    if (dash) {
		int startindex = -1;
		int endindex = -1;
		struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
		bu_vls_strcpy(&tmpstr, strindex);
		bu_vls_trunc(&tmpstr, dash - strindex);
		startindex = atoi(bu_vls_addr(&tmpstr));
		bu_vls_strcpy(&tmpstr, ++dash);
		endindex = atoi(bu_vls_addr(&tmpstr));
		bu_vls_free(&tmpstr);
		for (int elem = startindex; elem <= endindex; elem++) {
		    elements.insert(elem);
		}
	    } else if (comma) {
		struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
		bu_vls_strcpy(&tmpstr, strindex);
		while (strlen(bu_vls_addr(&tmpstr)) > 0) {
		    struct bu_vls tmpstr2 = BU_VLS_INIT_ZERO;
		    int idx = 0;
		    bu_vls_strcpy(&tmpstr2, bu_vls_addr(&tmpstr));
		    bu_vls_trunc(&tmpstr2, comma - bu_vls_addr(&tmpstr));
		    idx = atoi(bu_vls_addr(&tmpstr2));
		    bu_vls_free(&tmpstr2);
		    elements.insert(idx);
		    int stp = 0;
		    while (idx >= 10) {
			int idx2 = idx / 10;
			idx = idx2;
			stp++;
		    }
		    bu_vls_nibble(&tmpstr, stp+2);
		    comma = strchr(bu_vls_addr(&tmpstr), ',');
		}
		bu_vls_free(&tmpstr);
	    } else {
		int idx = atoi(strindex);
		elements.insert(idx);
	    }
	    if (BU_STR_EQUAL(part, "S")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_S.Count()) ret = brep_surface_info(brep, vls, (*e_it));
		}
	    } else if (BU_STR_EQUAL(part, "F")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_F.Count()) ret = brep_face_info(brep, vls, (*e_it));
		}
	    } else if (BU_STR_EQUAL(part, "T")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_T.Count()) ret = brep_trim_info(brep, vls, (*e_it));
		}
	    } else if (BU_STR_EQUAL(part, "E")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_E.Count()) ret = brep_edge_info(brep, vls, (*e_it));
		}
	    } else if (BU_STR_EQUAL(part, "L")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_L.Count()) ret = brep_loop_info(brep, vls, (*e_it));
		}
	    } else if (BU_STR_EQUAL(part, "SB")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_S.Count()) ret = brep_surface_bezier_info(brep, vls, (*e_it));
		}
	    } else if (BU_STR_EQUAL(part, "TB")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_T.Count()) ret = brep_trim_bezier_info(brep, vls, (*e_it));
		}
	    } else if (BU_STR_EQUAL(part, "C")) {
		for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
		    if (*e_it < brep->m_C3.Count()) ret = brep_curve_info(brep, vls, (*e_it));
		}
	    }
	}
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

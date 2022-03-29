/*                           S A T . C
 * BRL-CAD
 *
 * Based on implementations in GeometircTools:
 *
 * https://github.com/davideberly/GeometricTools
 *
 * David Eberly, Geometric Tools, Redmond WA 98052
 * Copyright (c) 1998-2022
 *
 * Distributed under:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer, must
 * be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are
 * solely in the form of machine-executable object code generated by a source
 * language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/** @file sat.c
 *
 * Implementations of various Separating Axis Theorem algorithms for detecting
 * collisions between various types of objects (detection only, no calculations
 * to find points or shapes.)
 *
 * OBBTree: A Hierarchical Structure for Rapid Interference Detection
 * http://www.cs.unc.edu/techreports/96-013.pdf
 *
 * Dynamic Collision Detection using Oriented Bounding Boxes
 * https://www.geometrictools.com/Documentation/DynamicCollisionDetection.pdf
 *
 * The test-intersection implementations use the method of separating axes.
 * https://www.geometrictools.com/Documentation/MethodOfSeparatingAxes.pdf
 *
 */

#include "common.h"
#include "vmath.h"
#include "bg/sat.h"

/* Check line against ABB
 *
 * See GTE/Mathematics/IntrLine3AlignedBox3.h
 */
int
bg_sat_line_abb(point_t origin, vect_t ldir, point_t aabb_center, vect_t aabb_extent)
{
    vect_t lineOrigin, dir;

    // Transform the line to the aligned-box coordinate system.
    VSUB2(lineOrigin, origin, aabb_center);

    VMOVE(dir, ldir);
    VUNITIZE(dir);

    vect_t WxD, absWdU;
    VCROSS(WxD, dir, lineOrigin);
    VSET(absWdU, fabs(WxD[0]), fabs(WxD[1]), fabs(WxD[2]));

    if (fabs(WxD[0]) > aabb_extent[1] * absWdU[2] + aabb_extent[2] * absWdU[1])
	return 0;

    if (fabs(WxD[1]) > aabb_extent[0] * absWdU[2] + aabb_extent[2] * absWdU[0])
	return 0;

    if (fabs(WxD[2]) > aabb_extent[0] * absWdU[1] + aabb_extent[1] * absWdU[0])
	return 0;

    return 1;
}

static void
GetProjectionOBB(fastf_t *imin, fastf_t *imax,
	vect_t axis, point_t center, vect_t extent, vect_t A0, vect_t A1, vect_t A2)
{
    fastf_t origin = VDOT(axis, center);
    fastf_t maximumExtent =
	fabs(extent[0] * VDOT(axis, A0)) +
	fabs(extent[1] * VDOT(axis, A1)) +
	fabs(extent[2] * VDOT(axis, A2));

    (*imin) = origin - maximumExtent;
    (*imax) = origin + maximumExtent;
}

static void
GetProjectionTri(fastf_t *imin, fastf_t *imax, vect_t axis, point_t v1, point_t v2, point_t v3)
{
    fastf_t dot[3];
    dot[0] = VDOT(axis, v1);
    dot[1] = VDOT(axis, v2);
    dot[2] = VDOT(axis, v3);

    (*imin) = dot[0];
    (*imax) = (*imin);

    if (dot[1] < (*imin)) {
	(*imin) = dot[1];
    } else if (dot[1] > (*imax)) {
	(*imax) = dot[1];
    }

    if (dot[2] < (*imin)) {
	(*imin) = dot[2];
    } else if (dot[2] > (*imax)) {
	(*imax) = dot[2];
    }
}

/* Check OBB against a triangle.
 *
 * See GTE/Mathematics/IntrTriangle3OrientedBox3.h.h
 */
int
bg_sat_tri_obb(
	point_t v1, point_t v2, point_t v3,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	)
{
    fastf_t min0 = 0.0, max0 = 0.0, min1 = 0.0, max1 = 0.0;
    vect_t D, edge[3];
    vect_t extent, A[3];
    VSET(extent, MAGNITUDE(obb_extent1), MAGNITUDE(obb_extent2), MAGNITUDE(obb_extent3));
    VMOVE(A[0], obb_extent1);
    VUNITIZE(A[0]);
    VMOVE(A[1], obb_extent2);
    VUNITIZE(A[1]);
    VMOVE(A[2], obb_extent3);
    VUNITIZE(A[2]);

    // Test direction of triangle normal.
    VSUB2(edge[0], v2, v1);
    VSUB2(edge[1], v3, v1);
    VCROSS(D, edge[0], edge[1]);
    min0 = VDOT(D, v1);
    max0 = min0;

    GetProjectionOBB(&min1, &max1, D, obb_center, extent, A[0], A[1], A[2]);
    if (max1 < min0 || max0 < min1)
	return 0;

    // Test direction of box faces.
    for (int32_t i = 0; i < 3; ++i) {
	VMOVE(D, A[i]);
	GetProjectionTri(&min0, &max0, D, v1, v2, v3);
	fastf_t DdC = VDOT(D, obb_center);
	min1 = DdC - extent[i];
	max1 = DdC + extent[i];
	if (max1 < min0 || max0 < min1)
	    return 0;
    }

    // Test direction of triangle-box edge cross products.
    VSUB2(edge[2], edge[1], edge[0]);
    for (int32_t i0 = 0; i0 < 3; ++i0) {
	for (int32_t i1 = 0; i1 < 3; ++i1) {
	    VCROSS(D, edge[i0], A[i1]);
	    GetProjectionTri(&min0, &max0, D, v1, v2, v3);
	    GetProjectionOBB(&min1, &max1, D, obb_center, extent, A[0], A[1], A[2]);
	    if (max1 < min0 || max0 < min1)
		return 0;
	}
    }

    return 1;
}

/* Check OBB against an ABB.
 *
 * See GTE/Mathematics/IntrAlignedBox3OrientedBox3.h
 */
int
bg_sat_abb_obb(
	point_t abb_min, point_t abb_max,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	)
{
    // Get the center/extent form of the aligned box. The axes are
    // implicitly A0[0] = (1,0,0), A0[1] = (0,1,0) and
    // A0[2] = (0,0,1).
    vect_t C0, E0;
    VADD2SCALE(C0, abb_max, abb_min, 0.5);
    VSUB2SCALE(E0, abb_max, abb_min, 0.5);

    // Convenience variables.
    vect_t C1, E1;
    VMOVE(C1, obb_center);
    VSET(E1, MAGNITUDE(obb_extent1), MAGNITUDE(obb_extent2), MAGNITUDE(obb_extent3));
    vect_t A1[3];
    VMOVE(A1[0], obb_extent1);
    VUNITIZE(A1[0]);
    VMOVE(A1[1], obb_extent2);
    VUNITIZE(A1[1]);
    VMOVE(A1[2], obb_extent3);
    VUNITIZE(A1[2]);

    fastf_t epsilon = VUNITIZE_TOL;
    fastf_t cutoff = 1.0 - epsilon;
    int existsParallelPair = 0;

    // Compute the difference of box centers.
    vect_t D;
    VSUB2(D, C1, C0);

    // dot01[i][j] = Dot(A0[i],A1[j]) = A1[j][i]
    fastf_t dot01[3][3];

    // |dot01[i][j]|
    fastf_t absDot01[3][3];

    // interval radii and distance between centers
    fastf_t r0, r1, r;

    // r0 + r1
    fastf_t r01;

    // Test for separation on the axis C0 + t*A0[0].
    for (int i = 0; i < 3; ++i) {
	dot01[0][i] = A1[i][0];
	absDot01[0][i] = fabs(A1[i][0]);
	if (absDot01[0][i] >= cutoff)
	    existsParallelPair = 1;
    }
    r = fabs(D[0]);
    r1 = E1[0] * absDot01[0][0] + E1[1] * absDot01[0][1] + E1[2] * absDot01[0][2];
    r01 = E0[0] + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1].
    for (int i = 0; i < 3; ++i) {
	dot01[1][i] = A1[i][1];
	absDot01[1][i] = fabs(A1[i][1]);
	if (absDot01[1][i] >= cutoff)
	    existsParallelPair = 1;
    }
    r = fabs(D[1]);
    r1 = E1[0] * absDot01[1][0] + E1[1] * absDot01[1][1] + E1[2] * absDot01[1][2];
    r01 = E0[1] + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2].
    for (int i = 0; i < 3; ++i)
    {
	dot01[2][i] = A1[i][2];
	absDot01[2][i] = fabs(A1[i][2]);
	if (absDot01[2][i] >= cutoff)
	{
	    existsParallelPair = 1;
	}
    }
    r = fabs(D[2]);
    r1 = E1[0] * absDot01[2][0] + E1[1] * absDot01[2][1] + E1[2] * absDot01[2][2];
    r01 = E0[2] + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A1[0].
    r = fabs(VDOT(D, A1[0]));
    r0 = E0[0] * absDot01[0][0] + E0[1] * absDot01[1][0] + E0[2] * absDot01[2][0];
    r01 = r0 + E1[0];
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A1[1].
    r = fabs(VDOT(D, A1[1]));
    r0 = E0[0] * absDot01[0][1] + E0[1] * absDot01[1][1] + E0[2] * absDot01[2][1];
    r01 = r0 + E1[1];
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A1[2].
    r = fabs(VDOT(D, A1[2]));
    r0 = E0[0] * absDot01[0][2] + E0[1] * absDot01[1][2] + E0[2] * absDot01[2][2];
    r01 = r0 + E1[2];
    if (r > r01)
	return 0;

    // At least one pair of box axes was parallel, so the separation is
    // effectively in 2D. The edge-edge axes do not need to be tested.
    if (existsParallelPair) {
	// The result.separating[] values are invalid because there is
	// no separation.
	return 1;
    }

    // Test for separation on the axis C0 + t*A0[0]xA1[0].
    r = fabs(D[2] * dot01[1][0] - D[1] * dot01[2][0]);
    r0 = E0[1] * absDot01[2][0] + E0[2] * absDot01[1][0];
    r1 = E1[1] * absDot01[0][2] + E1[2] * absDot01[0][1];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[0]xA1[1].
    r = fabs(D[2] * dot01[1][1] - D[1] * dot01[2][1]);
    r0 = E0[1] * absDot01[2][1] + E0[2] * absDot01[1][1];
    r1 = E1[0] * absDot01[0][2] + E1[2] * absDot01[0][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[0]xA1[2].
    r = fabs(D[2] * dot01[1][2] - D[1] * dot01[2][2]);
    r0 = E0[1] * absDot01[2][2] + E0[2] * absDot01[1][2];
    r1 = E1[0] * absDot01[0][1] + E1[1] * absDot01[0][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1]xA1[0].
    r = fabs(D[0] * dot01[2][0] - D[2] * dot01[0][0]);
    r0 = E0[0] * absDot01[2][0] + E0[2] * absDot01[0][0];
    r1 = E1[1] * absDot01[1][2] + E1[2] * absDot01[1][1];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1]xA1[1].
    r = fabs(D[0] * dot01[2][1] - D[2] * dot01[0][1]);
    r0 = E0[0] * absDot01[2][1] + E0[2] * absDot01[0][1];
    r1 = E1[0] * absDot01[1][2] + E1[2] * absDot01[1][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1]xA1[2].
    r = fabs(D[0] * dot01[2][2] - D[2] * dot01[0][2]);
    r0 = E0[0] * absDot01[2][2] + E0[2] * absDot01[0][2];
    r1 = E1[0] * absDot01[1][1] + E1[1] * absDot01[1][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2]xA1[0].
    r = fabs(D[1] * dot01[0][0] - D[0] * dot01[1][0]);
    r0 = E0[0] * absDot01[1][0] + E0[1] * absDot01[0][0];
    r1 = E1[1] * absDot01[2][2] + E1[2] * absDot01[2][1];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2]xA1[1].
    r = fabs(D[1] * dot01[0][1] - D[0] * dot01[1][1]);
    r0 = E0[0] * absDot01[1][1] + E0[1] * absDot01[0][1];
    r1 = E1[0] * absDot01[2][2] + E1[2] * absDot01[2][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2]xA1[2].
    r = fabs(D[1] * dot01[0][2] - D[0] * dot01[1][2]);
    r0 = E0[0] * absDot01[1][2] + E0[1] * absDot01[0][2];
    r1 = E1[0] * absDot01[2][1] + E1[1] * absDot01[2][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // The result.separating[] values are invalid because there is no
    // separation.
    return 1;
}

/* Check OBB against another OBB.
 *
 * See GTE/Mathematics/IntrOrientedBox3OrientedBox3.h
 */
int
bg_sat_obb_obb(
	point_t obb1_center, vect_t obb1_extent1, vect_t obb1_extent2, vect_t obb1_extent3,
	point_t obb2_center, vect_t obb2_extent1, vect_t obb2_extent2, vect_t obb2_extent3
	)
{
    // Convenience variables.
    vect_t C0, E0, C1, E1;
    VMOVE(C0, obb1_center);
    VSET(E0, MAGNITUDE(obb1_extent1), MAGNITUDE(obb1_extent2), MAGNITUDE(obb1_extent3));
    VMOVE(C1, obb2_center);
    VSET(E1, MAGNITUDE(obb2_extent1), MAGNITUDE(obb2_extent2), MAGNITUDE(obb2_extent3));

    vect_t A0[3];
    VMOVE(A0[0], obb1_extent1);
    VUNITIZE(A0[0]);
    VMOVE(A0[1], obb1_extent2);
    VUNITIZE(A0[1]);
    VMOVE(A0[2], obb1_extent3);
    VUNITIZE(A0[2]);

    vect_t A1[3];
    VMOVE(A1[0], obb2_extent1);
    VUNITIZE(A1[0]);
    VMOVE(A1[1], obb2_extent2);
    VUNITIZE(A1[1]);
    VMOVE(A1[2], obb2_extent3);
    VUNITIZE(A1[2]);

    fastf_t epsilon = VUNITIZE_TOL;
    fastf_t cutoff = 1.0 - epsilon;
    int existsParallelPair = 0;

    // Compute difference of box centers.
    vect_t D;
    VSUB2(D, C1, C0);

    // dot01[i][j] = Dot(A0[i],A1[j]) = A1[j][i]
    fastf_t dot01[3][3];

    // |dot01[i][j]|
    fastf_t absDot01[3][3];

    // Dot(D, A0[i])
    vect_t dotDA0;

    // interval radii and distance between centers
    fastf_t r0, r1, r;

    // r0 + r1
    fastf_t r01;

    // Test for separation on the axis C0 + t*A0[0].
    for (int i = 0; i < 3; ++i) {
	dot01[0][i] = VDOT(A0[0], A1[i]);
	absDot01[0][i] = fabs(A1[i][0]);
	if (absDot01[0][i] >= cutoff)
	    existsParallelPair = 1;
    }
    dotDA0[0] = VDOT(D, A0[0]);
    r = fabs(dotDA0[0]);
    r1 = E1[0] * absDot01[0][0] + E1[1] * absDot01[0][1] + E1[2] * absDot01[0][2];
    r01 = E0[0] + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1].
    for (int i = 0; i < 3; ++i) {
	dot01[1][i] = VDOT(A0[1], A1[i]);
	absDot01[1][i] = fabs(dot01[1][i]);
	if (absDot01[1][i] > cutoff)
	    existsParallelPair = 1;
    }
    dotDA0[1] = VDOT(D, A0[1]);
    r = fabs(dotDA0[1]);
    r1 = E1[0] * absDot01[1][0] + E1[1] * absDot01[1][1] + E1[2] * absDot01[1][2];
    r01 = E0[1] + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2].
    for (int i = 0; i < 3; ++i) {
	dot01[2][i] = VDOT(A0[2], A1[i]);
	absDot01[2][i] = fabs(dot01[2][i]);
	if (absDot01[2][i] > cutoff)
	    existsParallelPair = 1;
    }
    dotDA0[2] = VDOT(D, A0[2]);
    r = fabs(dotDA0[2]);
    r1 = E1[0] * absDot01[2][0] + E1[1] * absDot01[2][1] + E1[2] * absDot01[2][2];
    r01 = E0[2] + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A1[0].
    r = fabs(VDOT(D, A1[0]));
    r0 = E0[0] * absDot01[0][0] + E0[1] * absDot01[1][0] + E0[2] * absDot01[2][0];
    r01 = r0 + E1[0];
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A1[1].
    r = fabs(VDOT(D, A1[1]));
    r0 = E0[0] * absDot01[0][1] + E0[1] * absDot01[1][1] + E0[2] * absDot01[2][1];
    r01 = r0 + E1[1];
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A1[2].
    r = fabs(VDOT(D, A1[2]));
    r0 = E0[0] * absDot01[0][2] + E0[1] * absDot01[1][2] + E0[2] * absDot01[2][2];
    r01 = r0 + E1[2];
    if (r > r01)
	return 0;

    // At least one pair of box axes was parallel, so the separation is
    // effectively in 2D. The edge-edge axes do not need to be tested.
    if (existsParallelPair)
	return 1;

    // Test for separation on the axis C0 + t*A0[0]xA1[0].
    r = fabs(dotDA0[2] * dot01[1][0] - dotDA0[1] * dot01[2][0]);
    r0 = E0[1] * absDot01[2][0] + E0[2] * absDot01[1][0];
    r1 = E1[1] * absDot01[0][2] + E1[2] * absDot01[0][1];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[0]xA1[1].
    r = fabs(dotDA0[2] * dot01[1][1] - dotDA0[1] * dot01[2][1]);
    r0 = E0[1] * absDot01[2][1] + E0[2] * absDot01[1][1];
    r1 = E1[0] * absDot01[0][2] + E1[2] * absDot01[0][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[0]xA1[2].
    r = fabs(dotDA0[2] * dot01[1][2] - dotDA0[1] * dot01[2][2]);
    r0 = E0[1] * absDot01[2][2] + E0[2] * absDot01[1][2];
    r1 = E1[0] * absDot01[0][1] + E1[1] * absDot01[0][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1]xA1[0].
    r = fabs(dotDA0[0] * dot01[2][0] - dotDA0[2] * dot01[0][0]);
    r0 = E0[0] * absDot01[2][0] + E0[2] * absDot01[0][0];
    r1 = E1[1] * absDot01[1][2] + E1[2] * absDot01[1][1];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1]xA1[1].
    r = fabs(dotDA0[0] * dot01[2][1] - dotDA0[2] * dot01[0][1]);
    r0 = E0[0] * absDot01[2][1] + E0[2] * absDot01[0][1];
    r1 = E1[0] * absDot01[1][2] + E1[2] * absDot01[1][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[1]xA1[2].
    r = fabs(dotDA0[0] * dot01[2][2] - dotDA0[2] * dot01[0][2]);
    r0 = E0[0] * absDot01[2][2] + E0[2] * absDot01[0][2];
    r1 = E1[0] * absDot01[1][1] + E1[1] * absDot01[1][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2]xA1[0].
    r = fabs(dotDA0[1] * dot01[0][0] - dotDA0[0] * dot01[1][0]);
    r0 = E0[0] * absDot01[1][0] + E0[1] * absDot01[0][0];
    r1 = E1[1] * absDot01[2][2] + E1[2] * absDot01[2][1];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2]xA1[1].
    r = fabs(dotDA0[1] * dot01[0][1] - dotDA0[0] * dot01[1][1]);
    r0 = E0[0] * absDot01[1][1] + E0[1] * absDot01[0][1];
    r1 = E1[0] * absDot01[2][2] + E1[2] * absDot01[2][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // Test for separation on the axis C0 + t*A0[2]xA1[2].
    r = fabs(dotDA0[1] * dot01[0][2] - dotDA0[0] * dot01[1][2]);
    r0 = E0[0] * absDot01[1][2] + E0[1] * absDot01[0][2];
    r1 = E1[0] * absDot01[2][1] + E1[1] * absDot01[2][0];
    r01 = r0 + r1;
    if (r > r01)
	return 0;

    // The result.separating[] values are invalid because there is no
    // separation.
    return 1;
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

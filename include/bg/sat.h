/*                          S A T . H
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

/*----------------------------------------------------------------------*/
/* @file sat.h */
/** @addtogroup bg_sat */
/** @{ */

/**
 * @brief
 *
 * Implementation of Separating Axis Theorem intersection tests
 *
 */

#ifndef BG_SAT_H
#define BG_SAT_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * Test for an intersection between a line and an Axis-Aligned Bounding Box (AABB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_line_aabb(
	point_t origin, vect_t ldir,
	point_t aabb_center, vect_t aabb_extent
	);

/**
 * Test for an intersection between a line and an Oriented Bounding Box (OBB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_line_obb(
	point_t origin, vect_t ldir,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	);

/**
 * Test for an intersection between a triangle and an Axis-Aligned Bounding Box (AABB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_tri_aabb(
	point_t v1, point_t v2, point_t v3,
	point_t aabb_center, vect_t aabb_extent
	);

/**
 * Test for an intersection between a triangle and an Oriented Bounding Box (OBB).
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_tri_obb(
	point_t v1, point_t v2, point_t v3,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	);

/**
 * Test for an intersection between an Axis-Aligned Bounding Box (AABB) and an
 * Oriented Bounding Box (OBB). The latter is defined by a center point and
 * three perpendicular vectors from the center to the centers of the various
 * faces.
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_aabb_obb(
	point_t aabb_min, point_t aabb_max,
	point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3
	);

/**
 * Test for an intersection between two Oriented Bounding Boxes (OBBs). The
 * boxes are defined by a center point and three perpendicular vectors from the
 * center to the centers of the various faces.
 *
 * Returns 1 if they intersect, 0 otherwise.
 */
BG_EXPORT extern int
bg_sat_obb_obb(
	point_t obb1_center, vect_t obb1_extent1, vect_t obb1_extent2, vect_t obb1_extent3,
	point_t obb2_center, vect_t obb2_extent1, vect_t obb2_extent2, vect_t obb2_extent3
	);

__END_DECLS

#endif  /* BG_SAT_H */
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

/*
 *			N M G _ M I S C . C
 *
 *	As the name implies, these are miscellaneous routines that work with
 *	the NMG structures.
 *
 *
 *  Authors -
 *	John R. Anderson
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "nmg.h"
#include "raytrace.h"

#include "db.h"		/* for debugging stuff at bottom */

/*	N M G _ N E X T _ R A D I A L _ E D G E U S E
 *
 * Traverse radial edgeuse around specified edgeuse looking for
 * one that meets optional restrictions. If a shell is specified
 * only edgeuse from that shell will be considered. If wires is
 * non-zero, wire edges will be considered, otherwise, wire edges
 * are ignored.
 *
 * returns:
 *	radial edgeuse
 */

struct edgeuse *
nmg_next_radial_eu( eu , s , wires )
CONST struct edgeuse *eu;
CONST struct shell *s;
CONST int wires;
{
	struct edgeuse *ret_eu;

	NMG_CK_EDGEUSE( eu );
	NMG_CK_SHELL( s );

	if( s && nmg_find_s_of_eu( eu ) != s )
		rt_bomb( "nmg_find_radial_eu: eu is not in specified shell\n" );

	if( !wires && !nmg_find_fu_of_eu( eu ) )
		rt_bomb( "nmg_find_radial_eu: wire edges not specified, but eu is a wire!!\n" );

	ret_eu = eu->eumate_p->radial_p;
	while( ( !wires & (nmg_find_fu_of_eu( ret_eu ) == (struct faceuse *)NULL)) ||
		( (s != (struct shell *)NULL) & nmg_find_s_of_eu( ret_eu ) != s  ) )
			ret_eu = ret_eu->eumate_p->radial_p;

	return( ret_eu );
}

/*	N M G _ P R E V _ R A D I A L _ E D G E U S E
 *
 * Traverse radial edgeuse around specified edgeuse in opposite
 * direction from nmg_next_radial_eu, looking for
 * one that meets optional restrictions. If a shell is specified
 * only edgeuse from that shell will be considered. If wires is
 * non-zero, wire edges will be considered, otherwise, wire edges
 * are ignored.
 *
 * returns:
 *	radial edgeuse
 */

struct edgeuse *
nmg_prev_radial_eu( eu , s , wires )
CONST struct edgeuse *eu;
CONST struct shell *s;
CONST int wires;
{
	struct edgeuse *ret_eu;

	NMG_CK_EDGEUSE( eu );
	NMG_CK_SHELL( s );

	if( s && nmg_find_s_of_eu( eu ) != s )
		rt_bomb( "nmg_find_radial_eu: eu is not in specified shell\n" );

	if( !wires && !nmg_find_fu_of_eu( eu ) )
		rt_bomb( "nmg_find_radial_eu: wire edges not specified, but eu is a wire!!\n" );

	ret_eu = eu->radial_p->eumate_p;
	while( ( !wires & (nmg_find_fu_of_eu( ret_eu ) == (struct faceuse *)NULL)) ||
		( (s != (struct shell *)NULL) & nmg_find_s_of_eu( ret_eu ) != s  ) )
			ret_eu = ret_eu->radial_p->eumate_p;

	return( ret_eu );
}

/*	N M G _ R A D I A L _ F A C E _ C O U N T
 *
 * Counts the number of faces (actually, the number of radial edgeuse/mate pairs)
 * around eu. If s is specified, only edgeuses in shell s are counted. Wire
 * edgeuses are not counted.
 *
 * returns:
 *	number of edgeuse/mate pairs radiall around eu that meet restrictions
 */
int
nmg_radial_face_count( eu , s )
CONST struct edgeuse *eu;
CONST struct shell *s;
{
	int face_count=1;
	struct edgeuse *eu1;

	NMG_CK_EDGEUSE( eu );
	if( s )
		NMG_CK_SHELL( s );

	/* count radial faces on this edge */
	eu1 = eu->eumate_p->radial_p;
	while( eu1 != eu && eu1 != eu->eumate_p )
	{
		/* ignore other shells and don't count wires */
		if( (!s || nmg_find_s_of_eu( eu1 ) == s) &&
			nmg_find_fu_of_eu( eu1 ) != (struct faceuse *)NULL )
				face_count++;
		eu1 = eu1->eumate_p->radial_p;
	}

	return( face_count );
}

/*	N M G _ M O V E _ L U _ B E T W E E N _ F U S
 *
 * Moves lu from src faceuse to dest faceuse
 *
 * returns:
 *	0 - All is well
 *	1 - src faceuse is now empty
 */
int
nmg_move_lu_between_fus( dest , src , lu )
struct faceuse *dest;
struct faceuse *src;
struct loopuse *lu;
{
	struct loopuse *lumate;
	int src_is_empty;

	NMG_CK_FACEUSE( dest );
	NMG_CK_FACEUSE( dest->fumate_p );
	NMG_CK_FACEUSE( src );
	NMG_CK_FACEUSE( src->fumate_p );
	NMG_CK_LOOPUSE( lu );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_move_lu_between_fus( dest=x%x, src=x%x, lu=x%x)\n", dest, src, lu );

	if( lu->up.fu_p != src )
	{
		rt_log( "nmg_move_lu_between_fus( dest=x%x, src=x%x, lu=x%x)\n", dest, src, lu );
		rt_bomb( "\tlu is not in src faceuse\n" );
	}

	if( dest == src )
		return( 0 );

	lumate = lu->lumate_p;
	NMG_CK_LOOPUSE( lumate );

	/* remove lu from src faceuse */
	RT_LIST_DEQUEUE( &lu->l );
	src_is_empty = RT_LIST_IS_EMPTY( &src->lu_hd );

	/* remove lumate from src faceuse mate */
	RT_LIST_DEQUEUE( &lumate->l );
	if( src_is_empty != RT_LIST_IS_EMPTY( &src->fumate_p->lu_hd ) )
	{
		rt_log( "nmg_move_lu_between_fus( dest=x%x, src=x%x, lu=x%x)\n", dest, src, lu );
		if( src_is_empty )
			rt_bomb( "\tsrc faceuse contains only lu, but src->fumate_p has more!!\n" );
		else
			rt_bomb( "\tsrc->fumate_p faceuse contains only lu->lumate_p, but src has more!!\n" );
	}

	/* add lu to dest faceuse */
	RT_LIST_INSERT( &dest->lu_hd, &lu->l );

	/* add lumate to dest mate */
	RT_LIST_INSERT( &dest->fumate_p->lu_hd, &lumate->l );

	/* adjust up pointers */
	lu->up.fu_p = dest;
	lumate->up.fu_p = dest->fumate_p;

	return( src_is_empty );
}

/*	N M G _ L O O P _ P L A N E _ A R E A
 *
 *  Calculates a plane equation and the area of a loop
 *
 *  returns:
 *	the area of the loop
 *	less than zero indicates an error
 *
 *  pl is assigned the plane equation for the loop
 *
 */

fastf_t
nmg_loop_plane_area( lu , pl )
CONST struct loopuse *lu;
plane_t pl;
{
	fastf_t area;
	fastf_t vect_mag;
	fastf_t vect_mag_inv;
	fastf_t pt_dot_plane=0.0;
	fastf_t pt_count=0.0;
	plane_t plane;
	struct edgeuse *eu;
	vect_t trans;

	NMG_CK_LOOPUSE( lu );

	/* make sure we have a loop of edges */
	if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return( (fastf_t)(-1.0) );

	/* check if this loop is a crack */
	if( nmg_loop_is_a_crack( lu ) )
		return( (fastf_t)(-1.0) );

	/* calculate a translation to put one vertex at the origin
	 * not necessary, but good for accuracy.
	 * Also, origin must be in plane of loop for this
	 * method to work
	 */
	eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
	NMG_CK_VERTEXUSE( eu->vu_p );
	NMG_CK_VERTEX( eu->vu_p->v_p );
	NMG_CK_VERTEX_G( eu->vu_p->v_p->vg_p );
	VMOVE( trans , eu->vu_p->v_p->vg_p->coord );

	VSET( plane , 0.0 , 0.0 , 0.0 );

	/* Calculate area and plane normal.
	 * Cross product of each pair of vertices gives twice
	 * the area of the triangle formed by the origin and
	 * the two vertices. (positive if counter-clockwise,
	 * negative if clockwise). In counter_clockwise case,
	 * sum of all cross products around loop adds area for
	 * edges away from origin and subtracts area for edges
	 * near origin, leaving twice the area of the polygon.
	 */
	for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		struct edgeuse *next_eu;
		struct vertex *vp,*next_vp;
		vect_t cross;
		point_t p1,p2;

		vp = eu->vu_p->v_p;

		next_eu = RT_LIST_PNEXT_CIRC( edgeuse , &eu->l );
		NMG_CK_EDGEUSE( next_eu );
		NMG_CK_VERTEXUSE( next_eu->vu_p );

		next_vp = next_eu->vu_p->v_p;
		NMG_CK_VERTEX( next_vp );
		NMG_CK_VERTEX_G( next_vp->vg_p );

		VSUB2( p1 , vp->vg_p->coord , trans );
		VSUB2( p2 , next_vp->vg_p->coord , trans );
		VCROSS( cross , p1 , p2 );
		VADD2( plane , plane , cross );
	}

	vect_mag = MAGNITUDE( plane );

	/* Error if the area is too small to unitize the normal vector */
	if( vect_mag < SMALL_FASTF )
		return( (fastf_t)(-1.0) );

	area = 0.5 * vect_mag;
	vect_mag_inv = 1.0/vect_mag;

	VSCALE( plane , plane , vect_mag_inv );

	/* calculate plane[3] as average distance to plane */
	for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		pt_dot_plane += VDOT( plane , eu->vu_p->v_p->vg_p->coord );
		pt_count++;
	}

	/* Error if we don't have at least 3 vertices in this loop */
	if( pt_count < 3 )
		return( (fastf_t)(-1.0) );

	plane[3] = pt_dot_plane/pt_count;
	HMOVE( pl , plane );

	return( area );
}

/*	R T _ D I S T _ L I N E 3 _ L I N E 3
 *
 *  Calculate closest approach of two lines
 *
 *  returns:
 *	-2 -> lines are parallel and do not intersect
 *	-1 -> lines are parallel and collinear
 *	 0 -> lines intersect
 *	 1 -> lines do not intersect
 *  For return values less than zero, dist is not set.
 *  For return valuse of 0 or 1, dist[0] is the distance
 *  from p1 in the d1 direction to the point of closest
 *  approach for that line.  Similar for the second line.
 *  d1 and d2 must be unit direction vectors.
 */

int
rt_dist_line3_line3( dist , p1 , d1 , p2 , d2 , tol )
fastf_t dist[2];
CONST point_t p1,p2;
CONST vect_t d1,d2;
CONST struct rt_tol *tol;
{
	fastf_t d1_d2;
	point_t a1,a2;
	vect_t a1_to_a2;
	vect_t p2_to_p1;
	fastf_t min_dist;

	RT_CK_TOL( tol );

	if( !NEAR_ZERO( MAGSQ( d1 ) - 1.0 , tol->dist_sq ) )
	{
		rt_log( "rt_dist_line3_line3: non-unit length direction vector ( %f %f %f )\n" , V3ARGS( d1 ) );
		rt_bomb( "rt_dist_line3_line3\n" );
	}

	if( !NEAR_ZERO( MAGSQ( d2 ) - 1.0 , tol->dist_sq ) )
	{
		rt_log( "rt_dist_line3_line3: non-unit length direction vector ( %f %f %f )\n" , V3ARGS( d2 ) );
		rt_bomb( "rt_dist_line3_line3\n" );
	}

	d1_d2 = VDOT( d1 , d2 );

	if( RT_VECT_ARE_PARALLEL( d1_d2 , tol ) )
	{
		if( rt_dist_line_point( p1 , d1 , p2 ) > tol->dist )
			return( -2 ); /* parallel, but not collinear */
		else
			return( -1 ); /* parallel and collinear */
	}

	VSUB2( p2_to_p1 , p1 , p2 );
	dist[0] = (d1_d2 * VDOT( p2_to_p1 , d2 ) - VDOT( p2_to_p1 , d1 ))/(1.0 - d1_d2 * d1_d2 );
	dist[1] = dist[0] * d1_d2 + VDOT( p2_to_p1 , d2 );

	VJOIN1( a1 , p1 , dist[0] , d1 );
	VJOIN1( a2 , p2 , dist[1] , d2 );

	VSUB2( a1_to_a2 , a2 , a1 );
	min_dist = MAGNITUDE( a1_to_a2 );
	if( min_dist < tol->dist )
		return( 0 );
	else
		return( 1 );
}

/*	R T _ D I S T _ L I N E 3 _ L S E G 3
 *
 *  calculate intersection or closest approach of
 *  a line and a line segement.
 *
 *  returns:
 *	-2 -> line and line segment are parallel and collinear.
 *	-1 -> line and line segment are parallel, not collinear.
 *	 0 -> intersection between points a and b.
 *	 1 -> intersection outside a and b.
 *	 2 -> closest approach is between a and b.
 *	 3 -> closest approach is outside a and b.
 *
 *  dist[0] is actual distance from p in d direction to
 *  closest portion of segment.
 *  dist[1] is ratio of distance from a to b (0.0 at a, and 1.0 at b),
 *  dist[1] may be less than 0 or greater than 1.
 *  For return values less than 0, closest approach is defined as
 *  closest to point p.
 *  Direction vector, d, must be unit length.
 */

int
rt_dist_line3_lseg3( dist , p , d , a , b , tol )
fastf_t dist[2];
CONST point_t p;
CONST vect_t d;
CONST point_t a,b;
CONST struct rt_tol *tol;
{
	vect_t a_to_b;
	vect_t a_dir;
	fastf_t len_ab;
	int outside_segment;
	int ret;

	RT_CK_TOL( tol );

	VSUB2( a_to_b , b , a );
	len_ab = MAGNITUDE( a_to_b );
	VSCALE( a_dir , a_to_b , (1.0/len_ab) );

	ret = rt_dist_line3_line3( dist , p , d , a , a_dir , tol );

	if( ret < 0 )
	{
		vect_t to_a,to_b;
		fastf_t dist_to_a,dist_to_b;

		VSUB2( to_a , a , p );
		VSUB2( to_b , b , p );
		dist_to_a = VDOT( to_a , d );
		dist_to_b = VDOT( to_b , d );

		if( dist_to_a <= dist_to_b )
		{
			dist[0] = dist_to_a;
			dist[1] = 0.0;
		}
		else
		{
			dist[0] = dist_to_b;
			dist[1] = 1.0;
		}
		return( ret );
	}

	if( dist[1] >= (-tol->dist) && dist[1] <= len_ab + tol->dist )
	{
		/* intersect or closest approach between a and b */
		outside_segment = 0;
		dist[1] = dist[1]/len_ab;
		if( dist[1] < 0.0 )
			dist[1] = 0.0;
		if( dist[1] > 1.0 )
			dist[1] = 1.0;
	}
	else
	{
		outside_segment = 1;
		dist[1] = dist[1]/len_ab;
	}

	return( 2*ret + outside_segment );
}

/*	N M G _ T B L
 *	maintain a table of pointers (to magic numbers/structs)
 */
int
nmg_tbl(b, func, p)
struct nmg_ptbl *b;
int func;
long *p;
{
	if (func == TBL_INIT) {
		b->magic = NMG_PTBL_MAGIC;
		b->blen=64;
		b->buffer = (long **)rt_calloc(b->blen, sizeof(long *),
			"nmg_ptbl.buffer[]");
		b->end = 0;
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_INIT\n", b);
		return(0);
	} else if (func == TBL_RST) {
		NMG_CK_PTBL(b);
		b->end = 0;
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_RST\n", b);
		return(0);
	} else if (func == TBL_INS) {
		register int i;
		union {
			struct loopuse *lu;
			struct edgeuse *eu;
			struct vertexuse *vu;
			long *l;
		} pp;

		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_INS %8x\n", b, p);

		NMG_CK_PTBL(b);
		pp.l = p;

		if (b->blen == 0) (void)nmg_tbl(b, TBL_INIT, p);
		if (b->end >= b->blen)
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
			    sizeof(p)*(b->blen *= 4),
			    "pointer table" );

		b->buffer[i=b->end++] = p;
		return(i);
	} else if (func == TBL_LOC) {
		/* we do this a great deal, so make it go as fast as possible.
		 * this is the biggest argument I can make for changing to an
		 * ordered list.  Someday....
		 */
		register int	k;
		register long	**pp = b->buffer;

		NMG_CK_PTBL(b);
#		include "noalias.h"
		for( k = b->end-1; k >= 0; k-- )
			if (pp[k] == p) return(k);

		return(-1);
	} else if (func == TBL_INS_UNIQUE) {
		/* we do this a great deal, so make it go as fast as possible.
		 * this is the biggest argument I can make for changing to an
		 * ordered list.  Someday....
		 */
		register int	k;
		register long	**pp = b->buffer;

		NMG_CK_PTBL(b);
#		include "noalias.h"
		for( k = b->end-1; k >= 0; k-- )
			if (pp[k] == p) return(k);

		if (b->blen <= 0 || b->end >= b->blen)  {
			/* Table needs to grow */
			return( nmg_tbl( b, TBL_INS, p ) );
		}

		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_INS_UNIQUE %8x\n", b, p);

		b->buffer[k=b->end++] = p;
		return(-1);		/* To signal that it was added */
	} else if (func == TBL_RM) {
		/* we go backwards down the list looking for occurrences
		 * of p to delete.  We do it backwards to reduce the amount
		 * of data moved when there is more than one occurrence of p
		 * in the list.  A pittance savings, unless you're doing a
		 * lot of it.
		 */
		register int end = b->end, j, k, l;
		register long **pp = b->buffer;

		NMG_CK_PTBL(b);
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_RM %8x\n", b, p);
		for (l = b->end-1 ; l >= 0 ; --l)
			if (pp[l] == p){
				/* delete occurrence(s) of p */

				j=l+1;
				while (pp[l-1] == p) --l;

				end -= j - l;
#				include "noalias.h"
				for(k=l ; j < b->end ;)
					b->buffer[k++] = b->buffer[j++];
				b->end = end;
			}
		return(0);
	} else if (func == TBL_CAT) {
		union {
			long *l;
			struct nmg_ptbl *t;
		} d;

		NMG_CK_PTBL(b);
		d.l = p;
		NMG_CK_PTBL(d.t);
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_CAT %8x\n", b, p);

		if ((b->blen - b->end) < d.t->end) {
			
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
				sizeof(p)*(b->blen += d.t->blen),
				"pointer table (CAT)");
		}
		bcopy(d.t->buffer, &b->buffer[b->end], d.t->end*sizeof(p));
		return(0);
	} else if (func == TBL_FREE) {
		NMG_CK_PTBL(b);
		bzero((char *)b->buffer, b->blen * sizeof(p));
		rt_free((char *)b->buffer, "pointer table");
		bzero(b, sizeof(struct nmg_ptbl));
		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl(%8x) TBL_FREE\n", b);
		return (0);
	} else {
		NMG_CK_PTBL(b);
		rt_log("nmg_tbl(%8x) Unknown table function %d\n", b, func);
		rt_bomb("nmg_tbl");
	}
	return(-1);/* this is here to keep lint happy */
}




/* N M G _ P U R G E _ U N W A N T E D _ I N T E R S E C T I O N _ P O I N T S
 *
 *	Make sure that the list of intersection points doesn't contain
 *	any vertexuses from loops whose bounding boxes don;t overlap the
 *	bounding box of a loop in the given faceuse.
 *
 *	This is really a special purpose routine to help the intersection
 *	operations of the boolean process.  The only reason it's here instead
 *	of nmg_inter.c is that it knows too much about the format and contents
 *	of an nmg_ptbl structure.
 */
void
nmg_purge_unwanted_intersection_points(vert_list, fu, tol)
struct nmg_ptbl		*vert_list;
CONST struct faceuse	*fu;
CONST struct rt_tol	*tol;
{
	int			i;
	int			j;
	struct vertexuse	*vu;
	struct loopuse		*lu;
	CONST struct loop_g	*lg;
	CONST struct loopuse	*fu2lu;
	CONST struct loop_g	*fu2lg = (CONST struct loop_g *)NULL;
	int			overlap = 0;

	NMG_CK_FACEUSE(fu);
	RT_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_purge_unwanted_intersection_points(0x%08x, 0x%08x)\n", vert_list, fu);

	for (i=0 ; i < vert_list->end ; i++) {
		vu = (struct vertexuse *)vert_list->buffer[i];
		NMG_CK_VERTEXUSE(vu);
		lu = nmg_find_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		lg = lu->l_p->lg_p;
		NMG_CK_LOOP_G(lg);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("vu[%d]: 0x%08x (%g %g %g) lu: 0x%08x %s\n",
				i, vu, V3ARGS(vu->v_p->vg_p->coord),
				lu, nmg_orientation(lu->orientation) );
			rt_log("\tlu BBox: (%g %g %g) (%g %g %g)\n",
				V3ARGS(lg->min_pt), V3ARGS(lg->max_pt) );
		}
		if (lu->up.fu_p->f_p == fu->f_p)
			rt_log("I'm checking against my own face?\n");

		/* If the bounding box of a loop doesn't intersect the
		 * bounding box of a loop in the other face, it shouldn't
		 * be on the list of intersecting loops.
		 */
		overlap = 0;
		for (RT_LIST_FOR(fu2lu, loopuse, &fu->lu_hd )){
			NMG_CK_LOOPUSE(fu2lu);
			NMG_CK_LOOP(fu2lu->l_p);

			switch(fu2lu->orientation)  {
			case OT_BOOLPLACE:
				/*  If this loop is destined for removal
				 *  by sanitize(), skip it.
				 */
				continue;
			case OT_UNSPEC:
				/* If this is a loop of a lone vertex,
				 * it was deposited into the
				 * other loop as part of an intersection
				 * operation, and is quite important.
				 */
				if( RT_LIST_FIRST_MAGIC(&fu2lu->down_hd) != NMG_VERTEXUSE_MAGIC )
					rt_log("nmg_purge_unwanted_intersection_points() non self-loop OT_UNSPEC vertexuse in fu2?\n");
				break;
			case OT_SAME:
			case OT_OPPOSITE:
				break;
			default:
				rt_log("nmg_purge_unwanted_intersection_points encountered %s loop in fu2\n",
					nmg_orientation(fu2lu->orientation));
				/* Process it anyway */
				break;
			}

			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);

			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				rt_log("\tfu2lu BBox: (%g %g %g)  (%g %g %g) %s\n",
					V3ARGS(fu2lg->min_pt), V3ARGS(fu2lg->max_pt),
					nmg_orientation(fu2lu->orientation) );
			}

			if (V3RPP_OVERLAP_TOL(fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt, tol)) {
				overlap = 1;
				break;
			}
		}
		if (!overlap) {
			/* why is this vertexuse in the list? */
			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				rt_log("nmg_purge_unwanted_intersection_points This little bugger slipped in somehow.  Deleting it from the list.\n");
				nmg_pr_vu_briefly(vu, (char *)NULL);
			}
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC &&
			    lu->orientation == OT_UNSPEC )  {
				/* Drop this loop of a single vertex in sanitize() */
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("nmg_purge_unwanted_intersection_points() remarking as OT_BOOLPLACE\n");
				lu->orientation =
				  lu->lumate_p->orientation = OT_BOOLPLACE;
			}

			/* delete the entry from the vertex list */
			for (j=i ; j < vert_list->end ; j++)
				vert_list->buffer[j] = vert_list->buffer[j+1];

			--(vert_list->end);
			vert_list->buffer[vert_list->end] = (long *)NULL;
			--i;
		}
	}
}


/*				N M G _ I N _ O R _ R E F
 *
 *	if the given vertexuse "vu" is in the table given by "b" or if "vu"
 *	references a vertex which is refernced by a vertexuse in the table,
 *	then we return 1.  Otherwise, we return 0.
 */
int 
nmg_in_or_ref(vu, b)
struct vertexuse *vu;
struct nmg_ptbl *b;
{
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	int i;

	p.magic_p = b->buffer;
	for (i=0 ; i < b->end ; ++i) {
		if (p.vu[i] && *p.magic_p[i] == NMG_VERTEXUSE_MAGIC &&
		    (p.vu[i] == vu || p.vu[i]->v_p == vu->v_p))
			return(1);
	}
	return(0);
}

/*
 *			N M G _ R E B O U N D
 *
 *  Re-compute all the bounding boxes in the NMG model.
 *  Bounding boxes are presently found in these structures:
 *	loop_g
 *	face_g
 *	shell_a
 *	nmgregion_a
 *  The re-bounding must be performed in a bottom-up manner,
 *  computing the loops first, and working up to the nmgregions.
 */
void
nmg_rebound( m, tol )
struct model		*m;
CONST struct rt_tol	*tol;
{
	struct nmgregion	*r;
	struct shell		*s;
	struct faceuse		*fu;
	struct face		*f;
	struct loopuse		*lu;
	struct loop		*l;
	register int		*flags;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	flags = (int *)rt_calloc( m->maxindex, sizeof(int), "rebound flags[]" );

	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);

			/* Loops in faces in shell */
			for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				/* Loops in face */
				for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					l = lu->l_p;
					NMG_CK_LOOP(l);
					if( NMG_INDEX_FIRST_TIME(flags,l) )
						nmg_loop_g(l, tol);
				}
			}
			/* Faces in shell */
			for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				f = fu->f_p;
				NMG_CK_FACE(f);

				/* Rebound the face */
				if( NMG_INDEX_FIRST_TIME(flags,f) )
					nmg_face_bb( f, tol );
			}

			/* Wire loops in shell */
			for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				l = lu->l_p;
				NMG_CK_LOOP(l);
				if( NMG_INDEX_FIRST_TIME(flags,l) )
					nmg_loop_g(l, tol);
			}

			/*
			 *  Rebound the shell.
			 *  This routine handles wire edges and lone vertices.
			 */
			if( NMG_INDEX_FIRST_TIME(flags,s) )
				nmg_shell_a( s, tol );
		}

		/* Rebound the nmgregion */
		nmg_region_a( r, tol );
	}

	rt_free( (char *)flags, "rebound flags[]" );
}

/*
 *			N M G _ C O U N T _ S H E L L _ K I D S
 */
void
nmg_count_shell_kids(m, total_faces, total_wires, total_points)
CONST struct model *m;
unsigned long *total_wires;
unsigned long *total_faces;
unsigned long *total_points;
{
	short *tbl;

	CONST struct nmgregion *r;
	CONST struct shell *s;
	CONST struct faceuse *fu;
	CONST struct loopuse *lu;
	CONST struct edgeuse *eu;

	NMG_CK_MODEL(m);

	tbl = (short *)rt_calloc(m->maxindex+1, sizeof(char),
		"face/wire/point counted table");

	*total_faces = *total_wires = *total_points = 0;
	for (RT_LIST_FOR(r, nmgregion, &m->r_hd)) {
		for (RT_LIST_FOR(s, shell, &r->s_hd)) {
			if (s->vu_p) {
				total_points++;
				continue;
			}
			for (RT_LIST_FOR(fu, faceuse, &s->fu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, fu->f_p))
					(*total_faces)++;
			}
			for (RT_LIST_FOR(lu, loopuse, &s->lu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, lu->l_p))
					(*total_wires)++;
			}
			for (RT_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, eu->e_p))
					(*total_wires)++;
			}
		}
	}

	rt_free((char *)tbl, "face/wire/point counted table");
}

/*
 *	O R D E R _ T B L
 *
 *	private support routine for nmg_close_shell
 *	creates an array of indices into a table of edgeuses, ordered
 *	to create a loop.
 *
 *	Arguments:
 *	tbl is the table (provided by caller)
 *	index is the array of indices created by order_tbl
 *	tbl_size is the size of the table (provided by caller)
 *	loop_size is the number of edgeuses in the loop (calculated by order_tbl)
 */
static void
order_tbl( tbl , index , tbl_size , loop_size )
struct nmg_ptbl *tbl;
int **index;
int tbl_size;
int *loop_size;
{
	int i,j;
	int found;
	struct edgeuse *eu,*eu1;

	/* create an index into the table, ordered to create a loop */
	if( *index == NULL )
		(*index) = (int *)rt_calloc( tbl_size , sizeof( int ) , "Table index" );

	for( i=0 ; i<tbl_size ; i++ )
		(*index)[i] = (-1);

	/* start the loop at index = 0 */
	(*index)[0] = 0;
	*loop_size = 1;
	eu = (struct edgeuse *)NMG_TBL_GET( tbl , 0 );
	found = 1;
	i = 0;
	while( found )
	{
		found = 0;

		/* Look for edgeuse that starts where "eu" ends */
		for( j=1 ; j<tbl_size ; j++ )
		{
			eu1 = (struct edgeuse *)NMG_TBL_GET( tbl , j );
			if( eu1->vu_p->v_p == eu->eumate_p->vu_p->v_p )
			{
				/* Found it */
				found = 1;
				(*index)[++i] = j;
				(*loop_size)++;
				eu = eu1;
				break;
			}
		}
	}
}

/*
 *	N M G _ C L O S E _ S H E L L
 *
 *	Examines the passed shell and, if there are holes, closes them
 *	note that not much care is taken as to how the holes are closed
 *	so the results are not entirely predictable.
 *	A list of free edges is created (edges bounding only one face).
 *	New faces are constructed by taking two consecutive edges
 *	and making a face. The newly created edge is added to the list
 *	of free edges and the two used ones are removed.
 *
 */
void
nmg_close_shell( s , tol )
struct shell *s;
struct rt_tol *tol;
{
	struct nmg_ptbl eu_tbl;		/* table of free edgeuses from shell */
	struct nmg_ptbl vert_tbl;	/* table of vertices for use in nmg_cface */
	int *index;			/* array of indices into eu_tbl, ordered to form a loop */
	int loop_size;			/* number of edgeueses in loop */
	struct faceuse **fu_list;	/* array of pointers to faceuses, for use in nmg_gluefaces */
	int fu_counter=0;		/* number of faceuses in above array */
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1,*eu2,*eu3,*eu_new;
	int i,j;
	int found;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_close_shell: s = x%x\n" , s );

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	index = NULL;

	/* construct a table of free edges */
	(void)nmg_tbl( &eu_tbl , TBL_INIT , (long *)NULL );

	/* loop through all the faces in the shell */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		/* only look at OT_SAME faces */
		if( fu->orientation == OT_SAME )
		{
			/* count 'em */
			fu_counter++;
			/* loop through each loopuse in the face */
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				/* ignore loops that are just a vertex */
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) ==
					NMG_VERTEXUSE_MAGIC )
						continue;

				/* loop through all the edgeuses in the loop */
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					/* if this edgeuse is a free edge, add its mate to the list */
					if( eu->radial_p == eu->eumate_p )
						(void)nmg_tbl( &eu_tbl , TBL_INS , (long *) eu->eumate_p );
				}
			}
		}
	}

	/* if there is nothing in our list of free edges, the shell is already closed */
	if( NMG_TBL_END( &eu_tbl ) == 0 )
	{
		nmg_tbl( &eu_tbl , TBL_FREE , (long *)NULL );
		return;
	}

	/* put all the existing faces in a list (needed later for "nmg_gluefaces") */
	fu_list = (struct faceuse **)rt_calloc( fu_counter + NMG_TBL_END( &eu_tbl ) - 2 , sizeof( struct faceuse *) , "face use list " );
	fu_counter = 0;
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			fu_list[fu_counter++] = fu;
	}

	/* create a table of vertices */
	(void)nmg_tbl( &vert_tbl , TBL_INIT , (long *)NULL );

	while( NMG_TBL_END( &eu_tbl ) )
	{
		vect_t normal,v1,v2,tmp_norm;
		int give_up_on_face=0;

		/* Create an index into the table that orders the edgeuses into a loop */
		order_tbl( &eu_tbl , &index , NMG_TBL_END( &eu_tbl ) , &loop_size );

		/* Create new faces to close the shell */
		while( loop_size > 3 )
		{
			vect_t inside;			/* vector pointing to left of edge (inside face) */
			struct edgeuse **eu_used;	/* array of edgueses used, for deletion */
			vect_t v1,v2;			/* edge vectors */
			int edges_used;			/* number of edges used in making a face */
			int found_face=0;		/* flag - indicates that a face with the correct normal will be created */
			int start_index,end_index;	/* start and stop index for loop */
			int coplanar;			/* flag - indicates entire loop is coplanar */
			plane_t pl1,pl2;		/* planes for checking coplanarity of loop */
			point_t pt[3];			/* points for calculating planes */

			/* Look for an easy way out, maybe this loop is planar */
			/* first, calculate a plane from the first three non-collinear vertices */
			start_index = 0;
			end_index = start_index + 3;
			
			for( i=start_index ; i<end_index ; i++ )
			{
				eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
				VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
			}
			while( rt_mk_plane_3pts( pl1 , pt[0] , pt[1] , pt[2] , tol ) && end_index<loop_size )
			{
				start_index++;
				end_index++;
				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}
			}
			if( end_index == loop_size )
			{
				/* Could not even make a plane, this is some serious screw-up */
				rt_bomb( "nmg_close_shell: cannot make any planes from loop\n" );
			}

			/* now we have one plane, let's check the others */
			coplanar = 1;
			while( end_index < loop_size && coplanar )
			{
				end_index +=3;
				if( end_index > loop_size )
					end_index = loop_size;
				start_index = end_index - 3;

				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}

				/* if these three points make a plane, is it coplanar with
				 * our first one??? */
				if( !rt_mk_plane_3pts( pl2 , pt[0] , pt[1] , pt[2] , tol ) )
				{
					if( rt_coplanar( pl1 , pl2 , tol ) < 1 )
						coplanar = 0;
				}
			}

			if( coplanar )	/* excellent! - just make one big face */
			{
				/* put vertices in table */
				nmg_tbl( &vert_tbl , TBL_RST , (long *)NULL );
				for( i=0 ; i<loop_size ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					nmg_tbl( &vert_tbl , TBL_INS , (long *)eu->vu_p->v_p );
				}

				/* make face */
				fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , loop_size );

				/* already have face geometry, so don't need to call nmg_fu_planeeqn */
				nmg_face_g( fu , pl1 );

				/* add this face to list for glueing */
				fu_list[fu_counter++] = fu;

				/* now eliminate loop from table */
				eu_used = (struct edgeuse **)rt_calloc( loop_size , sizeof( struct edguse *) , "edges used list" );
				for( i=0 ; i<loop_size ; i++ )
					eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );

				for( i=0 ; i<loop_size ; i++ )
					nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );

				rt_free( (char *)eu_used , "edge used list" );

				/* set some flags to get us back to start of loop */
				found_face = 1;
				give_up_on_face = 1;
			}

			/* OK, so we have to do this one little-by-little */
			start_index = 0;
			while( !found_face )
			{
				vect_t	norm;

				/* refresh the vertex list */
				(void)nmg_tbl( &vert_tbl , TBL_RST , (long *)NULL );

				end_index = start_index + 1;
				if( end_index == loop_size )
					end_index = 0;

				/* Get two edgeuses from the loop */
				eu1 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[start_index] );
				nmg_tbl( &vert_tbl , TBL_INS , (long *)eu1->vu_p->v_p );

				VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );
				VCROSS( inside , normal , v1 );

				eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[end_index] );
				nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->vu_p->v_p );

				edges_used = 2;	
				/* if the edges are collinear, we can't make a face */
				while( rt_3pts_collinear(
					eu1->vu_p->v_p->vg_p->coord,
					eu2->vu_p->v_p->vg_p->coord,
					eu2->eumate_p->vu_p->v_p->vg_p->coord,
					tol ) && edges_used < loop_size )
				{
					/* So, add another edge */
					end_index++;
					if( end_index == loop_size )
						end_index = 0;
					eu1 = eu2;
					eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[end_index]);
					nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->vu_p->v_p );
					edges_used++;
				}

				found_face = 1;
				VSUB2( v2 , eu2->eumate_p->vu_p->v_p->vg_p->coord , eu2->vu_p->v_p->vg_p->coord );
				fu = nmg_find_fu_of_eu( eu1 );
				NMG_GET_FU_NORMAL( norm, fu );
				VCROSS( inside , norm , v1 );
				if( VDOT( inside , v2 ) > 0.0 )
				{
					/* this face normal would be in the wrong direction */
					found_face = 0;

					/* move along the loop by one edge and try again */
					start_index++;
					if( start_index > loop_size-2 )
					{
						/* can't make a face from this loop, so delete it */
						eu_used = (struct edgeuse **)rt_calloc( loop_size , sizeof( struct edguse *) , "edges used list" );
						for( i=0 ; i<loop_size ; i++ )
							eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
						for( i=0 ; i<loop_size ; i++ )
							nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );

						rt_free( (char *)eu_used , "edge used list" );

						give_up_on_face = 1;
						break;
					}
				}
			}

			if( give_up_on_face )
				break;			

			/* add last vertex to table */
			nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->eumate_p->vu_p->v_p );

			/* save list of used edges to be removed later */
			eu_used = (struct edgeuse **)rt_calloc( edges_used , sizeof( struct edguse *) , "edges used list" );
			for( i=0 ; i<edges_used ; i++ )
				eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );

			/* make a face */
			fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , edges_used+1 );
			if( nmg_fu_planeeqn( fu , tol ) )
				rt_log( "Failed planeeq\n" );

			/* add new face to the list of faces */
			fu_list[fu_counter++] = fu;

			/* find the newly created edgeuse */
			found = 0;
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) ==
					NMG_VERTEXUSE_MAGIC )
						continue;
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					if( eu->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , 0 )
					&& eu->eumate_p->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , edges_used) )
					{
						eu_new = eu;
						found = 1;
						break;
					}
					else if( eu->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , edges_used)
					&& eu->eumate_p->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , 0 ) )
					{
						eu_new = eu->eumate_p;
						found = 1;
						break;
					}

				}
				if( found )
					break;
			}

			/* out with the old, in with the new */
			for( i=0 ; i<edges_used ; i++ )
				nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );
			nmg_tbl( &eu_tbl , TBL_INS , (long *)eu_new );

			rt_free( (char *)eu_used , "edge used list" );

			/* re-order loop */
			order_tbl( &eu_tbl , &index , NMG_TBL_END( &eu_tbl ) , &loop_size );
		}

		if( give_up_on_face )
			continue;

		if( loop_size != 3 )
		{
			rt_log( "Error, loop size should be 3\n" );
			nmg_tbl( &eu_tbl , TBL_FREE , (long *)NULL );
			nmg_tbl( &vert_tbl , TBL_FREE , (long *)NULL );
			rt_free( (char *)index , "index" );
			rt_free( (char *)fu_list , "faceuse list " );
			return;
		}

		/* if the last 3 vertices are collinear, then don't make the last face */
		nmg_tbl( &vert_tbl , TBL_RST , (long *)NULL );
		for( i=0 ; i<3 ; i++ )
		{
			eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
			(void)nmg_tbl( &vert_tbl , TBL_INS , (long *)eu->vu_p->v_p);
		}

		if( !rt_3pts_collinear(
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 0 ))->vg_p->coord,
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 1 ))->vg_p->coord,
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 2 ))->vg_p->coord,
			tol ) )
		{
		
			/* Create last face from remaining 3 edges */
			fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , 3 );
			if( nmg_fu_planeeqn( fu , tol ) )
				rt_log( "Failed planeeq\n" );

			/* and add it to the list */
			fu_list[fu_counter++] = fu;

		}

		/* remove the last three edges from the table */
		eu1 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[0] );
		eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[1] );
		eu3 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[2] );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu1 );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu2 );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu3 );
	}

	/* finally, glue it all together */
	nmg_gluefaces( fu_list , fu_counter );

	/* Free up all the memory */
	rt_free( (char *)index , "index" );
	rt_free( (char *)fu_list , "faceuse list " );
	nmg_tbl( &eu_tbl , TBL_FREE , (long *)NULL );
	nmg_tbl( &vert_tbl , TBL_FREE , (long *)NULL );

	/* we may have constructed some coplanar faces */
	nmg_shell_coplanar_face_merge( s , tol , 1 );
	if( nmg_simplify_shell( s ) )
	{
		rt_log( "nmg_close_shell(): Simplified shell is empty" );
		return;
	}
}

/*
 *	N M G _ D U P _ S H E L L
 *
 *	Duplicate a shell and return the new copy. New shell is
 *	in the same region.
 *
 *  The vertex geometry is copied from the source faces into topologically
 *  distinct (new) vertex and vertex_g structs.
 *  They will start out being geometricly coincident, but it is anticipated
 *  that the caller will modify the geometry, e.g. as in an extrude operation.
 *
 *  NOTE: This routine creates a translation table that gives the
 *  correspondence between old and new structures, the caller is responsible
 *  for freeing this memory. Warning - NOT EVERY structure is assigned a
 *  correspondence.
 */
struct shell *
nmg_dup_shell( s , trans_tbl )
struct shell *s;
long ***trans_tbl;
{
	struct model *m;
	struct shell *new_s;
	struct faceuse *fu;
	struct loopuse *lu,*new_lu;
	struct edgeuse *eu;
	struct faceuse *new_fu;
	struct nmg_ptbl faces;

	if( rt_g.NMG_debug & DEBUG_BASIC) 
		rt_log( "nmg_dup_shell( s = x%x , trans_tbl = x%x )\n" , s , trans_tbl );

	NMG_CK_SHELL( s );

	m = nmg_find_model( (long *)s );

	/* create translation table double size to accomodate both copies */
	(*trans_tbl) = (long **)rt_calloc(m->maxindex*2, sizeof(long *),
		"nmg_dup_shell trans_tbl" );

	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	new_s = nmg_msv( s->r_p );
	NMG_INDEX_ASSIGN( (*trans_tbl) , s , (long *)new_s );

	/* copy face uses */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			new_fu = (struct faceuse *)NULL;
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( new_fu )
				{
					new_lu = nmg_dup_loop( lu , &new_fu->l.magic , (*trans_tbl) );
					NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
				}
				else
				{
					new_lu = nmg_dup_loop( lu , &new_s->l.magic , (*trans_tbl) );
					NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
					new_fu = nmg_mf( new_lu );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu , (long *)new_fu );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu->fumate_p , (long *)new_fu->fumate_p );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu->f_p , (long *)new_fu->f_p );
				}
			}
			if (fu->f_p->fg_p)
			{
#if 1
				/* Do it this way if you expect to change the normals */
				plane_t	n;
				NMG_GET_FU_PLANE( n, fu );
				nmg_face_g(new_fu, n);
#else
				/* Do it this way to share fu's geometry struct */
				nmg_jfg( fu, new_fu );
#endif

				/* XXX Perhaps this should be new_fu->f_p->fg_p ? */
				NMG_INDEX_ASSIGN( (*trans_tbl) , fu->f_p->fg_p , (long *)new_fu->f_p->fg_p );
			}
			new_fu->orientation = fu->orientation;
			new_fu->fumate_p->orientation = fu->fumate_p->orientation;
			nmg_tbl( &faces , TBL_INS , (long *)new_fu );
		}
	}

	/* glue new faces */
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

	/* copy wire loops */
	for( RT_LIST_FOR( lu , loopuse , &s->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		new_lu = nmg_dup_loop( lu , &new_s->l.magic , (*trans_tbl) );
		NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
	}

	/* copy wire edges */
	for( RT_LIST_FOR( eu , edgeuse , &s->eu_hd ) )
	{
		struct vertex *old_v1,*old_v2,*new_v1,*new_v2;
		struct edgeuse *new_eu;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		NMG_CK_EDGEUSE( eu->eumate_p );
		NMG_CK_VERTEXUSE( eu->eumate_p->vu_p );
		NMG_CK_VERTEX( eu->eumate_p->vu_p->v_p );

		old_v1 = eu->vu_p->v_p;
		new_v1 = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v1);
		old_v2 = eu->eumate_p->vu_p->v_p;
		new_v2 = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v2);

		/* make the wire edge */
		new_eu = nmg_me( new_v1 , new_v2 , new_s );
		NMG_INDEX_ASSIGN( (*trans_tbl) , eu , (long *)new_eu );

		new_v1 = new_eu->vu_p->v_p;
		NMG_INDEX_ASSIGN( (*trans_tbl) , old_v1 , (long *)new_v1 );
		if( !new_v1->vg_p )
		{
			nmg_vertex_gv( new_v1 , old_v1->vg_p->coord );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v1->vg_p , (long *)new_v1->vg_p );
		}

		new_v2 = new_eu->eumate_p->vu_p->v_p;
		NMG_INDEX_ASSIGN( (*trans_tbl) , old_v2 , (long *)new_v2 );
		if( !new_v2->vg_p )
		{
			nmg_vertex_gv( new_v2 , old_v2->vg_p->coord );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v2->vg_p , (long *)new_v2->vg_p );
		}

	}

#if 0
	/* XXX for this to work nmg_mvu and nmg_mvvu must not be private
	 *     perhaps there is another way???? */
	/* copy vertex use
	 * This must be done last, since other routines may steal it */
	if( s->vu_p )
	{
		old_vu = s->vu_p;
		NMG_CK_VERTEXUSE( old_vu );
		old_v = old_vu->v_p;
		NMG_CK_VERTEX( old_v );
		new_v = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v);
		if( new_v )
		{
			/* already copied vertex, just need a use */
			if( new_s->vu_p )
				(void )nmg_kvu( new_s->vu_p );
			new_s->vu_p = nmg_mvu( new_v , (long *)new_s , m );
		}
		else
		{
			/* make a new vertex and use */
			new_s->vu_p = nmg_mvvu( (long *)new_s , m );
			new_v = new_s->vu_p->v_p;

			/* put entry in table */
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v , (long *)new_v );

			/* assign the same geometry as the old copy */
			nmg_vertex_gv( new_v , old_v->vg_p->coord );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v->vg_p , (long *)new_v->vg_p );
		}
	}
#endif
	
	return( new_s );
}

/*	Routines to use the nmg_ptbl structures as a stack of edgeuse structures */

#define	NMG_PUSH( _ptr , _stack )	nmg_tbl( _stack , TBL_INS , (long *) _ptr );

struct edgeuse
*nmg_pop_eu( stack )
struct nmg_ptbl *stack;
{
	struct edgeuse *eu;

	/* return a NULL if stack is empty */
	if( NMG_TBL_END( stack ) == 0 )
		return( (struct edgeuse *)NULL );

	/* get last edgeuse on the stack */
	eu = (struct edgeuse *)NMG_TBL_GET( stack , NMG_TBL_END( stack )-1 );

	/* remove that edgeuse from the stack */
	nmg_tbl( stack , TBL_RM , (long *)eu );

	return( eu );
}

/*	N M G _ R E V E R S E _ F A C E _ A N D _ R A D I A L S
 *
 *	This routine calls "nmg_reverse_face" and also makes the radial
 *	pointers connect faces of like orientation (i.e., OT_SAME to OT_SAME and
 *	OT_OPPOSITE to OT_OPPOSITE).
 */

void
nmg_reverse_face_and_radials( fu , tol )
struct faceuse *fu;
CONST struct rt_tol *tol;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	struct faceuse *fu2;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_reverse_face_and_radials( fu = x%x )\n" , fu );

	NMG_CK_FACEUSE( fu );
	RT_CK_TOL( tol );

	/* reverse face */
	nmg_reverse_face( fu );

	(void)nmg_face_fix_radial_parity( fu , tol );
}

/*
 *	N M G _ F I N D _ T O P _ F A C E
 *
 *	Finds the topmost face in a shell (in z-direction).
 *	Expects to have a translation table (variable "flags") for
 *	the model, and will ignore face structures that have their
 *	flag set in the table.
 */

struct face *
nmg_find_top_face( s , flags )
struct shell *s;
long *flags;
{
	fastf_t max_z=(-MAX_FASTF);
	fastf_t max_slope=(-MAX_FASTF);
	vect_t edge;
	struct face *f_top=(struct face *)NULL;
	struct edge *e_top=(struct edge *)NULL;
	struct vertex *vp_top=(struct vertex *)NULL;
	struct loopuse *lu;
	struct faceuse *fu;
	struct edgeuse *eu,*eu1,*eu2;
	struct vertexuse *vu;
	int done;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_find_top_face( s = x%x , flags = x%x )\n" , s , flags );

	NMG_CK_SHELL( s );

	/* find vertex with greatest z coordinate */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( NMG_INDEX_TEST( flags , fu->f_p ) )
			continue;
		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC )
			{
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					if( eu->vu_p->v_p->vg_p->coord[Z] > max_z )
					{
						max_z = eu->vu_p->v_p->vg_p->coord[Z];
						vp_top = eu->vu_p->v_p;
					}
				}
			}
		}
	}
	if( vp_top == (struct vertex *)NULL )
	{
		rt_log( "Fix_normals: Could not find uppermost vertex" );
		return( (struct face *)NULL );
	}

	/* find edge from vp_top with largest slope in +z direction */
	for( RT_LIST_FOR( vu , vertexuse , &vp_top->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );
		if( *vu->up.magic_p == NMG_EDGEUSE_MAGIC )
		{
			struct vertexuse *vu1;

			eu = vu->up.eu_p;
			NMG_CK_EDGEUSE( eu );

			/* skip wire edges */
			if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
				continue;

			/* skip wire loops */
			if( *eu->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC )
				continue;

			/* skip finished faces */
			if( NMG_INDEX_TEST( flags , eu->up.lu_p->up.fu_p->f_p ) )
				continue;

			/* skip edges from other shells */
			if( nmg_find_s_of_eu( eu ) != s )
				continue;

			/* get vertex at other end of this edge */
			vu1 = eu->eumate_p->vu_p;
			NMG_CK_VERTEXUSE( vu1 );

			/* make a unit vector in direction of edgeuse */
			VSUB2( edge , vu1->v_p->vg_p->coord , vu->v_p->vg_p->coord );
			VUNITIZE( edge );

			/* check against current maximum slope */
			if( edge[Z] > max_slope )
			{
				max_slope = edge[Z];
				e_top = eu->e_p;
			}
		}
	}
	if( e_top == (struct edge *)NULL )
	{
		rt_log( "Fix_normals: Could not find uppermost edge" );
		return( (struct face *)NULL );
	}

	/* now find the face containing e_top with "left-pointing vector" having the greatest slope */
	max_slope = (-MAX_FASTF);
	eu = e_top->eu_p;
	eu1 = eu;
	done = 0;
	while( !done )
	{
		/* don't bother with anything but faces */
		if( *eu1->up.magic_p == NMG_LOOPUSE_MAGIC )
		{
			lu = eu1->up.lu_p;
			NMG_CK_LOOPUSE( lu );
			if( *lu->up.magic_p == NMG_FACEUSE_MAGIC && lu->orientation == OT_SAME )
			{
				vect_t left;
				vect_t edge_dir;
				vect_t normal;

				/* fu is a faceuse containing "eu1" */
				fu = lu->up.fu_p;
				NMG_CK_FACEUSE( fu );

				/* skip faces from other shells */
				if( fu->s_p != s )
				{
					/* go on to next radial face */
					eu1 = eu1->eumate_p->radial_p;

					/* check if we are back where we started */
					if( eu1 == eu )
						done = 1;

					continue;
				}

				/* make a vector in the direction of "eu1" */
				VSUB2( edge_dir , eu1->vu_p->v_p->vg_p->coord , eu1->eumate_p->vu_p->v_p->vg_p->coord );

				/* find the normal for this faceuse */
				NMG_GET_FU_NORMAL( normal, fu );

				/* edge direction cross normal gives vetor in face */
				VCROSS( left , edge_dir , normal );

				/* unitize to get slope */
				VUNITIZE( left );

				/* check against current max slope */
				if( left[Z] > max_slope )
				{
					max_slope = left[Z];
					f_top = fu->f_p;
				}
			}
		}
		/* go on to next radial face */
		eu1 = eu1->eumate_p->radial_p;

		/* check if we are back where we started */
		if( eu1 == eu )
			done = 1;
	}

	if( f_top == (struct face *)NULL )
	{
		rt_log( "Fix_normals: Could not find uppermost face" );
		return( (struct face *)NULL );
	}

	return( f_top );
}

/*	N M G _ S H E L L _ I S _ V O I D
 *
 * determines if the shell is a void shell or an exterior shell
 * by finding the topmost face (in the z-direction) and looking at
 * the Z component of the OT_SAME faceuse. Positive is an exterior
 * shell, negative is a void shell.
 *
 * returns:
 *	0 - shell is exterior shell
 *	1 - shell is a void shell
 *
 * It is expected that this shell is the result of nmg_decompose_shells.
 */
int
nmg_shell_is_void( s )
CONST struct shell *s;
{
	struct model *m;
	struct face *f;
	struct faceuse *fu;
	vect_t normal;
	long *flags;

	NMG_CK_SHELL( s );

	m = nmg_find_model( &s->l.magic );
	NMG_CK_MODEL( m );

	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_shell_is_void: flags " );

	f = nmg_find_top_face( s , flags );

	rt_free( (char *)flags , "nmg_shell_is_void: flags" );

	NMG_CK_FACE( f );
	NMG_CK_FACE_G( f->fg_p );
	fu = f->fu_p;
	NMG_CK_FACEUSE( fu );

	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
		rt_bomb( "nmg_shell_is_void: Neither faceuse nor mate have OT_SAME orient\n" );

	NMG_GET_FU_NORMAL( normal , fu );

	if( normal[Z] == 0.0 )
		rt_bomb( "nmg_shell_is_void: Cannot determine if shell is void\n" );
	else if( normal[Z] < 0.0 )
		return( 1 );
	else
		return( 0 );
}

/*	N M G _ P R O P A G A T E _ N O R M A L S
 *
 *	This routine expects "fu_in" to have a correctly oriented normal.
 *	It then checks all faceuses in the same shell it can reach via radial structures, and
 *	reverses faces and modifies radial structures as needed to result in
 *	a consistent NMG shell structure. The "flags" variable is a translation table
 *	for the model, and as each face is checked, its flag is set. Faces with flags
 *	that have already been set will not be checked by this routine.
 */

void
nmg_propagate_normals( fu_in , flags , tol )
struct faceuse *fu_in;
long *flags;
CONST struct rt_tol *tol;
{
	struct nmg_ptbl stack;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1;
	struct faceuse *fu;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_propagate_normals( fu_in = x%x , flags = x%x )\n" , fu_in , flags );

	NMG_CK_FACEUSE( fu_in );
	RT_CK_TOL( tol );

	fu = fu_in;
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
	{
		rt_log( "nmg_propagate_normals: Could not find OT_SAME orientation of faceuse x%x\n" , fu_in );
		return;
	}

	/* set flag for this face since we know this one is OK */
	NMG_INDEX_SET( flags , fu->f_p );

	/* Use the ptbl structure as a stack */
	nmg_tbl( &stack , TBL_INIT , (long *)NULL );

	/* push all edgeuses of "fu" onto the stack */
	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;
		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* don't push free edges on the stack */
			if( eu->radial_p->eumate_p != eu )
				NMG_PUSH( eu , &stack );
		}
	}

	/* now pop edgeuses from stack, go to radial face, fix its normal,
	 * and push its edgeuses onto the stack */

	while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
	{
		/* eu1 is an edgeuse on an OT_SAME face, so its radial
		 * should be in an OT_SAME also */

		NMG_CK_EDGEUSE( eu1 );

		/* go to the radial */
		eu = eu1->radial_p;
		NMG_CK_EDGEUSE( eu );

		/* make sure we are still on the same shell */
		while( nmg_find_s_of_eu( eu ) != fu_in->s_p && eu != eu1 && eu != eu1->eumate_p )
			eu = eu->eumate_p->radial_p;

		if( eu == eu1 || eu == eu1->eumate_p )
			continue;

		/* find the face that contains this edgeuse */
		if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
			continue;

		lu = eu->up.lu_p;
		NMG_CK_LOOPUSE( lu );

		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
			continue;

		fu = lu->up.fu_p;
		NMG_CK_FACEUSE( fu );

		/* if this face has already been processed, skip it */
		if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p ) )
		{
			/* if orientation is wrong, or if the radial edges are in the same direction
			 * then reverse the face and fix the radials */
			if( fu->orientation != OT_SAME ||
				( eu1->vu_p->v_p == eu->vu_p->v_p &&
				 eu1->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p ))
			{
				nmg_reverse_face_and_radials( fu , tol );
			}

			/* make sure we are dealing with an OT_SAME faceuse */
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;

			/* push all edgeuses of "fu" onto the stack */
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					/* don't push free edges on the stack */
					if( eu->radial_p->eumate_p != eu )
						NMG_PUSH( eu , &stack );
				}
			}
		}
	}

	/* free the stack */
	nmg_tbl( &stack , TBL_FREE , (long *)NULL );
}

/*	N M G _ F I X _ N O R M A L S
 *
 *	Finds the topmost face in the shell, assumes its normal must have
 *	a positive z-component, and makes it so if not.  Then propagates this
 *	orientation though the radial structures.  Disjoint shells are handled
 *	by flagging checked faces, and calling "nmg_find_top_face" again.
 *
 *	XXX - known bug: shells that are intended to describe void spaces (i.e., should
 *			 have inward pointing normals), will end up with outward pointing
 *			 normals.
 *
 */

void
nmg_fix_normals( s , tol )
struct shell *s;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct face *f_top;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1,*eu2;
	struct vertexuse *vu;
	long *flags;
	int missed_faces=1;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_fix_normals( s = x%x )\n" , s );

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	/* Make an index table to insure we visit each face once and only once */
	m = s->r_p->m_p;
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_fix_normals: flags" );

	/* loop to catch disjoint shells */
	while( missed_faces )
	{
		FAST fastf_t	z;

		/* find the top face */
		f_top = nmg_find_top_face( s , flags );	
		NMG_CK_FACE( f_top );
		if( f_top->flip )
			z = - f_top->fg_p->N[Z];
		else
			z =   f_top->fg_p->N[Z];

		/* f_top is the topmost face (in the +z direction), so its OT_SAME use should have a
		 * normal with a positive z component */
		if( z < 0.0 )
			nmg_reverse_face_and_radials( f_top->fu_p , tol );

		/* get OT_SAME use of top face */
		fu = f_top->fu_p;
		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		NMG_CK_FACEUSE( fu );

		/* fu is now known to be a correctly oriented faceuse,
		 * propagate this throughout the shell, face by face, by
		 * traversing the shell using the radial edge structure */

		nmg_propagate_normals( fu , flags , tol );

		/* check if all the faces have been processed */
		missed_faces = 0;
		for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );
			if( fu->orientation == OT_SAME )
			{
				if( !NMG_INDEX_TEST( flags , fu->f_p ) )
					missed_faces++;
			}
		}
	}

	/* free some memory */
	rt_free( (char *)flags , "nmg_fix_normals: flags" );
}

/*	N M G _ B R E A K _ L O N G _ E D G E S
 *
 *	This codes looks for situations as illustrated:
 *
 *    *------->*-------->*--------->*
 *    *<----------------------------*
 *
 *	where one long edgeuse (the bottom one above) and two or more
 *	shorter edgeusess (the tops ones) are collinear and have the same
 *	start and end vertices.  The code breaks the longer edgeuse into
 *	ones that can be radials of the shorter ones.
 *	Returns the number of splits performed.
 */

int
nmg_break_long_edges( s , tol )
struct shell *s;
struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int split_count=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_break_long_edges( s = x%x )\n" , s );

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	/* look at every edgeuse in the shell */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );

			/* skip loops of a single vertex */
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct vertexuse *vu;

				NMG_CK_EDGEUSE( eu );

				/* look for an edgeuse that terminates on this vertex */
				for( RT_LIST_FOR( vu , vertexuse , &eu->vu_p->v_p->vu_hd ) )
				{
					struct edgeuse *eu1;

					NMG_CK_VERTEXUSE( vu );

					/* skip vertexuses that are not part of an edge */
					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu1 = vu->up.eu_p;

					/* don't consider the edge we already found!!! */
					if( eu1 == eu )
						continue;

					/* if it terminates at the same vertex as eu, skip it */
					if( eu1->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p )
						continue;

					/* get the mate (so it terminates at "vu") */
					eu1 = eu1->eumate_p;

					/* make sure it is collinear with "eu" */
					if( rt_3pts_collinear( eu->vu_p->v_p->vg_p->coord ,
						eu->eumate_p->vu_p->v_p->vg_p->coord ,
						eu1->vu_p->v_p->vg_p->coord , tol ) )
					{
						/* make sure we break the longer edge
						 * and that the edges are in opposite directions */
						vect_t v0,v1;
						struct edgeuse *tmp_eu;

						VSUB2( v0 , eu->eumate_p->vu_p->v_p->vg_p->coord , eu->vu_p->v_p->vg_p->coord );
						VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );

						if (MAGSQ( v0 ) > MAGSQ( v1 ) && VDOT( v0 , v1 ) < 0.0 )
						{
							if( rt_g.NMG_debug & DEBUG_BASIC )
							{
								rt_log( "Breaking edge from ( %f %f %f ) to ( %f %f %f ) at ( %f %f %f )\n",
									V3ARGS( eu->vu_p->v_p->vg_p->coord ),
									V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ),
									V3ARGS( eu1->vu_p->v_p->vg_p->coord ) );
							}
							tmp_eu = nmg_esplit(eu1->vu_p->v_p, eu);
							split_count++;
						}
						else if( rt_g.NMG_debug & DEBUG_BASIC )
						{
							rt_log( "Not splitting collinear edges x%x and x%x:\n", eu , eu1 );
							rt_log( "\t( %f %f %f ) -> ( %f %f %f )\n",
								V3ARGS( eu->vu_p->v_p->vg_p->coord ),
								V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
							rt_log( "\t( %f %f %f ) -> ( %f %f %f )\n",
								V3ARGS( eu1->vu_p->v_p->vg_p->coord ),
								V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) );
						}
					}
				}
			}
		}
	}
	return( split_count );
}

/*	N M G _ M K _ N E W _ F A C E _ F R O M _ L O O P
 *
 *  Remove a loopuse from an existing face and construct a new face
 *  from that loop
 *
 *  Returns new faceuse as built by nmg_mf()
 *
 */
struct faceuse *
nmg_mk_new_face_from_loop( lu )
struct loopuse *lu;
{
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu1;
	struct loopuse *lu_mate;
	int ot_same_loops=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_mk_new_face_from_loop( lu = x%x )\n" , lu );

	NMG_CK_LOOPUSE( lu );

	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
	{
		rt_log( "nmg_mk_new_face_from_loop: loopuse is not in a faceuse\n" );
		return( (struct faceuse *)NULL );
	}

	fu = lu->up.fu_p;
	NMG_CK_FACEUSE( fu );

	s = fu->s_p;
	NMG_CK_SHELL( s );

	/* Count the number of exterior loops in this faceuse */
	for( RT_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu1 );
		if( lu1->orientation == OT_SAME )
			ot_same_loops++;
	}

	if( ot_same_loops == 1 && lu->orientation == OT_SAME )
	{
		rt_log( "nmg_mk_new_face_from_loop: cannot remove only exterior loop from faceuse\n" );
		return( (struct faceuse *)NULL );
	}

	lu_mate = lu->lumate_p;

	/* remove loopuse from faceuse */
	RT_LIST_DEQUEUE( &lu->l );

	/* remove its mate from faceuse mate */
	RT_LIST_DEQUEUE( &lu_mate->l );

	/* insert these loops in the shells list of wire loops
	 * put the original loopuse at the head of the list
	 * so that it will end up as the returned faceuse from "nmg_mf"
	 */
	RT_LIST_INSERT( &s->lu_hd , &lu_mate->l );
	RT_LIST_INSERT( &s->lu_hd , &lu->l );

	/* set the "up" pointers to the shell */
	lu->up.s_p = s;
	lu_mate->up.s_p = s;

	/* Now make the new face */
	return( nmg_mf( lu ) );
}

/* state for nmg_split_loops_into_faces */
struct nmg_split_loops_state
{
	long		*flags;		/* index based array of flags for model */
	struct rt_tol	*tol;
	int		split;		/* count of faces split */
};
/* state for nmg_unbreak_edge */

void
nmg_split_loops_handler( fu_p , sl_state , after )
long *fu_p;
genptr_t sl_state;
int after;
{
	struct faceuse *fu;
	struct nmg_split_loops_state *state;
	struct loopuse *lu;
	struct rt_tol *tol;
	int otsame_loops=0;
	int otopp_loops=0;

	fu = (struct faceuse *)fu_p;
	NMG_CK_FACEUSE( fu );

	state = (struct nmg_split_loops_state *)sl_state;
	tol = state->tol;

	if( fu->orientation != OT_SAME )
		return;

	if( !NMG_INDEX_TEST_AND_SET( state->flags , fu ) )  return;

	NMG_INDEX_SET( state->flags , fu->fumate_p );

	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );

		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		if( lu->orientation == OT_SAME )
			otsame_loops++;
		else if( lu->orientation == OT_OPPOSITE )
			otopp_loops++;
		else
		{
			rt_log( "nmg_split_loops_into_faces: facuse (x%x) with %s loopuse (x%x)\n",
				fu , nmg_orientation( lu->orientation ) , lu );
			return;
		}
	}

	/* if there is only one OT_SAME loop in this faceuse, nothing to do */
	if( otsame_loops == 1 )
		return;

	if( otopp_loops && otsame_loops )
	{
		struct nmg_ptbl inside_loops;

		nmg_tbl( &inside_loops , TBL_INIT , (long *)NULL );

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct faceuse *new_fu;
			struct loopuse *lu_next;
			struct loopuse *lu1;
			plane_t plane;
			int index;

			lu_next = RT_LIST_PNEXT( loopuse , &lu->l );

			if( otsame_loops == 1 )
			{
				/* done */
				nmg_tbl( &inside_loops , TBL_FREE , (long *)NULL );
				return;
			}
			if( lu->orientation != OT_SAME )
			{
				lu = lu_next;
				continue;
			}

			/* find OT_OPPOSITE loops for this exterior loop */
			for( RT_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
			{
				struct loopuse *lu2;
				int hole_in_lu=1;

				/* skip loops that are not OT_OPPOSITE */
				if( lu1->orientation != OT_OPPOSITE )
					continue;

				/* skip loops that are not within lu */
				if( nmg_classify_lu_lu( lu1 , lu , tol ) != NMG_CLASS_AinB )
					continue;

				/* lu1 is an OT_OPPOSITE loopuse within the OT_SAME lu
				 * but lu1 may be within yet another loopuse that is
				 * also within lu (nested loops)
				 */

				/* look for another OT_SAME loopuse within lu */
				for( RT_LIST_FOR( lu2 , loopuse , &fu->lu_hd ) )
				{
					if( lu2->orientation != OT_SAME )
						continue;

					if( nmg_classify_lu_lu( lu2 , lu , tol ) == NMG_CLASS_AinB )
					{
						/* lu2 is within lu, does it contain lu1?? */
						if( nmg_classify_lu_lu( lu1 , lu2 , tol ) == NMG_CLASS_AinB )
						{
							/* Yes, lu1 is within lu2, so lu1 is not
							 * a hole in lu
							 */
							hole_in_lu = 0;;
							break;
						}
					}
				}

				if( hole_in_lu )
					nmg_tbl( &inside_loops , TBL_INS , (long *)lu1 );
			}

			NMG_GET_FU_PLANE( plane , fu );

			new_fu = nmg_mk_new_face_from_loop( lu );
			nmg_face_g( new_fu , plane );
			for( index=0 ; index<NMG_TBL_END( &inside_loops ) ; index++ )
			{
				lu1 = (struct loopuse *)NMG_TBL_GET( &inside_loops , index );
				nmg_move_lu_between_fus( new_fu , fu , lu1 );
			}
			nmg_tbl( &inside_loops , TBL_RST , (long *)NULL );
			otsame_loops--;
		}
		nmg_tbl( &inside_loops , TBL_FREE , (long *)NULL );
	}
	else if( otsame_loops )
	{
		/* all loops are of the same orientation, just make a face for every loop */
		int first=1;
		struct faceuse *new_fu;

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *next_lu;

			next_lu = RT_LIST_PNEXT( loopuse , &lu->l );

			if( first )
				first = 0;
			else
			{
				plane_t plane;

				if( lu->orientation == OT_SAME )
				{
					NMG_GET_FU_PLANE( plane , fu );
				}
				else
				{
					NMG_GET_FU_PLANE( plane , fu->fumate_p );
				}
				new_fu = nmg_mk_new_face_from_loop( lu );
				nmg_face_g( new_fu , plane );
			}

			lu = next_lu;
		}
	}
	else
	{
		/* faceuse has only OT_OPPOSITE loopuses */
		rt_log( "nmg_split_loops_into_faces: fu (x%x) has only OT_OPPOSITE loopuses, ignored\n" , fu );
	}
}

/*	N M G _ S P L I T _ L O O P S _ I N T O _ F A C E S
 *
 *	Visits each faceuse and splits disjoint loops into
 *	seperate faces.
 *
 *	Returns the number of faces modified.
 */
int
nmg_split_loops_into_faces( magic_p , tol )
long		*magic_p;
struct rt_tol	*tol;
{
	struct model *m;
	struct nmg_visit_handlers htab;
	struct nmg_split_loops_state sl_state;
	long *flags;
	int count;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_split_loops_into_faces( magic_p = x%x )\n" , magic_p );

	RT_CK_TOL( tol );

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL( m );

	RT_CK_TOL( tol );

	htab = nmg_visit_handlers_null;		/* struct copy */
	htab.aft_faceuse = nmg_split_loops_handler;

	sl_state.split = 0;
	sl_state.flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_split_loops_into_faces: flags" );
	sl_state.tol = tol;

	nmg_visit( magic_p , &htab , (genptr_t *)&sl_state );

	count = sl_state.split;

	rt_free( (char *)sl_state.flags , "nmg_split_loops_into_faces: flags" );

	return( count );
}

/*	N M G _ D E C O M P O S E _ S H E L L
 *
 *	Accepts one shell and breaks it to the minimum number
 *	of disjoint shells.
 *
 *	explicit returns:
 *		# of resulting shells ( 1 indicates that nothing was done )
 *
 *	implicit returns:
 *		additional shells in the passed in shell's region.
 */
int
nmg_decompose_shell( s , tol )
struct shell *s;
struct rt_tol *tol;
{
	int missed_faces;
	int no_of_shells=1;
	int shell_no=1;
	int i,j;
	struct model *m;
	struct nmgregion *r;
	struct shell *new_s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *eu1;
	struct faceuse *missed_fu;
	struct nmg_ptbl stack;
	struct nmg_ptbl shared_edges;
	long *flags;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_decompose_shell( s = x%x ) START\n" , s );

	NMG_CK_SHELL( s );

	RT_CK_TOL( tol );

	(void)nmg_split_loops_into_faces( &s->l.magic , tol );

	/* Make an index table to insure we visit each face once and only once */
	r = s->r_p;
	NMG_CK_REGION( r );
	m = r->m_p;
	NMG_CK_MODEL( m );
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_decompose_shell: flags" );

	nmg_tbl( &stack , TBL_INIT , (long *)NULL );
	nmg_tbl( &shared_edges , TBL_INIT , (long *)NULL );

	/* get first faceuse from shell */
	fu = RT_LIST_FIRST( faceuse , &s->fu_hd );
	NMG_CK_FACEUSE( fu );
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
		rt_bomb( "First face in shell has no OT_SAME uses!!!!\n" );

	/* put all edguses of first faceuse on the stack */
	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )	
			continue;

		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* build two lists, one of winged edges, the other not */
			if( nmg_radial_face_count( eu , s ) > 2 )
				nmg_tbl( &shared_edges , TBL_INS_UNIQUE , (long *)eu );
			else
				nmg_tbl( &stack , TBL_INS_UNIQUE , (long *)eu );
		}
	}

	/* Mark first faceuse and mate with shell number */
	NMG_INDEX_ASSIGN( flags , fu , shell_no );
	NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

	/* now pop edgeuse of the stack and visit faces radial to edgeuse */
	while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
	{
		NMG_CK_EDGEUSE( eu1 );

		/* move to the radial */
		eu = eu1->radial_p;
		NMG_CK_EDGEUSE( eu );

		/* make sure we stay within the intended shell */
		while( nmg_find_s_of_eu( eu ) != s && eu != eu1 && eu != eu1->eumate_p )
			eu = eu->eumate_p->radial_p;

		if( eu == eu1 || eu == eu1->eumate_p )
			continue;

		fu = nmg_find_fu_of_eu( eu );
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		/* if this faceuse has already been visited, skip it */
		if( !NMG_INDEX_TEST( flags , fu ) )
		{
                        /* push all "winged" edgeuses of "fu" onto the stack */
                        for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
                        {
                                NMG_CK_LOOPUSE( lu );

                                if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
                                        continue;

                                for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
                                {
					/* build two lists, one of winged edges, the other not */
					if( nmg_radial_face_count( eu , s ) > 2 )
						nmg_tbl( &shared_edges , TBL_INS_UNIQUE , (long *)eu );
					else
						nmg_tbl( &stack , TBL_INS_UNIQUE , (long *)eu );
                                }
                        }
			/* Mark this faceuse and its mate with a shell number */
			NMG_INDEX_ASSIGN( flags , fu , shell_no );
			NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );
		}
	}

	/* count number of faces that were not visited */
	missed_faces = 0;
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			if( !NMG_INDEX_TEST( flags , fu ) )
			{
				missed_faces++;
				missed_fu = fu;
			}
		}
	}

	if( !missed_faces )	/* nothing to do, just one shell */
	{
		rt_free( (char *)flags , "nmg_decompose_shell: flags " );
		nmg_tbl( &stack , TBL_FREE , (long *)NULL );
		nmg_tbl( &shared_edges , TBL_FREE , (long *)NULL );
		return( no_of_shells );
	}

	while( missed_faces )
	{
		struct edgeuse *unassigned_eu;
		int *shells_at_edge;
		int new_shell_no=0;

		nmg_tbl( &stack , TBL_RST , (long *)NULL );

		/* Look at the list of shared edges to see if anything can be deduced */
		shells_at_edge = (int *)rt_calloc( no_of_shells+1 , sizeof( int ) , "nmg_decompose_shell: shells_at_edge" );

		for( i=0 ; i<NMG_TBL_END( &shared_edges ) ; i++ )
		{
			int faces_at_edge=0;
			int k;

			/* Construct a list of shells for this edge.
			 * shells_at_edge[i] is the number of edgeuses of this
			 * edge that have been assigned to shell number i.
			 * shells_at_edge[0] si the number of uses of this edge
			 * that have not been asigned to any shell yet
			 */
			for( k=0 ; k<=no_of_shells ; k++ )
				shells_at_edge[k] = 0;

			unassigned_eu = NULL;

			eu = (struct edgeuse *)NMG_TBL_GET( &shared_edges , i );
			NMG_CK_EDGEUSE( eu );

			eu1 = eu;
			do
			{
				struct faceuse *fu_of_eu;

				fu_of_eu = nmg_find_fu_of_eu( eu1 );

				faces_at_edge++;
				if( !NMG_INDEX_TEST( flags , fu_of_eu ) )
					unassigned_eu = eu1;
				shells_at_edge[ NMG_INDEX_GET( flags , fu_of_eu ) ]++;

				eu1 = nmg_next_radial_eu( eu1 , s , 0 );
			}
			while( eu1 != eu );

			if( shells_at_edge[0] == 1 && unassigned_eu )
			{
				/* Only one edgeuse at this edge is unassigned, should be
				 * able to determine which shell it belongs in */

				for( j=1 ; j<=no_of_shells ; j++ )
				{
					if( shells_at_edge[j] == 1 )
					{
						/* unassigned edgeuse should belong to shell j */
						new_shell_no = j;
						break;
					}
				}
			}
			else if( shells_at_edge[0] == 0 )
			{
				/* all uses of this edge have been assigned
				 * it can be deleted from the table
				 */
				nmg_tbl( &shared_edges , TBL_RM , (long *)eu );
			}
			if( new_shell_no )
				break;
		}

		if( !new_shell_no )
		{
			/* Couldn't find a definite edgeuse to start a new shell
			 * so use radial parity to pick an edgeuse that should not be
			 * part of the same shell as ones already assigned
			 */
			for( i=0 ; i<NMG_TBL_END( &shared_edges ) ; i++ )
			{
				struct faceuse *fu_of_eu1;
				int found_missed_face=0;

				eu = (struct edgeuse *)NMG_TBL_GET( &shared_edges , i );
				NMG_CK_EDGEUSE( eu );

				eu1 = eu;
				do
				{
					/* look for unassigned edgeuses */
					fu_of_eu1 = nmg_find_fu_of_eu( eu1 );
					NMG_CK_FACEUSE( fu_of_eu1 );
					if( !NMG_INDEX_TEST( flags , fu_of_eu1 ) )
					{
						struct edgeuse *eu2;
						struct faceuse *fu_of_eu2;

						/* look for a neighboring edgeuse that
						 * has been assigned
						 */
						eu2 = nmg_prev_radial_eu( eu1 , s , 0 );
						fu_of_eu2 = nmg_find_fu_of_eu( eu2 );
						NMG_CK_FACEUSE( fu_of_eu2 );
						if( NMG_INDEX_TEST( flags , fu_of_eu2 ) )
						{
							/* eu2 has been assigned
							 * compare orientation parity
							 */
							if( fu_of_eu2->orientation ==
								fu_of_eu1->orientation )
							{
								/* These should not be in the same
								 * shell, so start a new shell
								 * at this faceuse
								 */
								missed_fu = fu_of_eu1;
								found_missed_face = 1;
							}
						}
						if( found_missed_face )
							break;

						eu2 = nmg_next_radial_eu( eu1 , s , 0 );
						fu_of_eu2 = nmg_find_fu_of_eu( eu2 );
						NMG_CK_FACEUSE( fu_of_eu2 );
						if( NMG_INDEX_TEST( flags , fu_of_eu2 ) )
						{
							/* eu2 has been assigned
							 * compare orientation parity
							 */
							if( fu_of_eu2->orientation ==
								fu_of_eu1->orientation )
							{
								/* These should not be in the same
								 * shell, so start a new shell
								 * at this faceuse
								 */
								missed_fu = fu_of_eu1;
								found_missed_face = 1;
							}
						}

					}
					if( found_missed_face )
						break;
					eu1 = nmg_next_radial_eu( eu1 , s , 0 );
				}
				while( eu1 != eu );

				if( found_missed_face )
					break;
			}
		}

		rt_free( (char *)shells_at_edge , "nmg_decompose_shell: shells_at_edge" );

		/* make a new shell number */
		if( new_shell_no )
		{
			shell_no = new_shell_no;
			fu = nmg_find_fu_of_eu( unassigned_eu );
		}
		else
		{
			shell_no = (++no_of_shells);
			NMG_CK_FACEUSE( missed_fu );
			fu = missed_fu;
		}

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		if( !NMG_INDEX_TEST( flags , fu ) )
		{
			/* move this missed face to the new shell */

                        /* push all edgeuses of "fu" onto the stack */
                        for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
                        {
                                NMG_CK_LOOPUSE( lu );

                                if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
                                        continue;

                                for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
                                {
					/* build two lists, one of winged edges, the other not */
					if( nmg_radial_face_count( eu , s ) > 2 )
						nmg_tbl( &shared_edges , TBL_INS_UNIQUE , (long *)eu );
					else
						nmg_tbl( &stack , TBL_INS_UNIQUE , (long *)eu );
                                }
                        }

			/* Mark this faceuse with a shell number */
			NMG_INDEX_ASSIGN( flags , fu , shell_no );
			NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

		}
		else
			rt_bomb( "nmg_decompose_shell: Missed face wasn't missed???\n" );

		/* now pop edgeuse of the stack and visit faces radial to edgeuse */
		while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
		{
			NMG_CK_EDGEUSE( eu1 );

			/* move to the radial */
			eu = eu1->radial_p;
			NMG_CK_EDGEUSE( eu );

			/* stay within the original shell */
			while( nmg_find_s_of_eu( eu ) != s && eu != eu1 && eu != eu1->eumate_p )
				eu = eu->eumate_p->radial_p;

			if( eu == eu1 || eu == eu1->eumate_p )
				continue;

			fu = nmg_find_fu_of_eu( eu );
			NMG_CK_FACEUSE( fu );

			/* if this face has already been visited, skip it */
			if( !NMG_INDEX_TEST( flags , fu ) )
			{
	                        /* push all edgeuses of "fu" onto the stack */
	                        for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	                        {
	                                NMG_CK_LOOPUSE( lu );
	                                if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
	                                        continue;
	                                for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	                                {
						/* build two lists, one of winged edges, the other not */
						if( nmg_radial_face_count( eu , s ) > 2 )
							nmg_tbl( &shared_edges , TBL_INS_UNIQUE , (long *)eu );
						else
							nmg_tbl( &stack , TBL_INS_UNIQUE , (long *)eu );
	                                }
	                        }

				/* Mark this faceuse with a shell number */
				NMG_INDEX_ASSIGN( flags , fu , shell_no );
				NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

			}
		}

		/* count number of faces that were not visited */
		missed_faces = 0;
		for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );
			if( fu->orientation == OT_SAME )
			{
				if( !NMG_INDEX_TEST( flags , fu ) )
				{
					missed_faces++;
					missed_fu = fu;
				}
			}
		}
	}

	/* Now build the new shells */
	for( shell_no=2 ; shell_no<=no_of_shells ; shell_no++ )
	{

		/* Make a shell */
		new_s = nmg_msv( r );

		/* Move faces marked with this shell_no to this shell */
		fu = RT_LIST_FIRST( faceuse , &s->fu_hd );
		while( RT_LIST_NOT_HEAD( fu , &s->fu_hd ) )
		{
			struct faceuse *next_fu;

			next_fu = RT_LIST_NEXT( faceuse , &fu->l );
			while( RT_LIST_NOT_HEAD( next_fu , &s->fu_hd ) && next_fu->orientation != OT_SAME )
				next_fu = RT_LIST_NEXT( faceuse , &next_fu->l );

			if( fu->orientation != OT_SAME )
			{
				fu = next_fu;
				continue;
			}

			if( NMG_INDEX_GET( flags , fu ) == shell_no )
				nmg_mv_fu_between_shells( new_s , s , fu );

			fu = next_fu;
		}
		if( nmg_kvu( new_s->vu_p ) )
			rt_bomb( "nmg_decompose_shell: killed shell vertex, emptied shell!!\n" );
	}
	rt_free( (char *)flags , "nmg_decompose_shell: flags " );
	nmg_tbl( &stack , TBL_FREE , (long *)NULL );
	nmg_tbl( &shared_edges , TBL_FREE , (long *)NULL );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_decompose_shell END (%d shells)\n" , no_of_shells );

	return( no_of_shells );
}

/*
 *			N M G _ S T A S H _ M O D E L _ T O _ F I L E
 *
 *  Store an NMG model as a separate .g file, for later examination.
 *  Don't free the model, as the caller may still have uses for it.
 */
void
nmg_stash_model_to_file( filename, m, title )
CONST char		*filename;
CONST struct model	*m;
CONST char		*title;
{
	FILE	*fp;
	struct rt_external	ext;
	struct rt_db_internal	intern;
	union record		rec;

	rt_log("nmg_stash_model_to_file('%s', x%x, %s)\n", filename, m, title);

	NMG_CK_MODEL(m);

	if( (fp = fopen(filename, "w")) == NULL )  {
		perror(filename);
		return;
	}

	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_type = ID_NMG;
	intern.idb_ptr = (genptr_t)m;
	RT_INIT_EXTERNAL( &ext );

	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[ID_NMG].ft_export( &ext, &intern, 1.0 ) < 0 )  {
		rt_log("nmg_stash_model_to_file: solid export failure\n");
		db_free_external( &ext );
		rt_bomb("nmg_stash_model_to_file() ft_export() error\n");
	}
	NAMEMOVE( "error", ((union record *)ext.ext_buf)->s.s_name );

	bzero( (char *)&rec, sizeof(rec) );
	rec.u_id = ID_IDENT;
	strcpy( rec.i.i_version, ID_VERSION );
	strncpy( rec.i.i_title, title, sizeof(rec.i.i_title)-1 );
	fwrite( (char *)&rec, sizeof(rec), 1, fp );
	fwrite( ext.ext_buf, ext.ext_nbytes, 1, fp );
	fclose(fp);
	db_free_external( &ext );
	rt_log("nmg_stash_model_to_file(): wrote '%s' in %d bytes\n", filename, ext.ext_nbytes);
}

/* state for nmg_unbreak_edge */
struct nmg_unbreak_state
{
	long		*flags;		/* index based array of flags for model */
	int		unbroken;	/* count of edges mended */
};

/*
 *			N M G _ U N B R E A K _ H A N D L E R
 *
 *	edge visit routine for nmg_unbreak_region_edges.
 *
 *	checks if edge "e" is a candidate to unbroken,
 *	i.e., if it was brokem earlier by nmg_ebreak and
 *	now may be mended.
 *	looks for two consectutive edgeuses sharing the same
 *	edge geometry. Checks that the middle vertex has no
 *	other uses, and,  if so, kills the second edge.
 *	Also moves the vu of the first edgeuse mate to the vu
 *	of the killed edgeuse mate.
 */
void
nmg_unbreak_handler( ep , state , after )
long	*ep;
genptr_t state;
int	after;
{
	struct edgeuse *eu1,*eu2;
	struct edge *e;
	struct edge_g *eg;
	struct nmg_unbreak_state *ub_state;
	struct vertex	*vb;
	struct vertexuse *vu;

	e = (struct edge *)ep;
	NMG_CK_EDGE( e );

	ub_state = (struct nmg_unbreak_state *)state;

	/* make sure we only visit this edge once */
	if( !NMG_INDEX_TEST_AND_SET( ub_state->flags , e ) )  return;

	eg = e->eg_p;
	if( !eg )  {
		rt_log( "nmg_unbreak_handler: no geomtry for edge x%x\n" , e );
		return;
	}


	/* if the edge geometry doesn't have at least two uses, this
	 * is not a candidate for unbreaking */		
	if( eg->usage < 2 )  {
		/* rt_log("nmg_unbreak_handler: usage < 2\n"); */
		return;
	}

	/* Check for two consecutive uses, by looking forward. */
	eu1 = e->eu_p;
	NMG_CK_EDGEUSE( eu1 );
	eu2 = RT_LIST_PNEXT_CIRC( edgeuse , eu1 );
	if( eu2->e_p->eg_p != eg )
	{
		/* Can't look backward here, or nmg_unbreak_edge()
		 * will be asked to kill *this* edgeuse, which
		 * will blow our caller's mind.
		 */
		/* rt_log("nmg_unbreak_handler: edge geom not shared\n"); */
		return;
	}
	vb = eu2->vu_p->v_p;
	NMG_CK_VERTEX(vb);

	/* at this point, the situation is:

		     eu1          eu2
		*----------->*----------->*
		A------------B------------C
		*<-----------*<-----------*
		    eu1mate      eu2mate
	*/
	if( nmg_unbreak_edge( eu1 ) != 0 )  return;

	/* keep a count of unbroken edges */
	ub_state->unbroken++;

#if 0
	/* See if vertex "B" survived, meaning it has other uses */
	if( vb->l.magic != NMG_VERTEX_MAGIC )  return;

	/* It's really unclear how to proceed here. No context info. */
#endif
}

/*
 *			N M G _ U N B R E A K _ R E G I O N _ E D G E S
 *
 *	Uses the visit handler to call nmg_unbreak_handler for
 *	each edge below the region (or any other NMG element).
 *
 *	returns the number of edges mended
 */
int
nmg_unbreak_region_edges( magic_p )
long		*magic_p;
{
	struct model *m;
	struct nmg_visit_handlers htab;
	struct nmg_unbreak_state ub_state;
	long *flags;
	int count;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_unbreak_region_edges( magic_p = x%x )\n" , magic_p );

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL( m );	

	htab = nmg_visit_handlers_null;		/* struct copy */
	htab.vis_edge = nmg_unbreak_handler;

	ub_state.unbroken = 0;
	ub_state.flags = (long *)rt_calloc( m->maxindex+2 , sizeof( long ) , "nmg_unbreak_region_edges: flags" );

	nmg_visit( magic_p , &htab , (genptr_t *)&ub_state );

	count = ub_state.unbroken;

	rt_free( (char *)ub_state.flags , "nmg_unbreak_region_edges: flags" );

	return( count );
}

/*
 *			R T _ D I S T _ P T 3 _ L I N E 3
 *
 *  Find the distance from a point P to a line described
 *  by the endpoint A and direction dir, and the point of closest approach (PCA).
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*-------->
 *		A      PCA	dir
 *
 *  There are three distinct cases, with these return codes -
 *	0	P is within tolerance of point A.  *dist = 0, pca=A.
 *	1	P is within tolerance of line.  *dist = 0, pca=computed.
 *	2	P is "above/below" line.  *dist=|PCA-P|, pca=computed.
 *
 *
 * XXX For efficiency, a version of this routine that provides the
 * XXX distance squared would be faster.
 */
int
rt_dist_pt3_line3( dist, pca, a, dir, p, tol )
fastf_t		*dist;
point_t		pca;
CONST point_t	a, p;
CONST vect_t	dir;
CONST struct rt_tol *tol;
{
	vect_t	AtoP;		/* P-A */
	vect_t	unit_dir;	/* unitized dir vector */
	fastf_t	A_P_sq;		/* |P-A|**2 */
	fastf_t	t;		/* distance along ray of projection of P */
	fastf_t	dsq;		/* sqaure of distance from p to line */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "rt_dist_pt3_line3( a=( %f %f %f ), dir=( %f %f %f ), p=( %f %f %f )\n" ,
			V3ARGS( a ) , V3ARGS( dir ) , V3ARGS( p ) );

	RT_CK_TOL(tol);

	/* Check proximity to endpoint A */
	VSUB2(AtoP, p, a);
	if( (A_P_sq = MAGSQ(AtoP)) < tol->dist_sq )  {
		/* P is within the tol->dist radius circle around A */
		VMOVE( pca, a );
		*dist = 0.0;
		return( 0 );
	}

	VMOVE( unit_dir , dir );
	VUNITIZE( unit_dir );

	/* compute distance (in actual units) along line to PROJECTION of
	 * point p onto the line: point pca
	 */
	t = VDOT(AtoP, unit_dir);

	VJOIN1( pca , a , t , unit_dir );
	if( (dsq = A_P_sq - t*t) < tol->dist_sq )
	{
		/* P is within tolerance of the line */
		*dist = 0.0;
		return( 1 );
	}
	else
	{
		/* P is off line */
		*dist = sqrt( dsq );
		return( 2 );
	}
}

/*
 *	N M G _ M V _ S H E L L _ T O _ R E G I O N
 *
 *  Move a shell from one nmgregion to another.
 *  Will bomb if shell and region aren't in the same model.
 *
 *  returns:
 *	0 - all is well
 *	1 - nmgregion that gave up the shell is now empty!!!!
 *
 */
int
nmg_mv_shell_to_region( s , r )
struct shell *s;
struct nmgregion *r;
{
	int ret_val;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_mv_shell_to_region( s=x%x , r=x%x )\n" , s , r );

	NMG_CK_SHELL( s );
	NMG_CK_REGION( r );

	if( s->r_p == r )
	{
		rt_log( "nmg_mv_shell_to_region: Attempt to move shell to region it is already in\n" );
		return( 0 );
	}

	if( nmg_find_model( &s->l.magic ) != nmg_find_model( &r->l.magic ) )
		rt_bomb( "nmg_mv_shell_to_region: Cannot move shell to a different model\n" );

	RT_LIST_DEQUEUE( &s->l );
	if( RT_LIST_IS_EMPTY( &s->r_p->s_hd ) )
		ret_val = 1;
	else
		ret_val = 0;

	RT_LIST_APPEND( &r->s_hd , &s->l );

	s->r_p = r;

	return( ret_val );
}

/*	N M G _ F I N D _ I S E C T _ F A C E S
 *
 *	Find all faces that contain vertex "new_v"
 *	Put them in a nmg_ptbl "faces"
 *
 *	returns:
 *		the number of faces that contain the vertex
 *
 *	and fills in the table with the faces.
 *	Counts edges at this vertex where radial is mate (free_edges)
 */
int
nmg_find_isect_faces( new_v , faces , free_edges , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *faces;
int *free_edges;
CONST struct rt_tol *tol;
{
	struct faceuse *fu;
	struct face_g *fg;
	struct edgeuse *eu;
	struct vertexuse *vu;
	int i;
	int unique;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_find_isect_faces( new_v=x%x , faces=x%x )\n" , new_v , faces );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );
	NMG_CK_PTBL( faces );

	/* loop through vertex's vu list */
	for( RT_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );
		fu = nmg_find_fu_of_vu( vu );
		if( fu->orientation != OT_SAME )
			continue;;

		NMG_CK_FACEUSE( fu );
		fg = fu->f_p->fg_p;

		/* check if this face is different from the ones on list */
		unique = 1;
		for( i=0 ; i<NMG_TBL_END( faces ) ; i++ )
		{
			struct face *fp;

			fp = (struct face *)NMG_TBL_GET( faces , i );
			if( fp->fg_p == fg || rt_coplanar( fg->N , fp->fg_p->N , tol ) > 0 )
			{
				unique = 0;
				break;
			}
		}

		/* if it is not already on the list, add it */
		if( unique )
		{
			struct edgeuse *eu1;

			nmg_tbl( faces , TBL_INS , (long *)fu->f_p );
			/* Count the number of free edges containing new_v */

			if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;

			eu1 = vu->up.eu_p;
			if( eu1->eumate_p == eu1->radial_p )
				(*free_edges)++;
			else
			{
				eu1 = RT_LIST_PLAST_CIRC( edgeuse , eu1 );
				if( eu1->eumate_p == eu1->radial_p )
					(*free_edges)++;
			}
		}
	}
	return( NMG_TBL_END( faces ) );
}

/*	N M G _ S I M P L E _ V E R T E X _ S O L V E
 *
 *	given a vertex and a list of faces (not more than three)
 *	that should intersect at the vertex, calculate a new
 *	location for the vertex.
 *
 *	returns:
 *		0 - if everything is OK
 *		1 - failure
 *
 *	and modifies the geometry of the vertex to the new location
 */
int
nmg_simple_vertex_solve( new_v , faces )
struct vertex *new_v;
CONST struct nmg_ptbl *faces;
{
	struct vertex_g *vg;
	int failed=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_simple_vertex_solve( new_v=x%x , %d faces )\n" , new_v , NMG_TBL_END( faces ));

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( faces );

	vg = new_v->vg_p;
	NMG_CK_VERTEX_G( vg );

	switch( NMG_TBL_END( faces ) )
	{
		struct face *fp1,*fp2,*fp3;
		plane_t pl1;
		fastf_t	vert_move_len;

		case 0:
			rt_log( "nmg_simple_vertex_solve: vertex not in any face planes!!!\n" );
			failed = 1;
			break;

		case 1:		/* just move the vertex to the plane */
			fp1 = (struct face *)NMG_TBL_GET( faces , 0 );
			vert_move_len = DIST_PT_PLANE( vg->coord , fp1->fg_p->N );
			VJOIN1( vg->coord , vg->coord , -vert_move_len , fp1->fg_p->N );
			break;

		case 2:		/* create a third plane perpendicular to first two */
			fp1 = (struct face *)NMG_TBL_GET( faces , 0 );
			fp2 = (struct face *)NMG_TBL_GET( faces , 1 );

			VCROSS( pl1 , fp1->fg_p->N , fp2->fg_p->N );
			pl1[3] = VDOT( vg->coord , pl1 );
			if( rt_mkpoint_3planes( vg->coord , fp1->fg_p->N , fp2->fg_p->N , pl1 ) )
			{
				rt_log( "nmg_simple_vertex_solve: Cannot find new coords for two planes\n" );
				rt_log( "\tplanes are ( %f %f %f %f ) and ( %f %f %f %f )\n",
					V4ARGS( fp1->fg_p->N ) , V4ARGS( fp2->fg_p->N ) );
				rt_log( "\tcalculated third plane is ( %f %f %f %f )\n" , V4ARGS( pl1 ) );
				failed = 1;
				break;
			}
			break;

		case 3:		/* just intersect the three planes */
			fp1 = (struct face *)NMG_TBL_GET( faces , 0 );
			fp2 = (struct face *)NMG_TBL_GET( faces , 1 );
			fp3 = (struct face *)NMG_TBL_GET( faces , 2 );
			if( rt_mkpoint_3planes( vg->coord , fp1->fg_p->N , fp2->fg_p->N , fp3->fg_p->N ) )
			{
				rt_log( "nmg_simple_vertex_solve: failed for 3 planes:\n" );
				rt_log( "\t( %f %f %f %f )\n" , V4ARGS( fp1->fg_p->N ) );
				rt_log( "\t( %f %f %f %f )\n" , V4ARGS( fp2->fg_p->N ) );
				rt_log( "\t( %f %f %f %f )\n" , V4ARGS( fp3->fg_p->N ) );
				failed = 1;
				break;
			}
			break;
		default:
			failed = 1;
			rt_log( "nmg_simple_vertex_solve: Called for a complex vertex\n" );
			break;
	}

	return( failed );
}

/*	N M G _ C K _ V E R T _ O N _ F U S
 *
 *  Check all uses of a vertex to see if it lies within tolerance
 *  of all faces where it is used
 *
 * returns:
 *	0 - All is well
 *	1 - vertex is off face plane by at least tol->dist for at least one face
 */
int
nmg_ck_vert_on_fus( v , tol )
CONST struct vertex *v;
CONST struct rt_tol *tol;
{
	struct vertexuse *vu;
	fastf_t max_dist=0.0;
	int ret_val=0;

	NMG_CK_VERTEX( v );
	RT_CK_TOL( tol );

	NMG_CK_VERTEX_G( v->vg_p );

	for( RT_LIST_FOR( vu , vertexuse , &v->vu_hd ) )
	{
		struct faceuse *fu;
		fastf_t dist;

		fu = nmg_find_fu_of_vu( vu );
		if( !fu )
			continue;

		NMG_CK_FACEUSE( fu );
		NMG_CK_FACE( fu->f_p );
		NMG_CK_FACE_G( fu->f_p->fg_p );
		dist = DIST_PT_PLANE( v->vg_p->coord , fu->f_p->fg_p->N );
		dist = (dist < 0.0 ? (-dist) : dist);
		if( dist > tol->dist )
		{
			ret_val = 1;

			if( dist > max_dist )
				max_dist = dist;

			rt_log( "nmg_ck_vert_on_fus: v=x%x vu=x%x ( %f %f %f ) is %g from\n\tfaceuse x%x f x%x\n" , v , vu , V3ARGS( v->vg_p->coord ) , dist , fu , fu->f_p );
		}
	}

	if( ret_val )
		rt_log( "nmg_ck_vert_on_fus: v=x%x ( %f %f %f ) max distance of %g from faceuses\n" , v , V3ARGS( v->vg_p->coord ) , max_dist );

	return( ret_val );
}

/* struct used by nmg_complex_vertex_solve
 * to store info about one edge
 * that contains the vertex in question
 */
struct intersect_fus
{
	struct faceuse *fu[2];	/* fu's that intersect at this edge */
	struct edgeuse *eu;	/* edgeuse in fu[0] that emanates from vertex */
	point_t start;		/* calculated start point of edge line */
	vect_t dir;		/* calculated direction of edge line */
	point_t pt;		/* a point on the edge a small distance from the vertex */
	int got_pt;		/* flag indicating that the above point has been obtained */
	int free_edge;		/* flag indicating that this is a free edge */
	struct vertex *vp;	/* a vertex pointer for above point */
};

/*	N M G _ P R _ I N T E R
 *
 * debug printing of the table of intersect_fus structs used by extruder
 */

static void
nmg_pr_inter( new_v , int_faces )
CONST struct vertex *new_v;
CONST struct nmg_ptbl *int_faces;
{
	int i;
	struct rt_tol tol;

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( int_faces );

	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	rt_log( "\nint_faces at vertex x%x ( %f %f %f )\n" , new_v , V3ARGS( new_v->vg_p->coord ) );
	for( i=0 ; i<NMG_TBL_END( int_faces ) ; i++ )
	{
		struct intersect_fus *i_fus;
		struct face *fp1,*fp2;
		plane_t pl;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , i );

		rt_log( "edge number %d, x%x\n" , i , i_fus );
		if( i_fus->fu[0] )
			fp1 = i_fus->fu[0]->f_p;
		else
			fp1 = NULL;
		if( i_fus->fu[1] )
		{
			fp2 = i_fus->fu[1]->f_p;
			NMG_GET_FU_PLANE( pl , i_fus->fu[1] );
		}
		else
			fp2 = NULL;

		if( i_fus->fu[1] )
			rt_log( "\tfu1 = x%x (face=x%x), fu2 = x%x (face=x%x) ( %f %f %f %f )\n" , i_fus->fu[0] , fp1 , i_fus->fu[1] , fp2 , V4ARGS( pl ) );
		else
			rt_log( "\tfu1 = x%x (face=x%x), fu2 = x%x (face=x%x)\n" , i_fus->fu[0] , fp1 , i_fus->fu[1] , fp2 );

		if( i_fus->eu == NULL )
			rt_log( "\teu = NULL\n" );
		else if( i_fus->eu->vu_p == NULL )
			rt_log( "\teu = x%x , vu_p = NULL\n" , i_fus->eu );
		else
		{
			struct faceuse *fu;
			struct loopuse *lu;

			rt_log( "\teu = x%x, from x%x ( %f %f %f ) to x%x ( %f %f %f )\n" , i_fus->eu,
				i_fus->eu->vu_p->v_p , V3ARGS( i_fus->eu->vu_p->v_p->vg_p->coord ),
				i_fus->eu->eumate_p->vu_p->v_p , V3ARGS( i_fus->eu->eumate_p->vu_p->v_p->vg_p->coord ) );
			if( i_fus->fu[0] )
			{
				fu = nmg_find_fu_of_eu( i_fus->eu );
				if( fu != i_fus->fu[0] )
					rt_log( "****ERROR**** eu is not in fu1 it's in x%x\n" , fu );
			}
			else
			{
				fu = nmg_find_fu_of_eu( i_fus->eu );
				if( fu != i_fus->fu[1] )
					rt_log( "****ERROR**** eu is not in fu2, it's in x%x\n" , fu );
			}
#if 0
			/* XXXX sometimes this gives a zero length edge error in spite of the check!! */
			if( !rt_pt3_pt3_equal( i_fus->eu->vu_p->v_p->vg_p->coord, i_fus->eu->eumate_p->vu_p->v_p, &tol ) )
				nmg_pr_fu_around_eu( i_fus->eu , &tol );
#endif
		}

		rt_log( "\tstart = ( %f %f %f ) , dir = ( %f %f %f )\n" , V3ARGS( i_fus->start ) , V3ARGS( i_fus->dir ) );
		rt_log( "\tpt = ( %f %f %f )\n" , V3ARGS( i_fus->pt ) );
		rt_log( "\tfree_edge = %d\n" , i_fus->free_edge );
		if( i_fus->eu && i_fus->eu->vu_p )
		{
			if( i_fus->eu->eumate_p != i_fus->eu->radial_p &&
			    i_fus->free_edge )
				rt_log( "****ERROR**** this is NOT a free edge\n" );
			if( i_fus->eu->eumate_p == i_fus->eu->radial_p &&
			    !i_fus->free_edge )
				rt_log( "****ERROR**** This is a free edge\n" );
		}
		if( i_fus->vp )
			rt_log( "\tvp = x%x ( %f %f %f )\n" , i_fus->vp , V3ARGS( i_fus->vp->vg_p->coord ) );
		else
			rt_log( "\tvp = NULL\n" );
	}
}

/*	N M G _ G E T _ E D G E _ L I N E S
 *
 * Fill in the intersect_fus structures for edges around
 * new_v. Does not fill in "pt" or "vp".
 *
 *	returns:
 *		0 - All is well
 *		1 - Failure
 */

static int
nmg_get_edge_lines( new_v , int_faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	struct vertex_g *vg;
	struct vertexuse *vu;
	struct edgeuse *eu,*eu1;
	struct faceuse *fu;
	struct model *m;
	struct nmgregion *r;
	struct rt_tol tol_tmp;
	int done=0;
	int edge_no;

	NMG_CK_VERTEX( new_v );
	vg = new_v->vg_p;
	NMG_CK_VERTEX_G( vg );
	RT_CK_TOL( tol );
	NMG_CK_PTBL( int_faces );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_get_edge_lines( new_v=x%x , int_faces=x%x )\n" , new_v , int_faces );

	/* A temporary tolerance struct for times when I don't want tolerancing */
	tol_tmp.magic = RT_TOL_MAGIC;
	tol_tmp.dist = 0.0;
	tol_tmp.dist_sq = 0.0;
	tol_tmp.perp = 0.0;
	tol_tmp.para = 1.0;

	m = nmg_find_model( &new_v->magic );
	NMG_CK_MODEL( m );
	r = RT_LIST_FIRST( nmgregion , &m->r_hd );
	NMG_CK_REGION( r );
	NMG_CK_REGION_A( r->ra_p );

	/* look for a dangling edge emanating from this vertex */
	eu1 = (struct edgeuse *)NULL;
	for( RT_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
	{
		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eu = vu->up.eu_p->eumate_p;
		fu = nmg_find_fu_of_eu( eu );
		if( !fu )
			continue;

		if( fu->orientation != OT_SAME )
			continue;

		if( eu->eumate_p == eu->radial_p )
		{
			/* found a dangling edge, start processing here */
			vect_t tmp_vec,eu_dir;
			plane_t pl;
			struct intersect_fus *i_fus;

			/* create and initialize an intersect_fus struct for this edge */
			i_fus = (struct intersect_fus *)rt_malloc( sizeof( struct intersect_fus ) , "nmg_get_edge_lines: i_fus" );
			i_fus->fu[0] = NULL;
			i_fus->fu[1] = fu;
			i_fus->eu = eu;
			i_fus->vp = (struct vertex *)NULL;
			VSET( i_fus->pt , 0.0 , 0.0 , 0.0 );
			i_fus->got_pt = 0;
			i_fus->free_edge = 1;
			eu1 = RT_LIST_PNEXT_CIRC( edgeuse , &eu->l );

			VSUB2( i_fus->dir , eu->vu_p->v_p->vg_p->coord , eu->eumate_p->vu_p->v_p->vg_p->coord );
			VUNITIZE( i_fus->dir );
			NMG_GET_FU_PLANE( pl , fu );
			VJOIN1( i_fus->start , vg->coord , (-DIST_PT_PLANE( vg->coord , pl )) , pl );

			/* Save this info in the int_faces table */
			nmg_tbl( int_faces , TBL_INS , (long *)i_fus );

			break;
		}
	}

	if( !eu1 )
	{
		int found_start=0;

		/* get the an edgeuse emanating from new_v */
		for( RT_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
		{
			NMG_CK_VERTEXUSE( vu );
			if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;

			eu1 = vu->up.eu_p;

			fu = nmg_find_fu_of_eu( eu1 );
			NMG_CK_FACEUSE( fu );

			if( fu->orientation == OT_SAME )
			{
				found_start = 1;
				break;
			}
		}
		if( !found_start )
		{
			rt_log( "Cannot find edgeuse in OT_SAME faceuse starting at ( %f %f %f )\n",
				V3ARGS( new_v->vg_p->coord ) );
			return( 1 );
		}
	}

	eu = eu1;

	/* loop through all the edges emanating from new_v */
	while( !done )
	{
		fastf_t dist;
		vect_t normal1;
		point_t start;
		vect_t dir;
		vect_t eu_dir;
		int ret_val;
		struct intersect_fus *i_fus;
		struct faceuse *fu1,*fu2;

		NMG_CK_EDGEUSE( eu );

		if( eu->vu_p->v_p != new_v )
		{
			/* This can happen if the faces of the shell are not properly
			 * oriented such as might happen when an object is incorrectly
			 * modelled in FASTGEN and run through the patch-g converter
			 */
			rt_log( "nmg_get_edge_lines: Bad solid!!!\n" );
			for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
			{
				struct intersect_fus *i_fus;

				i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

				rt_free( (char *)i_fus , "nmg_get_edge_lines: i_fus" );
			}
			return( 1 );
		}

		/* get the direction of the original edge (away from the vertex) */
		VSUB2( eu_dir , eu->eumate_p->vu_p->v_p->vg_p->coord , eu->vu_p->v_p->vg_p->coord );

		/* get the two faces that intersect along this edge */
		fu1 = nmg_find_fu_of_eu( eu );
		fu2 = nmg_find_fu_of_eu( eu->radial_p );

		/* initialize the intersect structure for this edge */
		i_fus = (struct intersect_fus *)rt_malloc( sizeof( struct intersect_fus ) , "nmg_inside_vert: intersection list" );
		i_fus->fu[0] = fu1;
		if( eu->radial_p == eu->eumate_p )
		{
			i_fus->fu[1] = (struct faceuse *)NULL;
			i_fus->free_edge = 1;
			done = 1;
		}
		else
		{
			i_fus->fu[1] = fu2;
			i_fus->free_edge = 0;
		}
		i_fus->eu = eu;
		i_fus->vp = (struct vertex *)NULL;
		VSET( i_fus->pt , 0.0 , 0.0 , 0.0 );
		i_fus->got_pt = 0;
		VSET( i_fus->start , 0.0 , 0.0 , 0.0 );
		VSET( i_fus->dir , 0.0 , 0.0 , 0.0 );

		/* if edge is between loops of same face , don't calculate an edge line */
		if( fu1->f_p != fu2->f_p )
		{
			/* find the new edge line at the intersection of these two faces
			 * the line is defined by start and dir */

			NMG_GET_FU_NORMAL( normal1 , fu1 );
			if( ret_val=rt_isect_2planes( start , dir , fu1->f_p->fg_p->N , fu2->f_p->fg_p->N , new_v->vg_p->coord , &tol_tmp ) )
			{
				/* Cannot find line for this edge */
				rt_log( "nmg_inside_vert: Cannot find new edge between two planes\n" );
				rt_log( "return from rt_isect_2planes is %d\n" , ret_val );
				rt_log( "\tplanes are ( %f %f %f %f ) and ( %f %f %f %f )\n" ,
					V4ARGS( fu1->f_p->fg_p->N ),
					V4ARGS( fu2->f_p->fg_p->N ) );
				rt_log( "\tfus x%x and x%x, faces x%x and x%x\n" ,
					fu1, fu2, fu1->f_p, fu2->f_p );
				rt_bomb( "Can't find plane intersection\n" );
			}
			/* Make the start point at closest approach to old vertex */
			(void)rt_dist_pt3_line3( &dist , start , start , dir , new_v->vg_p->coord , tol );

			/* Make sure the calculated direction is away from the vertex */
			if( VDOT( eu_dir , dir ) < 0.0 )
				VREVERSE( dir , dir );
			VMOVE( i_fus->start , start );
			VMOVE( i_fus->dir , dir );
		}
		else if( i_fus->free_edge )
		{
			vect_t tmp_vec;
			plane_t pl;

			/* for a dangling edge, use the same direction as the original edge
			 * just move the start point to the new plane
			 */

			NMG_GET_FU_PLANE( pl , fu1 );

			VMOVE( i_fus->dir , eu_dir );
			VUNITIZE( i_fus->dir );

			VJOIN1( i_fus->start , vg->coord , (-DIST_PT_PLANE( vg->coord , pl )) , pl );

		}

		/* Save this info in the int_faces table */
		nmg_tbl( int_faces , TBL_INS , (long *)i_fus );

		if( !done )
		{
			/* move on to the next edge emanating from new_v */
			eu = eu->radial_p;
			eu = RT_LIST_PNEXT_CIRC( edgeuse , eu );
			if( eu == eu1 )
				done = 1;
		}
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After getting edge lines:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	return( 0 );
}

/*	N M G _ G E T _ M A X _ E D G E _ I N T E R S
 *
 * Fill in the "pt" portion of the "intersect_fus" structure
 * for edges around new_v by calculating the intersection with neighboring
 * edges and selecting the furthest one from new_v.
 */
static int
nmg_get_max_edge_inters( new_v , int_faces , faces , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct nmg_ptbl *faces;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct nmgregion *r;
	vect_t diag;
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC ) 
		rt_log( "nmg_get_max_edge_inters( new_v = x%x , %d intersect_fus structs , %d faces )\n" , new_v , NMG_TBL_END( int_faces ) , NMG_TBL_END( faces ) );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );
	NMG_CK_PTBL( int_faces );

	m = nmg_find_model( &new_v->magic );
	NMG_CK_MODEL( m );
	r = RT_LIST_FIRST( nmgregion , &m->r_hd );
	NMG_CK_REGION( r );
	NMG_CK_REGION_A( r->ra_p );

	/* loop through edges departing from new_v */
	for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus,*other_fus;
		fastf_t max_dist,dist[2];
		int next_edge_no,prev_edge_no;
		int other_index;
		int found1,found2;

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		/* don't calculate intersect point for edge between two loops of same face */
		if( edge_fus->fu[0] && edge_fus->fu[1] &&
			 edge_fus->fu[0]->f_p == edge_fus->fu[1]->f_p )
				continue;

		/* Find intersections with neighboring edges and keep the one
		 * furthest up the edge
		 */
		max_dist = (-MAX_FASTF);

		/* start with next edge */
		next_edge_no = edge_no + 1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			next_edge_no = 0;

		other_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		/* skip over edges betwen loops of same face */
		while( other_fus->fu[0] == other_fus->fu[1] && other_fus != edge_fus )
		{
			next_edge_no++;
			if( next_edge_no == NMG_TBL_END( int_faces ) )
				next_edge_no = 0;

			other_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		}

		/* if we found another edge, calculate its intersection with the edge */
		if( other_fus != edge_fus )
		{
			if( !rt_dist_line3_line3( dist , edge_fus->start , edge_fus->dir , other_fus->start , other_fus->dir , tol ) )
			{
				if( dist[0] > max_dist )
					max_dist = dist[0];
			}
		}

		/* now check the previous neighboring edge */
		prev_edge_no = edge_no - 1;
		if( prev_edge_no < 0 )
			prev_edge_no = NMG_TBL_END( int_faces ) - 1;

		other_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , prev_edge_no );

		while( other_fus->fu[0] == other_fus->fu[1] && other_fus != edge_fus )
		{
			prev_edge_no--;
			if( prev_edge_no < 0 )
				prev_edge_no = NMG_TBL_END( int_faces ) - 1;

			other_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , prev_edge_no );
		}

		if( other_fus != edge_fus )
		{
			if( rt_dist_line3_line3( dist , edge_fus->start , edge_fus->dir , other_fus->start , other_fus->dir , tol ) >= 0 )
			{
				if( dist[0] > max_dist )
					max_dist = dist[0];
			}
		}

		/* if any intersections have been found, save the point in edge_fus->pt */
		if( max_dist > (-MAX_FASTF) )
		{
			VJOIN1( edge_fus->pt , edge_fus->start , max_dist , edge_fus->dir );
			edge_fus->got_pt = 1;
		}
	}

	/* if no intersection was found, just use the edge-line start point */
	for( edge_no=0 ; edge_no < NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus;

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		if( !edge_fus->got_pt )
			VMOVE( edge_fus->pt , edge_fus->start )
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After nmg_get_max_edge_inters:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	return( 0 );
}

/*	N M G _ F U S E _ I N T E R S
 *
 * eliminate "j_fus" from the table "int_faces" and
 * adjust the info in "i_fus".
 * This is done when the "vp" vertices of the two structures
 * have been joined.
 */
static void
nmg_fuse_inters( i_fus , j_fus , int_faces , tol )
struct intersect_fus *i_fus;
struct intersect_fus *j_fus;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	struct edgeuse *radial_eu;
	struct edgeuse *prev_eu;

	NMG_CK_PTBL( int_faces );
	RT_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_fuse_inters: i_fus=x%x, j_fus=x%x, %d edges\n" , i_fus, j_fus, NMG_TBL_END( int_faces ) );

	/* remember the radial edge of the structure to be deleted */
	radial_eu = j_fus->eu->radial_p;

	/* if the vertices have been joined prev_eu and j_fus->eu should be adjacent */
	prev_eu = RT_LIST_PPREV_CIRC( edgeuse , &j_fus->eu->l );

	if( EDGESADJ( prev_eu , j_fus->eu ) )
	{
		nmg_keu( prev_eu );
		nmg_keu( j_fus->eu );
	}
	else
		rt_log( "nmg_fuse_inter_verts: ERROR: can't find adjacent edges to kill\n" );

	/* the other face for this edge is now j_fus->fu[1] */
	i_fus->fu[1] = j_fus->fu[1];

	/* if there are other faces along the edges that have been brought together
	 * do a radial join
	 */
	if( i_fus->fu[0] && j_fus->fu[1] )
	{
		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			rt_log( "radial join of eu's x%x and x%x\n" , i_fus->eu , radial_eu );
			rt_log( "\tx%x to x%x and x%x to x%x\n" ,
				i_fus->eu->vu_p->v_p, i_fus->eu->eumate_p->vu_p->v_p,
				radial_eu->vu_p->v_p, radial_eu->eumate_p->vu_p->v_p );
		}
		nmg_radial_join_eu( i_fus->eu , radial_eu , tol );
	}

	/* If this is a dangling edge, need to adjust the eu pointer */
	if( !i_fus->fu[0] )
		i_fus->eu = radial_eu;
	NMG_CK_EDGEUSE( i_fus->eu );

	/* if the deleted structure was for a dangling edge,
	 * then this edge is now dangling
	 */
	if( j_fus->free_edge )
		i_fus->free_edge = 1;

	nmg_tbl( int_faces , TBL_RM , (long *)j_fus );
	rt_free( (char *)j_fus , "nmg_split_edges_at_pts: j_fus " );

}

/*	N M G _ S P L I T _ E D G E S _ A T _ P T S
 *
 * Using the info in the table of intersect_fus structs,
 * split the edgeuse (eu) in each struct at the point (pt)
 * store the new vertices in the structure (vp) and assign
 * the geometry.
 *
 */
static void
nmg_split_edges_at_pts( new_v , int_faces , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_split_edges_at_pts( new_v = x%x , %d intersect_fus structs)\n" , new_v , NMG_TBL_END( int_faces ) );

	RT_CK_TOL( tol );
	NMG_CK_PTBL( int_faces );
	NMG_CK_VERTEX( new_v );

	/* loop through all edges departing from new_v */
	for( edge_no=0 ; edge_no < NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *i_fus;
		struct edgeuse *new_eu;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		/* skip edges between two loops of same face, for now */
		if( i_fus->fu[0] && i_fus->fu[1] && i_fus->fu[0]->f_p == i_fus->fu[1]->f_p )
			continue;

		if( rt_pt3_pt3_equal( new_v->vg_p->coord , i_fus->pt , tol ) )
		{
			vect_t eu_dir;

			/* if pt is within tolerance of new_v, don't split the edge */
			i_fus->vp = (struct vertex *)NULL;
			VMOVE( i_fus->pt , new_v->vg_p->coord );
			VMOVE( i_fus->start , new_v->vg_p->coord );
			VSUB2( i_fus->dir , i_fus->eu->eumate_p->vu_p->v_p->vg_p->coord , i_fus->eu->vu_p->v_p->vg_p->coord );
			VUNITIZE( i_fus->dir );
			continue;
		}
		new_eu = nmg_esplit( i_fus->vp , i_fus->eu );
		i_fus->vp = new_eu->vu_p->v_p;

		/* Need to keep track of correct eu in this case */
		if( i_fus->free_edge && !i_fus->fu[0] )
			i_fus->eu = new_eu;

		/* Assign geometry to the new vertex */
		if( i_fus && !i_fus->vp->vg_p )
			nmg_vertex_gv( i_fus->vp , i_fus->pt );
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After splitting edges:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	/* Now take care of edges between two loops of same face */
	edge_no = 0;
	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		int next_edge_no;
		struct intersect_fus *i_fus,*j_fus;

		next_edge_no = edge_no + 1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			next_edge_no = 0;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		/* look at all edges in the same face as i_fus->fu[1] */
		while( j_fus->fu[0] && j_fus->fu[1] &&
		       j_fus->fu[0]->f_p == j_fus->fu[1]->f_p &&
		       j_fus != i_fus )
		{
			struct edgeuse *new_eu;
			struct edgeuse *prev_eu;
			struct edgeuse *radial_eu;

			/* if both edges are dangling, there is nothing to do */
			if( i_fus->free_edge && j_fus->free_edge )
				break;

			/* if we haven't assigned a vertex, skip this edge */
			if( !i_fus->vp )
				break;

			/* split the neighbor at the first structure's "vp"
			 * this moves the neighboring edge's endpoint to
			 * fall on the first edge.
			 */
			new_eu = nmg_esplit( i_fus->vp , j_fus->eu );

			/* now we can ignore this edge */
			nmg_fuse_inters( i_fus , j_fus , int_faces , tol );

			/* go to the next edge */
			if( next_edge_no == NMG_TBL_END( int_faces ) )
				next_edge_no = 0;

			j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		}
		edge_no++;
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After loops of same face\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ R E M O V E _ S H O R T _ E U S _ I N T E R
 *
 * kill all zero length edgeuses in faces around new_v
 *
 */
static void
nmg_remove_short_eus_inter( new_v , int_faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no;
	struct vertexuse *vu;

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( int_faces );
	RT_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_remove_short_eus: new_v=x%x ( %f %f %f ), %d edges\n" , new_v, V3ARGS( new_v->vg_p->coord ), NMG_TBL_END( int_faces ) );

	/* first join any of the "vp" in the intersect_fus structs that are
	 * within tolerance of new-v
	 */
	for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus;

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		if( !edge_fus->vp )
			continue;

		if( !rt_pt3_pt3_equal( new_v->vg_p->coord , edge_fus->vp->vg_p->coord , tol ) )
			continue;

		nmg_jv( new_v , edge_fus->vp );
		edge_fus->vp = new_v;
	}

	/* look at all faces around new_v */
	vu = RT_LIST_FIRST( vertexuse , &new_v->vu_hd );
	while( RT_LIST_NOT_HEAD( vu , &new_v->vu_hd ) )
	{
		struct vertexuse *vu_next;
		struct faceuse *fu;
		struct loopuse *lu;
		struct faceuse *bad_fu=(struct faceuse *)NULL;
		int bad_loop=0;

		NMG_CK_VERTEXUSE( vu );

		vu_next = RT_LIST_PNEXT( vertexuse , &vu->l );

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
		{
			vu = vu_next;
			continue;
		}

		fu = nmg_find_fu_of_vu( vu );
		NMG_CK_FACEUSE( fu );

		/* look at all loops in these faces */
		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *lu_next;
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			lu_next = RT_LIST_PNEXT( loopuse , &lu->l );

			eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
			while( RT_LIST_NOT_HEAD( eu , &lu->down_hd ) )
			{
				struct edgeuse *eu_next;

				NMG_CK_EDGEUSE( eu );

				eu_next = RT_LIST_PNEXT( edgeuse , &eu->l );

				/* kill edges that are to/from same vertex */
				if( eu->vu_p->v_p == eu->eumate_p->vu_p->v_p )
				{
					while( (vu_next == eu->vu_p || vu_next == eu->eumate_p->vu_p ) &&
						RT_LIST_NOT_HEAD( vu_next , &new_v->vu_hd ) )
							vu_next = RT_LIST_PNEXT( vertexuse , &vu_next->l );
					while( (eu_next == eu || eu_next == eu->eumate_p) &&
						RT_LIST_NOT_HEAD( eu_next , &lu->down_hd ) )
							eu_next = RT_LIST_PNEXT( edgeuse , &eu_next->l );

					if( rt_g.NMG_debug & DEBUG_BASIC )
						rt_log( "\tkilling eu x%x (x%x)\n" , eu , eu->eumate_p );

					bad_loop = nmg_keu( eu );
				}
				/* kill edges with length less than tol->dist */
				else if( rt_pt3_pt3_equal( eu->vu_p->v_p->vg_p->coord , eu->eumate_p->vu_p->v_p->vg_p->coord , tol ) )
				{
					struct edgeuse *eu_next;
					struct edgeuse *prev_eu;

					prev_eu = RT_LIST_PPREV_CIRC( edgeuse , &eu->l );
					NMG_CK_EDGEUSE( prev_eu );

					prev_eu->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p;

					while( (vu_next == eu->vu_p || vu_next == eu->eumate_p->vu_p ) &&
						RT_LIST_NOT_HEAD( vu_next , &new_v->vu_hd ) )
							vu_next = RT_LIST_PNEXT( vertexuse , &vu_next->l );
					while( (eu_next == eu || eu_next == eu->eumate_p) &&
						RT_LIST_NOT_HEAD( eu_next , &lu->down_hd ) )
							eu_next = RT_LIST_PNEXT( edgeuse , &eu_next->l );

					if( rt_g.NMG_debug & DEBUG_BASIC )
						rt_log( "\tkilling eu x%x (x%x)\n" , eu , eu->eumate_p );

					bad_loop = nmg_keu( eu );
				}

				if( bad_loop )
				{
					/* emptied a loop, so kill it */
					while( (lu_next == lu || lu_next == lu->lumate_p) &&
						RT_LIST_NOT_HEAD( lu_next , &fu->lu_hd ) )
							lu_next = RT_LIST_PNEXT( loopuse , &lu_next->l );

					bad_fu = nmg_find_fu_of_lu( lu );
					if( !nmg_klu( lu ) )
						bad_fu = (struct faceuse *)NULL;

					break;
				}

				eu = eu_next;
			}
			if( bad_fu )
			{
				/* emptied a faceuse, so kill it */
				if( nmg_kfu( bad_fu ) )
				{
					/* I can't believe I emptied the whole thing!! */
					rt_log( "nmg_remove_short_eus_inter: nmg_kfu emptied shell!!!\n" );
					break;
				}
			}
			lu = lu_next;
		}

		vu = vu_next;
	}
}

/*	N M G _ S I M P L I F Y _ I N T E R
 *
 * Eliminates adjacent intersect_fus structs with collinear edges
 *
 */
static void
nmg_simplify_inter( new_v , int_faces , tol )
CONST struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no=0;
	int next_edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_simplify_inter( new_v=x%x ( %f %f %f ), int_faces=x%x)\n",
			new_v, V3ARGS( new_v->vg_p->coord ) , int_faces );

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( int_faces );
	RT_CK_TOL( tol );

	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		struct intersect_fus *i_fus;
		struct intersect_fus *j_fus;
		struct vertexuse *vu1,*vu2;
		struct edgeuse *eu;
		struct loopuse *lu;
		struct loopuse *new_lu;
		struct loopuse *dup_lu;
		struct faceuse *new_fu;
		struct faceuse *fu;
		long **trans_tbl;

		/* get two consectutive structures */
		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		next_edge_no = edge_no+1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			 next_edge_no = 0;
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		/* skip open space */
		if( (i_fus->free_edge || j_fus->free_edge) && next_edge_no == 0 )
		{
			edge_no++;
			continue;
		}

		/* Don't fuse free edges together */
		if( i_fus->free_edge && j_fus->free_edge )
		{
			edge_no++;
			continue;
		}

		/* if either vertex or edgeuse is null, skip */
		if( i_fus->vp == NULL || j_fus->vp == NULL ||
		    i_fus->eu == NULL || j_fus->eu == NULL )
		{
			edge_no++;
			continue;
		}

		/* If either vertex is new_v, skip */
		if( i_fus->vp == new_v || j_fus->vp == new_v )
		{
			edge_no++;
			continue;
		}

		/* the two vertices should never be the same */
		if( i_fus->vp == j_fus->vp )
			rt_bomb( "nmg_simplify_inter: Two vertices are the same\n" );

		NMG_CK_VERTEX( i_fus->vp );
		NMG_CK_VERTEX( j_fus->vp );
		NMG_CK_EDGEUSE( i_fus->eu );
		NMG_CK_EDGEUSE( j_fus->eu );

		/* if the two vertices are within tolerance,
		 * fuse them
		 */
		if( rt_pt3_pt3_equal( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , tol ) )
		{
			nmg_jv( i_fus->vp , j_fus->vp );
			nmg_fuse_inters( i_fus , j_fus , int_faces , tol );
			continue;
		}
		else if( rt_3pts_collinear( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , new_v->vg_p->coord , tol ) )
		{
			fastf_t i_dist,j_dist;
			vect_t i_dist_to_new_v,j_dist_to_new_v;

			/* all three points are collinear,
			 * may need to split edges
			 */

			VSUB2( i_dist_to_new_v , new_v->vg_p->coord , i_fus->vp->vg_p->coord );
			VSUB2( j_dist_to_new_v , new_v->vg_p->coord , j_fus->vp->vg_p->coord );

			if( VDOT( i_dist_to_new_v , j_dist_to_new_v ) < 0.0 )
			{
				/* points are collinear with new_v, but in opoosit directions */
				edge_no++;
				continue;
			}

			i_dist = MAGSQ( i_dist_to_new_v );
			j_dist = MAGSQ( j_dist_to_new_v );

			if( i_dist < tol->dist_sq || j_dist < tol->dist_sq )
				rt_bomb( "nmg_simplify_inter: vertex within tolerance of new_v\n" );

			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\tCollinear vertices x%x, x%x, and x%x\n",
						new_v , i_fus->vp , j_fus->vp );

			if( i_dist > j_dist && j_dist > tol->dist_sq )
			{
				/* j point is closer to new_v than i point
				 * split edge at j point
				 */

				if( rt_g.NMG_debug & DEBUG_BASIC )
					rt_log( "\tSplitting i_fus->eu x%x at vertex x%x\n" , i_fus->eu , j_fus->vp );

				(void)nmg_esplit( j_fus->vp , i_fus->eu );
				i_fus->vp = j_fus->vp;
				nmg_fuse_inters( i_fus , j_fus , int_faces , tol );

				continue;
			}
			else if( j_dist > i_dist && i_dist > tol->dist_sq )
			{
				/* i point is closer to new_v than j point
				 * split edge at i point
				 */

				if( rt_g.NMG_debug & DEBUG_BASIC )
					rt_log( "\tSplitting j_fus->eu x%x at vertex x%x\n" , j_fus->eu , i_fus->vp );

				(void)nmg_esplit( i_fus->vp , j_fus->eu );
				nmg_fuse_inters( i_fus , j_fus , int_faces , tol );
				continue;
			}
		}
		edge_no++;
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "\nAfter nmg_simplify_inter:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ M A K E _ F A C E S _ A T _ V E R T
 *
 * Make new faces around vertex new_v using info in
 * the table of intersect_fu structures. Each structure
 * contains a vertex on an edge departing new_v.
 * Vertices from two consecutive edges are combined with
 * new_v to form triangular faces around new_v
 *
 */
void
nmg_make_faces_at_vert( new_v , int_faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct loopuse *old_lu;
	int edge_no=0,next_edge_no,j;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_make_faces_at_vert( x%x , %d intersect_fus structs)\n" , new_v , NMG_TBL_END( int_faces ) );

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( int_faces );
	RT_CK_TOL( tol );

	m = nmg_find_model( &new_v->magic );

	if( NMG_TBL_END( int_faces ) == 1 )
	{
		struct intersect_fus *i_fus;

		/* only one intersect point is left, move new_v to it
		 * and don't make any faces
		 */
		if( i_fus->vp )
		{
			i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , 0 );

			VMOVE( new_v->vg_p->coord , i_fus->vp->vg_p->coord );
			nmg_jv( new_v , i_fus->vp );
		}
		return;
	}

	if( NMG_TBL_END( int_faces ) == 2 )
	{
		struct intersect_fus *i_fus,*j_fus;
		point_t center_pt;

		/* only two intersect points left, if they are not on free edges,
		 *  move new_v to the center of the connecting line. No new faces needed
		 */

		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , 0 );
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , 1 );

		if( i_fus->vp && j_fus->vp && !i_fus->free_edge && !j_fus->free_edge )
		{
			VCOMB2( center_pt , 0.5 , i_fus->vp->vg_p->coord , 0.5 , j_fus->vp->vg_p->coord );
			VMOVE( new_v->vg_p->coord , center_pt );
		}
		return;
	}

	/* Need to make new faces.
	 * loop around the vertex, looking at
	 * pairs of adjacent edges and deciding
	 * if a new face needs to be constructed
	 * from the two intersect vertices and new_v
	 */
	while( edge_no < NMG_TBL_END( int_faces ) )
	{
		struct intersect_fus *i_fus;
		struct intersect_fus *j_fus;
		struct vertexuse *vu1,*vu2;
		struct edgeuse *eu;
		struct loopuse *lu;
		struct loopuse *new_lu;
		struct loopuse *dup_lu;
		struct faceuse *new_fu;
		struct faceuse *fu;
		long **trans_tbl;

		/* get two consectutive structures */
		i_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );
		next_edge_no = edge_no+1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			 next_edge_no = 0;
		j_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		/* Don't construct a new face across open space */
		if( (i_fus->free_edge || j_fus->free_edge) && next_edge_no == 0 )
		{
			edge_no++;
			continue;
		}

		/* if the two vertices are the same, no face needed */
		if( i_fus->vp == j_fus->vp )
		{
			edge_no++;
			continue;
		}

		/* if either vertex is null, no face needed */
		if( i_fus->vp == NULL || j_fus->vp == NULL || i_fus->eu == NULL || j_fus->eu == NULL )
		{
			edge_no++;
			continue;
		}

		/* Don't make faces with two vertices the same */
		if( i_fus->vp == new_v || j_fus->vp == new_v )
		{
			edge_no++;
			continue;
		}

		NMG_CK_VERTEX( i_fus->vp );
		NMG_CK_VERTEX( j_fus->vp );
		NMG_CK_EDGEUSE( i_fus->eu );
		NMG_CK_EDGEUSE( j_fus->eu );

		/* don't make degenerate faces */
		if( rt_3pts_collinear( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , new_v->vg_p->coord , tol ) )
		{
			edge_no++;
			continue;
		}

		/* O.K., here is where we actually start making faces.
		 * Find uses of the two vertices in the same loopuse
		 */
		old_lu = j_fus->eu->up.lu_p;
		vu1 = (struct vertexuse *)NULL;
		vu2 = (struct vertexuse *)NULL;
		for( RT_LIST_FOR( eu , edgeuse , &old_lu->down_hd ) )
		{
			if( eu->vu_p->v_p == i_fus->vp )
				vu1 = eu->vu_p;
			else if( eu->vu_p->v_p == j_fus->vp )
				vu2 = eu->vu_p;
		}

		if( vu1 == NULL || vu2 == NULL )
		{
			rt_log( "nmg_make_faces_at_vert: ERROR: Can't find loop containing vertices x%x and x%x\n" , i_fus->vp, j_fus->vp );
			rt_log( "\t( %f %f %f ) and ( %f %f %f )\n" , V3ARGS( i_fus->vp->vg_p->coord ) , V3ARGS( j_fus->vp->vg_p->coord ) );
			edge_no++;
			continue;
		}

		/* make sure the two vertices have a third between,
		 * otherwise, don't cut the loop
		 */
		eu = vu1->up.eu_p;
		if( eu->eumate_p->vu_p == vu2 )
		{
			edge_no++;
			continue;
		}
		eu = vu2->up.eu_p;
		if( eu->eumate_p->vu_p == vu1 )
		{
			edge_no++;
			continue;
		}

		/* cut the face loop across the two vertices */
		new_lu = nmg_cut_loop( vu1 , vu2 );

		/* Fix orientations.
		 * We will never be cutting an OT_OPPOSITE loop
		 * so the will always be OT_SAME
		 */
		new_lu->orientation = OT_SAME;
		new_lu->lumate_p->orientation = OT_SAME;
		old_lu->orientation = OT_SAME;
		old_lu->lumate_p->orientation = OT_SAME;

		/* find which loopuse contains new_v
		 * this will be the one to become a new face
		 */
		lu = NULL;

		/* first check old_lu */
		for( RT_LIST_FOR( eu , edgeuse , &old_lu->down_hd ) )
		{
			if( eu->vu_p->v_p == new_v )
			{
				lu = old_lu;
				break;
			}
		}

		/* if not found check new_lu */
		if( lu == NULL )
		{
			for( RT_LIST_FOR( eu , edgeuse , &new_lu->down_hd ) )
			{
				if( eu->vu_p->v_p == new_v )
				{
					lu = old_lu;
					break;
				}
			}
		}

		if( lu == NULL )
		{
			fu = old_lu->up.fu_p;
			rt_log( "nmg_make_faces_at_vert: can't find loop for new face\n" );
			rt_log( "vu1 = x%x (x%x) vu2 = x%x (x%x)\n" , vu1 , vu1->v_p , vu2 , vu2->v_p );
			rt_log( "new_v = x%x\n" , new_v );
			rt_log( "old_lu = x%x , new_lu = x%x\n" , old_lu , new_lu );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			rt_bomb( "nmg_make_faces_at_vert: can't find loop for new face\n" );
		}

		/* make the new face from the new loop */
		new_fu = nmg_mk_new_face_from_loop( lu );

		/* update the intersect_fus structs (probably not necessary at this point) */
		j_fus->fu[0] = new_fu;
		i_fus->fu[1] = new_fu;

		NMG_CK_FACEUSE( new_fu );

		/* calculate a plane equation for the new face */
		if( nmg_fu_planeeqn( new_fu , tol ) )
		{
			rt_log( "nmg_make_faces_at_vert: Failed to calculate plane eqn for face:\n " );
			rt_log( "\tnew_v is x%x at ( %f %f %f )\n" , new_v , V3ARGS( new_v->vg_p->coord ) );
			if( rt_3pts_collinear( new_v , vu1->v_p , vu2->v_p , tol ) )
				rt_log( "\tPoints are collinear\n" );
			nmg_pr_fu_briefly( new_fu , " " );
		}
		nmg_face_bb( new_fu->f_p , tol );

		edge_no++;
	}
}

/*	N M G _ K I L L _ C R A C K S _ A T _ V E R T E X
 *
 * Look at all faces around vertex new_v and kill any two
 * consecutive eu's that go from a vertex to a second then back
 * to the original vertex
 */
void
nmg_kill_cracks_at_vertex( vp )
CONST struct vertex *vp;
{
	struct nmg_ptbl fus_at_vert;
	struct vertexuse *vu;
	struct faceuse *fu;
	int fu_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_kill_cracks_at_vertex( vp=x%x )\n" , vp );

	NMG_CK_VERTEX( vp );

	/* first make a list of all the faceuses at this vertex */
	nmg_tbl( &fus_at_vert , TBL_INIT , (long *)NULL );

	for( RT_LIST_FOR( vu , vertexuse , &vp->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );

		fu = nmg_find_fu_of_vu( vu );
		if( !fu )
			continue;

		NMG_CK_FACEUSE( fu );
		nmg_tbl( &fus_at_vert , TBL_INS_UNIQUE , (long *)fu );
	}

	/* Now look at these faceuses for cracks ( jaunts from a vertex and back to the same ) */
	for( fu_no=0 ; fu_no<NMG_TBL_END( &fus_at_vert ) ; fu_no++ )
	{
		struct loopuse *lu;
		int bad_face=0;

		fu = (struct faceuse *)NMG_TBL_GET( &fus_at_vert , fu_no );
		NMG_CK_FACEUSE( fu );

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		while( RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *lu_next;
			struct edgeuse *eu;
			int bad_loop=0;

			NMG_CK_LOOPUSE( lu );

			lu_next = RT_LIST_NEXT( loopuse , &lu->l );

			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			{
				lu = lu_next;
				continue;
			}

			eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
			while( RT_LIST_NOT_HEAD( eu , &lu->down_hd ) )
			{
				struct edgeuse *eu_prev;
				struct edgeuse *eu_next;

				NMG_CK_EDGEUSE( eu );

				eu_next = RT_LIST_NEXT( edgeuse , &eu->l );
				eu_prev = RT_LIST_PPREV_CIRC( edgeuse , &eu->l );
				NMG_CK_EDGEUSE( eu_prev );

				/* Check for a crack */
				if( EDGESADJ( eu , eu_prev ) )
				{
					/* found a crack, kill it */
					if( nmg_keu( eu ) )
					{
						/* This should never happen */
						rt_log( "ERROR: nmg_kill_cracks_at_vert: bad loopuse x%x\n" , lu );
						bad_loop = 1;
						break;
					}
					if( nmg_keu( eu_prev ) )
					{
						bad_loop = 1;
						break;
					}
				}
				eu = eu_next;
			}
			if( bad_loop )
			{
				if( nmg_klu( lu ) )
				{
					bad_face = 1;
					break;
				}
			}
			lu = lu_next;
		}
		if( bad_face )
		{
			if( nmg_kfu( fu ) )
				rt_log( "ERROR:nmg_kill_cracks_at_vert: bad shell!!!\n" );
		}
	}
	nmg_tbl( &fus_at_vert , TBL_FREE , (long *)NULL );
}

/*	N M G _ D I S T _ T O _ C R O S S
 *
 * Used by nmg_fix_crossed edges to calculate the point
 * where two edges cross
 *
 *	returns:
 *		distance to intersection if edge intersect
 *		-1.0 if they don't
 */
static fastf_t
nmg_dist_to_cross( i_fus , j_fus , new_pt , tol )
CONST struct intersect_fus *i_fus;
CONST struct intersect_fus *j_fus;
point_t new_pt;
CONST struct rt_tol *tol;
{
	plane_t pl;
	struct edgeuse *i_next_eu,*j_next_eu;
	struct vertex *i_end,*j_end;
	struct vertex *i_start,*j_start;
	point_t i_end_pt,j_end_pt;
	vect_t i_dir,j_dir;
	fastf_t dist[2];

	RT_CK_TOL( tol );

	if( i_fus->fu[1] )
		NMG_GET_FU_PLANE( pl , i_fus->fu[1] )

	/* get edgeuses leaving from new vertices */
	if( !i_fus->fu[0] )
		i_next_eu = RT_LIST_PPREV_CIRC( edgeuse , &i_fus->eu->l );
	else
		i_next_eu = RT_LIST_PNEXT_CIRC( edgeuse , &i_fus->eu->l );

	if( !j_fus->fu[0] )
		j_next_eu = RT_LIST_PPREV_CIRC( edgeuse , &j_fus->eu->l );
	else
		j_next_eu = RT_LIST_PNEXT_CIRC( edgeuse , &j_fus->eu->l );

	NMG_CK_EDGEUSE( i_next_eu );
	NMG_CK_EDGEUSE( j_next_eu );

	/* get endpoints for these edges */
	i_end = i_next_eu->eumate_p->vu_p->v_p;
	j_end = j_next_eu->eumate_p->vu_p->v_p;

	NMG_CK_VERTEX( i_end );
	NMG_CK_VERTEX( j_end );

	/* since the other end of these edges may not have been adjusted yet
	 * project the endpoints onto the face plane
	 */
	if( i_fus->fu[1] )
	{
		VJOIN1( i_end_pt , i_end->vg_p->coord , -(DIST_PT_PLANE( i_end->vg_p->coord , pl )) , pl )
		VJOIN1( j_end_pt , j_end->vg_p->coord , -(DIST_PT_PLANE( j_end->vg_p->coord , pl )) , pl )
	}
	else
	{
		VMOVE( i_end_pt , i_end->vg_p->coord )
		VMOVE( j_end_pt , j_end->vg_p->coord )
	}

	/* get start points, guaranteed to be on plane */
	i_start =  i_next_eu->vu_p->v_p;
	j_start =  j_next_eu->vu_p->v_p;

	NMG_CK_VERTEX( i_start );
	NMG_CK_VERTEX( j_start );

	/* calculate direction vectors for use by rt_isect_lseg3_lseg3 */
	VSUB2( i_dir , i_end_pt , i_start->vg_p->coord );
	VSUB2( j_dir , j_end_pt , j_start->vg_p->coord );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "nmg_dist_to_cross: checking edges x%x and x%x:\n" , i_fus , j_fus );
		rt_log( "\t( %f %f %f ) <-> ( %f %f %f )\n", V3ARGS( i_start->vg_p->coord ), V3ARGS( i_end_pt ) );
		rt_log( "\t( %f %f %f ) <-> ( %f %f %f )\n", V3ARGS( j_start->vg_p->coord ), V3ARGS( j_end_pt ) );
	}

	if( i_fus->free_edge && j_fus->free_edge )
	{
		fastf_t max_dist0;
		fastf_t max_dist1;
		int ret_val;

		if( rt_g.NMG_debug & DEBUG_BASIC )
			rt_log( "\tBoth are free edges\n" );

		max_dist0 = MAGNITUDE( i_dir );
		VSCALE( i_dir , i_dir , (1.0/max_dist0) )
		max_dist1 = MAGNITUDE( j_dir );
		VSCALE( j_dir , j_dir , (1.0/max_dist1) )

		/* check if these two edges intersect or pass near each other */
		if( (ret_val=rt_dist_line3_line3( dist , i_start->vg_p->coord , i_dir ,
			j_start->vg_p->coord , j_dir , tol )) >= 0 )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				rt_log( "max_dists = %f , %f\n" , max_dist0,max_dist1 );
				rt_log( "dist = %f , %f\n" , dist[0] , dist[1] );
			}

			/* if the closest approach or intersect point is
			 * within the edge endpoints, this is a real intersection
			 */
			if( dist[0] >= 0.0 && dist[0] <= max_dist0 &&
			    dist[1] >= 0.0 && dist[1] <= max_dist1 )
			{
				plane_t pl1,pl2,pl3;

				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					point_t tmp_pt;

					rt_log( "\t\tintersection!!\n" );
					VJOIN1( tmp_pt , i_start->vg_p->coord , dist[0] , i_dir );
					rt_log( "\t\t\t( %f %f %f )\n" , V3ARGS( tmp_pt ) );
					VJOIN1( tmp_pt , j_start->vg_p->coord , dist[1] , j_dir );
					rt_log( "\t\t\t( %f %f %f )\n" , V3ARGS( tmp_pt ) );
				}

				/* calculate the intersect point */
				NMG_GET_FU_PLANE( pl1 , j_fus->fu[1] );
				NMG_GET_FU_PLANE( pl2 , i_fus->fu[0] );
				VCROSS( pl3 , pl1 , pl2 );
				pl3[3] = VDOT( pl3 , i_fus->vp->vg_p->coord );
				rt_mkpoint_3planes( new_pt , pl1 , pl2 , pl3 );

				return( dist[0] );
			}
			else
				return( (fastf_t)(-1.0) );
		}
		else
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "ret_val = %d\n" , ret_val );

			return( (fastf_t)(-1.0) );
		}
	}
	else
	{
		/* check if these two edges intersect */
		if( rt_isect_lseg3_lseg3( dist , i_start->vg_p->coord , i_dir ,
			j_start->vg_p->coord , j_dir , tol ) == 1 )
		{
			fastf_t len0;

			len0 = MAGNITUDE( i_dir );

			/* calculate intersection point */
			if( dist[0] == 0.0 )
				VMOVE( new_pt , i_start->vg_p->coord )
			else if( dist[0] == 1.0 )
				VMOVE( new_pt , i_end_pt )
			else if( dist[1] == 0.0 )
				VMOVE( new_pt , j_start->vg_p->coord )
			else if( dist[1] == 1.0 )
				VMOVE( new_pt , j_end_pt )
			else
				VJOIN1( new_pt , i_start->vg_p->coord , dist[0] , i_dir )

			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\tdist=%f, new_pt=( %f %f %f )\n" , dist[0] , V3ARGS( new_pt ) );

			return( dist[0]*len0 );
		}
		else
			return( (fastf_t)(-1.0) );
	}
}

/*	N M G _ F I X _ C R O S S E D _ L O O P S
 *
 * Detect situations where edges have been split, but new vertices are
 * in wrong order. This typically happens as shown:
 *
 *                  new face planes
 *                  |
 *                  |
 *   \       \   /  |    /
 *    \       \ /<--|   /
 *     \       X       /
 *      \     / \     /
 *       \   /___\   /
 *        \         /
 *         \       /<- original face planes
 *          \     /
 *           \___/
 *
 * This can be detected by checking if the edges leaving from the new
 * vertices cross. If so, the middle face is deleted and the
 * two vertices are fused.
 *
 */
static void
nmg_fix_crossed_loops( new_v , int_faces , tol )
struct vertex *new_v;
struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_fix_crossed_loops( new_v=x%x ( %f %f %f ), %d edges)\n", new_v , V3ARGS( new_v->vg_p->coord ) , NMG_TBL_END( int_faces ) );

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( int_faces );
	RT_CK_TOL( tol );

	/* first check for edges that cross both adjacent edges */
	if( NMG_TBL_END( int_faces ) > 2 )
	{
		for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
		{
			int next_edge_no,prev_edge_no;
			struct intersect_fus *edge_fus;
			struct intersect_fus *next_fus,*prev_fus;
			fastf_t dist1,dist2;
			point_t pt1,pt2;

			edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

			if( !edge_fus->vp )
				continue;

			/* look at next edge */
			next_edge_no = edge_no + 1;
			if( next_edge_no == NMG_TBL_END( int_faces ) )
				next_edge_no = 0;

			next_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

			/* Don't want to fuse two dangling edges */
			if( next_fus->vp && (!edge_fus->free_edge || !next_fus->free_edge) )
				dist1 = nmg_dist_to_cross( edge_fus , next_fus , pt1 , tol );
			else
				dist1 = (-1.0);

			/* look at previous edge */
			prev_edge_no = edge_no - 1;
			if( prev_edge_no < 0 )
				prev_edge_no = NMG_TBL_END( int_faces ) - 1;

			prev_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , prev_edge_no );

			/* Don't want to fuse two dangling edges */
			if( prev_fus->vp && (!edge_fus->free_edge || !prev_fus->free_edge) )
				dist2 = nmg_dist_to_cross( edge_fus , prev_fus , pt2 , tol );
			else
				dist2 = (-1.0);

			/* if no intersections, continue */
			if( dist1 < tol->dist || dist2 < tol->dist )
				continue;

			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				rt_log( "fus=x%x, prev=x%x, next=x%x, dist1=%f, dist2=%f\n",
					edge_fus,next_fus,prev_fus,dist1,dist2 );
				rt_log( "\t( %f %f %f ), ( %f %f %f )\n" , V3ARGS( pt1 ) , V3ARGS( pt2 ) );
			}

			/* if both intersections are at the same point, merge all three */
			if( rt_pt3_pt3_equal( pt1 , pt2 , tol ) )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
					rt_log( "\tMerging all three points to pt1\n" );

				VMOVE( edge_fus->vp->vg_p->coord , pt1 );
				VMOVE( edge_fus->pt , pt1 );
				VMOVE( next_fus->vp->vg_p->coord , pt1 );
				VMOVE( next_fus->pt , pt1 );
				VMOVE( prev_fus->vp->vg_p->coord , pt1 );
				VMOVE( prev_fus->pt , pt1 );
			}
			else if( dist1 > dist2 )
			{
				/* merge edge point with next edge point */
				if( rt_g.NMG_debug & DEBUG_BASIC )
					rt_log( "\tMerging edge and next to pt1, moving prev to pt2\n");
				VMOVE( edge_fus->vp->vg_p->coord , pt1 );
				VMOVE( edge_fus->pt , pt1 );
				VMOVE( next_fus->vp->vg_p->coord , pt1 );
				VMOVE( next_fus->pt , pt1 );

				VMOVE( prev_fus->vp->vg_p->coord , pt2 );
				VMOVE( prev_fus->pt , pt2 );
			}
			else
			{
				/* merge edge point with previous point */
				if( rt_g.NMG_debug & DEBUG_BASIC )
					rt_log( "\tMerging edge and prev to pt2, moving next to pt1\n" );
				VMOVE( edge_fus->vp->vg_p->coord , pt2 );
				VMOVE( edge_fus->pt , pt2 );
				VMOVE( prev_fus->vp->vg_p->coord , pt2 );
				VMOVE( prev_fus->pt , pt2 );

				VMOVE( next_fus->vp->vg_p->coord , pt1 );
				VMOVE( next_fus->pt , pt1 );
			}
		}
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After fixing edges that intersect two edges:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	/* now look for edges that cross just a single adjacent edge */
	for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
	{
		int next_edge_no;
		struct intersect_fus *edge_fus;
		struct intersect_fus *next_fus;
		point_t pt;
		fastf_t dist;

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		if( !edge_fus->vp )
			continue;

		/* just look at next edge */
		next_edge_no = edge_no + 1;
		if( next_edge_no == NMG_TBL_END( int_faces ) )
			next_edge_no = 0;

		next_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , next_edge_no );

		if( !next_fus->vp )
			continue;

		/* check for intersection */
		dist = nmg_dist_to_cross( edge_fus , next_fus , pt , tol );

		if( dist > tol->dist )
		{
			/* there is an intersection */
			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				rt_log( "edge x%x intersect next edge x%x\n" , edge_fus , next_fus );
				rt_log( "\tdist=%f, ( %f %f %f )\n" , dist , V3ARGS( pt ) );
			}
			if( edge_fus->free_edge && next_fus->free_edge )
			{
				/* if both edges are free edges, move new_v to the intersection */
				VMOVE( edge_fus->vp->vg_p->coord , pt );
				VMOVE( edge_fus->pt , pt );
				VMOVE( next_fus->vp->vg_p->coord , pt );
				VMOVE( next_fus->pt , pt );
				VMOVE( new_v->vg_p->coord , pt );
			}
			else
			{
				/* just merge the two points */
				VMOVE( edge_fus->vp->vg_p->coord , pt );
				VMOVE( edge_fus->pt , pt );
				VMOVE( next_fus->vp->vg_p->coord , pt );
				VMOVE( next_fus->pt , pt );
			}
		}
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After nmg_fix_crossed_loops:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ C A L C _ N E W _ V
 *
 * Calculates a new geometry for new_v
 */
static void
nmg_calc_new_v( new_v , int_faces , tol )
struct vertex *new_v;
CONST struct nmg_ptbl *int_faces;
CONST struct rt_tol *tol;
{
	int edge_no;
	fastf_t edge_count=0.0;
	point_t ave_pt,prev_pt;
	struct intersect_fus *free_fus[2];
	struct intersect_fus *edge_fus;
	int free_edge_count=0;

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( int_faces );
	RT_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_calc_new_v\n" );

	edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , NMG_TBL_END( int_faces)-1 );
	if( edge_fus->vp )
		VMOVE( prev_pt , edge_fus->vp->vg_p->coord )
	else
		VMOVE( prev_pt , new_v->vg_p->coord );

	VSET( ave_pt , 0.0 , 0.0 , 0.0 )

	/* generally, just average the intersect_fus->vp's */
	for( edge_no=0 ; edge_no<NMG_TBL_END( int_faces ) ; edge_no++ )
	{

		edge_fus = (struct intersect_fus *)NMG_TBL_GET( int_faces , edge_no );

		if( edge_fus->free_edge )
		{
			free_fus[free_edge_count++] = edge_fus;
			if( free_edge_count > 2 )
				rt_bomb( "nmg_calc_new_v: Too many free edges\n" );
		}

		if( !edge_fus->vp )
			continue;

		if( !rt_pt3_pt3_equal( prev_pt , edge_fus->vp->vg_p->coord , tol ) )
		{
			VADD2( ave_pt , ave_pt , edge_fus->vp->vg_p->coord )
			edge_count += 1.0;
			VMOVE( prev_pt , edge_fus->vp->vg_p->coord )
		}
	}

	if( edge_count > 0.0 )
	{
		fastf_t scale;

		scale = 1.0/edge_count;
		VSCALE( ave_pt , ave_pt , scale )
	}
	else if( NMG_TBL_END( int_faces ) > 0 )
	{
		/* all the intersect vertices are within tolerance of prev_pt */
		VMOVE( ave_pt , prev_pt )
	}
	else
		rt_bomb( "nmg_calc_new_v: edge_count is zero\n" );

	/* if there are two free edges, new_v should be on the same plane as the free edges */
	if( free_edge_count == 2 )
		VBLEND2( new_v->vg_p->coord , 0.5 , free_fus[0]->vp->vg_p->coord , 0.5 , free_fus[1]->vp->vg_p->coord )
	else
		VMOVE( new_v->vg_p->coord , ave_pt )

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "After nmg_calc_new_v:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ C O M P L E X _ V E R T E X _ S O L V E
 *
 *	This is intended to handle the cases the "nmg_simple_vertex_solve"
 *	can't do (more than three faces intersecting at a vertex)
 *
 *	This routine may create new edges and/or faces and
 *
 *	Modifies the location of "new_v"
 *
 *	returns:
 *		0 - if everything is OK
 *		1 - failure
 */

int
nmg_complex_vertex_solve( new_v , faces , tol )
struct vertex *new_v;
CONST struct nmg_ptbl *faces;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct faceuse *fu1,*fu2,*fu;
	struct face *fp1;
	struct vertexuse *vu;
	struct edgeuse *eu1,*eu;
	struct nmg_ptbl int_faces;
	struct intersect_fus *k_fus;
	point_t ave_pt;
	vect_t ave_norm,x_vec,y_vec;
	point_t rpp_min;
	int i,j,done;
	int unique_verts;
	int added_faces;
	int edge_no;
	static int pl_count=0;

	/* More than 3 faces intersect at vertex (new_v)
	 * Calculate intersection point along each edge
	 * emanating from new_v */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_complex_vertex_solve( new_v = x%x , %d faces )\n" , new_v , NMG_TBL_END( faces ) );

	NMG_CK_VERTEX( new_v );
	NMG_CK_PTBL( faces );
	RT_CK_TOL( tol );

	m = nmg_find_model( &new_v->magic );

	VSET( rpp_min , 0.0 , 0.0 , 0.0 );

	nmg_tbl( &int_faces , TBL_INIT , (long *)NULL );

	/* get int_faces table (of intersect_fus structures) partially filled in
	 * with fu's, eu, and edge line definition
	 */
	if( nmg_get_edge_lines( new_v , &int_faces , tol ) )
	{
		nmg_tbl( &int_faces , TBL_FREE , (long *)NULL );
		return( 1 );
	}

	/* fill in "pt" portion of intersect_fus structures with points
	 * that are the intersections of the edge line with the other
	 * edges that meet at new_v. The intersection that is furthest
	 * up the edge away from new_v is selected
	 */
	if (nmg_get_max_edge_inters( new_v , &int_faces , faces , tol ) )
	{
		nmg_tbl( &int_faces , TBL_FREE , (long *)NULL );
		return( 1 );
	}

	/* split edges at intersection points */
	nmg_split_edges_at_pts( new_v , &int_faces , tol );

	/* fix intersection points that cause loops that cross themselves */
	nmg_fix_crossed_loops( new_v , &int_faces , tol );

	/* calculate geometry for new_v */
	nmg_calc_new_v( new_v , &int_faces , tol );

	nmg_remove_short_eus_inter( new_v , &int_faces , tol );

	nmg_simplify_inter( new_v , &int_faces , tol );

	/* Build needed faces */
	nmg_make_faces_at_vert( new_v , &int_faces , tol );

	/* Where faces were not built, cracks have formed */
	nmg_kill_cracks_at_vertex( new_v );

	/* free some memory */
	for( i=0 ; i<NMG_TBL_END( &int_faces ) ; i++ )
	{
		struct intersect_fus *i_fus;

		i_fus = (struct intersect_fus *)NMG_TBL_GET( &int_faces , i );
		rt_free( (char *)i_fus , "nmg_complex_vertex_solve: intersect_fus struct\n" );
	}
	nmg_tbl( &int_faces , TBL_FREE , (long *)NULL );
	return( 0 );
}

/*	N M G _ B A D _ F A C E _ N O R M A L S
 *
 *	Look for faceuses in the shell with normals that do
 *	not agree with the geometry (i.e., in the wrong direction)
 *
 *	return:
 *		1 - at least one faceuse with a bad normal was found
 *		0 - no faceuses with bad normals were found
 */
int
nmg_bad_face_normals( s , tol )
CONST struct shell *s;
CONST struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1,*eu2;
	vect_t old_normal;
	plane_t new_plane;
	vect_t a,b;
	point_t pa,pb,pc;

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		int done=0;
		fastf_t area;

		NMG_CK_FACEUSE( fu );

		/* only check OT_SAME faseuses */
		if( fu->orientation != OT_SAME )
			continue;

		/* get current normal */
		NMG_GET_FU_NORMAL( old_normal , fu );

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );

			if( lu->orientation != OT_SAME && lu->orientation != OT_OPPOSITE )
				continue;

			if( (area = nmg_loop_plane_area( lu , new_plane )) > 0.0 )
			{
				if( lu->orientation != OT_SAME )
					VREVERSE( new_plane , new_plane )
				break;
			}
		}

		if( area > 0.0 )
		{
			if( VDOT( old_normal , new_plane ) < 0.0 )
				return( 1 );
		}
	}
	return( 0 );
}

/*
 *	N M G _ F A C E S _ A R E _ R A D I A L
 *
 *	checks if two faceuses are radial to each other
 *
 *	returns
 *		1 - the two faceuses are radial to each other
 *		0 - otherwise
 *
 */
int
nmg_faces_are_radial( fu1 , fu2 )
CONST struct faceuse *fu1,*fu2;
{
	struct edgeuse *eu,*eu_tmp;
	struct loopuse *lu;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACEUSE( fu2 );

	/* look at every loop in the faceuse #1 */
	for( RT_LIST_FOR( lu , loopuse , &fu1->lu_hd ) )
	{
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		/* look at every edgeuse in the loop */
		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* now search radially around edge */
			eu_tmp = eu->eumate_p->radial_p;
			while( eu_tmp != eu && eu_tmp != eu->eumate_p )
			{
				struct faceuse *fu_tmp;

				/* find radial faceuse */
				fu_tmp = nmg_find_fu_of_eu( eu_tmp );

				/* if its the same as fu2 or its mate, the faceuses are radial */
				if( fu_tmp == fu2 || fu_tmp == fu2->fumate_p )
					return( 1 );

				/* go to next radial edgeuse */
				eu_tmp = eu_tmp->eumate_p->radial_p;
			}
		}
	}

	return( 0 );
}

/*	N M G _ M O V E _ E D G E _ T H R U _ P T
 *
 *	moves indicated edgeuse (mv_eu) so that it passes thru
 *	the given point (pt). The direction of the edgeuse
 *	is not changed, so new edgeuse is parallel to the original.
 *
 *	plane equations of all radial faces on this edge are changed
 *	and all vertices (except one anchor point) in radial loops are adjusted
 *	Note that the anchor point is chosen arbitrarily.
 *
 *	returns:
 *		1 - failure
 *		0 - success
 */

int
nmg_move_edge_thru_pt( mv_eu , pt , tol )
struct edgeuse *mv_eu;
CONST point_t pt;
CONST struct rt_tol *tol;
{
	struct nmgregion *r;
	struct faceuse *fu,*fu1;
	struct edgeuse *eu,*eu1;
	struct edge_g *eg;
	struct vertex *v1,*v2;
	struct model *m;
	vect_t e_dir;
	struct nmg_ptbl tmp_faces[2];
	struct nmg_ptbl faces;
	int count;
	long *flags;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_move_edge_thru_pt( mv_eu=x%x , pt=( %f %f %f) )\n" , mv_eu , V3ARGS( pt ) );

	NMG_CK_EDGEUSE( mv_eu );
	RT_CK_TOL( tol );

	m = nmg_find_model( &mv_eu->l.magic );
	NMG_CK_MODEL( m );

	/* get endpoint vertices */
	v1 = mv_eu->vu_p->v_p;
	NMG_CK_VERTEX( v1 );
	v2 = mv_eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX( v2 );

	/* get edge direction */
	eg = mv_eu->e_p->eg_p;
	if( eg )
	{
		VMOVE( e_dir , eg->e_dir );
		if( mv_eu->orientation == OT_OPPOSITE )
		{
			VREVERSE( e_dir , e_dir );
		}
	}
	else
	{
		VSUB2( e_dir , v2->vg_p->coord , v1->vg_p->coord );
		VUNITIZE( e_dir );
	}

	eu = mv_eu;
	fu1 = nmg_find_fu_of_eu( eu );

	if( fu1 == NULL )
	{
		vect_t to_pt;
		vect_t move_v;
		fastf_t edir_comp;
		point_t new_loc;

		/* This must be a wire edge, just adjust the endpoints */
		/* keep edge the same length, and move vertices perpendicular to e_dir */

		VSUB2( to_pt , pt , v1->vg_p->coord );
		edir_comp = VDOT( to_pt , e_dir );
		VJOIN1( move_v , to_pt , -edir_comp , e_dir );

		/* move the vertices */
		VADD2( new_loc , v1->vg_p->coord , move_v );
		nmg_vertex_gv( v1 , new_loc );

		VADD2( new_loc , v2->vg_p->coord , move_v );
		nmg_vertex_gv( v2 , new_loc );
		return( 0 );
	}

	/* can only handle edges with up to two radial faces */
	if( mv_eu->radial_p->eumate_p != mv_eu->eumate_p->radial_p && mv_eu->radial_p != mv_eu->eumate_p )
	{
		rt_log( "Cannot handle edges with more than two radial faces\n" );
		return( 1 );
	}

	nmg_tbl( &tmp_faces[0] , TBL_INIT , (long *)NULL );
	nmg_tbl( &tmp_faces[1] , TBL_INIT , (long *)NULL );

	/* cannot handle complex vertices yet */
	if( nmg_find_isect_faces( v1 , &tmp_faces[0] , &count , tol ) > 3 ||
	    nmg_find_isect_faces( v2 , &tmp_faces[1] , &count , tol ) > 3 )
	{
		rt_log( "nmg_move_edge_thru_pt: cannot handle complex vertices yet\n" );
		nmg_tbl( &tmp_faces[0] , TBL_FREE , (long *)NULL );
		nmg_tbl( &tmp_faces[1] , TBL_FREE , (long *)NULL );
		return( 1 );
	}

	/* Move edge geometry to new point */
	if( eg )
	{
		VMOVE( eg->e_pt , pt );
	}

	/* modify plane equation for each face radial to mv_eu */
	fu = fu1;
	do
	{
		struct edgeuse *eu_next;
		plane_t plane;
		int done;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_FACEUSE( fu );

		/* find an anchor point for face to rotate about
		 * go forward in loop until we find a vertex that is
		 * far enough from the line of mv_eu to produce a
		 * non-zero cross product
		 */
		eu_next = eu;
		done = 0;
		while( !done )
		{
			vect_t next_dir;
			struct vertex *anchor_v;
			fastf_t mag;

			/* get next edgeuse in loop */
			eu_next = RT_LIST_PNEXT_CIRC( edgeuse , &eu_next->l );

			/* check if we have circled the entire loop */
			if( eu_next == eu )
			{
				rt_log( "nmg_move_edge_thru_pt: cannot calculate new plane eqn\n" );
				return( 1 );
			}

			/* anchor point is endpoint of this edgeuse */
			anchor_v = eu_next->eumate_p->vu_p->v_p;

			/* calculate new plane */
			VSUB2( next_dir , anchor_v->vg_p->coord , pt );
			VCROSS( plane , e_dir , next_dir );
			mag = MAGNITUDE( plane );
			if( mag > SQRT_SMALL_FASTF )
			{
				/* this is an acceptable plane */
				mag = 1.0/mag;
				VSCALE( plane , plane , mag );
				plane[3] = VDOT( plane , pt );
				if( fu->orientation == OT_SAME )
					nmg_face_g( fu , plane );
				else
					nmg_face_g( fu->fumate_p , plane );
				done = 1;
			}
		}

		/* move on to next radial face */
		eu = eu->eumate_p->radial_p;
		fu = nmg_find_fu_of_eu( eu );
	}
	while( fu != fu1 && fu != fu1->fumate_p );

	/* now recalculate vertex coordinates for all affected vertices,
	 * could be lots of them
	 */

	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_move_edge_thru_pt: flags" );
	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	eu1 = mv_eu;
	fu1 = nmg_find_fu_of_eu( eu1 );
	fu = fu1;
	do
	{
		struct loopuse *lu;
		struct vertex *v;

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			{
				struct vertexuse *vu;
				vu = RT_LIST_FIRST( vertexuse , &lu->down_hd );
				if( NMG_INDEX_TEST_AND_SET( flags , vu->v_p ) )
				{
					nmg_tbl( &faces , TBL_RST , (long *)NULL );

					/* find all unique faces that intersect at this vertex (vu->v_p) */
					if( nmg_find_isect_faces( vu->v_p , &faces , &count , tol ) > 3 )
					{
						rt_log( "mg_move_edge_thru_pt: Cannot handle complex vertices\n" );
						nmg_tbl( &faces , TBL_FREE , (long *)NULL );
						rt_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
						return( 1 );
					}

					if( nmg_simple_vertex_solve( vu->v_p , &faces ) )
					{
						/* failed */
						rt_log( "nmg_move_edge_thru_pt: Could not solve simple vertex\n" );
						nmg_tbl( &faces , TBL_FREE , (long *)NULL );
						rt_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
						return( 1 );
					}
				}
				continue;
			}

			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct vertexuse *vu;

				vu = eu->vu_p;
				if( NMG_INDEX_TEST_AND_SET( flags , vu->v_p ) )
				{

					nmg_tbl( &faces , TBL_RST , (long *)NULL );

					/* find all unique faces that intersect at this vertex (vu->v_p) */
					if( nmg_find_isect_faces( vu->v_p , &faces , &count , tol ) > 3 )
					{
						rt_log( "mg_move_edge_thru_pt: Cannot handle complex vertices\n" );
						nmg_tbl( &faces , TBL_FREE , (long *)NULL );
						rt_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
						return( 1 );
					}

					if( nmg_simple_vertex_solve( vu->v_p , &faces ) )
					{
						/* failed */
						rt_log( "nmg_move_edge_thru_pt: Could not solve simple vertex\n" );
						nmg_tbl( &faces , TBL_FREE , (long *)NULL );
						rt_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
						return( 1 );
					}
				}
			}
		}

		/* move on to next radial face */
		eu1 = eu1->eumate_p->radial_p;
		fu = nmg_find_fu_of_eu( eu1 );
	}
	while( fu != fu1 && fu != fu1->fumate_p );

	rt_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

	return( 0 );
}

/*	N M G _ V L I S T _ T O _ W I R E _ E D G E S
 *
 *	Convert a vlist to NMG wire edges
 *
 */
void
nmg_vlist_to_wire_edges( s , vhead )
struct shell *s;
struct rt_list *vhead;
{
	struct rt_vlist *vp;
	struct edgeuse *eu;
	struct vertex *v1,*v2;
	point_t pt1,pt2;

	NMG_CK_SHELL( s );
	NMG_CK_LIST( vhead );

	v1 = (struct vertex *)NULL;
	v2 = (struct vertex *)NULL;

	vp = RT_LIST_FIRST( rt_vlist , vhead );
	if( vp->nused < 2 )
		return;

	for( RT_LIST_FOR( vp , rt_vlist , vhead ) )
	{
		register int i;
		register int nused = vp->nused;

		for( i=0 ; i<vp->nused ; i++ )
		{
			switch( vp->cmd[i] )
			{
				case RT_VLIST_LINE_MOVE:
				case RT_VLIST_POLY_MOVE:
					v1 = (struct vertex *)NULL;
					v2 = (struct vertex *)NULL;
					VMOVE( pt2 , vp->pt[i] );
					break;
				case RT_VLIST_LINE_DRAW:
				case RT_VLIST_POLY_DRAW:
					VMOVE( pt1 , pt2 );
					v1 = v2;
					VMOVE( pt2 , vp->pt[i] );
					v2 = (struct vertex *)NULL;
					eu = nmg_me( v1 , v2 , s );
					v1 = eu->vu_p->v_p;
					v2 = eu->eumate_p->vu_p->v_p;
					nmg_vertex_gv( v2 , pt2 );
					if( !v1->vg_p )
						nmg_vertex_gv( v1 , pt1 );
					break;
				case RT_VLIST_POLY_START:
				case RT_VLIST_POLY_END:
					break;
			}
		}
	}
}

void
nmg_follow_free_edges_to_vertex( vpa, vpb, bad_verts, s, eu, verts, found )
CONST struct vertex *vpa,*vpb;
struct nmg_ptbl *bad_verts;
CONST struct shell *s;
CONST struct edgeuse *eu;
struct nmg_ptbl *verts;
int *found;
{
	struct vertexuse *vu;

	NMG_CK_PTBL( bad_verts );
	NMG_CK_EDGEUSE( eu );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );
	if( s )
		NMG_CK_SHELL( s );

	NMG_CK_VERTEX( eu->eumate_p->vu_p->v_p );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "nmg_follow_free_edges_to_vertex( vpa=x%x, vpb=x%x s=x%x, eu=x%x, found=%d )\n",
			vpa,vpb,s,eu,*found );
	}

	for( RT_LIST_FOR( vu , vertexuse , &eu->eumate_p->vu_p->v_p->vu_hd ) )
	{
		struct edgeuse *eu1;

		NMG_CK_VERTEXUSE( vu );

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		if( s && (nmg_find_s_of_vu( vu ) != s ) )
			continue;

		eu1 = vu->up.eu_p;

		NMG_CK_EDGEUSE( eu1 );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			rt_log( "\tchecking eu x%x: x%x ( %f %f %f )\n\t\tto x%x ( %f %f %f )\n", eu1,
				eu1->vu_p->v_p, V3ARGS( eu1->vu_p->v_p->vg_p->coord ),
				eu1->eumate_p->vu_p->v_p, V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) );

		/* stick to free edges */
		if( eu1->eumate_p != eu1->radial_p )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tnot a dangling edge\n" );
			continue;
		}

		/* don`t go back the way we came */
		if( eu1 == eu->eumate_p )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tback the way we came\n" );
			continue;
		}

		if( eu1->eumate_p->vu_p->v_p == vpb )
		{
			/* found it!!! */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tfound goal\n" );
			nmg_tbl( verts , TBL_INS , (long *)vu->v_p );
			nmg_tbl( verts , TBL_INS , (long *)vpb );
			*found = 1;
		}
		else if( eu1->eumate_p->vu_p->v_p == vpa )
		{
			/* back where we started */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tback at start\n" );
			continue;
		}
		else if( nmg_tbl( bad_verts , TBL_LOC , (long *)eu1->eumate_p->vu_p->v_p ) != (-1))
		{
			/* this is the wrong way */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tA bad vertex\n" );
			continue;
		}
		else if( nmg_tbl( verts , TBL_LOC , (long *)eu1->eumate_p->vu_p->v_p ) != (-1))
		{
			/* This is a loop !!!! */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "a loop\n" );
			continue;
		}
		else
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tinserting vertex x%x\n" , vu->v_p );
			nmg_tbl( verts , TBL_INS , (long *)vu->v_p );
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tCalling follow edges\n" );
			nmg_follow_free_edges_to_vertex( vpa , vpb , bad_verts , s , eu1 , verts , found );
			if( *found < 0 )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					rt_log( "\t\treturn is %d\n" , *found );
					rt_log( "\t\t\tremove vertex x%x\n" , vu->v_p );
				}
				nmg_tbl( verts , TBL_RM , (long *)vu->v_p );
				*found = 0;
			}
		}
		if( *found )
			return;
	}

	*found = (-1);
}

static struct nmg_ptbl *
nmg_find_path( vpa, vpb, bad_verts, s )
CONST struct vertex *vpa,*vpb;
struct nmg_ptbl *bad_verts;
CONST struct shell *s;
{
	int done;
	static struct nmg_ptbl verts;
	struct vertexuse *vua;


	NMG_CK_PTBL( bad_verts );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		int i;

		rt_log( "nmg_find_path( vpa=x%x ( %f %f %f ), vpb=x%x ( %f %f %f )\n",
			vpa, V3ARGS( vpa->vg_p->coord ), vpb, V3ARGS( vpb->vg_p->coord ) );
		rt_log( "\t%d vertices to avoid\n" , NMG_TBL_END( bad_verts ) );
		for( i=0 ; i<NMG_TBL_END( bad_verts ) ; i++ )
		{
			struct vertex *vpbad;

			vpbad = (struct vertex *)NMG_TBL_GET( bad_verts , i );
			rt_log( "\tx%x ( %f %f %f )\n" , vpbad , V3ARGS( vpbad->vg_p->coord ) );
		}
	}

	nmg_tbl( &verts , TBL_INIT , (long *)NULL );
	nmg_tbl( &verts , TBL_INS , (long *)vpa );

	for( RT_LIST_FOR( vua , vertexuse , &vpa->vu_hd ) )
	{
		struct edgeuse *eua;

		NMG_CK_VERTEXUSE( vua );

		if( *vua->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		if( s && (nmg_find_s_of_vu( vua ) != s) )
			continue;

		eua = vua->up.eu_p;

		NMG_CK_EDGEUSE( eua );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			rt_log( "\tchecking eu x%x: x%x ( %f %f %f )\n\t\tto x%x ( %f %f %f )\n", eua,
				eua->vu_p->v_p, V3ARGS( eua->vu_p->v_p->vg_p->coord ),
				eua->eumate_p->vu_p->v_p, V3ARGS( eua->eumate_p->vu_p->v_p->vg_p->coord ) );

		if( eua->eumate_p != eua->radial_p )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tback the way we came!\n" );
			continue;
		}

		if( nmg_tbl( bad_verts , TBL_LOC , (long *)eua->eumate_p->vu_p->v_p ) != (-1) )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tOne of the bad vertices!!\n" );
			continue;
		}

		if( eua->eumate_p->vu_p->v_p == vpb )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "\t\tfound goal!!\n" );
			nmg_tbl( &verts , TBL_INS , (long *)vpb );
			return( &verts );
		}

		done = 0;
		if( rt_g.NMG_debug & DEBUG_BASIC )
			rt_log( "\tCall follow edges\n" );
		nmg_follow_free_edges_to_vertex( vpa, vpb, bad_verts, s, eua, &verts, &done );

		if( done == 1 )
			break;

		nmg_tbl( &verts , TBL_RST , ( long *)NULL );
		nmg_tbl( &verts , TBL_INS , (long *)vpa );
	}

	if( done != 1 )
		nmg_tbl( &verts , TBL_INIT , (long *)NULL );

	return( &verts );
}

void
nmg_glue_face_in_shell( fu , s , tol )
CONST struct faceuse *fu;
struct shell *s;
CONST struct rt_tol *tol;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE( fu );
	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		struct edgeuse *eu;

		NMG_CK_LOOPUSE( lu );
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			struct edgeuse *eu1;

			NMG_CK_EDGEUSE( eu );

			eu1 = nmg_findeu( eu->vu_p->v_p, eu->eumate_p->vu_p->v_p, s, eu, 1 );
			if( eu1 )
			{
				NMG_CK_EDGEUSE( eu1 );
				nmg_radial_join_eu( eu1, eu, tol );
			}
		}
	}
}

static void
nmg_make_connect_faces( dst , vpa , vpb , verts , tol )
struct shell *dst;
struct vertex *vpa,*vpb;
struct nmg_ptbl *verts;
CONST struct rt_tol *tol;
{
	int i;
	int verts_in_face=0;
	struct vertex *face_verts[20];
	int max_verts=20;
	int made_face;

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		rt_log( "nmg_make_connect_faces( dst=x%x, vpa=x%x ( %f %f %f ), vpb=x%x ( %f %f %f )\n",
				dst, vpa, V3ARGS( vpa->vg_p->coord ), vpb, V3ARGS( vpb->vg_p->coord ) );
		for( i=0 ; i<NMG_TBL_END( verts ) ; i++ )
		{
			struct vertex *v;

			v = (struct vertex *)NMG_TBL_GET( verts , i );
			rt_log( "\tx%x ( %f %f %f )\n" , v , V3ARGS( v->vg_p->coord ) );
		}
	}

	NMG_CK_SHELL( dst );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );
	NMG_CK_PTBL( verts );
	RT_CK_TOL( tol );

	face_verts[0] = (struct vertex *)NULL;
	for( i=0 ; i<NMG_TBL_END( verts ) ; i++ )
	{
		struct vertex *v;
		vect_t to_vpa,to_vpb;
		fastf_t dist_to_a_sq,dist_to_b_sq;

		v = (struct vertex *)NMG_TBL_GET( verts , i );
		NMG_CK_VERTEX( v );

		VSUB2( to_vpa , vpa->vg_p->coord , v->vg_p->coord );
		VSUB2( to_vpb , vpb->vg_p->coord , v->vg_p->coord );

		dist_to_a_sq = MAGSQ( to_vpa );
		dist_to_b_sq = MAGSQ( to_vpb );

		if( face_verts[0] == (struct vertex *)NULL )
		{
			if( dist_to_a_sq < dist_to_b_sq )
				face_verts[0] = vpa;
			else
				face_verts[0] = vpb;

			face_verts[1] = v;
			verts_in_face = 2;
		}
		else
		{
			face_verts[verts_in_face++] = v;
				
			if( !rt_3pts_collinear( face_verts[0]->vg_p->coord,
				face_verts[1]->vg_p->coord,
				face_verts[verts_in_face - 1]->vg_p->coord, tol) )
			{
				struct faceuse *new_fu;
				struct loopuse *lu;
				struct edgeuse *eu;

				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					int debug_int;

					rt_log( "make face:\n" );
					for( debug_int=0 ; debug_int<verts_in_face ; debug_int++ )
						rt_log( "\tx%x ( %f %f %f )\n" , face_verts[debug_int],
							V3ARGS( face_verts[debug_int]->vg_p->coord ) );
				}

				new_fu = nmg_cface( dst , face_verts , verts_in_face );
				if( nmg_fu_planeeqn( new_fu , tol ) )
					rt_bomb( "nmg_make_connect_faces: Failed to calculate plane eqn\n" );

				made_face = 1;

				/* glue this face in */
				nmg_glue_face_in_shell( new_fu , dst , tol );

				if( dist_to_b_sq <= dist_to_a_sq && face_verts[0] == vpa )
				{
					/* make middle face */
					face_verts[1] = v;
					face_verts[2] = vpb;

					if( rt_g.NMG_debug & DEBUG_BASIC )
					{
						int debug_int;

						rt_log( "make middle face:\n" );
						for( debug_int=0 ; debug_int<verts_in_face ; debug_int++ )
							rt_log( "\tx%x ( %f %f %f )\n" , face_verts[debug_int],
								V3ARGS( face_verts[debug_int]->vg_p->coord ) );
					}

					new_fu = nmg_cface( dst , face_verts , 3 );
					if( nmg_fu_planeeqn( new_fu , tol ) )
						rt_bomb( "nmg_make_connect_faces: Failed to calculate plane eqn\n" );

					nmg_glue_face_in_shell( new_fu , dst , tol );
				}

				/* get ready for next face, if necessary */
				if( i < NMG_TBL_END( verts ) )
				{
					if( dist_to_a_sq < dist_to_b_sq )
						face_verts[0] = vpa;
					else
						face_verts[0] = vpb;

					face_verts[1] = v;
					verts_in_face = 2;
				}
			}
			else
				made_face = 0;
		}
	}

	if( !made_face )
	{
		struct vertex *v1,*v2;
		struct edgeuse *eu;
		struct edgeuse *new_eu;

		/* didn't make last face, must be collinear points
		 * so split an edge to get the last vertex in
		 */

		v1 = (struct vertex *)NMG_TBL_GET( verts , NMG_TBL_END( verts ) - 2 );
		NMG_CK_VERTEX( v1 );

		v2 = (struct vertex *)NMG_TBL_GET( verts , NMG_TBL_END( verts ) - 1 );
		NMG_CK_VERTEX( v2 );

		eu = nmg_findeu( vpb , v1 , dst , (struct edgeuse *)NULL , 1 );
		if( !eu )
		{
			rt_log( "nmg_make_connect_faces: Cannot find eu from x%x to x%x\n" , vpb , v1 );
			rt_bomb( "\n" );
		}

		new_eu = nmg_esplit( v2 , eu );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			rt_log( "Split eu x%x (x%x -> x%x) at vertex x%x\n" , eu , eu->vu_p->v_p , eu->eumate_p->vu_p->v_p, v2 );
	}
}

/*	N M G _ O P E N _ S H E L L S _ C O N N E C T
 *
 *	Two open shells are connected along their free edges by building
 *	new faces.  The resluting closed shell is in "dst", and "src" shell
 *	is destroyed.  The "copy_tbl" is a translation table that provides
 *	a one-to-one translation between the vertices in the two shells, i.e.,
 *	NMG_INDEX_GETP(vertex, copy_tbl, v), where v is a pointer to a vertex
 *	in "dst" shell, provides a pointer to the corresponding vertex in "src" shell
 *
 *	returns:
 *		0 - All is well
 *		1 - failure
 */

/* 	structure for use by nmg_open_shells_connect */
struct dangle
{
	struct vertex *va,*vb;		/* vertices of a dangling edge in dst shell */
	struct vertex *v1,*v2;		/* corresponding vertices in src shell */
	struct nmg_ptbl bad_verts;	/* list of vertices to avoid when finding path
					 * from v1 to v2 */
};

int
nmg_open_shells_connect( dst , src , copy_tbl , tol )
struct shell *dst;
struct shell *src;
CONST long **copy_tbl;
CONST struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct nmg_ptbl faces;
	struct nmg_ptbl dangles;
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_open_shells_connect( dst=x%x , src=x%x , copy_tbl=x%x)\n" , dst , src , copy_tbl );

	NMG_CK_SHELL( dst );
	NMG_CK_SHELL( src );
	RT_CK_TOL( tol );

	if( nmg_ck_closed_surf( dst , tol ) )
	{
		rt_log( "nmg_open_shells_connect: destination shell is closed!\n" );
		return( 1 );
	}

	if( nmg_ck_closed_surf( src , tol ) )
	{
		rt_log( "nmg_open_shells_connect: source shell is closed!\n" );
		return( 1 );
	}

	nmg_tbl( &dangles , TBL_INIT , (long *)NULL );

	/* find free edges in "dst" shell */
	for( RT_LIST_FOR( fu , faceuse , &dst->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation != OT_SAME )
			continue;
		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
				continue;

			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				NMG_CK_EDGEUSE( eu );
				if( eu->eumate_p == eu->radial_p )
				{
					struct dangle *dang;
					struct vertexuse *test_vu;
					struct edgeuse *check_eu;
					struct vertex *vpbad1,*vpbada;
					int i;

					/* found a dangling edge, put it in the list */
					dang = (struct dangle *)rt_malloc( sizeof( struct dangle ) ,
						"nmg_open_shells_connect: dang" );

					dang->va = eu->vu_p->v_p;
					NMG_CK_VERTEX( dang->va );
					dang->v1 = NMG_INDEX_GETP(vertex, copy_tbl, dang->va );
					NMG_CK_VERTEX( dang->v1 );
					dang->vb = eu->eumate_p->vu_p->v_p;
					NMG_CK_VERTEX( dang->vb );
					dang->v2 = NMG_INDEX_GETP(vertex, copy_tbl, dang->vb );
					NMG_CK_VERTEX( dang->v2 );

					nmg_tbl( &dang->bad_verts , TBL_INIT , (long *)NULL );

					/* look for other dangling edges around this one
					 * to establish direction for nmg_find_path
					 * to look in
					 */

					for( RT_LIST_FOR( test_vu , vertexuse , &dang->va->vu_hd ) )
					{
						struct edgeuse *test_eu;

						if( *test_vu->up.magic_p != NMG_EDGEUSE_MAGIC )
							continue;

						test_eu = test_vu->up.eu_p;
						if( test_eu == eu )
							continue;

						if( test_eu->eumate_p->vu_p->v_p == dang->vb )
							continue;

						if( test_eu->eumate_p == test_eu->radial_p )
						{
							/* another dangling edge, don't want
							 * nmg_find_path to wander off
							 * in this direction
							 */
							vpbada = test_eu->eumate_p->vu_p->v_p;
							vpbad1 = NMG_INDEX_GETP( vertex , copy_tbl , vpbada );
							if( vpbad1 )
								nmg_tbl( &dang->bad_verts , TBL_INS , (long *)vpbad1 );
						}
					}

					for( RT_LIST_FOR( test_vu , vertexuse , &dang->vb->vu_hd ) )
					{
						struct edgeuse *test_eu;

						if( *test_vu->up.magic_p != NMG_EDGEUSE_MAGIC )
							continue;

						test_eu = test_vu->up.eu_p;
						if( test_eu == eu->eumate_p )
							continue;


						if( test_eu->eumate_p->vu_p->v_p == dang->va )
							continue;

						if( test_eu->eumate_p == test_eu->radial_p )
						{
							/* another dangling edge, don't want
							 * nmg_find_path to wander off
							 * in this direction
							 */
							vpbada = test_eu->eumate_p->vu_p->v_p;
							vpbad1 = NMG_INDEX_GETP( vertex , copy_tbl , vpbada );
							if( vpbad1 )
								nmg_tbl( &dang->bad_verts , TBL_INS , (long *)vpbad1 );
						}
					}
				nmg_tbl( &dangles , TBL_INS , (long *)dang );
				}
			}
		}
	}

	/* now build the faces to connect the dangling edges */
	while( NMG_TBL_END( &dangles ) )
	{
		struct dangle *dang;
		struct nmg_ptbl *verts;

		dang = (struct dangle *)NMG_TBL_GET( &dangles , NMG_TBL_END( &dangles ) - 1 );

		/* find vertices between vp1 and vp2 */
		verts = nmg_find_path( dang->v1 , dang->v2 , &dang->bad_verts , src );

		/* make faces connecting the two shells */
		if( NMG_TBL_END( verts ) > 1 )
			nmg_make_connect_faces( dst , dang->va , dang->vb , verts , tol );
		else
		{
			rt_log( "nmg_open_shells_connect: unable to make connecting face\n" );
			rt_log( "\tfor edge from x%x ( %f %f %f )\n\t\tto x%x ( %f %f %f )\n",
				dang->va, V3ARGS( dang->va->vg_p->coord ),
				dang->vb, V3ARGS( dang->vb->vg_p->coord ) );
		}

		nmg_tbl( verts , TBL_FREE , (long *)NULL );
		nmg_tbl( &dang->bad_verts , TBL_FREE , (long *)NULL );
		nmg_tbl( &dangles , TBL_RM , (long *)dang );
		rt_free( (char *)dang , "nmg_open_shells_connect: dang" );
	}

	nmg_js( dst , src , tol );

	/* now glue it all together */
	nmg_tbl( &faces , TBL_INIT , (long *)NULL );
	for( RT_LIST_FOR( fu , faceuse , &dst->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			nmg_tbl( &faces , TBL_INS , (long *)fu );
	}
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

	return( 0 );
}

/*	N M G _ I N _ V E R T
 *
 *	Move vertex so it is at the intersection of the newly created faces
 *
 *	This routine is used by "nmg_extrude_shell" to move vertices. Each
 *	plane has already been moved a distance inward and
 *	the surface normals have been reversed.
 *
 */
int
nmg_in_vert( new_v , tol )
struct vertex *new_v;
CONST struct rt_tol *tol;
{
	struct model *m;
	struct nmg_ptbl faces;
	int failed=0;
	int free_edges=0;
	int face_count;
	int i;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		rt_log( "nmg_in_vert( new_v=x%x )\n" , new_v );

	NMG_CK_VERTEX( new_v );
	RT_CK_TOL( tol );

	m = nmg_find_model( &new_v->magic );

	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	/* find all unique faces that intersect at this vertex (new_v) */
	face_count = nmg_find_isect_faces( new_v , &faces , &free_edges , tol );
	if( (face_count < 4 && !free_edges) || face_count < 3 )
	{
		if( nmg_simple_vertex_solve( new_v , &faces ) )
		{
			failed = 1;
			rt_log( "Could not solve simple vertex\n" );
		}
	}
	else
	{
		if( nmg_complex_vertex_solve( new_v , &faces , tol ) )
		{
			failed = 1;
			rt_log( "Could not solve complex vertex\n" );
		}
	}

	/* Free memory */
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

	return( failed );
}

/*		N M G _ M I R R O R _ M O D E L
 *
 *	mirror model across the y-axis
 *	this does not copy the model, it changes the
 *	model passed to it
 */

void
nmg_mirror_model( m )
struct model *m;
{
	struct nmg_ptbl vertices;
	struct nmgregion *r;
	int i;
	long *flags;

	NMG_CK_MODEL( m );

	/* mirror all vertices across the y axis */
	nmg_vertex_tabulate( &vertices , &m->magic );

	for( i=0 ; i<NMG_TBL_END( &vertices ) ; i++ )
	{
		struct vertex *v;

		v = (struct vertex *)NMG_TBL_GET( &vertices , i );
		NMG_CK_VERTEX( v );

		v->vg_p->coord[Y] = (-v->vg_p->coord[Y]);
	}
	(void)nmg_tbl( &vertices , TBL_FREE , (long *)NULL );

	/* adjust the direction of all the faces */
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_mirror_model: flags" );
	for( RT_LIST_FOR( r , nmgregion , &m->r_hd ) )
	{
		struct shell *s;

		for( RT_LIST_FOR( s , shell , &r->s_hd ) )
		{
			struct faceuse *fu;

			for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
			{
				int orientation;

				if( NMG_INDEX_TEST_AND_SET( flags , fu ) )
				{
					/* switch orientations of all faceuses */
					orientation = fu->orientation;
					fu->orientation = fu->fumate_p->orientation;
					fu->fumate_p->orientation = orientation;
					NMG_INDEX_SET( flags , fu->fumate_p );

					if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p->fg_p ) )
					{
						/* correct normal vector */
						fu->f_p->fg_p->N[Y] = (-fu->f_p->fg_p->N[Y]);
					}
				}
			}
		}
	}

	rt_free( (char *)flags , "nmg_mirror_model: flags " );
}

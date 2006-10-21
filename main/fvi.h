/* $Id: fvi.h,v 1.2 2003/10/10 09:36:35 btb Exp $ */
/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Header for fvi.c
 *
 * Old Log:
 * Revision 1.2  1995/08/23  21:34:29  allender
 * fix mcc compiler warning
 *
 * Revision 1.1  1995/05/16  15:56:38  allender
 * Initial revision
 *
 * Revision 2.1  1995/03/20  18:15:58  john
 * Added code to not store the normals in the tSegment structure.
 *
 * Revision 2.0  1995/02/27  11:32:02  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 *
 * Revision 1.10  1995/02/02  14:07:58  matt
 * Fixed confusion about which tSegment you are touching when you're
 * touching a wall.  This manifested itself in spurious lava burns.
 *
 * Revision 1.9  1994/12/04  22:48:04  matt
 * Physics & FVI now only build seglist for player objects, and they
 * responsilby deal with buffer full conditions
 *
 * Revision 1.8  1994/10/31  12:28:01  matt
 * Added new function ObjectIntersectsWall()
 *
 * Revision 1.7  1994/10/10  13:10:00  matt
 * Increased max_fvi_segs
 *
 * Revision 1.6  1994/09/25  00:38:29  matt
 * Made the 'find the point in the bitmap where something hit' system
 * publicly accessible.
 *
 * Revision 1.5  1994/08/01  13:30:35  matt
 * Made fvi() check holes in transparent walls, and changed fvi() calling
 * parms to take all input data in query structure.
 *
 * Revision 1.4  1994/07/13  21:47:59  matt
 * FVI() and physics now keep lists of segments passed through which the
 * tTrigger code uses.
 *
 * Revision 1.3  1994/07/08  14:27:26  matt
 * Non-needed powerups don't get picked up now; this required changing FVI to
 * take a list of ingore objects rather than just one ignore tObject.
 *
 * Revision 1.2  1994/06/09  09:58:39  matt
 * Moved FindVectorIntersection() from physics.c to new file fvi.c
 *
 * Revision 1.1  1994/06/09  09:26:14  matt
 * Initial revision
 *
 *
 */


#ifndef _FVI_H
#define _FVI_H

#include "vecmat.h"
#include "segment.h"
#include "object.h"

//return values for FindVectorIntersection() - what did we hit?
#define HIT_NONE		0		//we hit nothing
#define HIT_WALL		1		//we hit - guess - a wall
#define HIT_OBJECT	2		//we hit an tObject - which one?  no way to tell...
#define HIT_BAD_P0	3		//start point not is specified tSegment

#define MAX_FVI_SEGS 100

typedef struct fvi_hit_info {
	int 			nType;						//what sort of intersection
	short 		nSegment;					//what tSegment hit_pnt is in
	short			nSegment2;
	short 		nSide;						//if hit wall, which tSide
	short 		nSideSegment;				//what tSegment the hit tSide is in
	short 		nObject;						//if tObject hit, which tObject
	vmsVector	vPoint;						//where we hit
	vmsVector 	vNormal;						//if hit wall, ptr to its surface normal
	int			nNestCount;
} fvi_hit_info;

//this data structure gets filled in by FindVectorIntersection()
typedef struct fvi_info {
	fvi_hit_info	hit;
	short 			nSegments;					//how many segs we went through
	short 			segList [MAX_FVI_SEGS];	//list of segs vector went through
} fvi_info;

//flags for fvi query
#define FQ_CHECK_OBJS	1		//check against objects?
#define FQ_TRANSWALL		2		//go through transparent walls
#define FQ_TRANSPOINT	4		//go through trans wall if hit point is transparent
#define FQ_GET_SEGLIST	8		//build a list of segments
#define FQ_IGNORE_POWERUPS	16		//ignore powerups
#define FQ_SEE_OBJS		32

//this data contains the parms to fvi()
typedef struct fvi_query {
	vmsVector *p0,*p1;
	short			startSeg;
	fix			rad;
	short			thisObjNum;
	short			*ignoreObjList;
	int			flags;
} fvi_query;

//Find out if a vector intersects with anything.
//Fills in hit_data, an fvi_info structure (see above).
//Parms:
//  p0 & startseg 	describe the start of the vector
//  p1 					the end of the vector
//  rad 					the radius of the cylinder
//  thisobjnum 		used to prevent an tObject with colliding with itself
//  ingore_obj_list	NULL, or ptr to a list of objnums to ignore, terminated with -1
//  check_objFlag	determines whether collisions with objects are checked
//Returns the hit_data->hitType
int FindVectorIntersection(fvi_query *fq,fvi_info *hit_data);

//finds the uv coords of the given point on the given seg & tSide
//fills in u & v. if l is non-NULL fills it in also
void FindHitPointUV(fix *u,fix *v,fix *l, vmsVector *pnt,tSegment *seg,int nSide,int facenum);

//Returns true if the tObject is through any walls
int ObjectIntersectsWall(tObject *objp);

int PixelTranspType (short nTexture, short nOrient, fix u, fix v);	//-1: supertransp., 0: opaque, 1: transparent

#endif


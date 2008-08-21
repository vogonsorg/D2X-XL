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

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <stdio.h>		//	for printf ()
#include <stdlib.h>		// for d_rand () and qsort ()
#include <string.h>		// for memset ()

#include "inferno.h"
#include "mono.h"
#include "u_mem.h"

#include "error.h"
#include "physics.h"
#include "gameseg.h"

#ifdef EDITOR
#include "editor/editor.h"
#endif

#define	PARALLAX	0		//	If !0, then special debugging for Parallax eyes enabled.

//	Length in segments of avoidance path
#define	AVOID_SEG_LENGTH	7
//#define _DEBUG
#ifndef _DEBUG
#	define	PATH_VALIDATION	0
#else
#	define	PATH_VALIDATION	1
#endif

void AIPathSetOrientAndVel (tObject *objP, vmsVector* vGoalPoint, int playerVisibility, vmsVector *vec_to_player);
void MaybeAIPathGarbageCollect (void);
void AIPathGarbageCollect (void);
#if PATH_VALIDATION
void ValidateAllPaths (void);
int ValidatePath (int debugFlag, tPointSeg* pointSegP, int numPoints);
#endif

//	------------------------------------------------------------------------

void CreateRandomXlate (sbyte *xt)
{
	int	i, j;
	sbyte h;

for (i = 0; i < MAX_SIDES_PER_SEGMENT; i++)
	xt [i] = i;
for (i = 0; i<MAX_SIDES_PER_SEGMENT; i++) {
	j = (d_rand () * MAX_SIDES_PER_SEGMENT) / (D_RAND_MAX+1);
	Assert ((j >= 0) && (j < MAX_SIDES_PER_SEGMENT));
	h = xt [j];
	xt [j] = xt [i];
	xt [i] = h;
	}
}

//	-----------------------------------------------------------------------------------------------------------

tPointSeg *InsertTransitPoint (tPointSeg *curSegP, tPointSeg *predSegP, tPointSeg *succSegP, ubyte nConnSide)
{
	vmsVector	vCenter, vPoint;
	short			nSegment;

COMPUTE_SIDE_CENTER (&vCenter, gameData.segs.segments + predSegP->nSegment, nConnSide);
vPoint = predSegP->point - vCenter;
vPoint[X] /= 16;
vPoint[Y] /= 16;
vPoint[Z] /= 16;
curSegP->point = vCenter - vPoint;
nSegment = FindSegByPos (curSegP->point, succSegP->nSegment, 1, 0);
if (nSegment == -1) {
#if TRACE
	con_printf (1, "Warning: point not in ANY tSegment in aipath.c/InsertCenterPoints().\n");
#endif
	curSegP->point = vCenter;
	FindSegByPos (curSegP->point, succSegP->nSegment, 1, 0);
	}
curSegP->nSegment = succSegP->nSegment;
return curSegP;
}

//	-----------------------------------------------------------------------------------------------------------

int OptimizePath (tPointSeg *pointSegP, int nSegs)
{
	int			i, j;
	vmsVector	temp1, temp2;
	fix			dot, mag1, mag2 = 0;

for (i = 1; i < nSegs - 1; i += 2) {
	if (i == 1) {
		temp1 = pointSegP [i].point - pointSegP [i-1].point;
		mag1 = temp1.mag();
		}
	else {
		temp1 = temp2;
		mag1 = mag2;
		}
	temp2 = pointSegP [i+1].point - pointSegP [i].point;
	mag2 = temp1.mag();
	dot = vmsVector::dot(temp1, temp2);
	if (dot * 9/8 > FixMul (mag1, mag2))
		pointSegP [i].nSegment = -1;
	}
//	Now, scan for points with nSegment == -1
for (i = j = 0; i < nSegs; i++)
	if (pointSegP [i].nSegment != -1)
		pointSegP [j++] = pointSegP [i];
return j;
}

//	-----------------------------------------------------------------------------------------------------------
//	Insert the point at the center of the tSide connecting two segments between the two points.
// This is messy because we must insert into the list.  The simplest (and not too slow) way to do this is to start
// at the end of the list and go backwards.
int InsertCenterPoints (tPointSeg *pointSegP, int numPoints)
{
	int	i, j;

for (i = 0; i < numPoints; i++) {
	j = i + 2;
	InsertTransitPoint (pointSegP + i + 1, pointSegP + i, pointSegP + j, pointSegP [j].nConnSide);
	}
return OptimizePath (pointSegP, numPoints);
}

#ifdef EDITOR
int	SafetyFlag_override = 0;
int	RandomFlag_override = 0;
int	Ai_path_debug=0;
#endif

//	-----------------------------------------------------------------------------------------------------------
//	Move points halfway to outside of tSegment.
static tPointSeg newPtSegs [MAX_SEGMENTS_D2X];

void MoveTowardsOutside (tPointSeg *ptSegs, int *nPoints, tObject *objP, int bRandom)
{
	int			i, j;
	int			nNewSeg;
	fix			xSegSize;
	int			nSegment;
	vmsVector	a, b, c, d, e;
	vmsVector	vGoalPos;
	int			count;
	int			nTempSeg;
	tFVIQuery	fq;
	tFVIData		hit_data;
	int			nHitType;

j = *nPoints;
if (j > MAX_SEGMENTS)
	j = MAX_SEGMENTS;
for (i = 1, --j; i < j; i++) {
	// -- ptSegs [i].nSegment = FindSegByPos (&ptSegs [i].point, ptSegs [i].nSegment, 0);
	nTempSeg = FindSegByPos(ptSegs [i].point, ptSegs [i].nSegment, 1, 0);
	Assert (nTempSeg != -1);
	if (nTempSeg < 0) {
		break;
		}
	ptSegs [i].nSegment = nTempSeg;
	nSegment = ptSegs [i].nSegment;

	if (i == 1) {
		a = ptSegs [i].point - ptSegs [i-1].point;
		vmsVector::normalize(a);
		}
	else
		a = b;
	b = ptSegs [i+1].point - ptSegs [i].point;
	c = ptSegs [i+1].point - ptSegs [i-1].point;
	//	I don't think we can use quick version here and this is _very_ rarely called. --MK, 07/03/95
	vmsVector::normalize(b);
	if (abs (vmsVector::dot(a, b)) > 3*F1_0/4) {
		if (abs (a[Z]) < F1_0/2) {
			if (bRandom) {
				e[X] = (d_rand ()- 16384) / 2;
				e[Y] = (d_rand ()- 16384) / 2;
				e[Z] = abs (e[X]) + abs (e[Y]) + 1;
				vmsVector::normalize(e);
				}
			else {
				e[X] =
				e[Y] = 0;
				e[Z] = F1_0;
				}
			}
		else {
			if (bRandom) {
				e[Y] = (d_rand ()-16384)/2;
				e[Z] = (d_rand ()-16384)/2;
				e[X] = abs (e[Y]) + abs (e[Z]) + 1;
				vmsVector::normalize(e);
				}
			else {
				e[X] = F1_0;
				e[Y] =
				e[Z] = 0;
				}
			}
		}
	else {
		d = vmsVector::cross(a, b);
		e = vmsVector::cross(c, d);
		vmsVector::normalize(e);
		}
#ifdef _DEBUG
	if (VmVecMag (&e) < F1_0/2)
		Int3 ();
#endif
	xSegSize = vmsVector::dist(gameData.segs.vertices [gameData.segs.segments [nSegment].verts [0]], gameData.segs.vertices [gameData.segs.segments [nSegment].verts [6]]);
	if (xSegSize > F1_0*40)
		xSegSize = F1_0*40;
	vGoalPos = ptSegs [i].point + e * (xSegSize/4);
	count = 3;
	while (count) {
		fq.p0					= &ptSegs [i].point;
		fq.startSeg			= ptSegs [i].nSegment;
		fq.p1					= &vGoalPos;
		fq.radP0				=
		fq.radP1				= objP->size;
		fq.thisObjNum		= OBJ_IDX (objP);
		fq.ignoreObjList	= NULL;
		fq.flags				= 0;
		nHitType = FindVectorIntersection (&fq, &hit_data);
		if (nHitType == HIT_NONE)
			count = 0;
		else {
			if ((count == 3) && (nHitType == HIT_BAD_P0))
				return;
			vGoalPos[X] = ((*fq.p0)[X] + hit_data.hit.vPoint[X])/2;
			vGoalPos[Y] = ((*fq.p0)[Y] + hit_data.hit.vPoint[Y])/2;
			vGoalPos[Z] = ((*fq.p0)[Z] + hit_data.hit.vPoint[Z])/2;
			count--;
			if (count == 0)	//	Couldn't move towards outside, that's ok, sometimes things can't be moved.
				vGoalPos = ptSegs [i].point;
			}
		}
	//	Only move towards outside if remained inside tSegment.
	nNewSeg = FindSegByPos (vGoalPos, ptSegs [i].nSegment, 1, 0);
	if (nNewSeg == ptSegs [i].nSegment) {
		newPtSegs [i].point = vGoalPos;
		newPtSegs [i].nSegment = nNewSeg;
		}
	else {
		newPtSegs [i].point = ptSegs [i].point;
		newPtSegs [i].nSegment = ptSegs [i].nSegment;
		}
	}
if (j > 1)
	memcpy (ptSegs + 1, newPtSegs + 1, (j - 1) * sizeof (tPointSeg));
}


//	-----------------------------------------------------------------------------------------------------------
//	Create a path from objP->position.vPos to the center of nEndSeg.
//	Return a list of (segment_num, point_locations) at pointSegP
//	Return number of points in *numPoints.
//	if nMaxDepth == -1, then there is no maximum depth.
//	If unable to create path, return -1, else return 0.
//	If randomFlag !0, then introduce randomness into path by looking at sides in random order.  This means
//	that a path between two segments won't always be the same, unless it is unique.p.
//	If bSafeMode is set, then additional points are added to "make sure" that points are reachable.p.  I would
//	like to say that it ensures that the tObject can move between the points, but that would require knowing what
//	the tObject is (which isn't passed, right?) and making fvi calls (slow, right?).  So, consider it the more_or_less_safeFlag.
//	If nEndSeg == -2, then end seg will never be found and this routine will drop out due to depth (probably called by CreateNSegmentPath).
int CreatePathPoints (tObject *objP, int nStartSeg, int nEndSeg, tPointSeg *pointSegP, short *numPoints,
							  int nMaxDepth, int bRandom, int bSafeMode, int nAvoidSeg)
{
	short				nCurSeg;
	short				nSide, hSide;
	int				qTail = 0, qHead = 0;
	int				h, i, j;
	sbyte				bVisited [MAX_SEGMENTS_D2X];
	segQueueEntry	segmentQ [MAX_SEGMENTS_D2X];
	short				depth [MAX_SEGMENTS_D2X];
	int				nCurDepth;
	sbyte				randomXlate [MAX_SIDES_PER_SEGMENT];
	tPointSeg		*origPointSegs = pointSegP;
	int				lNumPoints;
	tSegment			*segP;
	vmsVector		vCenter;
	int				nThisSeg, nParentSeg;
	tFVIQuery		fq;
	tFVIData			hit_data;
	int				hitType;
	int				bAvoidPlayer;

#if PATH_VALIDATION
ValidateAllPaths ();
#endif

if ((objP->nType == OBJ_ROBOT) && (objP->cType.aiInfo.behavior == AIB_RUN_FROM)) {
	if (nAvoidSeg != -32767) {
		bRandom = 1;
		nAvoidSeg = gameData.objs.console->nSegment;
		}
	// Int3 ();
	}
bAvoidPlayer = gameData.objs.console->nSegment == nAvoidSeg;
if (nMaxDepth == -1)
	nMaxDepth = MAX_PATH_LENGTH;
lNumPoints = 0;
memset (bVisited, 0, sizeof (bVisited [0])* (gameData.segs.nLastSegment+1));
memset (depth, 0, sizeof (depth [0])* (gameData.segs.nLastSegment+1));
//	If there is a tSegment we're not allowed to visit, mark it.
if (nAvoidSeg != -1) {
	Assert (nAvoidSeg <= gameData.segs.nLastSegment);
	if ((nStartSeg != nAvoidSeg) && (nEndSeg != nAvoidSeg)) {
		bVisited [nAvoidSeg] = 1;
		depth [nAvoidSeg] = 0;
		}
	}
if (bRandom)
	CreateRandomXlate (randomXlate);
nCurSeg = nStartSeg;
bVisited [nCurSeg] = 1;
nCurDepth = 0;
while (nCurSeg != nEndSeg) {
	segP = gameData.segs.segments + nCurSeg;
	if (bRandom && (d_rand () < 8192))	//create a different xlate at random time intervals
		CreateRandomXlate (randomXlate);

	for (nSide = 0; nSide < MAX_SIDES_PER_SEGMENT; nSide++) {
		hSide = bRandom ? randomXlate [nSide] : nSide;
		if (!IS_CHILD (segP->children [hSide]))
			continue;
		if (!((WALL_IS_DOORWAY (segP, hSide, NULL) & WID_FLY_FLAG) ||
			  (AIDoorIsOpenable (objP, segP, hSide))))
			continue;
		nThisSeg = segP->children [hSide];
		Assert (nThisSeg > -1 && nThisSeg <= gameData.segs.nLastSegment);
		if (bVisited [nThisSeg])
			continue;
		if (bAvoidPlayer && ((nCurSeg == nAvoidSeg) || (nThisSeg == nAvoidSeg))) {
			COMPUTE_SIDE_CENTER (&vCenter, segP, hSide);
			fq.p0					= &objP->position.vPos;
			fq.startSeg			= objP->nSegment;
			fq.p1					= &vCenter;
			fq.radP0				=
			fq.radP1				= objP->size;
			fq.thisObjNum		= OBJ_IDX (objP);
			fq.ignoreObjList	= NULL;
			fq.flags				= 0;
			hitType = FindVectorIntersection (&fq, &hit_data);
			if (hitType != HIT_NONE)
				continue;
			}
		Assert (nThisSeg > -1 && nThisSeg <= gameData.segs.nLastSegment);
		Assert (nCurSeg > -1 && nCurSeg <= gameData.segs.nLastSegment);
		if (nThisSeg < 0)
			continue;
		if (nCurSeg < 0)
			continue;
		segmentQ [qTail].start = nCurSeg;
		segmentQ [qTail].end = nThisSeg;
		segmentQ [qTail].nConnSide = (ubyte) hSide;
		bVisited [nThisSeg] = 1;
		depth [qTail++] = nCurDepth+1;
		if (depth [qTail-1] == nMaxDepth) {
			nEndSeg = segmentQ [qTail-1].end;
			goto cpp_done1;
			}	// end if (depth [...
		}	//	for (nSide.p...

	if (qHead >= qTail) {
		//	Couldn't get to goal, return a path as far as we got, which probably acceptable to the unparticular caller.
		nEndSeg = segmentQ [qTail-1].end;
		break;
		}
	nCurSeg = segmentQ [qHead].end;
	nCurDepth = depth [qHead];
	qHead++;

cpp_done1: ;
	}	//	while (nCurSeg ...
//	Set qTail to the tSegment which ends at the goal.
while (segmentQ [--qTail].end != nEndSeg)
	if (qTail < 0) {
		*numPoints = lNumPoints;
		return -1;
	}
for (i = qTail; i >= 0; ) {
	nParentSeg = segmentQ [i].start;
	lNumPoints++;
	if (nParentSeg == nStartSeg)
		break;
	while (segmentQ [--i].end != nParentSeg)
		Assert (i >= 0);
	}

if (bSafeMode && ((pointSegP - gameData.ai.pointSegs) + 2 * lNumPoints + 1 >= MAX_POINT_SEGS)) {
	//	Ouch! Cannot insert center points in path.  So return unsafe path.
#if TRACE
	con_printf (CONDBG, "Resetting all paths because of bSafeMode.p.\n");
#endif
	AIResetAllPaths ();
	*numPoints = lNumPoints;
	return -1;
	}
pointSegP->nSegment = nStartSeg;
COMPUTE_SEGMENT_CENTER_I (&pointSegP->point, nStartSeg);
if (bSafeMode)
	lNumPoints *= 2;
j = lNumPoints++;
h = bSafeMode + 1;
for (i = qTail; i >= 0; j -= h) {
	nThisSeg = segmentQ [i].end;
	nParentSeg = segmentQ [i].start;
	pointSegP [j].nSegment = nThisSeg;
	COMPUTE_SEGMENT_CENTER_I (&pointSegP [j].point, nThisSeg);
	pointSegP [j].nConnSide = segmentQ [i].nConnSide;
	if (nParentSeg == nStartSeg)
		break;
	while (segmentQ [--i].end != nParentSeg)
		Assert (qTail >= 0);
	}
if (bSafeMode) {
	for (i = 0; i < lNumPoints - 1; i = j) {
		j = i + 2;
		InsertTransitPoint (pointSegP + i + 1, pointSegP + i, pointSegP + j, pointSegP [j].nConnSide);
		}
	lNumPoints = OptimizePath (pointSegP, lNumPoints);
	}
pointSegP += lNumPoints;

#if PATH_VALIDATION
ValidatePath (2, origPointSegs, lNumPoints);
#endif

#if PATH_VALIDATION
ValidatePath (3, origPointSegs, lNumPoints);
#endif

// -- MK, 10/30/95 -- This code causes apparent discontinuities in the path, moving a point
//	into a new tSegment.  It is not necessarily bad, but it makes it hard to track down actual
//	discontinuity problems.
if (objP->nType == OBJ_ROBOT)
	if (ROBOTINFO (objP->id).companion)
		MoveTowardsOutside (origPointSegs, &lNumPoints, objP, 0);

#if PATH_VALIDATION
ValidatePath (4, origPointSegs, lNumPoints);
#endif

*numPoints = lNumPoints;
return 0;
}

int	Last_buddy_polish_path_frame;

//	-------------------------------------------------------------------------------------------------------
//	SmoothPath
//	Takes an existing path and makes it nicer.
//	Drops as many leading points as possible still maintaining direct accessibility
//	from current position to first point.
//	Will not shorten path to fewer than 3 points.
//	Returns number of points.
//	Starting position in pointSegP doesn't change.p.
//	Changed, MK, 10/18/95.  I think this was causing robots to get hung up on walls.
//				Only drop up to the first three points.
int SmoothPath (tObject *objP, tPointSeg *pointSegP, int numPoints)
{
	int			i, nFirstPoint = 0;
	tFVIQuery	fq;
	tFVIData		hit_data;
	int			hitType;


if (numPoints <= 4)
	return numPoints;

//	Prevent the buddy from polishing his path twice in one frame, which can cause him to get hung up.  Pretty ugly, huh?
if (ROBOTINFO (objP->id).companion) {
	if (gameData.app.nFrameCount == Last_buddy_polish_path_frame)
		return numPoints;
	Last_buddy_polish_path_frame = gameData.app.nFrameCount;
	}
fq.p0					= &objP->position.vPos;
fq.startSeg			= objP->nSegment;
fq.radP0				=
fq.radP1				= objP->size;
fq.thisObjNum		= OBJ_IDX (objP);
fq.ignoreObjList	= NULL;
fq.flags				= 0;
for (i = 0; i < 2; i++) {
	fq.p1 = &pointSegP [i].point;
	hitType = FindVectorIntersection (&fq, &hit_data);
	if (hitType != HIT_NONE)
		break;
	nFirstPoint = i + 1;
	}
if (nFirstPoint) {
	//	Scrunch down all the pointSegP.
	for (i = nFirstPoint; i < numPoints; i++)
		pointSegP [i - nFirstPoint] = pointSegP [i];
	}
return numPoints - nFirstPoint;
}

//	-------------------------------------------------------------------------------------------------------
//	Make sure that there are connections between all segments on path.
//	Note that if path has been optimized, connections may not be direct, so this function is useless, or worse.p.
//	Return true if valid, else return false.p.

#if PATH_VALIDATION

int ValidatePath (int debugFlag, tPointSeg *pointSegP, int numPoints)
{
	int i, nCurSeg, nSide, nNextSeg;

nCurSeg = pointSegP->nSegment;
if ((nCurSeg < 0) || (nCurSeg > gameData.segs.nLastSegment)) {
#if TRACE
	con_printf (CONDBG, "Path beginning at index %i, length=%i is bogus!\n", pointSegP-gameData.ai.pointSegs, numPoints);
#endif
	Int3 ();		//	Contact Mike: Debug trap for elusive, nasty bug.
	return 0;
	}
#if TRACE
if (debugFlag == 999)
	con_printf (CONDBG, "That's curious...\n");
#endif
if (numPoints == 0)
	return 1;
for (i = 1; i < numPoints; i++) {
	nNextSeg = pointSegP [i].nSegment;
	if ((nNextSeg < 0) || (nNextSeg > gameData.segs.nLastSegment)) {
#if TRACE
		con_printf (CONDBG, "Path beginning at index %i, length=%i is bogus!\n", pointSegP-gameData.ai.pointSegs, numPoints);
#endif
		Int3 ();		//	Contact Mike: Debug trap for elusive, nasty bug.
		return 0;
		}
	if (nCurSeg != nNextSeg) {
		for (nSide=0; nSide<MAX_SIDES_PER_SEGMENT; nSide++)
			if (gameData.segs.segments [nCurSeg].children [nSide] == nNextSeg)
				break;
		if (nSide == MAX_SIDES_PER_SEGMENT) {
#if TRACE
			con_printf (CONDBG, "Path beginning at index %i, length=%i is bogus!\n", pointSegP-gameData.ai.pointSegs, numPoints);
#endif
			Int3 ();
			return 0;
			}
		nCurSeg = nNextSeg;
		}
	}
return 1;
}

#endif

//	-----------------------------------------------------------------------------------------------------------

#if PATH_VALIDATION

void ValidateAllPaths (void)
{
	int	i;
	tObject	*objP = OBJECTS;
	tAIStatic	*aiP;

for (i = 0; i <= gameData.objs.nLastObject [0]; i++, objP++) {
	if (OBJECTS [i].nType == OBJ_ROBOT) {
		aiP = &objP->cType.aiInfo;
		if (objP->controlType == CT_AI) {
			if ((aiP->nHideIndex != -1) && (aiP->nPathLength > 0))
				if (!ValidatePath (4, &gameData.ai.pointSegs [aiP->nHideIndex], aiP->nPathLength)) {
					Int3 ();	//	This path is bogus! Who corrupted it! Danger!Danger!
								//	Contact Mike, he caused this mess.
					//force_dump_aiObjects_all ("Error in ValidateAllPaths");
					aiP->nPathLength=0;	//	This allows people to resume without harm...
					}
			}
		}
	}
}
#endif

// -- //	-------------------------------------------------------------------------------------------------------
// -- //	Creates a path from the OBJECTS current tSegment (objP->nSegment) to the specified tSegment for the tObject to
// -- //	hide in gameData.ai.localInfo [nObject].nGoalSegment.
// -- //	Sets	objP->cType.aiInfo.nHideIndex, 		a pointer into gameData.ai.pointSegs, the first tPointSeg of the path.
// -- //			objP->cType.aiInfo.nPathLength, 		length of path
// -- //			gameData.ai.freePointSegs				global pointer into gameData.ai.pointSegs array
// -- void create_path (tObject *objP)
// -- {
// -- 	tAIStatic	*aiP = &objP->cType.aiInfo;
// -- 	tAILocal		*ailP = &gameData.ai.localInfo [OBJ_IDX (objP)];
// -- 	int			nStartSeg, nEndSeg;
// --
// -- 	nStartSeg = objP->nSegment;
// -- 	nEndSeg = ailP->nGoalSegment;
// --
// -- 	if (nEndSeg == -1)
// -- 		CreateNSegmentPath (objP, 3, -1);
// --
// -- 	if (nEndSeg == -1) {
// -- 		; //con_printf (CONDBG, "Object %i, nHideSegment = -1, not creating path.\n", OBJ_IDX (objP));
// -- 	} else {
// -- 		CreatePathPoints (objP, nStartSeg, nEndSeg, gameData.ai.freePointSegs, &aiP->nPathLength, -1, 0, 0, -1);
// -- 		aiP->nHideIndex = gameData.ai.freePointSegs - gameData.ai.pointSegs;
// -- 		aiP->nCurPathIndex = 0;
// -- #if PATH_VALIDATION
// -- 		ValidatePath (5, gameData.ai.freePointSegs, aiP->nPathLength);
// -- #endif
// -- 		gameData.ai.freePointSegs += aiP->nPathLength;
// -- 		if (gameData.ai.freePointSegs - gameData.ai.pointSegs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
// -- 			//Int3 ();	//	Contact Mike: This is curious, though not deadly. /eip++;g
// -- 			//force_dump_aiObjects_all ("Error in create_path");
// -- 			AIResetAllPaths ();
// -- 		}
// -- 		aiP->PATH_DIR = 1;		//	Initialize to moving forward.
// -- 		aiP->SUBMODE = AISM_HIDING;		//	Pretend we are hiding, so we sit here until bothered.
// -- 	}
// --
// -- 	MaybeAIPathGarbageCollect ();
// --
// -- }

//	-------------------------------------------------------------------------------------------------------
//	Creates a path from the OBJECTS current tSegment (objP->nSegment) to the specified tSegment for the tObject to
//	hide in gameData.ai.localInfo [nObject].nGoalSegment.
//	Sets	objP->cType.aiInfo.nHideIndex, 		a pointer into gameData.ai.pointSegs, the first tPointSeg of the path.
//			objP->cType.aiInfo.nPathLength, 		length of path
//			gameData.ai.freePointSegs				global pointer into gameData.ai.pointSegs array
//	Change, 10/07/95: Used to create path to gameData.objs.console->position.vPos.p.  Now creates path to gameData.ai.vBelievedPlayerPos.p.

void CreatePathToPlayer (tObject *objP, int max_length, int bSafeMode)
{
	tAIStatic	*aiP = &objP->cType.aiInfo;
	tAILocal		*ailP = &gameData.ai.localInfo [OBJ_IDX (objP)];
	int			nStartSeg, nEndSeg;

if (max_length == -1)
	max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;

ailP->timePlayerSeen = gameData.time.xGame;			//	Prevent from resetting path quickly.
ailP->nGoalSegment = gameData.ai.nBelievedPlayerSeg;

nStartSeg = objP->nSegment;
nEndSeg = ailP->nGoalSegment;

if (nEndSeg != -1) {
	CreatePathPoints (objP, nStartSeg, nEndSeg, gameData.ai.freePointSegs, &aiP->nPathLength, max_length, 1, bSafeMode, -1);
	aiP->nPathLength = SmoothPath (objP, gameData.ai.freePointSegs, aiP->nPathLength);
	aiP->nHideIndex = (int) (gameData.ai.freePointSegs - gameData.ai.pointSegs);
	aiP->nCurPathIndex = 0;
	gameData.ai.freePointSegs += aiP->nPathLength;
	if (gameData.ai.freePointSegs - gameData.ai.pointSegs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		AIResetAllPaths ();
		return;
		}
	aiP->PATH_DIR = 1;		//	Initialize to moving forward.
	// -- UNUSED!aiP->SUBMODE = AISM_GOHIDE;		//	This forces immediate movement.
	ailP->mode = AIM_FOLLOW_PATH;
	if (ailP->playerAwarenessType < PA_RETURN_FIRE)
		ailP->playerAwarenessType = 0;		//	If robot too aware of tPlayer, will set mode to chase
	}
MaybeAIPathGarbageCollect ();
}

//	-------------------------------------------------------------------------------------------------------
//	Creates a path from the tObject's current tSegment (objP->nSegment) to tSegment goalseg.
void CreatePathToSegment (tObject *objP, short goalseg, int max_length, int bSafeMode)
{
	tAIStatic	*aiP = &objP->cType.aiInfo;
	tAILocal		*ailP = &gameData.ai.localInfo [OBJ_IDX (objP)];
	short			nStartSeg, nEndSeg;

if (max_length == -1)
	max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;
ailP->timePlayerSeen = gameData.time.xGame;			//	Prevent from resetting path quickly.
ailP->nGoalSegment = goalseg;
nStartSeg = objP->nSegment;
nEndSeg = ailP->nGoalSegment;
if (nEndSeg != -1) {
	CreatePathPoints (objP, nStartSeg, nEndSeg, gameData.ai.freePointSegs, &aiP->nPathLength, max_length, 1, bSafeMode, -1);
	aiP->nHideIndex = (int) (gameData.ai.freePointSegs - gameData.ai.pointSegs);
	aiP->nCurPathIndex = 0;
	gameData.ai.freePointSegs += aiP->nPathLength;
	if (gameData.ai.freePointSegs - gameData.ai.pointSegs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		AIResetAllPaths ();
		return;
		}
	aiP->PATH_DIR = 1;		//	Initialize to moving forward.
	// -- UNUSED!aiP->SUBMODE = AISM_GOHIDE;		//	This forces immediate movement.
	if (ailP->playerAwarenessType < PA_RETURN_FIRE)
		ailP->playerAwarenessType = 0;		//	If robot too aware of tPlayer, will set mode to chase
	}
MaybeAIPathGarbageCollect ();
}

//	-------------------------------------------------------------------------------------------------------
//	Creates a path from the OBJECTS current tSegment (objP->nSegment) to the specified tSegment for the tObject to
//	hide in gameData.ai.localInfo [nObject].nGoalSegment
//	Sets	objP->cType.aiInfo.nHideIndex, 		a pointer into gameData.ai.pointSegs, the first tPointSeg of the path.
//			objP->cType.aiInfo.nPathLength, 		length of path
//			gameData.ai.freePointSegs				global pointer into gameData.ai.pointSegs array
void CreatePathToStation (tObject *objP, int max_length)
{
	tAIStatic	*aiP = &objP->cType.aiInfo;
	tAILocal		*ailP = &gameData.ai.localInfo [OBJ_IDX (objP)];
	int			nStartSeg, nEndSeg;

if (max_length == -1)
	max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;

ailP->timePlayerSeen = gameData.time.xGame;			//	Prevent from resetting path quickly.

nStartSeg = objP->nSegment;
nEndSeg = aiP->nHideSegment;


if (nEndSeg != -1) {
	CreatePathPoints (objP, nStartSeg, nEndSeg, gameData.ai.freePointSegs, &aiP->nPathLength, max_length, 1, 1, -1);
	aiP->nPathLength = SmoothPath (objP, gameData.ai.freePointSegs, aiP->nPathLength);
	aiP->nHideIndex = (int) (gameData.ai.freePointSegs - gameData.ai.pointSegs);
	aiP->nCurPathIndex = 0;
	gameData.ai.freePointSegs += aiP->nPathLength;
	if (gameData.ai.freePointSegs - gameData.ai.pointSegs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		AIResetAllPaths ();
		return;
		}
	aiP->PATH_DIR = 1;		//	Initialize to moving forward.
	ailP->mode = AIM_FOLLOW_PATH;
	if (ailP->playerAwarenessType < PA_RETURN_FIRE)
		ailP->playerAwarenessType = 0;
	}
MaybeAIPathGarbageCollect ();
}


//	-------------------------------------------------------------------------------------------------------
//	Create a path of length nPathLength for an tObject, stuffing info in aiInfo field.

static int nObject = 0;

void CreateNSegmentPath (tObject *objP, int nPathLength, short nAvoidSeg)
{
	tAIStatic	*aiP = &objP->cType.aiInfo;
	tAILocal		*ailP = gameData.ai.localInfo + OBJ_IDX (objP);
	nObject = OBJ_IDX (objP);

if (CreatePathPoints (objP, objP->nSegment, -2, gameData.ai.freePointSegs, &aiP->nPathLength, nPathLength, 1, 0, nAvoidSeg) == -1) {
	gameData.ai.freePointSegs += aiP->nPathLength;
	while ((CreatePathPoints (objP, objP->nSegment, -2, gameData.ai.freePointSegs, &aiP->nPathLength, --nPathLength, 1, 0, -1) == -1)) {
		Assert (nPathLength);
		}
	}
aiP->nHideIndex = (int) (gameData.ai.freePointSegs - gameData.ai.pointSegs);
aiP->nCurPathIndex = 0;
#if PATH_VALIDATION
ValidatePath (8, gameData.ai.freePointSegs, aiP->nPathLength);
#endif
gameData.ai.freePointSegs += aiP->nPathLength;
if (gameData.ai.freePointSegs - gameData.ai.pointSegs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
	AIResetAllPaths ();
	}
aiP->PATH_DIR = 1;		//	Initialize to moving forward.
ailP->mode = AIM_FOLLOW_PATH;
//	If this robot is visible (playerVisibility is not available) and it's running away, move towards outside with
//	randomness to prevent a stream of bots from going away down the center of a corridor.
if (gameData.ai.localInfo [OBJ_IDX (objP)].nPrevVisibility) {
	if (aiP->nPathLength) {
		int nPoints = aiP->nPathLength;
		MoveTowardsOutside (gameData.ai.pointSegs + aiP->nHideIndex, &nPoints, objP, 1);
		aiP->nPathLength = nPoints;
		}
	}
MaybeAIPathGarbageCollect ();
}

//	-------------------------------------------------------------------------------------------------------

void CreateNSegmentPathToDoor (tObject *objP, int nPathLength, short nAvoidSeg)
{
CreateNSegmentPath (objP, nPathLength, nAvoidSeg);
}

#define Int3_if (cond) if (!cond) Int3 ();

//	----------------------------------------------------------------------------------------------------

void MoveObjectToGoal (tObject *objP, vmsVector *vGoalPoint, short nGoalSeg)
{
	tAIStatic	*aiP = &objP->cType.aiInfo;
	int			nSegment;

if (aiP->nPathLength < 2)
	return;
Assert (objP->nSegment != -1);
#ifdef _DEBUG
if (objP->nSegment != nGoalSeg)
	if (FindConnectedSide (gameData.segs.segments + objP->nSegment, gameData.segs.segments + nGoalSeg) == -1) {
		fix dist = FindConnectedDistance (&objP->position.vPos, objP->nSegment, vGoalPoint, nGoalSeg, 30, WID_FLY_FLAG, 0);
#	if TRACE
		if (gameData.fcd.nConnSegDist > 2)	//	This global is set in FindConnectedDistance
			con_printf (1, "Warning: Object %i hopped across %i segments, a distance of %7.3f.\n", OBJ_IDX (objP), gameData.fcd.nConnSegDist, f2fl (dist));
#	endif
		}
#endif
Assert (aiP->nPathLength >= 2);
if (aiP->nCurPathIndex <= 0) {
	if (aiP->behavior == AIB_STATION) {
		CreatePathToStation (objP, 15);
		return;
		}
	aiP->nCurPathIndex = 1;
	aiP->PATH_DIR = 1;
	}
else if (aiP->nCurPathIndex >= aiP->nPathLength - 1) {
	if (aiP->behavior == AIB_STATION) {
		CreatePathToStation (objP, 15);
		if (!aiP->nPathLength) {
			tAILocal	*ailP = &gameData.ai.localInfo [OBJ_IDX (objP)];
			ailP->mode = AIM_IDLING;
			}
		return;
		}
	Assert (aiP->nPathLength != 0);
	aiP->nCurPathIndex = aiP->nPathLength - 2;
	aiP->PATH_DIR = -1;
	}
else
	aiP->nCurPathIndex += aiP->PATH_DIR;
objP->position.vPos = *vGoalPoint;
nSegment = FindObjectSeg (objP);
#if TRACE
if (nSegment != nGoalSeg)
	con_printf (1, "Object #%i goal supposed to be in tSegment #%i, but in tSegment #%i\n", OBJ_IDX (objP), nGoalSeg, nSegment);
#endif
if (nSegment == -1) {
	Int3 ();	//	Oops, tObject is not in any tSegment.
				// Contact Mike: This is impossible.p.
	//	Hack, move tObject to center of tSegment it used to be in.
	COMPUTE_SEGMENT_CENTER_I (&objP->position.vPos, objP->nSegment);
	}
else
	RelinkObject (OBJ_IDX (objP), nSegment);
}

// -- too much work -- //	----------------------------------------------------------------------------------------------------------
// -- too much work -- //	Return true if the tObject the companion wants to kill is reachable.p.
// -- too much work -- int attackKillObject (tObject *objP)
// -- too much work -- {
// -- too much work -- 	tObject		*kill_objp;
// -- too much work -- 	tFVIData		hit_data;
// -- too much work -- 	int			fate;
// -- too much work -- 	tFVIQuery	fq;
// -- too much work --
// -- too much work -- 	if (gameData.escort.nKillObject == -1)
// -- too much work -- 		return 0;
// -- too much work --
// -- too much work -- 	kill_objp = &OBJECTS [gameData.escort.nKillObject];
// -- too much work --
// -- too much work -- 	fq.p0						= &objP->position.vPos;
// -- too much work -- 	fq.startSeg				= objP->nSegment;
// -- too much work -- 	fq.p1						= &kill_objP->position.vPos;
// -- too much work -- 	fq.rad					= objP->size;
// -- too much work -- 	fq.thisObjNum			= OBJ_IDX (objP);
// -- too much work -- 	fq.ignoreObjList	= NULL;
// -- too much work -- 	fq.flags					= 0;
// -- too much work --
// -- too much work -- 	fate = FindVectorIntersection (&fq, &hit_data);
// -- too much work --
// -- too much work -- 	if (fate == HIT_NONE)
// -- too much work -- 		return 1;
// -- too much work -- 	else
// -- too much work -- 		return 0;
// -- too much work -- }

//	----------------------------------------------------------------------------------------------------------
//	Optimization: If current velocity will take robot near goal, don't change velocity
void AIFollowPath (tObject *objP, int playerVisibility, int nPrevVisibility, vmsVector *vec_to_player)
{
	tAIStatic		*aiP = &objP->cType.aiInfo;

	vmsVector	vGoalPoint, new_vGoalPoint;
	fix			xDistToGoal;
	tRobotInfo	*robptr = &ROBOTINFO (objP->id);
	int			forced_break, original_dir, original_index;
	fix			xDistToPlayer;
	short			nGoalSeg;
	tAILocal		*ailP = gameData.ai.localInfo + OBJ_IDX (objP);
	fix			thresholdDistance;


if ((aiP->nHideIndex == -1) || (aiP->nPathLength == 0)) {
	if (ailP->mode == AIM_RUN_FROM_OBJECT) {
		CreateNSegmentPath (objP, 5, -1);
		//--Int3_if ((aiP->nPathLength != 0);
		ailP->mode = AIM_RUN_FROM_OBJECT;
		}
	else {
		CreateNSegmentPath (objP, 5, -1);
		//--Int3_if ((aiP->nPathLength != 0);
		}
	}

if ((aiP->nHideIndex + aiP->nPathLength > gameData.ai.freePointSegs - gameData.ai.pointSegs) && (aiP->nPathLength>0)) {
	Int3 ();	//	Contact Mike: Bad.  Path goes into what is believed to be D2_FREE space.p.
	//	This is debugging code.p.  Figure out why garbage collection
	//	didn't compress this tObject's path information.
	AIPathGarbageCollect ();
	//force_dump_aiObjects_all ("Error in AIFollowPath");
	AIResetAllPaths ();
	}

if (aiP->nPathLength < 2) {
	if ((aiP->behavior == AIB_SNIPE) || (ailP->mode == AIM_RUN_FROM_OBJECT)) {
		if (gameData.objs.console->nSegment == objP->nSegment) {
			CreateNSegmentPath (objP, AVOID_SEG_LENGTH, -1);			//	Can't avoid tSegment tPlayer is in, robot is already in it!(That's what the -1 is for)
			//--Int3_if ((aiP->nPathLength != 0);
			}
		else {
			CreateNSegmentPath (objP, AVOID_SEG_LENGTH, gameData.objs.console->nSegment);
				//--Int3_if ((aiP->nPathLength != 0);
			}
		if (aiP->behavior == AIB_SNIPE) {
			if (robptr->thief)
				ailP->mode = AIM_THIEF_ATTACK;	//	It gets bashed in CreateNSegmentPath
			else
				ailP->mode = AIM_SNIPE_FIRE;	//	It gets bashed in CreateNSegmentPath
			}
		else {
			ailP->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in CreateNSegmentPath
			}
		}
	else if (robptr->companion == 0) {
		ailP->mode = AIM_IDLING;
		aiP->nPathLength = 0;
		return;
		}
	}

vGoalPoint = gameData.ai.pointSegs [aiP->nHideIndex + aiP->nCurPathIndex].point;
nGoalSeg = gameData.ai.pointSegs [aiP->nHideIndex + aiP->nCurPathIndex].nSegment;
xDistToGoal = vmsVector::dist(vGoalPoint, objP->position.vPos);
if (gameStates.app.bPlayerIsDead)
	xDistToPlayer = vmsVector::dist(objP->position.vPos, gameData.objs.viewer->position.vPos);
else
	xDistToPlayer = vmsVector::dist(objP->position.vPos, gameData.objs.console->position.vPos);
	//	Efficiency hack: If far away from tPlayer, move in big quantized jumps.
if (!(playerVisibility || nPrevVisibility) && (xDistToPlayer > F1_0*200) && !(gameData.app.nGameMode & GM_MULTI)) {
	if (xDistToGoal < F1_0*2) {
		MoveObjectToGoal (objP, &vGoalPoint, nGoalSeg);
		return;
		}
	else {
		tRobotInfo	*robptr = &ROBOTINFO (objP->id);
		fix	cur_speed = robptr->xMaxSpeed [gameStates.app.nDifficultyLevel]/2;
		fix	distance_travellable = FixMul (gameData.time.xFrame, cur_speed);
		// int	nConnSide = FindConnectedSide (objP->nSegment, nGoalSeg);
		//	Only move to goal if allowed to fly through the tSide.p.
		//	Buddy-bot can create paths he can't fly, waiting for player.
		// -- bah, this isn't good enough, buddy will fail to get through any door!if (WALL_IS_DOORWAY (&gameData.segs.segments]objP->nSegment], nConnSide) & WID_FLY_FLAG) {
		if (!ROBOTINFO (objP->id).companion && !ROBOTINFO (objP->id).thief) {
			if (distance_travellable >= xDistToGoal) {
				MoveObjectToGoal (objP, &vGoalPoint, nGoalSeg);
				}
			else {
				fix	prob = FixDiv (distance_travellable, xDistToGoal);
				int	rand_num = d_rand ();
				if ((rand_num >> 1) < prob) {
					MoveObjectToGoal (objP, &vGoalPoint, nGoalSeg);
					}
				}
			return;
			}
		}
	}
//	If running from tPlayer, only run until can't be seen.
if (ailP->mode == AIM_RUN_FROM_OBJECT) {
	if ((playerVisibility == 0) && (ailP->playerAwarenessType == 0)) {
		fix vel_scale = F1_0 - gameData.time.xFrame/2;
		if (vel_scale < F1_0/2)
			vel_scale = F1_0/2;
		objP->mType.physInfo.velocity *= vel_scale;
		return;
		}
	else if (!(gameData.app.nFrameCount ^ ((OBJ_IDX (objP)) & 0x07))) {		//	Done 1/8 frames.
		//	If tPlayer on path (beyond point robot is now at), then create a new path.
		tPointSeg	*curpsp = &gameData.ai.pointSegs [aiP->nHideIndex];
		short			player_segnum = gameData.objs.console->nSegment;
		int			i;
		//	This is probably being done every frame, which is wasteful.
		for (i=aiP->nCurPathIndex; i<aiP->nPathLength; i++) {
			if (curpsp [i].nSegment == player_segnum) {
				if (player_segnum != objP->nSegment) {
					CreateNSegmentPath (objP, AVOID_SEG_LENGTH, player_segnum);
					}
				else {
					CreateNSegmentPath (objP, AVOID_SEG_LENGTH, -1);
					}
				Assert (aiP->nPathLength != 0);
				ailP->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in CreateNSegmentPath
				break;
				}
			}
		if (playerVisibility) {
			ailP->playerAwarenessType = 1;
			ailP->playerAwarenessTime = F1_0;
			}
		}
	}
if (aiP->nCurPathIndex < 0) {
	aiP->nCurPathIndex = 0;
	}
else if (aiP->nCurPathIndex >= aiP->nPathLength) {
	if (ailP->mode == AIM_RUN_FROM_OBJECT) {
		CreateNSegmentPath (objP, AVOID_SEG_LENGTH, gameData.objs.console->nSegment);
		ailP->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in CreateNSegmentPath
		Assert (aiP->nPathLength != 0);
		}
	else {
		aiP->nCurPathIndex = aiP->nPathLength-1;
		}
	}
vGoalPoint = gameData.ai.pointSegs [aiP->nHideIndex + aiP->nCurPathIndex].point;
//	If near goal, pick another goal point.
forced_break = 0;		//	Gets set for short paths.
original_dir = aiP->PATH_DIR;
original_index = aiP->nCurPathIndex;
thresholdDistance = FixMul (objP->mType.physInfo.velocity.mag(), gameData.time.xFrame)*2 + F1_0*2;
new_vGoalPoint = gameData.ai.pointSegs [aiP->nHideIndex + aiP->nCurPathIndex].point;
while ((xDistToGoal < thresholdDistance) && !forced_break) {
	//	Advance to next point on path.
	aiP->nCurPathIndex += aiP->PATH_DIR;
	//	See if next point wraps past end of path (in either direction), and if so, deal with it based on mode.p.
	if ((aiP->nCurPathIndex >= aiP->nPathLength) || (aiP->nCurPathIndex < 0)) {
		//	If mode = hiding, then stay here until get bonked or hit by player.
		// --	if (ailP->mode == AIM_BEHIND) {
		// --		ailP->mode = AIM_IDLING;
		// --		return;		// Stay here until bonked or hit by player.
		// --	} else

		//	Buddy bot.  If he's in mode to get away from tPlayer and at end of line,
		//	if tPlayer visible, then make a new path, else just return.
		if (robptr->companion) {
			if (gameData.escort.nSpecialGoal == ESCORT_GOAL_SCRAM) {
				if (playerVisibility) {
					CreateNSegmentPath (objP, 16 + d_rand () * 16, -1);
					aiP->nPathLength = SmoothPath (objP, &gameData.ai.pointSegs [aiP->nHideIndex], aiP->nPathLength);
					Assert (aiP->nPathLength != 0);
					ailP->mode = AIM_WANDER;	//	Special buddy mode.p.
					//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
					return;
					}
				else {
					ailP->mode = AIM_WANDER;	//	Special buddy mode.p.
					objP->mType.physInfo.velocity.setZero();
					objP->mType.physInfo.rotVel.setZero();
					//!!Assert ((aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength);
					return;
					}
				}
			}
		if (aiP->behavior == AIB_FOLLOW) {
			CreateNSegmentPath (objP, 10, gameData.objs.console->nSegment);
			//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
			}
		else if (aiP->behavior == AIB_STATION) {
			CreatePathToStation (objP, 15);
			if ((aiP->nHideSegment != gameData.ai.pointSegs [aiP->nHideIndex+aiP->nPathLength-1].nSegment) ||
				 (aiP->nPathLength == 0)) {
				ailP->mode = AIM_IDLING;
				}
			else {
				//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
				}
			return;
			}
		else if (ailP->mode == AIM_FOLLOW_PATH) {
			CreatePathToPlayer (objP, 10, 1);
			if (aiP->nHideSegment != gameData.ai.pointSegs [aiP->nHideIndex+aiP->nPathLength-1].nSegment) {
				ailP->mode = AIM_IDLING;
				return;
				}
			else {
				//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
				}
			}
		else if (ailP->mode == AIM_RUN_FROM_OBJECT) {
			CreateNSegmentPath (objP, AVOID_SEG_LENGTH, gameData.objs.console->nSegment);
			ailP->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in CreateNSegmentPath
			if (aiP->nPathLength < 1) {
				CreateNSegmentPath (objP, AVOID_SEG_LENGTH, gameData.objs.console->nSegment);
				ailP->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in CreateNSegmentPath
				if (aiP->nPathLength < 1) {
					aiP->behavior = AIB_NORMAL;
					ailP->mode = AIM_IDLING;
					return;
					}
				}
			//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
			}
		else {
			//	Reached end of the line.p.  First see if opposite end point is reachable, and if so, go there.p.
			//	If not, turn around.
			int			nOppositeEndIndex;
			vmsVector	*vOppositeEndPoint;
			tFVIData		hit_data;
			int			fate;
			tFVIQuery	fq;

			// See which end we're nearer and look at the opposite end point.
			if (abs (aiP->nCurPathIndex - aiP->nPathLength) < aiP->nCurPathIndex) {
				//	Nearer to far end (ie, index not 0), so try to reach 0.
				nOppositeEndIndex = 0;
				}
			else {
				//	Nearer to 0 end, so try to reach far end.
				nOppositeEndIndex = aiP->nPathLength-1;
				}
			//--Int3_if (( (nOppositeEndIndex >= 0) && (nOppositeEndIndex < aiP->nPathLength));
			vOppositeEndPoint = &gameData.ai.pointSegs [aiP->nHideIndex + nOppositeEndIndex].point;
			fq.p0					= &objP->position.vPos;
			fq.startSeg			= objP->nSegment;
			fq.p1					= vOppositeEndPoint;
			fq.radP0				=
			fq.radP1				= objP->size;
			fq.thisObjNum		= OBJ_IDX (objP);
			fq.ignoreObjList	= NULL;
			fq.flags				= 0; 				//what about trans walls???
			fate = FindVectorIntersection (&fq, &hit_data);
			if (fate != HIT_WALL) {
				//	We can be circular! Do it!
				//	Path direction is unchanged.
				aiP->nCurPathIndex = nOppositeEndIndex;
				}
			else {
				aiP->PATH_DIR = -aiP->PATH_DIR;
				aiP->nCurPathIndex += aiP->PATH_DIR;
				}
				//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
			}
		break;
		}
	else {
		new_vGoalPoint = gameData.ai.pointSegs [aiP->nHideIndex + aiP->nCurPathIndex].point;
		vGoalPoint = new_vGoalPoint;
		xDistToGoal = vmsVector::dist(vGoalPoint, objP->position.vPos);
		//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
		}
	//	If went all the way around to original point, in same direction, then get out of here!
	if ((aiP->nCurPathIndex == original_index) && (aiP->PATH_DIR == original_dir)) {
		CreatePathToPlayer (objP, 3, 1);
		//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
		forced_break = 1;
		}
		//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
	}	//	end while
//	Set velocity (objP->mType.physInfo.velocity) and orientation (objP->position.mOrient) for this tObject.
//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
AIPathSetOrientAndVel (objP, &vGoalPoint, playerVisibility, vec_to_player);
//--Int3_if (( (aiP->nCurPathIndex >= 0) && (aiP->nCurPathIndex < aiP->nPathLength));
}

//	----------------------------------------------------------------------------------------------------------

typedef struct {
	short	path_start, nObject;
} obj_path;

int _CDECL_ QSCmpPathIndex (obj_path *i1, obj_path *i2)
{
if (i1->path_start < i2->path_start)
	return -1;
if (i1->path_start > i2->path_start)
	return 1;
return 0;
}

//	----------------------------------------------------------------------------------------------------------
//	Set orientation matrix and velocity for objP based on its desire to get to a point.
void AIPathSetOrientAndVel (tObject *objP, vmsVector *vGoalPoint, int playerVisibility, vmsVector *vec_to_player)
{
	vmsVector	vCurVel = objP->mType.physInfo.velocity;
	vmsVector	vNormCurVel;
	vmsVector	vNormToGoal;
	vmsVector	vCurPos = objP->position.vPos;
	vmsVector	vNormFwd;
	fix			xSpeedScale;
	fix			dot;
	tRobotInfo	*robptr = &ROBOTINFO (objP->id);
	fix			xMaxSpeed;

//	If evading tPlayer, use highest difficulty level speed, plus something based on diff level
xMaxSpeed = robptr->xMaxSpeed [gameStates.app.nDifficultyLevel];
if ((gameData.ai.localInfo [OBJ_IDX (objP)].mode == AIM_RUN_FROM_OBJECT) || (objP->cType.aiInfo.behavior == AIB_SNIPE))
	xMaxSpeed = xMaxSpeed*3/2;
vNormToGoal = *vGoalPoint - vCurPos;
vmsVector::normalize(vNormToGoal);
vNormCurVel = vCurVel;
vmsVector::normalize(vNormCurVel);
vNormFwd = objP->position.mOrient[FVEC];
vmsVector::normalize(vNormFwd);
dot = vmsVector::dot(vNormToGoal, vNormFwd);
//	If very close to facing opposite desired vector, perturb vector
if (dot < -15*F1_0/16) {
	vNormCurVel = vNormToGoal;
	}
else {
	vNormCurVel[X] += vNormToGoal[X]/2;
	vNormCurVel[Y] += vNormToGoal[Y]/2;
	vNormCurVel[Z] += vNormToGoal[Z]/2;
	}
vmsVector::normalize(vNormCurVel);
//	Set speed based on this robot nType's maximum allowed speed and how hard it is turning.
//	How hard it is turning is based on the dot product of (vector to goal) and (current velocity vector)
//	Note that since 3*F1_0/4 is added to dot product, it is possible for the robot to back up.
//	Set speed and orientation.
if (dot < 0)
	dot /= -4;

//	If in snipe mode, can move fast even if not facing that direction.
if ((objP->cType.aiInfo.behavior == AIB_SNIPE) && (dot < F1_0/2))
	dot = (dot + F1_0) / 2;
xSpeedScale = FixMul (xMaxSpeed, dot);
vNormCurVel *= xSpeedScale;
objP->mType.physInfo.velocity = vNormCurVel;
if ((gameData.ai.localInfo [OBJ_IDX (objP)].mode == AIM_RUN_FROM_OBJECT) || (robptr->companion == 1) || (objP->cType.aiInfo.behavior == AIB_SNIPE)) {
	if (gameData.ai.localInfo [OBJ_IDX (objP)].mode == AIM_SNIPE_RETREAT_BACKWARDS) {
		if ((playerVisibility) && (vec_to_player != NULL))
			vNormToGoal = *vec_to_player;
		else
			vNormToGoal = -vNormToGoal;
		}
	AITurnTowardsVector (&vNormToGoal, objP, robptr->turnTime [NDL-1]/2);
	}
else
	AITurnTowardsVector (&vNormToGoal, objP, robptr->turnTime [gameStates.app.nDifficultyLevel]);
}

int	nLastFrameGarbageCollected = 0;

//	----------------------------------------------------------------------------------------------------------
//	Garbage colledion -- Free all unused records in gameData.ai.pointSegs and compress all paths.
void AIPathGarbageCollect (void)
{
	int			nFreePathIdx = 0;
	int			nPathObjects = 0;
	int			nObject;
	int			objind, i, nOldIndex;
	tObject		*objP;
	tAIStatic	*aiP;
	obj_path		objectList [MAX_OBJECTS_D2X];

#ifdef _DEBUG
force_dump_aiObjects_all ("***** Start AIPathGarbageCollect *****");
#endif
nLastFrameGarbageCollected = gameData.app.nFrameCount;
#if PATH_VALIDATION
ValidateAllPaths ();
#endif
	//	Create a list of OBJECTS which have paths of length 1 or more.p.
objP = OBJECTS;
for (nObject = 0; nObject <= gameData.objs.nLastObject [0]; nObject++, objP++) {
	if ((objP->nType == OBJ_ROBOT) &&
		 ((objP->controlType == CT_AI) || (objP->controlType == CT_MORPH))) {
		aiP = &objP->cType.aiInfo;
		if (aiP->nPathLength) {
			objectList [nPathObjects].path_start = aiP->nHideIndex;
			objectList [nPathObjects++].nObject = nObject;
			}
		}
	}

qsort (objectList, nPathObjects, sizeof (objectList [0]),
		 (int (_CDECL_ *) (void const *, void const *))QSCmpPathIndex);

for (objind=0; objind < nPathObjects; objind++) {
	nObject = objectList [objind].nObject;
	objP = OBJECTS + nObject;
	aiP = &objP->cType.aiInfo;
	nOldIndex = aiP->nHideIndex;
	aiP->nHideIndex = nFreePathIdx;
	for (i = 0; i < aiP->nPathLength; i++)
		gameData.ai.pointSegs [nFreePathIdx++] = gameData.ai.pointSegs [nOldIndex++];
	}
gameData.ai.freePointSegs = gameData.ai.pointSegs + nFreePathIdx;

////printf ("After garbage collection, D2_FREE index = %i\n", gameData.ai.freePointSegs - gameData.ai.pointSegs);
#ifdef _DEBUG
force_dump_aiObjects_all ("***** Finish AIPathGarbageCollect *****");
for (i = 0, objP = OBJECTS; i <= gameData.objs.nLastObject [0]; i++, objP++) {
	aiP = &objP->cType.aiInfo;
	if ((objP->nType == OBJ_ROBOT) && (objP->controlType == CT_AI))
		if ((aiP->nHideIndex + aiP->nPathLength > gameData.ai.freePointSegs - gameData.ai.pointSegs) &&
			 (aiP->nPathLength>0))
			Int3 ();		//	Contact Mike: Debug trap for nasty, elusive bug.
	}
#	if PATH_VALIDATION
ValidateAllPaths ();
#	endif
#endif
}

//	-----------------------------------------------------------------------------
//	Do garbage collection if not been done for awhile, or things getting really critical.
void MaybeAIPathGarbageCollect (void)
{
if (gameData.ai.freePointSegs - gameData.ai.pointSegs > MAX_POINT_SEGS - MAX_PATH_LENGTH) {
	if (nLastFrameGarbageCollected+1 >= gameData.app.nFrameCount) {
		//	This is kind of bad.  Garbage collected last frame or this frame.p.
		//	Just destroy all paths.  Too bad for the robots.  They are memory wasteful.
		AIResetAllPaths ();
#if TRACE
		con_printf (1, "Warning: Resetting all paths.  gameData.ai.pointSegs buffer nearly exhausted.\n");
#endif
		}
	else {
			//	We are really close to full, but didn't just garbage collect, so maybe this is recoverable.p.
#if TRACE
		con_printf (1, "Warning: Almost full garbage collection being performed: ");
#endif
		AIPathGarbageCollect ();
#if TRACE
		con_printf (1, "Free records = %i/%i\n", MAX_POINT_SEGS - (gameData.ai.freePointSegs - gameData.ai.pointSegs), MAX_POINT_SEGS);
#endif
		}
	}
else if (gameData.ai.freePointSegs - gameData.ai.pointSegs > 3*MAX_POINT_SEGS/4) {
	if (nLastFrameGarbageCollected + 16 < gameData.app.nFrameCount) {
		AIPathGarbageCollect ();
		}
	}
else if (gameData.ai.freePointSegs - gameData.ai.pointSegs > MAX_POINT_SEGS/2) {
	if (nLastFrameGarbageCollected + 256 < gameData.app.nFrameCount) {
		AIPathGarbageCollect ();
		}
	}
}

//	-----------------------------------------------------------------------------
//	Reset all paths.  Do garbage collection.
//	Should be called at the start of each level.
void AIResetAllPaths (void)
{
	int		i;
	tObject	*objP = OBJECTS;

for (i = gameData.objs.nLastObject [0]; i; i--, objP++)
	if (objP->controlType == CT_AI) {
		objP->cType.aiInfo.nHideIndex = -1;
		objP->cType.aiInfo.nPathLength = 0;
		}
AIPathGarbageCollect ();
}

//	---------------------------------------------------------------------------------------------------------
//	Probably called because a robot bashed a tWall, getting a bunch of retries.
//	Try to resume path.
void AttemptToResumePath (tObject *objP)
{
	//int				nObject = OBJ_IDX (objP);
	tAIStatic		*aiP = &objP->cType.aiInfo;
//	int				nGoalSegnum, object_segnum,
	int				nAbsIndex, nNewPathIndex;

if ((aiP->behavior == AIB_STATION) && (ROBOTINFO (objP->id).companion != 1))
	if (d_rand () > 8192) {
		tAILocal			*ailP = &gameData.ai.localInfo [OBJ_IDX (objP)];

		aiP->nHideSegment = objP->nSegment;
//Int3 ();
		ailP->mode = AIM_IDLING;
#if TRACE
		con_printf (1, "Note: Bashing hide tSegment of robot %i to current tSegment because he's lost.\n", OBJ_IDX (objP));
#endif
		}

//	object_segnum = objP->nSegment;
nAbsIndex = aiP->nHideIndex+aiP->nCurPathIndex;
//	nGoalSegnum = gameData.ai.pointSegs [nAbsIndex].nSegment;

nNewPathIndex = aiP->nCurPathIndex - aiP->PATH_DIR;

if ((nNewPathIndex >= 0) && (nNewPathIndex < aiP->nPathLength)) {
	aiP->nCurPathIndex = nNewPathIndex;
	}
else {
	//	At end of line and have nowhere to go.
	MoveTowardsSegmentCenter (objP);
	CreatePathToStation (objP, 15);
	}
}

//	----------------------------------------------------------------------------------------------------------
//					DEBUG FUNCTIONS FOLLOW
//	----------------------------------------------------------------------------------------------------------

#ifdef EDITOR
int	Test_size = 1000;

void test_create_path_many (void)
{
	tPointSeg	tPointSegs [200];
	short			numPoints;
	int			i;

for (i=0; i<Test_size; i++) {
	Cursegp = &gameData.segs.segments [ (d_rand () * (gameData.segs.nLastSegment + 1)) / D_RAND_MAX];
	Markedsegp = &gameData.segs.segments [ (d_rand () * (gameData.segs.nLastSegment + 1)) / D_RAND_MAX];
	CreatePathPoints (&OBJECTS [0], CurSEG_IDX (segp), MarkedSEG_IDX (segp), tPointSegs, &numPoints, -1, 0, 0, -1);
	}

}

void test_create_path (void)
{
	tPointSeg	tPointSegs [200];
	short			numPoints;

	CreatePathPoints (&OBJECTS [0], CurSEG_IDX (segp), MarkedSEG_IDX (segp), tPointSegs, &numPoints, -1, 0, 0, -1);

}

void show_path (int nStartSeg, int nEndSeg, tPointSeg *psp, short length)
{
	//printf (" [%3i:%3i (%3i):] ", nStartSeg, nEndSeg, length);

	while (length--)
		//printf ("%3i ", psp [length].nSegment);

	//printf ("\n");
}

//	For all segments in mine, create paths to all segments in mine, print results.
void test_create_all_paths (void)
{
	int	nStartSeg, nEndSeg;
	short	resultant_length;

	gameData.ai.freePointSegs = gameData.ai.pointSegs;

	for (nStartSeg=0; nStartSeg<=gameData.segs.nLastSegment-1; nStartSeg++) {
		if (gameData.segs.segments [nStartSeg].nSegment != -1) {
			for (nEndSeg=nStartSeg+1; nEndSeg<=gameData.segs.nLastSegment; nEndSeg++) {
				if (gameData.segs.segments [nEndSeg].nSegment != -1) {
					CreatePathPoints (&OBJECTS [0], nStartSeg, nEndSeg, gameData.ai.freePointSegs, &resultant_length, -1, 0, 0, -1);
					show_path (nStartSeg, nEndSeg, gameData.ai.freePointSegs, resultant_length);
				}
			}
		}
	}
}

//--anchor--int	Num_anchors;
//--anchor--int	AnchorDistance = 3;
//--anchor--int	EndDistance = 1;
//--anchor--int	Anchors [MAX_SEGMENTS];

//--anchor--int get_nearest_anchorDistance (int nSegment)
//--anchor--{
//--anchor--	short	resultant_length, minimum_length;
//--anchor--	int	anchor_index;
//--anchor--
//--anchor--	minimum_length = 16383;
//--anchor--
//--anchor--	for (anchor_index=0; anchor_index<Num_anchors; anchor_index++) {
//--anchor--		CreatePathPoints (&OBJECTS [0], nSegment, Anchors [anchor_index], gameData.ai.freePointSegs, &resultant_length, -1, 0, 0, -1);
//--anchor--		if (resultant_length != 0)
//--anchor--			if (resultant_length < minimum_length)
//--anchor--				minimum_length = resultant_length;
//--anchor--	}
//--anchor--
//--anchor--	return minimum_length;
//--anchor--
//--anchor--}
//--anchor--
//--anchor--void create_new_anchor (int nSegment)
//--anchor--{
//--anchor--	Anchors [Num_anchors++] = nSegment;
//--anchor--}
//--anchor--
//--anchor--//	A set of anchors is within N units of all segments in the graph.
//--anchor--//	AnchorDistance = how close anchors can be.p.
//--anchor--//	EndDistance = how close you can be to the end.
//--anchor--void test_create_all_anchors (void)
//--anchor--{
//--anchor--	int	nearest_anchorDistance;
//--anchor--	int	nSegment, i;
//--anchor--
//--anchor--	Num_anchors = 0;
//--anchor--
//--anchor--	for (nSegment=0; nSegment<=gameData.segs.nLastSegment; nSegment++) {
//--anchor--		if (gameData.segs.segments [nSegment].nSegment != -1) {
//--anchor--			nearest_anchorDistance = get_nearest_anchorDistance (nSegment);
//--anchor--			if (nearest_anchorDistance > AnchorDistance)
//--anchor--				create_new_anchor (nSegment);
//--anchor--		}
//--anchor--	}
//--anchor--
//--anchor--	//	Set selected segs.
//--anchor--	for (i=0; i<Num_anchors; i++)
//--anchor--		Selected_segs [i] = Anchors [i];
//--anchor--	N_selected_segs = Num_anchors;
//--anchor--
//--anchor--}
//--anchor--
//--anchor--int	Test_path_length = 5;
//--anchor--
//--anchor--void test_create_n_segment_path (void)
//--anchor--{
//--anchor--	tPointSeg	tPointSegs [200];
//--anchor--	short			numPoints;
//--anchor--
//--anchor--	CreatePathPoints (&OBJECTS [0], CurSEG_IDX (segp), -2, tPointSegs, &numPoints, Test_path_length, 0, 0, -1);
//--anchor--}

short	Player_path_length=0;
int	Player_hide_index=-1;
int	Player_cur_path_index=0;
int	Player_following_pathFlag=0;

//	------------------------------------------------------------------------------------------------------------------
//	Set orientation matrix and velocity for objP based on its desire to get to a point.
void player_path_set_orient_and_vel (tObject *objP, vmsVector *vGoalPoint)
{
	vmsVector	vCurVel = objP->mType.physInfo.velocity;
	vmsVector	vNormCurVel;
	vmsVector	vNormToGoal;
	vmsVector	vCurPos = objP->position.vPos;
	vmsVector	vNormFwd;
	fix			xSpeedScale;
	fix			dot;
	fix			xMaxSpeed;

	xMaxSpeed = ROBOTINFO (objP->id).xMaxSpeed [gameStates.app.nDifficultyLevel];

	VmVecSub (&vNormToGoal, vGoalPoint, &vCurPos);
	vmsVector::normalize(vNormToGoal);

	vNormCurVel = vCurVel;
	vmsVector::normalize(vNormCurVel);

	vNormFwd = objP->position.mOrient[FVEC];
	vmsVector::normalize(vNormFwd);

	dot = vmsVector::dot(vNormToGoal, &vNormFwd);
	if (gameData.ai.localInfo [OBJ_IDX (objP)].mode == AIM_SNIPE_RETREAT_BACKWARDS) {
		dot = -dot;
	}

	//	If very close to facing opposite desired vector, perturb vector
	if (dot < -15*F1_0/16) {
		vNormCurVel = vNormToGoal;
	} else {
		vNormCurVel[X] += vNormToGoal[X]/2;
		vNormCurVel[Y] += vNormToGoal[Y]/2;
		vNormCurVel[Z] += vNormToGoal[Z]/2;
	}

	vmsVector::normalize(vNormCurVel);

	//	Set speed based on this robot nType's maximum allowed speed and how hard it is turning.
	//	How hard it is turning is based on the dot product of (vector to goal) and (current velocity vector)
	//	Note that since 3*F1_0/4 is added to dot product, it is possible for the robot to back up.

	//	Set speed and orientation.
	if (dot < 0)
		dot /= 4;

	xSpeedScale = FixMul (xMaxSpeed, dot);
	VmVecScale (&vNormCurVel, xSpeedScale);
	objP->mType.physInfo.velocity = vNormCurVel;
	AITurnTowardsVector (&vNormToGoal, objP, F1_0);

}

//	----------------------------------------------------------------------------------------------------------
//	Optimization: If current velocity will take robot near goal, don't change velocity
void player_follow_path (tObject *objP)
{
	vmsVector	vGoalPoint;
	fix			xDistToGoal;
	int			count, forced_break, original_index;
	int			nGoalSeg;
	fix			thresholdDistance;

	if (!Player_following_pathFlag)
		return;

	if (Player_hide_index == -1)
		return;

	if (Player_path_length < 2)
		return;

	vGoalPoint = gameData.ai.pointSegs [Player_hide_index + Player_cur_path_index].point;
	nGoalSeg = gameData.ai.pointSegs [Player_hide_index + Player_cur_path_index].nSegment;
	Assert ((nGoalSeg >= 0) && (nGoalSeg <= gameData.segs.nLastSegment);
	xDistToGoal = vmsVector::dist(vGoalPoint, &objP->position.vPos);

	if (Player_cur_path_index < 0)
		Player_cur_path_index = 0;
	else if (Player_cur_path_index >= Player_path_length)
		Player_cur_path_index = Player_path_length-1;

	vGoalPoint = gameData.ai.pointSegs [Player_hide_index + Player_cur_path_index].point;

	count=0;

	//	If near goal, pick another goal point.
	forced_break = 0;		//	Gets set for short paths.
	//original_dir = 1;
	original_index = Player_cur_path_index;
	thresholdDistance = FixMul (VmVecMagQuick (&objP->mType.physInfo.velocity), gameData.time.xFrame)*2 + F1_0*2;

	while ((xDistToGoal < thresholdDistance) && !forced_break) {

		//	----- Debug stuff -----
		if (count++ > 20) {
#if TRACE
			con_printf (1, "Problem following path for player.  Aborting.\n");
#endif
			break;
		}

		//	Advance to next point on path.
		Player_cur_path_index += 1;

		//	See if next point wraps past end of path (in either direction), and if so, deal with it based on mode.p.
		if ((Player_cur_path_index >= Player_path_length) || (Player_cur_path_index < 0)) {
			Player_following_pathFlag = 0;
			forced_break = 1;
		}

		//	If went all the way around to original point, in same direction, then get out of here!
		if (Player_cur_path_index == original_index) {
#if TRACE
			con_printf (CONDBG, "Forcing break because tPlayer path wrapped, count = %i.\n", count);
#endif
			Player_following_pathFlag = 0;
			forced_break = 1;
		}

		vGoalPoint = gameData.ai.pointSegs [Player_hide_index + Player_cur_path_index].point;
		xDistToGoal = vmsVector::dist(vGoalPoint, &objP->position.vPos);

	}	//	end while

	//	Set velocity (objP->mType.physInfo.velocity) and orientation (objP->position.mOrient) for this tObject.
	player_path_set_orient_and_vel (objP, &vGoalPoint);

}


//	------------------------------------------------------------------------------------------------------------------
//	Create path for tPlayer from current tSegment to goal tSegment.
void create_player_path_to_segment (int nSegment)
{
	tObject		*objP = gameData.objs.console;

	Player_path_length=0;
	Player_hide_index=-1;
	Player_cur_path_index=0;
	Player_following_pathFlag=0;

	if (CreatePathPoints (objP, objP->nSegment, nSegment, gameData.ai.freePointSegs, &Player_path_length, 100, 0, 0, -1) == -1) {
#if TRACE
		con_printf (CONDBG, "Unable to form path of length %i for myself\n", 100);
#endif
		}

	Player_following_pathFlag = 1;

	Player_hide_index = gameData.ai.freePointSegs - gameData.ai.pointSegs;
	Player_cur_path_index = 0;
	gameData.ai.freePointSegs += Player_path_length;
	if (gameData.ai.freePointSegs - gameData.ai.pointSegs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		//Int3 ();	//	Contact Mike: This is curious, though not deadly. /eip++;g
		AIResetAllPaths ();
	}

}

int	Player_nGoalSegment = -1;

void check_create_player_path (void)
{
	if (Player_nGoalSegment != -1)
		create_player_path_to_segment (Player_nGoalSegment);

	Player_nGoalSegment = -1;
}

#endif

//	----------------------------------------------------------------------------------------------------------
//					DEBUG FUNCTIONS ENDED
//	----------------------------------------------------------------------------------------------------------


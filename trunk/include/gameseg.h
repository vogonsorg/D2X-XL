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

#ifndef _GAMESEG_H
#define _GAMESEG_H

#include "pstypes.h"
#include "fix.h"
#include "vecmat.h"
#include "segment.h"

#define PLANE_DIST_TOLERANCE	250

//figure out what seg the given point is in, tracing through segments
int get_new_seg(CFixVector *p0,int startseg);

class CSegMasks {
	public:
	   short m_face;     //which faces sphere pokes through (12 bits)
		sbyte m_side;     //which sides sphere pokes through (6 bits)
		sbyte m_center;   //which sides center point is on back of (6 bits)
		sbyte m_valid;

	public:
		CSeg () { Init (); }
		void Init (void) {
			m_face = 0;
			m_side = m_center = 0;
			m_valid = 0;
			}

		inline CSegMask& operator|= (CSegMask other) {
			if (other.m_valid) {
				m_center |= other.m_center;
				m_face |= other.m_face;
				m_side |= other.m_side;
				}
			return *this;
			}
	};


void ComputeSideRads (short nSegment, short CSide, fix *prMin, fix *prMax);
int FindConnectedSide (CSegment *base_seg, CSegment *con_seg);

// -----------------------------------------------------------------------------------
//this macro returns true if the nSegment for an CObject is correct
#define check_obj_seg(obj) (GetSegMasks(&(obj)->pos,(obj)->nSegment,0).centermask == 0)

//Tries to find a CSegment for a point, in the following way:
// 1. Check the given CSegment
// 2. Recursively trace through attached segments
// 3. Check all the segmentns
//Returns nSegment if found, or -1
int FindSegByPos(const CFixVector& vPos, int nSegment, int bExhaustive, int bSkyBox, int nThread = 0);
short FindClosestSeg (CFixVector& vPos);

//--repair-- // Create data specific to segments which does not need to get written to disk.
//--repair-- extern void create_local_segment_data(void);

//      Sort of makes sure create_local_segment_data has been called for the currently executing mine.
//      Returns 1 if Lsegments appears valid, 0 if not.
int check_lsegments_validity(void);

//      ----------------------------------------------------------------------------------------------------------
//      Determine whether seg0 and seg1 are reachable using widFlag to go through walls.
//      For example, set to WID_RENDPAST_FLAG to see if sound can get from one CSegment to the other.
//      set to WID_FLY_FLAG to see if a robot could fly from one to the other.
//      Search up to a maximum depth of max_depth.
//      Return the distance.
fix FindConnectedDistance(CFixVector *p0, short seg0, CFixVector *p1, short seg1, int max_depth, int widFlag, int bUseCache);

//create a matrix that describes the orientation of the given CSegment
void ExtractOrientFromSegment(CFixMatrix *m,CSegment *seg);

//      In CSegment.c
//      Make a just-modified CSegment valid.
//              check all sides to see how many faces they each should have (0,1,2)
//              create new vector normals
void ValidateSegments(void);

//      Extract the forward vector from CSegment *sp, return in *vp.
//      The forward vector is defined to be the vector from the the center of the front face of the CSegment
// to the center of the back face of the CSegment.
void extract_forward_vector_from_segment(CSegment *sp,CFixVector *vp);

//      Extract the right vector from CSegment *sp, return in *vp.
//      The forward vector is defined to be the vector from the the center of the left face of the CSegment
// to the center of the right face of the CSegment.
void extract_right_vector_from_segment(CSegment *sp,CFixVector *vp);

//      Extract the up vector from CSegment *sp, return in *vp.
//      The forward vector is defined to be the vector from the the center of the bottom face of the CSegment
// to the center of the top face of the CSegment.
void extract_up_vector_from_segment(CSegment *sp,CFixVector *vp);

int GetVertsForNormal (int v0, int v1, int v2, int v3, int *pv0, int *pv1, int *pv2, int *pv3);
int GetVertsForNormalTri (int v0, int v1, int v2, int *pv0, int *pv1, int *pv2);

void ComputeVertexNormals (void);
void ResetVertexNormals (void);
float FaceSize (short nSegment, ubyte nSide);

#endif



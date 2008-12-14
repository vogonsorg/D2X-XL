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
COPYRIGHT 1993-1998PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include "3d.h"
#include "globvars.h"

// -----------------------------------------------------------------------------------
//checks for overflow & divides if ok, fillig in r
//returns true if div is ok, else false
#ifdef _WIN32
inline
#endif
int CheckMulDiv (fix *r, fix a, fix b, fix c)
{
#ifdef _WIN32
	QLONG	q;
	if (!c)
		return 0;
	q = mul64 (a, b) / (QLONG) c;
	if ((q > 0x7fffffff) || (q < -0x7fffffff))
		return 0;
	*r = (fix) q;
	return 1;
#else
	tQuadInt q,qt;

	q.low=q.high=0;
	FixMulAccum (&q,a,b);
	qt = q;
	if (qt.high < 0)
		FixQuadNegate (&qt);
	qt.high *= 2;
	if (qt.low > 0x7fff)
		qt.high++;
	if (qt.high >= c)
		return 0;
	else {
		*r = FixDivQuadLong (q.low,q.high,c);
		return 1;
	}
#endif
}

// -----------------------------------------------------------------------------------
//projects a point
void G3ProjectPoint (g3sPoint *p)
{
if ((p->p3_flags & PF_PROJECTED) || (p->p3_codes & CC_BEHIND))
	return;
CFloatVector3	v;
v [X] = X2F (p->p3_vec [X]) * viewInfo.scalef [X];
v [Y] = X2F (p->p3_vec [Y]) * viewInfo.scalef [Y];
v [Z] = X2F (p->p3_vec [Z]) * viewInfo.scalef [Z];
p->p3_screen.x = (fix) (fxCanvW2 + v [X] * fxCanvW2 / v [Z]);
p->p3_screen.y = (fix) (fxCanvH2 - v [Y] * fxCanvH2 / v [Z]);
p->p3_flags |= PF_PROJECTED;
}

// -----------------------------------------------------------------------------------
//from a 2d point, compute the vector through that point
void G3Point2Vec (CFixVector *v,short sx,short sy)
{
	CFixVector tempv;
	CFixMatrix tempm;

tempv [X] =  FixMulDiv (FixDiv ((sx<<16) - xCanvW2,xCanvW2),viewInfo.scale [Z], viewInfo.scale [X]);
tempv [Y] = -FixMulDiv (FixDiv ((sy<<16) - xCanvH2,xCanvH2),viewInfo.scale [Z], viewInfo.scale [Y]);
tempv [Z] = f1_0;
CFixVector::Normalize(tempv);
tempm = viewInfo.view [1].Transpose();
*v = tempm * tempv;
}

// -----------------------------------------------------------------------------------
//delta rotation functions
CFixVector *G3RotateDeltaX (CFixVector *dest,fix dx)
{
	(*dest) [X] = FixMul (viewInfo.view [0].RVec() [X], dx);
	(*dest) [Y] = FixMul (viewInfo.view [0].UVec() [X], dx);
	(*dest) [Z] = FixMul (viewInfo.view [0].FVec() [X], dx);

	return dest;
}

// -----------------------------------------------------------------------------------

CFixVector *G3RotateDeltaY (CFixVector *dest,fix dy)
{
	(*dest) [X] = FixMul (viewInfo.view [0].RVec() [Y],dy);
	(*dest) [Y] = FixMul (viewInfo.view [0].UVec() [Y],dy);
	(*dest) [Z] = FixMul (viewInfo.view [0].FVec() [Y],dy);

	return dest;
}

// -----------------------------------------------------------------------------------

CFixVector *G3RotateDeltaZ (CFixVector *dest,fix dz)
{
	(*dest) [X] = FixMul (viewInfo.view [0].RVec() [Z],dz);
	(*dest) [Y] = FixMul (viewInfo.view [0].UVec() [Z],dz);
	(*dest) [Z] = FixMul (viewInfo.view [0].FVec() [Z],dz);

	return dest;
}

// -----------------------------------------------------------------------------------

const CFixVector& G3RotateDeltaVec (CFixVector& dest, const CFixVector& src) {
	dest = viewInfo.view [0] * src;
	return dest;
}

// -----------------------------------------------------------------------------------

ubyte G3AddDeltaVec (g3sPoint *dest, g3sPoint *src, CFixVector *vDelta)
{
dest->p3_vec = src->p3_vec + *vDelta;
dest->p3_flags = 0;		//not projected
return G3EncodePoint (dest);
}

// -----------------------------------------------------------------------------------
//calculate the depth of a point - returns the z coord of the rotated point
fix G3CalcPointDepth(const CFixVector& pnt)
{
#ifdef _WIN32
	QLONG q = mul64 (pnt [X] - viewInfo.pos [X], viewInfo.view [0].FVec() [X]);
	q += mul64 (pnt [Y] - viewInfo.pos [Y], viewInfo.view [0].FVec() [Y]);
	q += mul64 (pnt [Z] - viewInfo.pos [Z], viewInfo.view [0].FVec() [Z]);
	return (fix) (q >> 16);
#else
	tQuadInt q;

	q.low=q.high=0;
	FixMulAccum (&q, (pnt [X] - viewInfo.pos [X]),viewInfo.view [0].FVec() [X]);
	FixMulAccum (&q, (pnt [Y] - viewInfo.pos [Y]),viewInfo.view [0].FVec() [Y]);
	FixMulAccum (&q, (pnt [Z] - viewInfo.pos [Z]),viewInfo.view [0].FVec() [Z]);
	return FixQuadAdjust (&q);
#endif
}

// -----------------------------------------------------------------------------------
//eof


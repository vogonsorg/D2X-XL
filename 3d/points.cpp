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

#include "descent.h"
#include "3d.h"
#include "globvars.h"
#include "canvas.h"

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

//------------------------------------------------------------------------------

inline int ScreenScale (void)
{
return (!gameStates.render.cameras.bActive || gameOpts->render.cameras.bHires) ? 1 : 2;
}

inline int ScreenWidth (void)
{
return screen.Width () /*/ ScreenScale ()*/;
}

inline int ScreenHeight (void)
{
return screen.Height () /*/ ScreenScale ()*/;
}

// -----------------------------------------------------------------------------------

ubyte OglProjectPoint (CFloatVector3& p, tScreenPos& s, ubyte flags, ubyte codes)
{
if ((flags & PF_PROJECTED) || (codes & CC_BEHIND))
	return flags;
double x, y, z;
gluProject ((double) p.v.coord.x, (double) p.v.coord.y, (double) p.v.coord.z, 
				&transformation.m_info.oglModelview [0], &transformation.m_info.oglProjection [0], transformation.m_info.oglViewport, 
				&x, &y, &z);
#if 1
s.x = fix (x);
s.y = fix (y);
#else
s.x = fix (CCanvas::xCanvW2f - x * CCanvas::xCanvW2f / z);
s.y = fix (CCanvas::fCanvH2 - y * CCanvas::fCanvH2 / z);
#endif
#if DBG
tScreenPos h;
OglProjectPoint (p, h);
if ((h.x != s.x) || (h.y != s.y))
	s = h;
#endif
return flags | PF_PROJECTED;
}

// -----------------------------------------------------------------------------------
//projects a point

ubyte ProjectPoint (CFloatVector3& p, tScreenPos& s, ubyte flags, ubyte codes)
{
if ((flags & PF_PROJECTED) || (codes & CC_BEHIND))
	return flags;
CFloatVector3 v = transformation.m_info.projection * p;
s.x = fix (CCanvas::fCanvW2 * (1.0f - v.v.coord.x / fabs (v.v.coord.z)));
s.y = fix (CCanvas::fCanvH2 * (1.0f - v.v.coord.y / fabs (v.v.coord.z)));
return flags | PF_PROJECTED;
}

// -----------------------------------------------------------------------------------

ubyte ProjectPoint (CFixVector& v, tScreenPos& s, ubyte flags, ubyte codes)
{
CFloatVector3 h;
h.Assign (v);
return ProjectPoint (h, s, flags);
}

// -----------------------------------------------------------------------------------

void G3ProjectPoint (g3sPoint *p)
{
p->m_flags = ProjectPoint (p->m_vec, p->m_screen, p->m_flags, p->m_codes);
}

// -----------------------------------------------------------------------------------
//from a 2d point, compute the vector through that point
void G3Point2Vec (CFixVector *v,short sx,short sy)
{
	CFixVector h;
	CFixMatrix m;

h.v.coord.x =  FixMulDiv (FixDiv ((sx<<16) - CCanvas::xCanvW2, CCanvas::xCanvW2), transformation.m_info.scale.v.coord.z, transformation.m_info.scale.v.coord.x);
h.v.coord.y = -FixMulDiv (FixDiv ((sy<<16) - CCanvas::xCanvH2, CCanvas::xCanvH2), transformation.m_info.scale.v.coord.z, transformation.m_info.scale.v.coord.y);
h.v.coord.z = I2X (1);
CFixVector::Normalize (h);
m = transformation.m_info.view [1].Transpose();
*v = m * h;
}

// -----------------------------------------------------------------------------------

ubyte G3AddDeltaVec (g3sPoint *dest, g3sPoint *src, CFixVector *vDelta)
{
dest->m_vec = src->m_vec + *vDelta;
dest->m_flags = 0;		//not projected
return G3EncodePoint (dest);
}

// -----------------------------------------------------------------------------------
//calculate the depth of a point - returns the z coord of the rotated point
fix G3CalcPointDepth (const CFixVector& pnt)
{
#ifdef _WIN32
	QLONG q = mul64 (pnt.v.coord.x - transformation.m_info.pos.v.coord.x, transformation.m_info.view [0].m.dir.f.v.coord.x);
	q += mul64 (pnt.v.coord.y - transformation.m_info.pos.v.coord.y, transformation.m_info.view [0].m.dir.f.v.coord.y);
	q += mul64 (pnt.v.coord.z - transformation.m_info.pos.v.coord.z, transformation.m_info.view [0].m.dir.f.v.coord.z);
	return (fix) (q >> 16);
#else
	tQuadInt q;

	q.low=q.high=0;
	FixMulAccum (&q, (pnt.v.coord.x - transformation.m_info.pos.v.coord.x),transformation.m_info.view [0].m.dir.f.v.coord.x);
	FixMulAccum (&q, (pnt.v.coord.y - transformation.m_info.pos.v.coord.y),transformation.m_info.view [0].m.dir.f.v.coord.y);
	FixMulAccum (&q, (pnt.v.coord.z - transformation.m_info.pos.v.coord.z),transformation.m_info.view [0].m.dir.f.v.coord.z);
	return FixQuadAdjust (&q);
#endif
}

// -----------------------------------------------------------------------------------
//eof


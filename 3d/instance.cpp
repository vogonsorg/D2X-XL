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
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <stdlib.h>

#include "inferno.h"
#include "error.h"
#include "3d.h"
#include "globvars.h"
#include "ogl_defs.h"
#include "vecmat.h"
#include "interp.h"
#include "oof.h"

#define MAX_INSTANCE_DEPTH	10

struct tViewInfo instanceStack [MAX_INSTANCE_DEPTH];

int nInstanceDepth = 0;

//------------------------------------------------------------------------------

inline void OglMove (float *pv)
{
glTranslatef (-pv [0], -pv [1], -pv [2]);
}

//------------------------------------------------------------------------------

inline void OglRot (float *pm)
{
glMultMatrixf (pm);
}

//------------------------------------------------------------------------------

void VmsMove (vmsVector *pv)
{
glVectorf p;
OglMove (OOF_VecVms2Gl (p, pv));
}

//------------------------------------------------------------------------------

inline void VmsRot (vmsMatrix *pm)
{
glMatrixf m;

memset (m, 0, sizeof (m));
m [0] = X2F (pm->rVec.p.x);
m [1] = X2F (pm->rVec.p.y);
m [2] = X2F (pm->rVec.p.z);
m [4] = X2F (pm->uVec.p.x);
m [5] = X2F (pm->uVec.p.y);
m [6] = X2F (pm->uVec.p.z);
m [8] = X2F (pm->fVec.p.x);
m [9] = X2F (pm->fVec.p.y);
m [10] = X2F (pm->fVec.p.z);
m [15] = 1;
OglRot (m);
}

//------------------------------------------------------------------------------

int G3PushMatrix (void)
{
if (nInstanceDepth >= MAX_INSTANCE_DEPTH)
	return 0;
instanceStack [nInstanceDepth++] = viewInfo;
#if 0
glMatrixMode (GL_PROJECTION);
glPushMatrix ();
#endif
glMatrixMode (GL_MODELVIEW);
glPushMatrix ();
return 1;
}

//------------------------------------------------------------------------------

int G3PopMatrix (void)
{
if (nInstanceDepth <= 0)
	return 0;
viewInfo = instanceStack [--nInstanceDepth];
#if 0
glMatrixMode (GL_PROJECTION);
glPopMatrix ();
#endif
glMatrixMode (GL_MODELVIEW);
glPopMatrix ();
return 1;
}

//------------------------------------------------------------------------------
//instance at specified point with specified orientation
//if matrix==NULL, don't modify matrix.  This will be like doing an offset   
void G3StartInstanceMatrix (vmsVector *vPos, vmsMatrix *mOrient)
{
	vmsVector	vOffs;
	vmsMatrix	mTrans, mRot;

//Assert (nInstanceDepth < MAX_INSTANCE_DEPTH);
if (!G3PushMatrix ())
	return;
if (gameStates.ogl.bUseTransform) {
	vmsVector	h;

	if (nInstanceDepth > 1) {
		glScalef (-1.0f, -1.0f, -1.0f);
		VmsMove (vPos);
		glScalef (-1.0f, -1.0f, -1.0f);
		if (mOrient)
			VmsRot (mOrient);
		}
	else {
		glLoadIdentity ();
		//glScalef (X2F (viewInfo.scale.p.x), X2F (viewInfo.scale.p.y), -X2F (viewInfo.scale.p.z));
		glScalef (1, 1, -1);
		OglRot (viewInfo.glViewf);
		VmVecSub (&h, &viewInfo.pos, vPos);
		VmsMove (&h);
		if (gameData.models.nScale) {
			float fScale = X2F (gameData.models.nScale);
			glScalef (fScale, fScale, fScale);
			}
		if (mOrient)
			VmsRot (mOrient);
		}
	}

//step 1: subtract object position from view position
VmVecSub (&vOffs, &viewInfo.pos, vPos);
if (mOrient) {
	int i;
	//step 2: rotate view vector through tObject matrix
	VmVecRotate (&viewInfo.pos, &vOffs, mOrient);
	//step 3: rotate tObject matrix through view_matrix (vm = ob * vm)
	VmCopyTransposeMatrix (&mTrans, mOrient);
	for (i = 0; i < 2; i++) {
		VmMatMul (&mRot, &mTrans, viewInfo.view + i);
		viewInfo.view [i] = mRot;
		VmsMatToFloat (viewInfo.viewf + i, viewInfo.view + i);
		}
	}
VmVecFixToFloat (&viewInfo.posf, &viewInfo.pos);
}

//------------------------------------------------------------------------------
//instance at specified point with specified orientation
//if angles==NULL, don't modify matrix.  This will be like doing an offset
void G3StartInstanceAngles (vmsVector *pos, vmsAngVec *angles)
{
	vmsMatrix tm;

VmAngles2Matrix (&tm, angles ? angles : &avZero);
G3StartInstanceMatrix (pos, &tm);
}

//------------------------------------------------------------------------------
//pops the old context
void G3DoneInstance ()
{
if (!G3PopMatrix ())
	return;
#if 0
if (gameStates.ogl.bUseTransform) {
	glMatrixMode (GL_MODELVIEW);
	glPopMatrix ();
	}
VmVecFixToFloat (&viewInfo.posf, &viewInfo.pos);
VmsMatToFloat (viewInfo.viewf, viewInfo.view);
#endif
}

//------------------------------------------------------------------------------
//eof

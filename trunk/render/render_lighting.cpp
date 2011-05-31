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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "descent.h"
#include "error.h"
#include "u_mem.h"
#include "light.h"
#include "dynlight.h"
#include "lightmap.h"
#include "automap.h"
#include "texmerge.h"
#include "ogl_lib.h"
#include "ogl_color.h"
#include "ogl_fastrender.h"
#include "glare.h"
#include "rendermine.h"
#include "renderthreads.h"
#include "gpgpu_lighting.h"
#include "fastrender.h"

// -----------------------------------------------------------------------------------

int SegmentIsVisible (CSegment *segP)
{
if (automap.Display ())
	return 1;
#if 0
return RotateVertexList (8, segP->m_verts).ccAnd == 0;
#else
ubyte code = 0xFF;

#if DBG
if (segP->Index () == nDbgSeg)
	nDbgSeg = nDbgSeg;
#endif
for (int i = 0; i < 8; i++) {
#if 0 //DBG
	gameData.segs.points [segP->m_verts [i]].m_flags = 0;
#endif
	code &= ProjectRenderPoint (segP->m_verts [i]);
	if (!code)
		return 1;
	}
return 0;
#endif
}

//------------------------------------------------------------------------------

static int FaceIsVisible (CSegFace* faceP)
{
#if DBG
if ((faceP->m_info.nSegment == nDbgSeg) && ((nDbgSide < 0) || (faceP->m_info.nSide == nDbgSide)))
	nDbgSeg = nDbgSeg;
#endif
if (!FaceIsVisible (faceP->m_info.nSegment, faceP->m_info.nSide))
	return faceP->m_info.bVisible = 0;
if ((faceP->m_info.bSparks == 1) && gameOpts->render.effects.bEnabled && gameOpts->render.effects.bEnergySparks)
	return faceP->m_info.bVisible = 0;
return faceP->m_info.bVisible = 1;
}

//------------------------------------------------------------------------------

int SetupFace (short nSegment, short nSide, CSegment *segP, CSegFace *faceP, CFaceColor *faceColorP, float *fAlphaP)
{
	ubyte	bTextured, bCloaked, bTransparent, bWall;
	int	nColor = 0;

#if DBG
if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
	nDbgSeg = nDbgSeg;
if (FACE_IDX (faceP) == nDbgFace)
	nDbgFace = nDbgFace;
#endif

if (!FaceIsVisible (faceP))
	return -1;
bWall = IS_WALL (faceP->m_info.nWall);
if (bWall) {
	faceP->m_info.widFlags = segP->IsDoorWay (nSide, NULL);
	if (!(faceP->m_info.widFlags & WID_RENDER_FLAG)) //(WID_RENDER_FLAG | WID_RENDPAST_FLAG)))
		return -1;
	}
else
	faceP->m_info.widFlags = WID_RENDER_FLAG;
faceP->m_info.nCamera = IsMonitorFace (nSegment, nSide, 0);
bTextured = 1;
bCloaked = 0;
bTransparent = 0;
if (bWall)
	*fAlphaP = WallAlpha (nSegment, nSide, faceP->m_info.nWall, faceP->m_info.widFlags, faceP->m_info.nCamera >= 0, faceP->m_info.bAdditive,
								 &faceColorP [1], &nColor, &bTextured, &bCloaked, &bTransparent);
else
	*fAlphaP = 1;
faceP->m_info.bTextured = bTextured;
faceP->m_info.bCloaked = bCloaked;
faceP->m_info.bTransparent |= bTransparent;
if (faceP->m_info.bSegColor) {
	if ((faceP->m_info.nSegColor = IsColoredSegFace (nSegment, nSide))) {
		faceP->m_info.color = *ColoredSegmentColor (nSegment, nSide, faceP->m_info.nSegColor);
		faceColorP [2].Assign (faceP->m_info.color);
		if (faceP->m_info.nBaseTex < 0)
			*fAlphaP = faceP->m_info.color.Alpha ();
		nColor = 2;
		}
	else
		faceP->m_info.bVisible = (faceP->m_info.nBaseTex >= 0);
	}
if ((*fAlphaP < 1) || ((nColor == 2) && (faceP->m_info.nBaseTex < 0)))
	faceP->m_info.bTransparent = 1;
return nColor;
}

//------------------------------------------------------------------------------

void ComputeDynamicFaceLight (int nStart, int nEnd, int nThread)
{
PROF_START
	CSegFace*		faceP;
	CFloatVector*	colorP;
	CFaceColor		c, faceColor [3];
#if 0
	ubyte			nThreadFlags [3] = {1 << nThread, 1 << !nThread, ~(1 << nThread)};
#endif
	short				nVertex, nSegment, nSide;
	float				fAlpha;
	int				h, i, nColor, nLights = 0;
	//int				bVertexLight = gameStates.render.bPerPixelLighting != 2;
	int				bLightmaps = lightmapManager.HaveLightmaps ();
	bool				bNeedLight = !gameStates.render.bFullBright && (gameStates.render.bPerPixelLighting != 2);
	static			CFaceColor brightColor;

for (i = 0; i < 3; i++)
	faceColor [i].Set (0.0f, 0.0f, 0.0f, 1.0f);
//memset (&gameData.render.lights.dynamic.shader.index, 0, sizeof (gameData.render.lights.dynamic.shader.index));
ogl.SetTransform (1);
gameStates.render.nState = 0;
#	if GPGPU_VERTEX_LIGHTING
if (ogl.m_states.bVertexLighting)
	ogl.m_states.bVertexLighting = gpgpuLighting.Compute (-1, 0, NULL);
#endif

for (i = nStart; i < nEnd; i++) {
	faceP = FACES.faces + i;
	nSegment = faceP->m_info.nSegment;
	nSide = faceP->m_info.nSide;

#if DBG
	if (nSegment == nDbgSeg)
		nSegment = nSegment;
#endif
	if (0 > (nColor = SetupFace (nSegment, nSide, SEGMENTS + nSegment, faceP, faceColor, &fAlpha))) {
		faceP->m_info.bVisible = 0;
		continue;
		}
	if (bNeedLight)
		nLights = lightManager.SetNearestToSegment (nSegment, -1, 0, 0, nThread);	//only get light emitting objects here (variable geometry lights are caught in lightManager.SetNearestToVertex ())
#if DBG
	if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide))) {
		nSegment = nSegment;
		if (lightManager.Index (0, nThread).nActive)
			nSegment = nSegment;
		}
#endif
	AddFaceListItem (faceP, nThread);
	faceP->m_info.color.Assign (faceColor [nColor]);
	colorP = FACES.color + faceP->m_info.nIndex;
	for (h = 0; h < 4; h++, colorP++) {
		if (gameStates.render.bFullBright)
			*colorP = nColor ? faceColor [nColor] : brightColor;
		else {
			if (bLightmaps) {
				c = faceColor [nColor];
				if (nColor)
					colorP->Assign (c);
				}
			else {
				nVertex = faceP->m_info.index [h];
#if DBG
				if (nVertex == nDbgVertex)
					nDbgVertex = nDbgVertex;
#endif
				if (gameStates.render.bPerPixelLighting == 2)
					*colorP = gameData.render.color.ambient [nVertex];
				else {
					c.color = gameData.render.color.ambient [nVertex];
					CFaceColor *vertColorP = gameData.render.color.vertices + nVertex;
					if (vertColorP->index != gameStates.render.nFrameFlipFlop + 1) {
						if (nLights + lightManager.VariableVertLights (nVertex) == 0) 
#pragma omp critical
							{
							*vertColorP = c;
							vertColorP->index = gameStates.render.nFrameFlipFlop + 1;
							}
						else {
#if DBG
							if (nVertex == nDbgVertex)
								nDbgVertex = nDbgVertex;
#endif
							G3VertexColor (nSegment, nSide, nVertex,
												gameData.segs.points [nVertex].GetNormal ()->XYZ (), gameData.segs.fVertices [nVertex].XYZ (),
											   NULL, &c, 1, 0, nThread);
							lightManager.Index (0, nThread) = lightManager.Index (1, nThread);
							lightManager.ResetNearestToVertex (nVertex, nThread);
							}
#if DBG
						if (nVertex == nDbgVertex)
							nVertex = nVertex;
#endif
						}
					*colorP = *vertColorP;
					}
				}
			if (nColor == 1)
				*colorP *= faceColor [nColor];
			colorP->Alpha () = fAlpha;
			}
		}
	lightManager.Material ().bValid = 0;
	}
PROF_END(ptLighting)
ogl.SetTransform (0);
}

//------------------------------------------------------------------------------

void FixTriangleFan (CSegment* segP, CSegFace* faceP)
{
if (segP->Type (faceP->m_info.nSide) == SIDE_IS_TRI_13) 
#if USE_OPEN_MP > 1
#pragma omp critical
#endif
	{	//rearrange vertex order for TRIANGLE_FAN rendering
#if !USE_OPENMP
	SDL_mutexP (tiRender.semaphore);
#endif
	segP->SetType (faceP->m_info.nSide, faceP->m_info.nType = SIDE_IS_TRI_02);

	short	h = faceP->m_info.index [0];
	memcpy (faceP->m_info.index, faceP->m_info.index + 1, 3 * sizeof (short));
	faceP->m_info.index [3] = h;

	CFloatVector3 v = FACES.vertices [faceP->m_info.nIndex];
	memcpy (FACES.vertices + faceP->m_info.nIndex, FACES.vertices + faceP->m_info.nIndex + 1, 3 * sizeof (CFloatVector3));
	FACES.vertices [faceP->m_info.nIndex + 3] = v;

	tTexCoord2f tc = FACES.texCoord [faceP->m_info.nIndex];
	memcpy (FACES.texCoord + faceP->m_info.nIndex, FACES.texCoord + faceP->m_info.nIndex + 1, 3 * sizeof (tTexCoord2f));
	FACES.texCoord [faceP->m_info.nIndex + 3] = tc;
	tc = FACES.lMapTexCoord [faceP->m_info.nIndex];
	memcpy (FACES.lMapTexCoord + faceP->m_info.nIndex, FACES.lMapTexCoord + faceP->m_info.nIndex + 1, 3 * sizeof (tTexCoord2f));
	FACES.lMapTexCoord [faceP->m_info.nIndex + 3] = tc;
	if (faceP->m_info.nOvlTex) {
		tc = FACES.ovlTexCoord [faceP->m_info.nIndex];
		memcpy (FACES.ovlTexCoord + faceP->m_info.nIndex, FACES.ovlTexCoord + faceP->m_info.nIndex + 1, 3 * sizeof (tTexCoord2f));
		FACES.ovlTexCoord [faceP->m_info.nIndex + 3] = tc;
		}
#if !USE_OPENMP
	SDL_mutexV (tiRender.semaphore);
#endif
	}
}

//------------------------------------------------------------------------------

void ComputeDynamicQuadLight (int nStart, int nEnd, int nThread)
{
#if 0
	static int bSemaphore [2] = {0, 0};

while (bSemaphore [nThread])
	G3_SLEEP (0);
bSemaphore [nThread] = 1;
#endif
PROF_START
	CSegment*		segP;
	tSegFaces*		segFaceP;
	CSegFace*		faceP;
	CFloatVector*	colorP;
	CFaceColor		c, faceColor [3];
#if 0
	ubyte			nThreadFlags [3] = {1 << nThread, 1 << !nThread, ~(1 << nThread)};
#endif
	short			nVertex, nSegment, nSide;
	float			fAlpha;
	int			h, i, j, nColor, nLights = 0,
					bVertexLight = gameStates.render.bPerPixelLighting != 2,
					bLightmaps = lightmapManager.HaveLightmaps ();
	static		CFaceColor brightColor = {{1,1,1,1},1};

//memset (&gameData.render.lights.dynamic.shader.index, 0, sizeof (gameData.render.lights.dynamic.shader.index));
for (i = 0; i < 3; i++)
	faceColor [i].Set (0.0f, 0.0f, 0.0f, 1.0f);
ogl.SetTransform (1);
gameStates.render.nState = 0;
#	if GPGPU_VERTEX_LIGHTING
if (ogl.m_states.bVertexLighting)
	ogl.m_states.bVertexLighting = gpgpuLighting.Compute (-1, 0, NULL);
#endif
for (i = nStart; i < nEnd; i++) {
	if (0 > (nSegment = gameData.render.mine.segRenderList [0][i]))
		continue;
	segP = SEGMENTS + nSegment;
	segFaceP = SEGFACES + nSegment;
	if (!(/*gameStates.app.bMultiThreaded ||*/ SegmentIsVisible (segP))) {
		gameData.render.mine.segRenderList [0][i] = -gameData.render.mine.segRenderList [0][i];
		continue;
		}
#if DBG
	if (nSegment == nDbgSeg)
		nSegment = nSegment;
#endif
	if (bVertexLight && !gameStates.render.bFullBright)
		nLights = lightManager.SetNearestToSegment (nSegment, -1, 0, 0, nThread);	//only get light emitting objects here (variable geometry lights are caught in lightManager.SetNearestToVertex ())
	for (j = segFaceP->nFaces, faceP = segFaceP->faceP; j; j--, faceP++) {
		nSide = faceP->m_info.nSide;
#if DBG
		if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide))) {
			nSegment = nSegment;
			if (lightManager.Index (0, nThread).nActive)
				nSegment = nSegment;
			}
#endif
		if (0 > (nColor = SetupFace (nSegment, nSide, segP, faceP, faceColor, &fAlpha))) {
			faceP->m_info.bVisible = 0;
			continue;
			}
		if (!AddFaceListItem (faceP, nThread))
			continue;
		FixTriangleFan (segP, faceP);
		faceP->m_info.color = faceColor [nColor].color;
//			SetDynLightMaterial (nSegment, faceP->m_info.nSide, -1);
		colorP = FACES.color + faceP->m_info.nIndex;
		for (h = 0; h < 4; h++, colorP++) {
			if (gameStates.render.bFullBright)
				*colorP = nColor ? faceColor [nColor].color : brightColor.color;
			else {
				c = faceColor [nColor];
				if (nColor)
					*colorP = c;
				if (!bLightmaps) {
					nVertex = faceP->m_info.index [h];
#if DBG
					if (nVertex == nDbgVertex)
						nDbgVertex = nDbgVertex;
#endif
					if (gameStates.render.bPerPixelLighting == 2)
						*colorP = gameData.render.color.ambient [nVertex].color;
					else {
						CFaceColor *vertColorP = gameData.render.color.vertices + nVertex;
						if (vertColorP->index != gameStates.render.nFrameFlipFlop + 1) {
#if DBG
							if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
								nSegment = nSegment;
#endif
							if (nLights + lightManager.VariableVertLights ()[nVertex] == 0) 
#pragma omp critical
								{
								vertColorP->index = gameStates.render.nFrameFlipFlop + 1;
								*vertColorP = c + gameData.render.color.ambient [nVertex];
								}
							else {
#if DBG
								if (nVertex == nDbgVertex)
									nDbgVertex = nDbgVertex;
#endif
								G3VertexColor (nSegment, nSide, nVertex,
													gameData.segs.points [nVertex].GetNormal ()->XYZ (), gameData.segs.fVertices [nVertex].XYZ (), 
													NULL, &c, 1, 0, nThread);
								lightManager.Index (0, nThread) = lightManager.Index (1, nThread);
								lightManager.ResetNearestToVertex (nVertex, nThread);
								}
#if DBG
							if (nVertex == nDbgVertex)
								nVertex = nVertex;
#endif
							}
						*colorP = *vertColorP;
						}
					}
				if (nColor == 1)
					*colorP *= faceColor [nColor];
				colorP->Alpha () = fAlpha;
				}
			}
		lightManager.Material ().bValid = 0;
		}
	}
#	if GPGPU_VERTEX_LIGHTING
if (ogl.m_states.bVertexLighting)
	gpgpuLighting.Compute (-1, 2, NULL);
#endif
PROF_END(ptLighting)
ogl.SetTransform (0);
#if 0
bSemaphore [nThread] = 0;
#endif
}

//------------------------------------------------------------------------------

void ComputeDynamicTriangleLight (int nStart, int nEnd, int nThread)
{
PROF_START
	CSegment*		segP;
	tSegFaces*		segFaceP;
	CSegFace*		faceP;
	tFaceTriangle*	triP;
	CFloatVector*	colorP;
	CFaceColor		c, faceColor [3] = {{{0,0,0,1},1},{{0,0,0,0},1},{{0,0,0,1},1}};
#if 0
	ubyte			nThreadFlags [3] = {1 << nThread, 1 << !nThread, ~(1 << nThread)};
#endif
	short			nVertex, nSegment, nSide;
	float			fAlpha;
	int			h, i, j, k, nIndex, nColor, nLights = 0;
	bool			bNeedLight = !gameStates.render.bFullBright && (gameStates.render.bPerPixelLighting != 2);

	static		CFaceColor brightColor = {{1,1,1,1},1};

ogl.SetTransform (1);
gameStates.render.nState = 0;
#	if GPGPU_VERTEX_LIGHTING
if (ogl.m_states.bVertexLighting)
	ogl.m_states.bVertexLighting = gpgpuLighting.Compute (-1, 0, NULL);
#endif
for (i = nStart; i < nEnd; i++) {
	if (0 > (nSegment = gameData.render.mine.segRenderList [0][i]))
		continue;
	segP = SEGMENTS + nSegment;
	segFaceP = SEGFACES + nSegment;
	if (!(/*gameStates.app.bMultiThreaded ||*/ SegmentIsVisible (segP))) {
		gameData.render.mine.segRenderList [0][i] = -gameData.render.mine.segRenderList [0][i] - 1;
		continue;
		}
#if DBG
	if (nSegment == nDbgSeg)
		nSegment = nSegment;
#endif
	bool bComputeLight = bNeedLight;
	for (j = segFaceP->nFaces, faceP = segFaceP->faceP; j; j--, faceP++) {
		nSide = faceP->m_info.nSide;
#if DBG
		if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide))) {
			nSegment = nSegment;
			if (lightManager.Index (0, nThread).nActive)
				nSegment = nSegment;
			}
#endif
		if (0 > (nColor = SetupFace (nSegment, nSide, segP, faceP, faceColor, &fAlpha))) {
			faceP->m_info.bVisible = 0;
			continue;
			}
		if (!AddFaceListItem (faceP, nThread))
			continue;
		faceP->m_info.color = faceColor [nColor].color;
		if (!(bNeedLight || nColor) && faceP->m_info.bHasColor)
			continue;
		if (bComputeLight) {
			nLights = lightManager.SetNearestToSegment (nSegment, -1, 0, 0, nThread);	//only get light emitting objects here (variable geometry lights are caught in lightManager.SetNearestToVertex ())
			bComputeLight = false;
			}
		faceP->m_info.bHasColor = 1;
		for (k = faceP->m_info.nTris, triP = FACES.tris + faceP->m_info.nTriIndex; k; k--, triP++) {
			nIndex = triP->nIndex;
			colorP = FACES.color + nIndex;
			for (h = 0; h < 3; h++, colorP++, nIndex++) {
				if (gameStates.render.bFullBright)
					*colorP = nColor ? faceColor [nColor] : brightColor;
				else {
					c = faceColor [nColor];
					if (nColor)
						*colorP = c;
					nVertex = triP->index [h];
#if DBG
					if (nVertex == nDbgVertex)
						nDbgVertex = nDbgVertex;
#endif
					if (gameStates.render.bPerPixelLighting == 2)
						*colorP = gameData.render.color.ambient [nVertex];
					else {
						CFaceColor *vertColorP = gameData.render.color.vertices + nVertex;
#if 0 //DBG
						vertColorP->Red () = 
						vertColorP->Green () = 
						vertColorP->Blue () = 1.0f;
#else
						if (vertColorP->index != gameStates.render.nFrameFlipFlop + 1) {
							if (nLights + lightManager.VariableVertLights (nVertex) == 0) 
#pragma omp critical
								{
								vertColorP->index = gameStates.render.nFrameFlipFlop + 1;
								*vertColorP = c + gameData.render.color.ambient [nVertex];
								}
							else {
								G3VertexColor (nSegment, nSide, nVertex, FACES.normals + nIndex, FACES.vertices + nIndex, NULL, &c, 1, 0, nThread);
								lightManager.Index (0, nThread) = lightManager.Index (1, nThread);
								lightManager.ResetNearestToVertex (nVertex, nThread);
								}
#	if DBG
							if (nVertex == nDbgVertex)
								nVertex = nVertex;
#	endif
							}
#endif
						*colorP = *vertColorP;
						}
					if (nColor) 
						*colorP *= faceColor [nColor];
					colorP->Alpha () = fAlpha;
					}
				}
			}
		lightManager.Material ().bValid = 0;
		}
	}
#	if GPGPU_VERTEX_LIGHTING
if (ogl.m_states.bVertexLighting)
	gpgpuLighting.Compute (-1, 2, NULL);
#endif
PROF_END(ptLighting)
ogl.SetTransform (0);
}

//------------------------------------------------------------------------------

void ComputeStaticFaceLight (int nStart, int nEnd, int nThread)
{
	CSegment*		segP;
	tSegFaces*		segFaceP;
	CSegFace*		faceP;
	CFloatVector*	colorP;
	CFaceColor		c, faceColor [3] = {{{0,0,0,1},1},{{0,0,0,0},1},{{0,0,0,1},1}};
#if 0
	ubyte			nThreadFlags [3] = {1 << nThread, 1 << !nThread, ~(1 << nThread)};
#endif
	short			nVertex, nSegment, nSide;
	fix			xLight;
	float			fAlpha;
	tUVL			*uvlP;
	int			h, i, j, uvi, nColor;

	static		CFaceColor brightColor = {{1,1,1,1},1};

ogl.SetTransform (1);
gameStates.render.nState = 0;
for (i = nStart; i < nEnd; i++) {
	if (0 > (nSegment = gameData.render.mine.segRenderList [0][i]))
		continue;
	segP = SEGMENTS + nSegment;
	segFaceP = SEGFACES + nSegment;
	if (!(/*gameStates.app.bMultiThreaded ||*/ SegmentIsVisible (segP))) {
		gameData.render.mine.segRenderList [0][i] = -gameData.render.mine.segRenderList [0][i] - 1;
		continue;
		}
	for (j = segFaceP->nFaces, faceP = segFaceP->faceP; j; j--, faceP++) {
		nSide = faceP->m_info.nSide;
#if DBG
		if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
			nSegment = nSegment;
#endif
		if (0 > (nColor = SetupFace (nSegment, nSide, segP, faceP, faceColor, &fAlpha))) {
			faceP->m_info.bVisible = 0;
			continue;
			}
		if (!AddFaceListItem (faceP, nThread))
			continue;
		faceP->m_info.color = faceColor [nColor].color;
		colorP = FACES.color + faceP->m_info.nIndex;
		uvlP = segP->m_sides [nSide].m_uvls;
		for (h = 0, uvi = 0 /*(segP->m_sides [nSide].m_nType == SIDE_IS_TRI_13)*/; h < 4; h++, colorP++, uvi++) {
			if (gameStates.render.bFullBright)
				*colorP = nColor ? faceColor [nColor] : brightColor;
			else {
				c = faceColor [nColor];
				nVertex = faceP->m_info.index [h];
#if DBG
				if (nVertex == nDbgVertex)
					nDbgVertex = nDbgVertex;
#endif
				SetVertexColor (nVertex, &c);
				xLight = SetVertexLight (nSegment, nSide, nVertex, &c, uvlP [uvi % 4].l);
				AdjustVertexColor (NULL, &c, xLight);
				}
			*colorP = c;
			colorP->Alpha () = fAlpha;
			}
		}
	}
ogl.SetTransform (0);
}

//------------------------------------------------------------------------------

int CountRenderFaces (void)
{
	CSegment*	segP;
	short			nSegment;
	int			h, i, j, nFaces, nSegments;

ogl.m_states.bUseTransform = 1; // prevent vertex transformation from setting FVERTICES!
for (i = nSegments = nFaces = 0; i < gameData.render.mine.nRenderSegs [0]; i++) {
	segP = SEGMENTS + gameData.render.mine.segRenderList [0][i];
	if (SegmentIsVisible (segP)) {
		nSegments++;
		nFaces += SEGFACES [i].nFaces;
		}
	else
		gameData.render.mine.segRenderList [0][i] = -gameData.render.mine.segRenderList [0][i] - 1;
	}
tiRender.nMiddle = 0;
if (nFaces) {
	for (h = nFaces / 2, i = j = 0; i < gameData.render.mine.nRenderSegs [0]; i++) {
		if (0 > (nSegment = gameData.render.mine.segRenderList [0][i]))
			continue;
		j += SEGFACES [nSegment].nFaces;
		if (j > h) {
			tiRender.nMiddle = i;
			break;
			}
		}
	}
ogl.m_states.bUseTransform = 0;
return nSegments;
}

//------------------------------------------------------------------------------
// eof

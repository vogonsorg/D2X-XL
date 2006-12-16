/* $Id: interp.c, v 1.14 2003/03/19 19:21:34 btb Exp $ */
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

#ifdef RCS
static char rcsid [] = "$Id: interp.c, v 1.14 2003/03/19 19:21:34 btb Exp $";
#endif

#include <math.h>
#include <stdlib.h>
#include "error.h"

#include "inferno.h"
#include "interp.h"
#include "globvars.h"
#include "gr.h"
#include "byteswap.h"
#include "u_mem.h"
#include "console.h"
#include "ogl_init.h"
#include "network.h"
#include "render.h"
#include "gameseg.h"

//------------------------------------------------------------------------------

#define SHADOW_TEST				0
#define NORM_INF					1
#define INFINITY					fInfinity [gameOpts->render.shadows.nReach]

float fInfinity [3] = {100.0f, 200.0f, 400.0f};	//5, 10, 20 standard cubes
float fInf;

extern tRgbaColorf shadowColor [2], modelColor [2];
extern vmsVector viewerEye;

static tOOF_vector vLightPos, vViewerPos, vCenter;

#define OP_EOF          0   //eof
#define OP_DEFPOINTS    1   //defpoints
#define OP_FLATPOLY     2   //flat-shaded polygon
#define OP_TMAPPOLY     3   //texture-mapped polygon
#define OP_SORTNORM     4   //sort by normal
#define OP_RODBM        5   //rod bitmap
#define OP_SUBCALL      6   //call a subobject
#define OP_DEFP_START   7   //defpoints with start
#define OP_GLOW         8   //glow value for next poly

//#define N_OPCODES (sizeof (opcode_table) / sizeof (*opcode_table))

#define MAX_POINTS_PER_POLY 25

short nHighestTexture;
int g3d_interp_outline;

g3sPoint	*modelPointList = NULL;

#define MAX_INTERP_COLORS 100

//this is a table of mappings from RGB15 to palette colors
struct {short pal_entry, rgb15;} interpColorTable [MAX_INTERP_COLORS];

#if DBG_SHADOWS
extern int bShadowTest;
extern int bFrontCap;
extern int bRearCap;
extern int bShadowVolume;
extern int bFrontFaces;
extern int bBackFaces;
extern int bSWCulling;
#endif
#if SHADOWS
extern int bZPass;
#endif
static int bTriangularize = 0;
static int bIntrinsicFacing = 0;
static int bFlatPolys = 1;
static int bTexPolys = 1;

vmsAngVec zeroAngles = {0, 0, 0};

g3sPoint *pointList [MAX_POINTS_PER_POLY];

static short nGlow = -1;

//------------------------------------------------------------------------------

void CHECK ()
{
	int po = 0;
if (gameData.models.pofData [0][1][108].subObjs.pSubObjs &&
	 abs (gameData.models.pofData [0][1][108].subObjs.pSubObjs [0].nParent) > 1)
	po = po;
if (abs (gameData.models.pofData [0][1][108].subObjs.nSubObjs) > 10)
	po = po;
}
//------------------------------------------------------------------------------

inline int G3CheckPointFacing (tOOF_vector *pv, tOOF_vector *pNorm, tOOF_vector *pDir)
{
	tOOF_vector	h;

return OOF_VecMul (OOF_VecSub (&h, pDir, pv), pNorm) > 0;
}

//------------------------------------------------------------------------------

inline int G3CheckLightFacing (tOOF_vector *pv, tOOF_vector *pNorm)
{
return G3CheckPointFacing (pv, pNorm, &vLightPos);
}

//------------------------------------------------------------------------------

inline int G3CheckViewerFacing (tOOF_vector *pv, tOOF_vector *pNorm)
{
return OOF_VecMul (pv, pNorm) >= 0;
}

//------------------------------------------------------------------------------

inline tOOF_vector *G3RotateFaceNormal (tPOF_face *pf)
{
return (tOOF_vector *) OOF_VecVms2Oof (&pf->vNormf, G3RotatePoint (&pf->vRotNorm, &pf->vNorm, 0));
}
					
//------------------------------------------------------------------------------

inline tOOF_vector *G3TransformFaceCenter (tPOF_face *pf)
{
	vmsVector	v;

return (tOOF_vector *) OOF_VecVms2Oof (&pf->vCenterf, G3TransformPoint (&v, &pf->vCenter, 0));
}
					
//------------------------------------------------------------------------------

inline int G3FaceIsLit (tPOFObject *po, tPOF_face *pf)
{
return pf->bFacingLight = G3CheckLightFacing (&pf->vCenterf, &pf->vNormf);
}

//------------------------------------------------------------------------------

inline int G3FaceIsFront (tPOFObject *po, tPOF_face *pf)
{
return pf->bFrontFace = G3CheckViewerFacing (&pf->vCenterf, &pf->vNormf);
}

//------------------------------------------------------------------------------

int G3GetFaceWinding (tOOF_vector *v0, tOOF_vector *v1, tOOF_vector *v2)
{
return ((v1->x - v0->x) * (v2->y - v1->y) < 0) ? GL_CW : GL_CCW;
}

//------------------------------------------------------------------------------
//gives the interpreter an array of points to use
void G3SetModelPoints (g3sPoint *pointlist)
{
modelPointList = pointlist;
}

#define WORDVAL(p)	(*((short *) (p)))
#define WORDPTR(p)	((short *) (p))
#define FIXPTR(p)		((fix *) (p))
#define VECPTR(p)		((vmsVector *) (p))

//------------------------------------------------------------------------------

void RotatePointList (g3sPoint *dest, vmsVector *src, g3sNormal *norms, int n, int o)
{
	fVector	*pfv = (fVector *) (gameData.models.fPolyModelVerts + o);

dest += o;
if (norms)
	norms += o;
while (n--) {
	if (gameStates.ogl.bUseTransform) {
		pfv->p.x = (float) src->x / 65536.0f;
		pfv->p.y = (float) src->y / 65536.0f;
		pfv->p.z = (float) src->z / 65536.0f;
		dest->p3_index = o++;
		pfv++;
		}
	else
		dest->p3_index = -1;
#if 1
	if (norms) {
		if (norms->nFaces > 1) {
			norms->vNormal.p.x /= norms->nFaces;
			norms->vNormal.p.y /= norms->nFaces;
			norms->vNormal.p.z /= norms->nFaces;
			norms->nFaces = 1;
			VmVecNormalizef (&norms->vNormal, &norms->vNormal);
			}
		dest->p3_normal = *norms++;
		}
	else
#endif
		dest->p3_normal.nFaces = 0;
	G3TransformAndEncodePoint (dest++, src++);
	}
}

//------------------------------------------------------------------------------

inline void RotatePointListToVec (vmsVector *dest, vmsVector *src, int n)
{
while (n--)
	G3TransformPoint (dest++, src++, 0);
}

//------------------------------------------------------------------------------

#if 1 //defined (WORDS_BIGENDIAN) || defined (__BIG_ENDIAN__)

inline void ShortSwap (short *s)
{
*s = SWAPSHORT (*s);
}

//------------------------------------------------------------------------------

inline void FixSwap (fix *f)
{
*f = (fix)SWAPINT ((int)*f);
}

//------------------------------------------------------------------------------

inline void VmsVectorSwap (vmsVector *v)
{
FixSwap (FIXPTR (&v->x));
FixSwap (FIXPTR (&v->y));
FixSwap (FIXPTR (&v->z));
}

//------------------------------------------------------------------------------

inline void FixAngSwap (fixang *f)
{
*f = (fixang) SWAPSHORT ((short)*f);
}

//------------------------------------------------------------------------------

inline void VmsAngVecSwap (vmsAngVec *v)
{
FixAngSwap (&v->p);
FixAngSwap (&v->b);
FixAngSwap (&v->h);
}

//------------------------------------------------------------------------------

void G3SwapPolyModelData (ubyte *data)
{
	int i;
	short n;
	uvl *uvl_val;
	ubyte *p = data;

for (;;) {
	ShortSwap (WORDPTR (p));
	switch (WORDVAL (p)) {
		case OP_EOF:
			return;

		case OP_DEFPOINTS:
			ShortSwap (WORDPTR (p + 2));
			n = WORDVAL (p+2);
			for (i = 0; i < n; i++)
				VmsVectorSwap (VECPTR ((p + 4) + (i * sizeof (vmsVector))));
			p += n*sizeof (struct vmsVector) + 4;
			break;

		case OP_DEFP_START:
			ShortSwap (WORDPTR (p + 2));
			ShortSwap (WORDPTR (p + 4));
			n = WORDVAL (p+2);
			for (i = 0; i < n; i++)
				VmsVectorSwap (VECPTR ((p + 8) + (i * sizeof (vmsVector))));
			p += n*sizeof (struct vmsVector) + 8;
			break;

		case OP_FLATPOLY:
			ShortSwap (WORDPTR (p+2));
			n = WORDVAL (p+2);
			VmsVectorSwap (VECPTR (p + 4));
			VmsVectorSwap (VECPTR (p + 16));
			ShortSwap (WORDPTR (p+28));
			for (i=0; i < n; i++)
				ShortSwap (WORDPTR (p + 30 + (i * 2)));
			p += 30 + ((n&~1)+1)*2;
			break;

		case OP_TMAPPOLY:
			ShortSwap (WORDPTR (p+2));
			n = WORDVAL (p+2);
			VmsVectorSwap (VECPTR (p + 4));
			VmsVectorSwap (VECPTR (p + 16));
			for (i = 0; i < n; i++) {
				uvl_val = (uvl *) ((p+30+ ((n&~1)+1)*2) + (i * sizeof (uvl)));
				FixSwap (&uvl_val->u);
				FixSwap (&uvl_val->v);
			}
			ShortSwap (WORDPTR (p+28));
			for (i=0;i<n;i++)
				ShortSwap (WORDPTR (p + 30 + (i * 2)));
			p += 30 + ((n&~1)+1)*2 + n*12;
			break;

		case OP_SORTNORM:
			VmsVectorSwap (VECPTR (p + 4));
			VmsVectorSwap (VECPTR (p + 16));
			ShortSwap (WORDPTR (p + 28));
			ShortSwap (WORDPTR (p + 30));
			G3SwapPolyModelData (p + WORDVAL (p+28));
			G3SwapPolyModelData (p + WORDVAL (p+30));
			p += 32;
			break;

		case OP_RODBM:
			VmsVectorSwap (VECPTR (p + 20));
			VmsVectorSwap (VECPTR (p + 4));
			ShortSwap (WORDPTR (p+2));
			FixSwap (FIXPTR (p + 16));
			FixSwap (FIXPTR (p + 32));
			p+=36;
			break;

		case OP_SUBCALL:
			ShortSwap (WORDPTR (p+2));
			VmsVectorSwap (VECPTR (p+4));
			ShortSwap (WORDPTR (p+16));
			G3SwapPolyModelData (p + WORDVAL (p+16));
			p += 20;
			break;

		case OP_GLOW:
			ShortSwap (WORDPTR (p + 2));
			p += 4;
			break;

		default:
			Error ("invalid polygon model\n"); //Int3 ();
		}
	}
}

#endif

//------------------------------------------------------------------------------

#ifdef WORDS_NEED_ALIGNMENT
void G3AddPolyModelChunk (ubyte *old_base, ubyte *new_base, int offset, 
	       chunk *chunk_list, int *no_chunks)
{
	Assert (*no_chunks + 1 < MAX_CHUNKS); //increase MAX_CHUNKS if you get this
	chunk_list [*no_chunks].old_base = old_base;
	chunk_list [*no_chunks].new_base = new_base;
	chunk_list [*no_chunks].offset = offset;
	chunk_list [*no_chunks].correction = 0;
	 (*no_chunks)++;
}

//------------------------------------------------------------------------------
/*
 * finds what chunks the data points to, adds them to the chunk_list, 
 * and returns the length of the current chunk
 */
int G3GetPolyModelChunks (ubyte *data, ubyte *new_data, chunk *list, int *no)
{
	short n;
	ubyte *p = data;

for (;;) {
	switch (INTEL_SHORT (WORDVAL (p))) {
		case OP_EOF:
			return p + 2 - data;
		case OP_DEFPOINTS:
			n = INTEL_SHORT (WORDVAL (p+2));
			p += n*sizeof (struct vmsVector) + 4;
			break;
		case OP_DEFP_START:
			n = INTEL_SHORT (WORDVAL (p+2));
			p += n*sizeof (struct vmsVector) + 8;
			break;
		case OP_FLATPOLY:
			n = INTEL_SHORT (WORDVAL (p+2));
			p += 30 + ((n&~1)+1)*2;
			break;
		case OP_TMAPPOLY:
			n = INTEL_SHORT (WORDVAL (p+2));
			p += 30 + ((n&~1)+1)*2 + n*12;
			break;
		case OP_SORTNORM:
			G3AddPolyModelChunk (p, p - data + new_data, 28, list, no);
			G3AddPolyModelChunk (p, p - data + new_data, 30, list, no);
			p += 32;
			break;
		case OP_RODBM:
			p+=36;
			break;
		case OP_SUBCALL:
			G3AddPolyModelChunk (p, p - data + new_data, 16, list, no);
			p+=20;
			break;
		case OP_GLOW:
			p += 4;
			break;
		default:
			Error ("invalid polygon model\n");
		}
	}
return p + 2 - data;
}
#endif //def WORDS_NEED_ALIGNMENT

//------------------------------------------------------------------------------

void G3PolyModelVerify (ubyte *data)
{
	short n;
	ubyte *p = data;

for (;;) {
	switch (WORDVAL (p)) {
		case OP_EOF:
			return;
		case OP_DEFPOINTS:
			n = (WORDVAL (p+2));
			p += n*sizeof (struct vmsVector) + 4;
			break;
		case OP_DEFP_START:
			n = (WORDVAL (p+2));
			p += n*sizeof (struct vmsVector) + 8;
			break;
		case OP_FLATPOLY:
			n = (WORDVAL (p+2));
			p += 30 + ((n&~1)+1)*2;
			break;
		case OP_TMAPPOLY:
			n = (WORDVAL (p+2));
			p += 30 + ((n&~1)+1)*2 + n*12;
			break;
		case OP_SORTNORM:
			G3PolyModelVerify (p + WORDVAL (p + 28));
			G3PolyModelVerify (p + WORDVAL (p + 30));
			p += 32;
			break;
		case OP_RODBM:
			p+=36;
			break;
		case OP_SUBCALL:
			G3PolyModelVerify (p + WORDVAL (p + 16));
			p+=20;
			break;
		case OP_GLOW:
			p += 4;
			break;
		default:
			Error ("invalid polygon model\n");
		}
	}
}

//------------------------------------------------------------------------------

inline int G3CheckAndSwap (void *modelP)
{
	short	h = WORDVAL (modelP);

if ((h >= 0) && (h <= OP_GLOW))
	return 1;
ShortSwap (&h);
if ((h < 0) || (h > OP_GLOW))
	return 0;
G3SwapPolyModelData (modelP);
return 1;
}

//------------------------------------------------------------------------------
//calls the tObject interpreter to render an tObject.  The tObject renderer
//is really a seperate pipeline. returns true if drew
fix G3PolyModelSize (void *modelP)
{
	ubyte			*p = modelP;
	int			i, n, nv;
	vmsVector	*v, 
					pMin = {0x7fffffff, 0x7fffffff, 0x7fffffff}, 
					pMax = {-0x7fffffff, -0x7fffffff, -0x7fffffff};

G3CheckAndSwap (modelP);
for (;;)
	switch (WORDVAL (p)) {
		case OP_EOF:
			goto done;
		case OP_DEFPOINTS:
			n = WORDVAL (p + 2);
			v = VECPTR (p + 4);
			for (i = n; i; i--, v++) {
				if (pMin.x > v->x)
					pMin.x = v->x;
				else if (pMax.x < v->x)
					pMax.x = v->x;
				if (pMin.y > v->y)
					pMin.y = v->y;
				else if (pMax.y < v->y)
					pMax.y = v->y;
				if (pMin.z > v->z)
					pMin.z = v->z;
				else if (pMax.z < v->z)
					pMax.z = v->z;
				}
			p += n * sizeof (struct vmsVector) + 4;
			break;

		case OP_DEFP_START: 
			n = WORDVAL (p + 2);
			v = VECPTR (p + 8);
			for (i = n; i; i--, v++) {
				if (pMin.x > v->x)
					pMin.x = v->x;
				else if (pMax.x < v->x)
					pMax.x = v->x;
				if (pMin.y > v->y)
					pMin.y = v->y;
				else if (pMax.y < v->y)
					pMax.y = v->y;
				if (pMin.z > v->z)
					pMin.z = v->z;
				else if (pMax.z < v->z)
					pMax.z = v->z;
				}
			p += n * sizeof (struct vmsVector) + 8;
			break;

		case OP_FLATPOLY:
			nv = WORDVAL (p + 2);
			p += 30 + (nv | 1) * 2;
			break;

		case OP_TMAPPOLY:
			nv = WORDVAL (p + 2);
			p += 30 + (nv | 1) * 2 + nv * 12;
			break;

		case OP_SORTNORM:
			p += 32;
			break;


		case OP_RODBM:
			p += 36;
			break;

		case OP_SUBCALL:
			p += 20;
			break;


		case OP_GLOW:
			p += 4;
			break;

		default:
			Error ("invalid polygon model\n");
		}
done:
{
	double	dx = (pMax.x - pMin.x) / 2;
	double	dy = (pMax.y - pMin.y) / 2;
	double	dz = (pMax.z - pMin.z) / 2;

return (fix) sqrt (dx * dx + dy * dy + dz + dz);
}
//return VmVecDist (&pMin, &pMax) / 2;
}

//------------------------------------------------------------------------------

bool G3CountPolyModelItems (void *modelP, short *pnSubObjs, short *pnVerts, short *pnFaces, short *pnFaceVerts, short *pnAdjFaces)
{
	ubyte *p = modelP;

G3CheckAndSwap (modelP);
for (;;)
	switch (WORDVAL (p)) {
		case OP_EOF:
			return 1;

		case OP_DEFPOINTS: {
			int n = WORDVAL (p+2);
			(*pnVerts) += n;
			p += n * sizeof (struct vmsVector) + 4;
			break;
			}

		case OP_DEFP_START: {
			int n = WORDVAL (p+2);
			int s = WORDVAL (p+4);
			p += n * sizeof (struct vmsVector) + 8;
			(*pnVerts) += n;
			break;
			}

		case OP_FLATPOLY: {
			int nv = WORDVAL (p+2);
			*pnAdjFaces += nv;
			if (bTriangularize) {
				(*pnFaces) += nv - 2;
				(*pnFaceVerts) += (nv - 2) * 3;
				}
			else { //if (nv < 5) {
				(*pnFaces)++;
				(*pnFaceVerts) += nv;
				}
#if 0
			else {
				(*pnFaces) += 2;
				(*pnFaceVerts) += ((nv + 1) / 2) * 2;
				}
#endif
			p += 30 + ((nv & ~ 1) + 1) * 2;
			break;
			}

		case OP_TMAPPOLY: {
			int nv = WORDVAL (p + 2);
			*pnAdjFaces += nv;
			if (bTriangularize) {
				(*pnFaces) += nv - 2;
				(*pnFaceVerts) += (nv - 2) * 3;
				}
			else { //if (nv < 5) {
				(*pnFaces)++;
				(*pnFaceVerts) += nv;
				}
#if 0
			else {
				(*pnFaces) += 2;
				(*pnFaceVerts) += ((nv + 1) / 2) * 2;
				}
#endif
			p += 30 + ((nv&~1)+1)*2 + nv*12;
			break;
			}

		case OP_SORTNORM:
//			(*pnSubObjs)++;
			G3CountPolyModelItems (p+WORDVAL (p+28), pnSubObjs, pnVerts, pnFaces, pnFaceVerts, pnAdjFaces);
//			(*pnSubObjs)++;
			G3CountPolyModelItems (p+WORDVAL (p+30), pnSubObjs, pnVerts, pnFaces, pnFaceVerts, pnAdjFaces);
			p += 32;
			break;


		case OP_RODBM: {
			p+=36;
			break;
			}

		case OP_SUBCALL: {
			(*pnSubObjs)++;
			G3CountPolyModelItems (p+WORDVAL (p+16), pnSubObjs, pnVerts, pnFaces, pnFaceVerts, pnAdjFaces);
			p += 20;
			break;
			}

		case OP_GLOW:
			p += 4;
			break;

		default:
			Error ("invalid polygon model\n");
	}
return 1;
}

//------------------------------------------------------------------------------

short G3FindPolyModelVert (vmsVector *pVerts, vmsVector *pv, int nVerts)
{
	int	i;

for (i = 0; i < nVerts; i++, pVerts++)
	if ((pVerts->x == pv->x) && (pVerts->y == pv->y) && (pVerts->z == pv->z))
		return i;
return nVerts;
}

//------------------------------------------------------------------------------

void G3CalcFaceNormal (tPOFObject *po, tPOF_face *pf)
{
if (bTriangularize) {
	vmsVector	*pv = po->pvVerts;
	short			*pfv = pf->pVerts;

	VmVecNormal (&pf->vNorm, pv + pfv [0], pv + pfv [1], pv + pfv [2]);
	}
else
	OOF_VecVms2Oof (&pf->vNormf, &pf->vNorm);
}

//------------------------------------------------------------------------------

tOOF_vector *G3CalcFaceCenterf (tPOFObject *po, tPOF_face *pf)
{
	tOOF_vector	*pv = po->pvVertsf;
	short			*pfv = pf->pVerts;
	int			i;
	static tOOF_vector	c;

c.x = c.y = c.z = 0.0f;
for (i = pf->nVerts; i; i--, pfv++)
	OOF_VecInc (&c, pv + *pfv);
OOF_VecScale (&c, 1.0f / pf->nVerts);
return &c;
}

//------------------------------------------------------------------------------

vmsVector *G3CalcFaceCenter (tPOFObject *po, tPOF_face *pf)
{
	vmsVector	*pv = po->pvVerts;
	short			*pfv = pf->pVerts;
	tOOF_vector	cf;
	static vmsVector c;

cf = *G3CalcFaceCenterf (po, pf);
c.x = (fix) (cf.x * F1_0);
c.y = (fix) (cf.y * F1_0);
c.z = (fix) (cf.z * F1_0);
return &c;
}

//------------------------------------------------------------------------------

inline void G3AddPolyModelTri (tPOFObject *po, tPOF_face *pf, short v0, short v1, short v2)
{
	vmsVector	*pv = po->pvVerts;
	short			*pfv = po->pFaceVerts + po->iFaceVert;

pf->pVerts = pfv;
pf->nVerts = 3;
pfv [0] = v0;
pfv [1] = v1;
pfv [2] = v2;
G3CalcFaceNormal (po, pf);
}

//------------------------------------------------------------------------------

#if 0

short G3FindPolyModelFace (tPOFObject *po, tPOF_face *pf)
{
	int			h, i, j, k, l;
	tPOF_face	*pfh;

for (h = pf->nVerts, i = po->iFace, pfh = po->faces.pFaces; i; i--, pfh++)
	if (pfh->nVerts == h) {
		for (k = h, j = 0; j < h; j++)
			for (l = 0; l < h; l++)
				if (pf->pVerts [j] == pfh->pVerts [l]) {
					k--;
					break;
					}
		if (!k)
			return i;
		}
return -1;
}

#else
//------------------------------------------------------------------------------

int G3FindPolyModelFace (tPOFObject *po, short *p, int nv)
{
	tPOF_face	*pf;
	int			h, i, j, k;
	short			*pfv;

for (i = po->iFace, pf = po->faces.pFaces; i; i--, pf++) {
	if (pf->nVerts != nv)
		continue;
	pfv = pf->pVerts;
	for (j = 0, h = *p; j < nv; j++) {
		if (h == pfv [j]) {
			h = j;
			for (k = 0; k < nv; k++, j = (j + 1) % nv) {
				if (p [k] != pfv [j]) {
					j = h;
					for (k = 0; k < nv; k++) {
						if (p [k] != pfv [j])
							break;
						if (!j)
							j = nv;
						j--;
						}
					break;
					}
				}
			if (k == nv)
				return 1;
			break;
			}
		}
	}
return 0;
}
#endif
//------------------------------------------------------------------------------

tPOF_face *G3AddPolyModelFace (tPOFObject *po, tPOFSubObject *pso, tPOF_face *pf, 
										 vmsVector *pn, ubyte *p, int bShadowData)
{
	short			nv = WORDVAL (p+2);
	vmsVector	*pv = po->pvVerts, v;
	short			*pfv;
	short			i, v0;

//if (G3FindPolyModelFace (po, WORDPTR (p+30), nv))
//	return pf;
if (bShadowData) {
	pf->vNorm = *pn;
	if (bTriangularize) {
		pfv = WORDPTR (p+30);
		v0 = *pfv;
		if (nv == 3) {
			G3AddPolyModelTri (po, pf, pfv [0], pfv [1], pfv [2]);
			po->iFaceVert += 3;
			pf->bGlow = (nGlow >= 0);
			pf++;
			po->iFace++;
			pso->faces.nFaces++;
			}
		else if (nv == 4) {
			VmVecSub (&v, pv + pfv [3], pv + pfv [1]);
			if (VmVecDot (pn, &v) < 0) {
				G3AddPolyModelTri (po, pf, pfv [0], pfv [1], pfv [3]);
				po->iFaceVert += 3;
				pf->bGlow = (nGlow >= 0);
				pf++;
				po->iFace++;
				pso->faces.nFaces++;
				G3AddPolyModelTri (po, pf, pfv [1], pfv [2], pfv [3]);
				po->iFaceVert += 3;
				pf->bGlow = (nGlow >= 0);
				pf++;
				po->iFace++;
				pso->faces.nFaces++;
				}
			else {
				G3AddPolyModelTri (po, pf, pfv [0], pfv [1], pfv [2]);
				po->iFaceVert += 3;
				pf->bGlow = (nGlow >= 0);
				pf++;
				po->iFace++;
				pso->faces.nFaces++;
				G3AddPolyModelTri (po, pf, pfv [0], pfv [2], pfv [3]);
				po->iFaceVert += 3;
				pf->bGlow = (nGlow >= 0);
				pf++;
				po->iFace++;
				pso->faces.nFaces++;
				}
			}
		else {
			for (i = 1; i < nv - 1; i++) {
				G3AddPolyModelTri (po, pf, v0, pfv [i], pfv [i + 1]);
				po->iFaceVert += 3;
				pf->bGlow = (nGlow >= 0);
				pf++;
				po->iFace++;
				pso->faces.nFaces++;
				}
			}
		}
	else { //if (nv < 5) {
		pfv = pf->pVerts = po->pFaceVerts + po->iFaceVert;
		pf->nVerts = nv;
		memcpy (pfv, WORDPTR (p+30), nv * sizeof (short));
		pf->bGlow = (nGlow >= 0);
		G3CalcFaceNormal (po, pf);
#if 0
		if (memcmp (&pf->vCenter, po->pvVerts + *pv, sizeof (vmsVector))) //make sure we have a vertex from the face
			pf->vCenter = po->pvVerts [*pv];
		VmVecNormal (&n, po->pvVerts + pv [0], po->pvVerts + pv [1], po->pvVerts + pv [2]); //check the precomputed normal
		if (memcmp (&pf->vNorm, &n, sizeof (vmsVector)))
			pf->vNorm = n;
#endif
#if 0
		if (G3FindPolyModelFace (po, pf) < 0) //check for duplicate faces
#endif
			{
			po->iFaceVert += nv;
			pf++;
			po->iFace++;
			pso->faces.nFaces++;
			}
		}
#if 0
	else {
		short h = (nv + 1) / 2;
		pfv = pf->pVerts = po->pFaceVerts + po->iFaceVert;
		pf->nVerts = h;
		memcpy (pfv, WORDPTR (p+30), h * sizeof (short));
		pf->bGlow = (nGlow >= 0);
		G3CalcFaceNormal (po, pf);
		po->iFaceVert += h;
		pf->bTest = 1;
		pf++;
		po->iFace++;
		pso->faces.nFaces++;
		pfv = pf->pVerts = po->pFaceVerts + po->iFaceVert;
		pf->nVerts = h;
		memcpy (pfv, WORDPTR (p + 30) + nv / 2, h * sizeof (short));
		pf->bGlow = (nGlow >= 0);
		G3CalcFaceNormal (po, pf);
		pf->bTest = 1;
		po->iFaceVert += h;
		pf++;
		po->iFace++;
		pso->faces.nFaces++;
		}
#endif
	}
else {
	fVector		nf;
	g3sNormal	*pvn;

	pfv = WORDPTR (p+30);
	VmsVecToFloat (&nf, pn);
	for (i = 0; i < nv; i++) {
		pvn = po->pVertNorms + pfv [i];
		VmVecIncf (&pvn->vNormal, &nf);
		pvn->nFaces++;
		}
	}
return pf;
}

//------------------------------------------------------------------------------

void G3RotatePolyModelNormals (tPOFObject *po, tPOFSubObject *pso)
{
	tPOF_face	*pf;
	int			i;

for (i = pso->faces.nFaces, pf = pso->faces.pFaces; i; i--, pf++) {
	G3RotateFaceNormal (pf);
	G3TransformFaceCenter (pf);
	}
}

//------------------------------------------------------------------------------

tOOF_vector *G3PolyModelVerts2Float (tPOFObject *po)
{
	int			i;
	vmsVector	*pv;
	tOOF_vector	*pvf;

for (i = po->nVerts, pv = po->pvVerts, pvf = po->pvVertsf; i; i--, pv++, pvf++) {
	pvf->x = f2fl (pv->x);
	pvf->y = f2fl (pv->y);
	pvf->z = f2fl (pv->z);
	}
return po->pvVertsf;
}

//------------------------------------------------------------------------------

#define MAXGAP	0.01f;

int G3FindPolyModelEdge (tPOFSubObject *pso, tPOF_face *pf0, int v0, int v1)
{
	int			h, i, j, n;
	tPOF_face	*pf1;
	short			*pfv;

for (i = pso->faces.nFaces, pf1 = pso->faces.pFaces; i; i--, pf1++) {
	if (pf1 != pf0) {
		for (j = 0, n = pf1->nVerts, pfv = pf1->pVerts; j < n; j++) {
			h = (j + 1) % n;
			if (((pfv [j] == v0) && (pfv [h] == v1)) || ((pfv [j] == v1) && (pfv [h] == v0)))
				return (int) (pf1 - pso->faces.pFaces);
			}
		}
	}
return -1;
}

//------------------------------------------------------------------------------

int G3GetAdjFaces (tPOFObject *po)
{
	short				h, i, j, k, n;
	tPOFSubObject	*pso = po->subObjs.pSubObjs;
	tPOF_face		*pf;
	short				*pfv;

po->nAdjFaces = 0;
G3PolyModelVerts2Float (po);
for (i = po->subObjs.nSubObjs; i; i--, pso++) {
	pso->pAdjFaces = po->pAdjFaces + po->nAdjFaces;
	for (j = pso->faces.nFaces, pf = pso->faces.pFaces; j; j--, pf++) {
		pf->nAdjFaces = po->nAdjFaces;
		for (h = 0, n = pf->nVerts, pfv = pf->pVerts; h < n; h++) {
			k = G3FindPolyModelEdge (pso, pf, pfv [h], pfv [(h + 1) % n]);
			if (k >= 0)
				po->pAdjFaces [po->nAdjFaces++] = k;
			}
		}
	}
return 1;
}

//------------------------------------------------------------------------------

int G3CalcModelFacing (tPOFObject *po, tPOFSubObject *pso)
{
	short				i;
	tPOF_face		*pf;

G3RotatePolyModelNormals (po, pso);
for (i = pso->faces.nFaces, pf = pso->faces.pFaces; i; i--, pf++) {
	G3FaceIsLit (po, pf);
#if 0
	G3FaceIsFront (po, pf);
#endif
	}
return pso->faces.nFaces;
}

//------------------------------------------------------------------------------

int G3GetLitFaces (tPOFObject *po, tPOFSubObject *pso)
{
	short				i;
	tPOF_face		*pf;

G3CalcModelFacing (po, pso);
pso->litFaces.nFaces = 0;
pso->litFaces.pFaces = po->litFaces.pFaces + po->litFaces.nFaces;
for (i = pso->faces.nFaces, pf = pso->faces.pFaces; i; i--, pf++) {
	if (pf->bFacingLight) {
		po->litFaces.pFaces [po->litFaces.nFaces++] = pf;
		pso->litFaces.nFaces++;
		}
	}
return 1;
}

//------------------------------------------------------------------------------

void G3GetPolyModelCenters (tPOFObject *po)
{
	short				i, j;
	tPOFSubObject	*pso = po->subObjs.pSubObjs;
	tPOF_face		*pf;

for (i = po->subObjs.nSubObjs; i; i--, pso++)
	for (j = pso->faces.nFaces, pf = pso->faces.pFaces; j; j--, pf++)
		pf->vCenter = *G3CalcFaceCenter (po, pf);
}

//------------------------------------------------------------------------------

bool G3GetPolyModelItems (void *modelP, vmsAngVec *pAnimAngles, tPOFObject *po, 
								  int bInitModel, int bShadowData, int nThis, int nParent)
{
	ubyte				*p = modelP;
	tPOFSubObject	*pso = po->subObjs.pSubObjs + nThis;
	tPOF_face		*pf = po->faces.pFaces + po->iFace;
	int				nChild;

G3CheckAndSwap (modelP);
nGlow = -1;
if (bInitModel && !pso->faces.pFaces) {
	pso->faces.pFaces = pf;
	pso->nParent = nParent;
	}
for (;;)
	switch (WORDVAL (p)) {
		case OP_EOF:
			return 1;
		case OP_DEFPOINTS: {
			int n = WORDVAL (p+2);
			if (bInitModel)
				memcpy (po->pvVerts, VECPTR (p+4), n * sizeof (vmsVector));
			else
				RotatePointListToVec (po->pvVerts, VECPTR (p+4), n);
			//po->nVerts += n;
			p += n * sizeof (struct vmsVector) + 4;
			break;
			}

		case OP_DEFP_START: {
			int n = WORDVAL (p+2);
			int s = WORDVAL (p+4);
			if (n + s > po->nVerts)
				n = n;
			if (bInitModel)
				memcpy (po->pvVerts + s, VECPTR (p+8), n * sizeof (vmsVector));
			else
				RotatePointListToVec (po->pvVerts + s, VECPTR (p+8), n);
			//po->nVerts += n;
			p += n * sizeof (struct vmsVector) + 8;
			break;
			}

		case OP_FLATPOLY: {
			int nv = WORDVAL (p+2);
			if (bInitModel && bFlatPolys) {
				pf = G3AddPolyModelFace (po, pso, pf, VECPTR (p+16), p, bShadowData);
				}
			p += 30 + (nv | 1) * 2;
			break;
			}

		case OP_TMAPPOLY: {
			int nv = WORDVAL (p + 2);
			if (bInitModel && bTexPolys) {
				pf = G3AddPolyModelFace (po, pso, pf, VECPTR (p+16), p, bShadowData);
				}
			p += 30 + (nv | 1) * 2 + nv * 12;
			break;
			}

		case OP_SORTNORM:
			G3GetPolyModelItems (p+WORDVAL (p+28), pAnimAngles, po, bInitModel, bShadowData, nThis, nParent);
			if (bInitModel)
				pf = po->faces.pFaces + po->iFace;
			G3GetPolyModelItems (p+WORDVAL (p+30), pAnimAngles, po, bInitModel, bShadowData, nThis, nParent);
			if (bInitModel)
				pf = po->faces.pFaces + po->iFace;
			p += 32;
			break;


		case OP_RODBM:
			p+=36;
			break;

		case OP_SUBCALL: {
			vmsAngVec *a;

			if (pAnimAngles)
				a = &pAnimAngles [WORDVAL (p+2)];
			else
				a = &zeroAngles;
			nChild = ++po->iSubObj;
			po->subObjs.pSubObjs [nChild].vPos = *VECPTR (p+4);
			po->subObjs.pSubObjs [nChild].vAngles = *a;
			G3StartInstanceAngles (VECPTR (p+4), a);
			G3GetPolyModelItems (p+WORDVAL (p+16), pAnimAngles, po, bInitModel, bShadowData, nChild, nThis);
			G3DoneInstance ();
			if (bInitModel)
				pf = po->faces.pFaces + po->iFace;
			p += 20;
			break;
			}

		case OP_GLOW:
			nGlow = WORDVAL (p+2);
			p += 4;
			break;

		default:
			Error ("invalid polygon model\n");
	}
return 1;
}

//------------------------------------------------------------------------------

#define pof_free(_p)	{if (_p) d_free (_p);}

int G3FreePolyModelItems (tPOFObject *po)
{
pof_free (po->subObjs.pSubObjs);
pof_free (po->pvVerts);
pof_free (po->pvVertsf);
pof_free (po->pVertNorms);
pof_free (po->faces.pFaces);
pof_free (po->litFaces.pFaces);
pof_free (po->pAdjFaces);
pof_free (po->pFaceVerts);
memset (po, 0, sizeof (po));
return 0;
}

//------------------------------------------------------------------------------

int G3AllocPolyModelItems (void *modelP, tPOFObject *po, int bShadowData)
{
	short	nFaceVerts = 0;
	int	h;

//memset (po, 0, sizeof (po));
po->subObjs.nSubObjs = 1;
G3CountPolyModelItems (modelP, &po->subObjs.nSubObjs, &po->nVerts, &po->faces.nFaces, &nFaceVerts, &po->nAdjFaces);
h = po->subObjs.nSubObjs * sizeof (tPOFSubObject);
if (!(po->subObjs.pSubObjs = (tPOFSubObject *) d_malloc (h)))
	return G3FreePolyModelItems (po);
memset (po->subObjs.pSubObjs, 0, h);
if (!(po->pvVerts = (vmsVector *) d_malloc (po->nVerts * sizeof (vmsVector))))
	return G3FreePolyModelItems (po);
if (bShadowData) {
	if (!(po->faces.pFaces = (tPOF_face *) d_malloc (po->faces.nFaces * sizeof (tPOF_face))))
		return G3FreePolyModelItems (po);
	if (!(po->litFaces.pFaces = (tPOF_face **) d_malloc (po->faces.nFaces * sizeof (tPOF_face *))))
		return G3FreePolyModelItems (po);
	if (!(po->pAdjFaces = (short *) d_malloc (po->nAdjFaces * sizeof (short))))
		return G3FreePolyModelItems (po);
	if (!(po->pvVertsf = (tOOF_vector *) d_malloc (po->nVerts * sizeof (tOOF_vector))))
		return G3FreePolyModelItems (po);
	if (!(po->pFaceVerts = (short *) d_malloc (nFaceVerts * sizeof (short))))
		return G3FreePolyModelItems (po);
	}
if (!(po->pVertNorms = (g3sNormal *) d_malloc (po->nVerts * sizeof (g3sNormal))))
	return G3FreePolyModelItems (po);
memset (po->pVertNorms, 0, po->nVerts * sizeof (g3sNormal));
return po->nState = 1;
}

//------------------------------------------------------------------------------

void G3SetCullAndStencil (int bCullFront, int bZPass)
{
glEnable (GL_CULL_FACE);
if (bCullFront) {
	glCullFace (GL_FRONT);
	if (bZPass)
		glStencilOp (GL_KEEP, GL_KEEP, GL_INCR_WRAP);
	else
		glStencilOp (GL_KEEP, GL_INCR_WRAP, GL_KEEP);
	}
else {
	glCullFace (GL_BACK);
	if (bZPass)
		glStencilOp (GL_KEEP, GL_KEEP, GL_DECR_WRAP);
	else
		glStencilOp (GL_KEEP, GL_DECR_WRAP, GL_KEEP);
	}
}

//------------------------------------------------------------------------------

void G3RenderShadowVolumeFace (tOOF_vector *pv)
{
	tOOF_vector	v [4];

memcpy (v, pv, 2 * sizeof (tOOF_vector));
OOF_VecSub (v+3, v, &gameData.render.shadows.vLightPos);
OOF_VecSub (v+2, v+1, &gameData.render.shadows.vLightPos);
#if NORM_INF
OOF_VecScale (v+3, fInf / OOF_VecMag (v+3));
OOF_VecScale (v+2, fInf / OOF_VecMag (v+2));
#else
OOF_VecScale (v+3, fInf);
OOF_VecScale (v+2, fInf);
#endif
OOF_VecInc (v+2, v+1);
OOF_VecInc (v+3, v);
glVertexPointer (3, GL_FLOAT, 0, v);
#if DBG_SHADOWS
if (bShadowTest)
	glDrawArrays (GL_LINES, 0, 4);
else
#endif
glDrawArrays (GL_QUADS, 0, 4);
}

//------------------------------------------------------------------------------

void G3RenderFarShadowCapFace (tOOF_vector *pv, int nv)
{
	tOOF_vector	v0, v1;

#if DBG_SHADOWS
if (bShadowTest == 1)
	glColor4fv ((GLfloat *) shadowColor);
else if (bShadowTest > 1)
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
#	if 0
if (bShadowTest)
	glBegin (GL_LINE_LOOP);
else
#	endif
#endif
glBegin (GL_TRIANGLE_FAN);
for (pv += nv; nv; nv--) {
	v0 = *(--pv);
	OOF_VecSub (&v1, &v0, &vLightPos);
#if NORM_INF
	OOF_VecScale (&v1, fInf / OOF_VecMag (&v1));
#else
	OOF_VecScale (&v1, fInf);
#endif
	OOF_VecInc (&v0, &v1);
	glVertex3fv ((GLfloat *) &v0);
	}	
glEnd ();
}

//------------------------------------------------------------------------------

int G3RenderSubModelShadowVolume (tPOFObject *po, tPOFSubObject *pso, int bCullFront)
{
	tOOF_vector	*pvf, v [4];
	tPOF_face	*pf, **ppf;
	short			*pfv, *paf;
	short			i, j, n;

#if DBG_SHADOWS
if (!bShadowVolume)
	return 1;
if (bCullFront && !bBackFaces)
	return 1;
if (!bCullFront && !bFrontFaces)
	return 1;
if (bShadowTest == 1)
	glColor4fv ((GLfloat *) shadowColor);
else if (bShadowTest > 1)
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
#endif
G3SetCullAndStencil (bCullFront, bZPass);
pvf = po->pvVertsf;
#if DBG_SHADOWS
if (bShadowTest < 2)
#endif
glEnableClientState (GL_VERTEX_ARRAY);
#if DBG_SHADOWS
else if (bShadowTest == 2)
	glLineWidth (3);
else {
	glLineWidth (3);
	glBegin (GL_LINES);
	}
#endif
for (i = pso->litFaces.nFaces, ppf = pso->litFaces.pFaces; i; i--, ppf++) {
	pf = *ppf;
	paf = po->pAdjFaces + pf->nAdjFaces;
	for (j = 0, n = pf->nVerts, pfv = pf->pVerts; j < n; j++) {
		if (!pso->faces.pFaces [*paf++].bFacingLight) {
			v [1] = pvf [pfv [j]];
			v [0] = pvf [pfv [(j + 1) % n]];
#if DBG_SHADOWS
			if (bShadowTest < 3) {
				glColor4fv ((GLfloat *) (shadowColor + bCullFront));
#endif
				OOF_VecSub (v+3, v, &vLightPos);
				OOF_VecSub (v+2, v+1, &vLightPos);
#if NORM_INF
				OOF_VecScale (v+3, fInf / OOF_VecMag (v+3));
				OOF_VecScale (v+2, fInf / OOF_VecMag (v+2));
#else
				OOF_VecScale (v+3, fInf);
				OOF_VecScale (v+2, fInf);
#endif
				OOF_VecInc (v+2, v+1);
				OOF_VecInc (v+3, v);
#if 1//def RELEASE
				glVertexPointer (3, GL_FLOAT, 0, v);
				glDrawArrays (GL_QUADS, 0, 4);
#else
				glVertex3fv ((GLfloat *) v);
				glVertex3fv ((GLfloat *) (v+1));
				glVertex3fv ((GLfloat *) (v+2));
				glVertex3fv ((GLfloat *) (v+3));
#endif
#if DBG_SHADOWS
				}
			else {
				glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
				glVertex3fv ((GLfloat *) v);
				glVertex3fv ((GLfloat *) (v + 1));
				}
#endif
			}
		}
CHECK();
	}
#if DBG_SHADOWS
glLineWidth (1);
if (bShadowTest != 2)
#endif
glDisableClientState (GL_VERTEX_ARRAY);
return 1;
}

//------------------------------------------------------------------------------

static int _I = 3;
static int _O = 13;

int G3RenderSubModelShadowCaps (tPOFObject *po, tPOFSubObject *pso, int bCullFront)
{
	tOOF_vector	*pvf, v0, v1;
	tPOF_face	*pf, **ppf;
	short			*pfv, i, j;

#if DBG_SHADOWS
if (bShadowTest) {
	glColor4fv ((GLfloat *) (modelColor + bCullFront));
	glDepthFunc (GL_LEQUAL);
	}
#endif
G3SetCullAndStencil (bCullFront, bZPass);
pvf = po->pvVertsf;
#if DBG_SHADOWS
if (bRearCap)
#endif
for (i = pso->litFaces.nFaces, ppf = pso->litFaces.pFaces; i; i--, ppf++) {
	pf = *ppf;
	if (!pf->bFacingLight)
		continue;
#if 0//def _DEBUG
	if (pf->bFacingLight && (bShadowTest > 3)) {
		glColor4f (0.20f, 0.8f, 1.0f, 1.0f);
		v1 = v0 = pf->vCenterf;
		glBegin (GL_LINES);
		glVertex3fv ((GLfloat *) &v0);
		OOF_VecInc (&v0, &pf->vNormf);
		glVertex3fv ((GLfloat *) &v0);
		glEnd ();
		glColor4d (0,0,1,1);
		glBegin (GL_LINES);
		glVertex3fv ((GLfloat *) &v1);
		glVertex3fv ((GLfloat *) &vLightPos);
		glEnd ();
		glColor4fv ((GLfloat *) (modelColor + bCullFront));
		}
	if (bShadowTest && (bShadowTest != 2)) {
		glLineWidth (1);
		glBegin (GL_LINE_LOOP);
		}
	else
#endif
		glBegin (GL_TRIANGLE_FAN);
	for (j = pf->nVerts, pfv = pf->pVerts + j; j; j--) {
		v0 = pvf [*--pfv];
#if 1
		OOF_VecSub (&v1, &v0, &vLightPos);
#if DBG_SHADOWS
		if (bShadowTest < 4) 
#endif
			{
#if NORM_INF
#	if DBG_SHADOWS
			if (bShadowTest == 2)
				OOF_VecScale (&v1, 5.0f / OOF_VecMag (&v1));
			else
#	endif
				OOF_VecScale (&v1, fInf / OOF_VecMag (&v1));
#else
			OOF_VecScale (&v1, fInf);
#endif
			OOF_VecInc (&v0, &v1);
#endif
			}
		glVertex3fv ((GLfloat *) &v0);
		}	
	glEnd ();
CHECK();
	}
#if DBG_SHADOWS
if (bFrontCap)
#endif
for (i = pso->litFaces.nFaces, ppf = pso->litFaces.pFaces; i; i--, ppf++) {
	pf = *ppf;
	if (!pf->bFacingLight)
		continue;
#if DBG_SHADOWS
	if (pf->bFacingLight && (bShadowTest > 3)) {
		glColor4f (1.0f, 0.8f, 0.2f, 1.0f);
		v1 = v0 = pf->vCenterf;
		glBegin (GL_LINES);
		glVertex3fv ((GLfloat *) &v0);
		OOF_VecInc (&v0, &pf->vNormf);
		glVertex3fv ((GLfloat *) &v0);
		glEnd ();
		glColor4d (1,0,0,1);
		glBegin (GL_LINES);
		glVertex3fv ((GLfloat *) &v1);
		glVertex3fv ((GLfloat *) &vLightPos);
		glEnd ();
		glColor4fv ((GLfloat *) (modelColor + bCullFront));
		}
	if (bShadowTest && (bShadowTest != 2)) {
		glLineWidth (1);
		glBegin (GL_LINE_LOOP);
		}
	else
#endif
		glBegin (GL_TRIANGLE_FAN);
	for (j = pf->nVerts, pfv = pf->pVerts; j; j--) {
		v0 = pvf [*pfv++];
		glVertex3fv ((GLfloat *) &v0);
		}
	glEnd ();
CHECK();
	}
return 1;
}

//------------------------------------------------------------------------------

int G3DrawSubModelShadow (tPOFObject *po, tPOFSubObject *pso)
{
	int			h = 1, i;

if (pso->nParent >= 0)
	G3StartInstanceAngles (&pso->vPos, &pso->vAngles);
h = (int) (pso - po->subObjs.pSubObjs);
for (i = 0; i < po->subObjs.nSubObjs; i++)
	if (po->subObjs.pSubObjs [i].nParent == h)
		G3DrawSubModelShadow (po, po->subObjs.pSubObjs + i);
#if 0
if (pso - po->subObjs.pSubObjs == 2)
#endif
{
G3GetLitFaces (po, pso);
h = G3RenderSubModelShadowVolume (po, pso, 0) &&
	 G3RenderSubModelShadowVolume (po, pso, 1) &&
	 G3RenderSubModelShadowCaps (po, pso, 0) &&
	 G3RenderSubModelShadowCaps (po, pso, 1);
}
if (pso->nParent >= 0)
	G3DoneInstance ();
return h;
}

//------------------------------------------------------------------------------

int G3GatherPolyModelItems (tObject *objP, void *modelP, vmsAngVec *pAnimAngles, tPOFObject *po,
									 int bShadowData)
{
	int			j;
	vmsVector	*pv;
	tOOF_vector	*pvf;

if (!(po->nState || G3AllocPolyModelItems (modelP, po, bShadowData)))
	return 0;
if (po->nState == 1) {
	G3StartInstanceMatrix (&objP->position.vPos, &objP->position.mOrient);
	G3GetPolyModelItems (modelP, pAnimAngles, po, 1, bShadowData, 0, -1);
	if (bShadowData) {
		vCenter.x = vCenter.y = vCenter.z = 0.0f;
		for (j = po->nVerts, pv = po->pvVerts, pvf = po->pvVertsf; j; j--, pv++, pvf++) {
			vCenter.x += f2fl (pv->x);
			vCenter.y += f2fl (pv->y);
			vCenter.z += f2fl (pv->z);
			}
		OOF_VecScale (&vCenter, 1.0f / (float) po->nVerts);
		po->vCenter.x = fl2f (vCenter.x);
		po->vCenter.y = fl2f (vCenter.y);
		po->vCenter.z = fl2f (vCenter.z);

		G3GetAdjFaces (po);
		G3GetPolyModelCenters (po);
		}
	po->nState = 2;
	G3DoneInstance ();
	}
if (bShadowData) {
	po->iSubObj = 0;
	G3StartInstanceMatrix (&objP->position.vPos, &objP->position.mOrient);
	G3GetPolyModelItems (modelP, pAnimAngles, po, 0, 1, 0, -1);
	G3DoneInstance ();
	}
return 1;
}

//------------------------------------------------------------------------------

int CanSeePoint (tObject *objP, vmsVector *vPoint)
{
	fvi_query	fq;
	int			nHitType;
	fvi_info		hit_data;

	//see if we can see this tPlayer

fq.p0 = &objP->position.vPos;
fq.p1 = vPoint;
fq.rad = 0;
fq.thisObjNum = OBJ_IDX (objP);
fq.flags = FQ_TRANSWALL;
if (gameStates.app.bSpectating && (objP == gameData.objs.viewer))
	fq.startSeg = FindSegByPoint (&objP->position.vPos, objP->nSegment);
else
	fq.startSeg = objP->nSegment;
fq.ignoreObjList = NULL;
nHitType = FindVectorIntersection (&fq, &hit_data);
return nHitType != HIT_WALL;
}

//	-----------------------------------------------------------------------------

float NearestShadowedWallDist (tObject *objP, vmsVector *vLightPos)
{
	fvi_query	fq;
	fvi_info		fi;
	vmsVector	v;

	static float fClip [3] = {1.2f, 1.5f, 2.0f};

if (!gameOpts->render.shadows.bClip)
	return INFINITY;
fq.p0					= &objP->position.vPos;
VmVecSub (&v, fq.p0, vLightPos);
VmVecNormalize (&v);
VmVecScale (&v, (fix) F1_0 * (fix) INFINITY);
fq.startSeg			= objP->nSegment;
fq.p1					= &v;
fq.rad				= 0; //objP->size;
fq.thisObjNum		= OBJ_IDX (objP);
fq.ignoreObjList	= NULL;
fq.flags				= FQ_TRANSWALL; // -- Why were we checking gameData.objs.objects? | FQ_CHECK_OBJS;		//what about trans walls???
if (FindVectorIntersection (&fq, &fi) != HIT_WALL)
	return INFINITY;
return f2fl (VmVecDist (fq.p0, &fi.hit.vPoint)) * fClip [gameOpts->render.shadows.nReach];
}

//	-----------------------------------------------------------------------------

int G3DrawPolyModelShadow (tObject *objP, void *modelP, vmsAngVec *pAnimAngles, int nModel)
{
#if SHADOWS
	vmsVector		v, vLightDir;
	short				*pnl;
	int				h, i, j, bCalcCenter = 0;
	tPOFObject		*po = gameData.models.pofData [gameStates.app.bD1Mission][1] + nModel;

if (nModel == 75)
	nModel = nModel;
Assert (objP->id < MAX_ROBOT_TYPES);
if (!gameStates.render.bShadowMaps) {
	if (!G3GatherPolyModelItems (objP, modelP, pAnimAngles, po, 1))
		return 0;
	}
CHECK();
OglActiveTexture (GL_TEXTURE0_ARB);
glEnable (GL_TEXTURE_2D);
pnl = gameData.render.lights.dynamic.nNearestSegLights [objP->nSegment];
gameData.render.shadows.nLight = 0;
if (gameOpts->render.shadows.bFast) {
	for (i = 0; (gameData.render.shadows.nLight < gameOpts->render.shadows.nLights) && (*pnl >= 0); i++, pnl++) {
		gameData.render.shadows.pLight = gameData.render.lights.dynamic.shader.lights + *pnl;
		if (!gameData.render.shadows.pLight->bState)
			continue;
		if (!CanSeePoint (objP, &gameData.render.shadows.pLight->vPos))
			continue;
		VmVecSub (&vLightDir, &objP->position.vPos, &gameData.render.shadows.pLight->vPos);
		VmVecNormalize (&vLightDir);
		if (gameData.render.shadows.nLight) {
			for (j = 0; j < gameData.render.shadows.nLight; j++)
				if (abs (VmVecDot (&vLightDir, gameData.render.shadows.vLightDir + j)) > 2 * F1_0 / 3) // 60 deg
					break;
			if (j < gameData.render.shadows.nLight)
				continue;
			}
		gameData.render.shadows.vLightDir [gameData.render.shadows.nLight++] = vLightDir;
		if (gameStates.render.bShadowMaps)
			RenderShadowMap (gameData.render.lights.dynamic.lights + (gameData.render.shadows.pLight - gameData.render.lights.dynamic.shader.lights));
		else {
			G3TransformPoint (&v, &gameData.render.shadows.pLight->vPos, 0);
			OOF_VecVms2Oof (&vLightPos, &v);
			CHECK ();
			fInf = NearestShadowedWallDist (objP, &gameData.render.shadows.pLight->vPos);
			CHECK ();
			G3PolyModelVerts2Float (po);
			G3StartInstanceMatrix (&objP->position.vPos, &objP->position.mOrient);
			po->litFaces.nFaces = 0;
			G3DrawSubModelShadow (po, po->subObjs.pSubObjs);
			G3DoneInstance ();
			}
		}
	}
else {
	h = OBJ_IDX (objP);
	j = gameData.render.shadows.pLight - gameData.render.lights.dynamic.shader.lights;
	pnl = gameData.render.shadows.objLights [h];
	for (i = 0; i < gameOpts->render.shadows.nLights; i++, pnl++) {
		if (*pnl < 0)
			break;
		if (*pnl == j) {
			vLightPos = gameData.render.shadows.vLightPos;
			G3PolyModelVerts2Float (po);
			G3StartInstanceMatrix (&objP->position.vPos, &objP->position.mOrient);
			po->litFaces.nFaces = 0;
			G3DrawSubModelShadow (po, po->subObjs.pSubObjs);
			G3DoneInstance ();
			}
		}
	}
glDisable (GL_TEXTURE_2D);
#endif
CHECK();
return 1;
}

//------------------------------------------------------------------------------

bool G3DrawPolyModel (
	tObject		*objP, 
	void			*modelP, 
	grsBitmap	**modelBitmaps, 
	vmsAngVec	*pAnimAngles, 
	fix			xModelLight, 
	fix			*xGlowValues, 
	tRgbColorf	*objColorP,
	tPOFObject  *po,
	int			nModel)
{
	ubyte *p = modelP;
	short	h;

#if DBG_SHADOWS
if (bShadowTest)
	return 1;
#endif
#if CONTOUR_OUTLINE
//if (extraGameInfo [0].bShadows && (gameStates.render.nShadowPass < 3))
	return 1;
#endif
G3CheckAndSwap (modelP);
if (gameStates.render.bHaveDynLights && gameOpts->render.bDynLighting && 
	 objP && ((objP->nType == OBJ_ROBOT) || (objP->nType == OBJ_PLAYER)) && !po) {
	po = gameData.models.pofData [gameStates.app.bD1Mission][0] + nModel;
	G3GatherPolyModelItems (objP, modelP, pAnimAngles, po, 0);
	}
nGlow = -1;		//glow off by default
gameData.render.pVerts = gameData.models.fPolyModelVerts;
glEnable (GL_CULL_FACE);
glCullFace (GL_BACK);
for (;;) {
	h = WORDVAL (p);
	if (h == OP_EOF)
		break;
	else if (h == OP_DEFPOINTS) {
		int n = WORDVAL (p+2);

		RotatePointList (modelPointList, VECPTR (p+4), po ? po->pVertNorms : NULL, n, 0);
		p += n * sizeof (struct vmsVector) + 4;
		break;
		}
	else if (h == OP_DEFP_START) {
		int n = WORDVAL (p+2);
		int s = WORDVAL (p+4);

		RotatePointList (modelPointList, VECPTR (p+8), po ? po->pVertNorms : NULL, n, s);
		p += n * sizeof (struct vmsVector) + 8;
		}
	else if (h == OP_FLATPOLY) {
		int nv = WORDVAL (p+2);
		Assert (nv < MAX_POINTS_PER_POLY);
		if (G3CheckNormalFacing (VECPTR (p+4), VECPTR (p+16)) > 0) {
			int i, l = 32;
			GrSetColorRGB15bpp (WORDVAL (p+28), (ubyte) (255 * GrAlpha ()));
			GrFadeColorRGB (32.0 / (double) l);
			if (objColorP) {
				objColorP->red = (float) grdCurCanv->cv_color.color.red / 255.0f; 
				objColorP->green = (float) grdCurCanv->cv_color.color.green / 255.0f;
				objColorP->blue = (float) grdCurCanv->cv_color.color.blue / 255.0f;
				}
			p += 30;
			for (i = 0; i < nv; i++)
				pointList [i] = modelPointList + WORDPTR (p) [i];
			G3DrawPoly (nv, pointList);
			}
		else
			p += 30;
		p += (nv | 1) * 2;
		}
	else if (h == OP_TMAPPOLY) {
		int nv = WORDVAL (p+2);
		uvl *uvlList;
		Assert ( nv < MAX_POINTS_PER_POLY );
		if (G3CheckNormalFacing (VECPTR (p+4), VECPTR (p+16)) > 0) {
			int i;
			fix light;

			//calculate light from surface normal
			if (nGlow < 0) {			//no glow
				light = -VmVecDot (&viewInfo.view [0].fVec, VECPTR (p+16));
				light = f1_0/4 + (light * 3) / 4;
				light = FixMul (light, xModelLight);
				}
			else {				//yes glow
				light = xGlowValues [nGlow];
				nGlow = -1;
				}
			//now poke light into l values
			uvlList = (uvl *) (p + 30 + (nv | 1) * 2);
			for (i = 0; i < nv; i++)
				uvlList [i].l = light;

			if (objColorP) {
				unsigned char c = modelBitmaps [WORDVAL (p+28)]->bm_avgColor;
				objColorP->red = CPAL2Tr (gamePalette, c);
				objColorP->green = CPAL2Tg (gamePalette, c);
				objColorP->blue = CPAL2Tb (gamePalette, c);
				}
			p += 30;
			for (i = 0; i < nv; i++)
				pointList [i] = modelPointList + WORDPTR (p) [i];
			G3DrawTexPoly (nv, pointList, uvlList, modelBitmaps [WORDVAL (p-2)], VECPTR (p+16), 1);
			}
		else
			p += 30;
		p += (nv | 1) * 2 + nv * 12;
		}
	else if (h == OP_SORTNORM) {
		if (G3CheckNormalFacing (VECPTR (p+16), VECPTR (p+4)) > 0) {		//facing
			//draw back then front
			G3DrawPolyModel (NULL, p+WORDVAL (p+30), modelBitmaps, pAnimAngles, xModelLight, xGlowValues, objColorP, po, nModel);
			G3DrawPolyModel (NULL, p+WORDVAL (p+28), modelBitmaps, pAnimAngles, xModelLight, xGlowValues, objColorP, po, nModel);
			}
		else {			//not facing.  draw front then back
			G3DrawPolyModel (NULL, p+WORDVAL (p+28), modelBitmaps, pAnimAngles, xModelLight, xGlowValues, objColorP, po, nModel);
			G3DrawPolyModel (NULL, p+WORDVAL (p+30), modelBitmaps, pAnimAngles, xModelLight, xGlowValues, objColorP, po, nModel);
			}
		p += 32;
		}
	else if (h == OP_RODBM) {
		g3sPoint rodBotP, rodTopP;
		G3TransformAndEncodePoint (&rodBotP, VECPTR (p+20));
		G3TransformAndEncodePoint (&rodTopP, VECPTR (p+4));
		G3DrawRodTexPoly (modelBitmaps [WORDVAL (p+2)], &rodBotP, WORDVAL (p+16), &rodTopP, WORDVAL (p+32), f1_0);
		p+=36;
		}
	else if (h == OP_SUBCALL) {
		vmsAngVec *a;

		if (pAnimAngles)
			a = pAnimAngles + WORDVAL (p+2);
		else
			a = &zeroAngles;
		G3StartInstanceAngles (VECPTR (p+4), a);
		G3DrawPolyModel (NULL, p+WORDVAL (p+16), modelBitmaps, pAnimAngles, xModelLight, xGlowValues, objColorP, po, nModel);
		G3DoneInstance ();
		p += 20;
		}
	else if (h == OP_GLOW) {
		if (xGlowValues)
			nGlow = WORDVAL (p+2);
		p += 4;
		}
	else 
		Error ("invalid polygon model\n");
	}
return 1;
}

#ifndef NDEBUG
int nest_count;
#endif

//------------------------------------------------------------------------------
//alternate interpreter for morphing tObject
bool G3DrawMorphingModel (void *modelP, grsBitmap **modelBitmaps, vmsAngVec *pAnimAngles, 
								  fix xModelLight, vmsVector *new_points, int nModel)
{
	ubyte *p = modelP;
	fix *xGlowValues = NULL;

G3CheckAndSwap (modelP);
nGlow = -1;		//glow off by default
gameData.render.pVerts = gameData.models.fPolyModelVerts;
for (;;) {
	switch (WORDVAL (p)) {
		case OP_EOF:
			return 1;

		case OP_DEFPOINTS: {
			int n = WORDVAL (p+2);
			RotatePointList (modelPointList, new_points, NULL, n, 0);
			p += n*sizeof (struct vmsVector) + 4;
			break;
			}

		case OP_DEFP_START: {
			int n = WORDVAL (p+2);
			int s = WORDVAL (p+4);
			RotatePointList (modelPointList, new_points, NULL, n, s);
			p += n*sizeof (struct vmsVector) + 8;
			break;
			}

		case OP_FLATPOLY: {
			int nv = WORDVAL (p+2);
			int i, nTris;
			GrSetColor (WORDVAL (p+28));
			for (i = 0; i < 2; i++)
				pointList [i] = modelPointList + WORDPTR (p+30) [i];
			for (nTris=nv-2;nTris;nTris--) {
				pointList [2] = modelPointList + WORDPTR (p+30) [i++];
				G3CheckAndDrawPoly (3, pointList, NULL, NULL);
				pointList [1] = pointList [2];
				}
			p += 30 + ((nv&~1)+1)*2;
			break;
			}

		case OP_TMAPPOLY: {
			int nv = WORDVAL (p+2);
			uvl *uvlList;
			uvl morph_uvls [3];
			int i, nTris;
			fix light;
			//calculate light from surface normal
			if (nGlow < 0) {			//no glow
				light = -VmVecDot (&viewInfo.view [0].fVec, VECPTR (p+16));
				light = f1_0/4 + (light*3)/4;
				light = FixMul (light, xModelLight);
				}
			else {				//yes glow
				light = xGlowValues [nGlow];
				nGlow = -1;
				}
			//now poke light into l values
			uvlList = (uvl *) (p+30+ ((nv&~1)+1)*2);
			for (i = 0; i < 3; i++)
				morph_uvls [i].l = light;
			for (i = 0; i < 2; i++) {
				pointList [i] = modelPointList + WORDPTR (p+30) [i];
				morph_uvls [i].u = uvlList [i].u;
				morph_uvls [i].v = uvlList [i].v;
				}
			for (nTris = nv - 2; nTris; nTris--) {
				pointList [2] = modelPointList + WORDPTR (p+30) [i];
				morph_uvls [2].u = uvlList [i].u;
				morph_uvls [2].v = uvlList [i].v;
				i++;
				G3CheckAndDrawTMap (3, pointList, uvlList, modelBitmaps [WORDVAL (p+28)], NULL, NULL);
				pointList [1] = pointList [2];
				morph_uvls [1].u = morph_uvls [2].u;
				morph_uvls [1].v = morph_uvls [2].v;
				}
			p += 30 + ((nv&~1)+1)*2 + nv*12;
			break;
			}

		case OP_SORTNORM:
			if (G3CheckNormalFacing (VECPTR (p+16), VECPTR (p+4)) > 0) {		//facing
				//draw back then front
				G3DrawMorphingModel (p+WORDVAL (p+30), modelBitmaps, pAnimAngles, xModelLight, new_points, nModel);
				G3DrawMorphingModel (p+WORDVAL (p+28), modelBitmaps, pAnimAngles, xModelLight, new_points, nModel);

				}
			else {			//not facing.  draw front then back
				G3DrawMorphingModel (p+WORDVAL (p+28), modelBitmaps, pAnimAngles, xModelLight, new_points, nModel);
				G3DrawMorphingModel (p+WORDVAL (p+30), modelBitmaps, pAnimAngles, xModelLight, new_points, nModel);
				}
			p += 32;
			break;

		case OP_RODBM: {
			g3sPoint rodBotP, rodTopP;
			G3TransformAndEncodePoint (&rodBotP, VECPTR (p+20));
			G3TransformAndEncodePoint (&rodTopP, VECPTR (p+4));
			G3DrawRodTexPoly (modelBitmaps [WORDVAL (p+2)], &rodBotP, WORDVAL (p+16), &rodTopP, WORDVAL (p+32), f1_0);
			p+=36;
			break;
			}

		case OP_SUBCALL: {
			vmsAngVec *a;
			if (pAnimAngles)
				a = &pAnimAngles [WORDVAL (p+2)];
			else
				a = &zeroAngles;
			G3StartInstanceAngles (VECPTR (p+4), a);
			G3DrawPolyModel (NULL, p+WORDVAL (p+16), modelBitmaps, pAnimAngles, xModelLight, xGlowValues, NULL, NULL, nModel);
			G3DoneInstance ();
			p += 20;
			break;
			}

		case OP_GLOW:
			if (xGlowValues)
				nGlow = WORDVAL (p+2);
			p += 4;
			break;
		}
	}
return 1;
}

//------------------------------------------------------------------------------

void InitSubModel (ubyte *p)
{
Assert (++nest_count < 1000);
for (;;) {
	switch (WORDVAL (p)) {
		case OP_EOF:
			return;

		case OP_DEFPOINTS: {
			int n = WORDVAL (p+2);
			p += n*sizeof (struct vmsVector) + 4;
			break;
			}

		case OP_DEFP_START: {
			int n = WORDVAL (p+2);
			p += n*sizeof (struct vmsVector) + 8;
			break;
			}

		case OP_FLATPOLY: {
			int nv = WORDVAL (p+2);
			Assert (nv > 2);		//must have 3 or more points
//				*WORDPTR (p+28) = (short)GrFindClosestColor15bpp (WORDVAL (p+28);
			p += 30 + ((nv&~1)+1)*2;
			break;
			}

		case OP_TMAPPOLY: {
			int nv = WORDVAL (p+2);
			Assert (nv > 2);		//must have 3 or more points
			if (WORDVAL (p+28) > nHighestTexture)
				nHighestTexture = WORDVAL (p+28);
			p += 30 + ((nv&~1)+1)*2 + nv*12;
			break;
			}

		case OP_SORTNORM:
			InitSubModel (p+WORDVAL (p+28));
			InitSubModel (p+WORDVAL (p+30));
			p += 32;
			break;

		case OP_RODBM:
			p += 36;
			break;

		case OP_SUBCALL: {
			InitSubModel (p+WORDVAL (p+16));
			p += 20;
			break;
			}

		case OP_GLOW:
			p += 4;
			break;

		default:
			Error ("invalid polygon model\n");
		}
	}
}

//------------------------------------------------------------------------------
//init code for bitmap models
void G3InitPolyModel (void *modelP)
{
	#ifndef NDEBUG
	nest_count = 0;
	#endif

nHighestTexture = -1;
G3CheckAndSwap (modelP);
InitSubModel ((ubyte *) modelP);
}

//------------------------------------------------------------------------------

void G3FreeAllPolyModelItems (void)
{
	int	h, i, j;

for (h = 0; h < 2; h++)
	for (i = 0; i < 2; i++)
		for (j = 0; j < MAX_POLYGON_MODELS; j++)
			G3FreePolyModelItems (gameData.models.pofData [h][i] + j);
memset (gameData.models.pofData, 0, sizeof (gameData.models.pofData));
}

//------------------------------------------------------------------------------
//eof

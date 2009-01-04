#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#	include <conf.h>
#endif
//#include  "oof.h
#include "inferno.h"
#include "u_mem.h"
#include "error.h"
#include "light.h"
#include "ogl_lib.h"
#include "ogl_color.h"
#include "network.h"
#include "render.h"
#include "strutil.h"
#include "interp.h"
#include "ase.h"

static char	szLine [1024];
static char	szLineBackup [1024];
static int nLine = 0;
static CFile *aseFile = NULL;
static char *pszToken = NULL;
static int bErrMsg = 0;

#define ASE_ROTATE_MODEL	1
#define ASE_FLIP_TEXCOORD	1

using namespace ASE;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int CModel::Error (const char *pszMsg)
{
if (!bErrMsg) {
	if (pszMsg)
		PrintLog ("   %s: error in line %d (%s)\n", aseFile->Name (), nLine, pszMsg);
	else
		PrintLog ("   %s: error in line %d\n", aseFile->Name (), nLine);
	bErrMsg = 1;
	}
return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

static float FloatTok (const char *delims)
{
pszToken = strtok (NULL, delims);
if (!(pszToken && *pszToken))
	CModel::Error ("missing data");
return pszToken ? (float) atof (pszToken) : 0;
}

//------------------------------------------------------------------------------

static int IntTok (const char *delims)
{
pszToken = strtok (NULL, delims);
if (!(pszToken && *pszToken))
	CModel::Error ("missing data");
return pszToken ? atoi (pszToken) : 0;
}

//------------------------------------------------------------------------------

static char CharTok (const char *delims)
{
pszToken = strtok (NULL, delims);
if (!(pszToken && *pszToken))
	CModel::Error ("missing data");
return pszToken ? *pszToken : '\0';
}

//------------------------------------------------------------------------------

static char *StrTok (const char *delims)
{
pszToken = strtok (NULL, delims);
if (!(pszToken && *pszToken))
	CModel::Error ("missing data");
return pszToken ? pszToken : reinterpret_cast<char*> ("");
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

static void ReadVector (CFile& cf, CFloatVector3 *pv)
{
#if ASE_ROTATE_MODEL
	float x = FloatTok (" \t");
	float z = -FloatTok (" \t");
	float y = FloatTok (" \t");
	*pv = CFloatVector3::Create(x, y, z);
#else	// need to rotate model for Descent
	int	i;

for (i = 0; i < 3; i++)
	pv [i] = FloatTok (" \t");
#endif
}

//------------------------------------------------------------------------------

static char* ReadLine (CFile& cf)
{
while (!cf.EoF ()) {
	cf.GetS (szLine, sizeof (szLine));
	nLine++;
	strcpy (szLineBackup, szLine);
	strupr (szLine);
	if ((pszToken = strtok (szLine, " \t")))
		return pszToken;
	}
return NULL;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int ASE_ReleaseTextures (void)
{
	CModel	*pm;
	int			bCustom, i;

PrintLog ("releasing ASE model textures\n");
for (bCustom = 0; bCustom < 2; bCustom++)
	for (i = gameData.models.nHiresModels, pm = gameData.models.aseModels [bCustom]; i; i--, pm++)
		pm->ReleaseTextures ();
return 0;
}

//------------------------------------------------------------------------------

int ASE_ReloadTextures (void)
{
	CModel	*pm;
	int		bCustom, i;

PrintLog ("reloading ASE model textures\n");
for (bCustom = 0; bCustom < 2; bCustom++)
	for (i = gameData.models.nHiresModels, pm = gameData.models.aseModels [bCustom]; i; i--, pm++)
		if (!pm->ReloadTextures (bCustom)) {
			return 0;
			}
return 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void CSubModel::Init (void)
{
m_next = NULL;
memset (m_szName, 0, sizeof (m_szName));
memset (m_szParent, 0, sizeof (m_szParent));
m_nSubModel = 0;
m_nParent = 0;
m_nBitmap = 0;
m_nFaces = 0;
m_nVerts = 0;
m_nTexCoord = 0;
m_nIndex = 0;
m_bRender = 1;
m_bGlow = 0;
m_bThruster = 0;
m_bWeapon = 0;
m_nGun = -1;
m_nBomb = -1;
m_nMissile = -1;
m_nType = 0;
m_nWeaponPos = 0;
m_nGunPoint = -1;
m_nBullets = -1;
m_bBarrel = 0;
m_vOffset.SetZero ();
}

//------------------------------------------------------------------------------

void CSubModel::Destroy (void)
{
m_faces.Destroy ();
m_verts.Destroy ();
m_texCoord.Destroy ();
Init ();
}

//------------------------------------------------------------------------------

int CSubModel::ReadNode (CFile& cf)
{
	int	i;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*TM_POS")) {
		for (i = 0; i < 3; i++)
			m_vOffset [i] = 0; //FloatTok (" \t");
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CSubModel::ReadMeshVertexList (CFile& cf)
{
	CVertex	*pv;
	int					i;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MESH_VERTEX")) {
		if (!m_verts)
			return CModel::Error ("no vertices found");
		i = IntTok (" \t");
		if ((i < 0) || (i >= m_nVerts))
			return CModel::Error ("invalid vertex number");
		pv = m_verts + i;
		ReadVector (cf, &pv->m_vertex);
		}	
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CSubModel::ReadMeshFaceList (CFile& cf)
{
	CFace	*pf;
	int	i;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MESH_FACE")) {
		if (!m_faces)
			return CModel::Error ("no faces found");
		i = IntTok (" \t");
		if ((i < 0) || (i >= m_nFaces))
			return CModel::Error ("invalid face number");
		pf = m_faces + i;
		for (i = 0; i < 3; i++) {
			strtok (NULL, " :\t");
			pf->m_nVerts [i] = IntTok (" :\t");
			}
		do {
			pszToken = StrTok (" :\t");
			if (!*pszToken)
				return CModel::Error ("unexpected end of file");
			} while (strcmp (pszToken, "*MESH_MTLID"));
		pf->m_nBitmap = IntTok (" ");
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CSubModel::ReadVertexTexCoord (CFile& cf)
{
	tTexCoord2f		*pt;
	int				i;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MESH_TVERT")) {
		if (!m_texCoord)
			return CModel::Error ("no texture coordinates found");
		i = IntTok (" \t");
		if ((i < 0) || (i >= m_nTexCoord))
			return CModel::Error ("invalid texture coordinate number");
		pt = m_texCoord + i;
#if ASE_FLIP_TEXCOORD
		pt->v.u = FloatTok (" \t");
		pt->v.v = -FloatTok (" \t");
#else
		for (i = 0; i < 2; i++)
			pt->a [i] = FloatTok (" \t");
#endif
		}	
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CSubModel::ReadFaceTexCoord (CFile& cf)
{
	CFace	*pf;
	int	i;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MESH_TFACE")) {
		if (!m_faces)
			return CModel::Error ("no faces found");
		i = IntTok (" \t");
		if ((i < 0) || (i >= m_nFaces))
			return CModel::Error ("invalid face number");
		pf = m_faces + i;
		for (i = 0; i < 3; i++)
			pf->m_nTexCoord [i] = IntTok (" \t");
		}	
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CSubModel::ReadMeshNormals (CFile& cf)
{
	CFace*	pf;
	CVertex*	pv;
	int		i;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MESH_FACENORMAL")) {
		if (!m_faces)
			return CModel::Error ("no faces found");
		i = IntTok (" \t");
		if ((i < 0) || (i >= m_nFaces))
			return CModel::Error ("invalid face number");
		pf = m_faces + i;
		ReadVector (cf, &pf->m_vNormal);
		}
	else if (!strcmp (pszToken, "*MESH_VERTEXNORMAL")) {
		if (!m_verts)
			return CModel::Error ("no vertices found");
		i = IntTok (" \t");
		if ((i < 0) || (i >= m_nVerts))
			return CModel::Error ("invalid vertex number");
		pv = m_verts + i;
		ReadVector (cf, &pv->m_normal);
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CSubModel::ReadMesh (CFile& cf, int& nFaces, int& nVerts)
{
if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MESH_NUMVERTEX")) {
		if (m_verts.Buffer ())
			return CModel::Error ("duplicate vertex list");
		m_nVerts = IntTok (" \t");
		if (!m_nVerts)
			return CModel::Error ("no vertices found");
		nVerts += m_nVerts;
		if (!(m_verts.Create (m_nVerts)))
			return CModel::Error ("out of memory");
		m_verts.Clear ();
		}
	else if (!strcmp (pszToken, "*MESH_NUMTVERTEX")) {
		if (m_texCoord.Buffer ())
			return CModel::Error ("no texture coordinates found");
		m_nTexCoord = IntTok (" \t");
		if (m_nTexCoord) {
			if (!(m_texCoord.Create (m_nTexCoord)))
				return CModel::Error ("out of memory");
			}
		}
	else if (!strcmp (pszToken, "*MESH_NUMFACES")) {
		if (m_faces.Buffer ())
			return CModel::Error ("no faces found");
		m_nFaces = IntTok (" \t");
		if (!m_nFaces)
			return CModel::Error ("no faces specified");
		nFaces += m_nFaces;
		if (!(m_faces.Create (m_nFaces)))
			return CModel::Error ("out of memory");
		m_faces.Clear ();
		}
	else if (!strcmp (pszToken, "*MESH_VERTEX_LIST")) {
		if (!ReadMeshVertexList (cf))
			return CModel::Error (NULL);
		}
	else if (!strcmp (pszToken, "*MESH_FACE_LIST")) {
		if (!ReadMeshFaceList (cf))
			return CModel::Error (NULL);
		}
	else if (!strcmp (pszToken, "*MESH_NORMALS")) {
		if (!ReadMeshNormals (cf))
			return CModel::Error (NULL);
		}
	else if (!strcmp (pszToken, "*MESH_TVERTLIST")) {
		if (!ReadVertexTexCoord (cf))
			return CModel::Error (NULL);
		}
	else if (!strcmp (pszToken, "*MESH_TFACELIST")) {
		if (!ReadFaceTexCoord (cf))
			return CModel::Error (NULL);
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CSubModel::Read (CFile& cf, int& nFaces, int& nVerts)
{
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*NODE_NAME")) {
		strcpy (m_szName, StrTok (" \t\""));
		if (strstr (m_szName, "$GUNPNT"))
			m_nGunPoint = atoi (m_szName + 8);
		if (strstr (m_szName, "$BULLETS"))
			m_nBullets = 1;
		else if (strstr (m_szName, "GLOW") != NULL) 
			m_bGlow = 1;
		else if (strstr (m_szName, "$DUMMY") != NULL)
			m_bRender = 0;
		else if (strstr (m_szName, "$THRUSTER") != NULL)
			m_bThruster = 1;
		else if (strstr (m_szName, "$WINGTIP") != NULL) {
			m_bWeapon = 1;
			m_nGun = 0;
			m_nBomb =
			m_nMissile = -1;
			m_nType = atoi (m_szName + 8) + 1;
			}
		else if (strstr (m_szName, "$GUN") != NULL) {
			m_bWeapon = 1;
			m_nGun = atoi (m_szName + 4) + 1;
			m_nWeaponPos = atoi (m_szName + 6) + 1;
			m_nBomb =
			m_nMissile = -1;
			}
		else if (strstr (m_szName, "$BARREL") != NULL) {
			m_bWeapon = 1;
			m_nGun = atoi (m_szName + 7) + 1;
			m_nWeaponPos = atoi (m_szName + 9) + 1;
			m_nBomb =
			m_nMissile = -1;
			m_bBarrel = 1;
			}
		else if (strstr (m_szName, "$MISSILE") != NULL) {
			m_bWeapon = 1;
			m_nMissile = atoi (m_szName + 8) + 1;
			m_nWeaponPos = atoi (m_szName + 10) + 1;
			m_nGun =
			m_nBomb = -1;
			}
		else if (strstr (m_szName, "$BOMB") != NULL) {
			m_bWeapon = 1;
			m_nBomb = atoi (m_szName + 6) + 1;
			m_nGun =
			m_nMissile = -1;
			}
		}
	else if (!strcmp (pszToken, "*NODE_PARENT")) {
		strcpy (m_szParent, StrTok (" \t\""));
		}
	if (!strcmp (pszToken, "*NODE_TM")) {
		if (!ReadNode (cf))
			return CModel::Error (NULL);
		}
	else if (!strcmp (pszToken, "*MESH")) {
		if (!ReadMesh (cf, nFaces, nVerts))
			return CModel::Error (NULL);
		}
	else if (!strcmp (pszToken, "*MATERIAL_REF")) {
		m_nBitmap = IntTok (" \t");
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void CModel::Init (void)
{
m_subModels = NULL;
m_nModel = 0;
m_nType = 0;
m_nSubModels = 0;
m_nVerts = 0;
m_nFaces = 0;
}

//------------------------------------------------------------------------------

void CModel::Destroy (void)
{
	CSubModel	*psmi, *psmj;

for (psmi = m_subModels; psmi; ) {
	psmj = psmi;
	psmi = psmi->m_next;
	delete psmj;
	}
FreeTextures ();
Init ();
}

//------------------------------------------------------------------------------

int CModel::ReleaseTextures (void)
{
m_textures.Release ();
return 0;
}

//------------------------------------------------------------------------------

int CModel::ReloadTextures (int bCustom)
{
return m_textures.Bind (m_nType, bCustom); 
}

//------------------------------------------------------------------------------

int CModel::FreeTextures (void)
{
m_textures.Destroy ();
return 0;
}
//------------------------------------------------------------------------------

int CModel::ReadTexture (CFile& cf, int nBitmap, int nType, int bCustom)
{
	CBitmap	*bmP = m_textures.m_bitmaps + nBitmap;
	char		fn [FILENAME_LEN], *ps;
	int		l;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
bmP->SetFlat (0);
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*BITMAP")) {
		if (bmP->Buffer ())	//duplicate
			return CModel::Error ("duplicate item");
		*fn = '\001';
		CFile::SplitPath (StrTok ("\""), NULL, fn + 1, NULL);
		if (!ReadModelTGA (strlwr (fn), bmP, nType, bCustom))
			return CModel::Error ("texture not found");
		l = (int) strlen (fn) + 1;
		if (!m_textures.m_names [nBitmap].Create (l))
			return CModel::Error ("out of memory");
		memcpy (m_textures.m_names [nBitmap].Buffer (), fn, l);
		if ((ps = strstr (fn, "color")))
			m_textures.m_nTeam [nBitmap] = atoi (ps + 5) + 1;
		else
			m_textures.m_nTeam [nBitmap] = 0;
		bmP->SetTeam (m_textures.m_nTeam [nBitmap]);
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CModel::ReadMaterial (CFile& cf, int nType, int bCustom)
{
	int		i;
	CBitmap	*bmP;

i = IntTok (" \t");
if ((i < 0) || (i >= m_textures.m_nBitmaps))
	return CModel::Error ("invalid bitmap number");
if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
bmP = m_textures.m_bitmaps + i;
bmP->SetFlat (1);
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MATERAL_DIFFUSE")) {
		tRgbColorb	avgRGB;
		avgRGB.red = (ubyte) (FloatTok (" \t") * 255 + 0.5);
		avgRGB.green = (ubyte) (FloatTok (" \t") * 255 + 0.5);
		avgRGB.blue = (ubyte) (FloatTok (" \t") * 255 + 0.5);
		bmP->SetAvgColor (avgRGB);
		}
	else if (!strcmp (pszToken, "*MAP_DIFFUSE")) {
		if (!ReadTexture (cf, i, nType, bCustom))
			return CModel::Error (NULL);
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------

int CModel::ReadMaterialList (CFile& cf, int nType, int bCustom)
{
if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
if (!(pszToken = ReadLine (cf)))
	return CModel::Error ("unexpected end of file");
if (strcmp (pszToken, "*MATERIAL_COUNT"))
	return CModel::Error ("material count missing");
int nBitmaps = IntTok (" \t");
if (!nBitmaps)
	return CModel::Error ("no bitmaps specified");
if (!(m_textures.Create (nBitmaps)))
	return CModel::Error ("out of memory");
while ((pszToken = ReadLine (cf))) {
	if (*pszToken == '}')
		return 1;
	if (!strcmp (pszToken, "*MATERIAL")) {
		if (!ReadMaterial (cf, nType, bCustom))
			return CModel::Error (NULL);
		}
	}
return CModel::Error ("unexpected end of file");
}

//------------------------------------------------------------------------------


int CModel::ReadSubModel (CFile& cf)
{
	CSubModel	*psm;

if (CharTok (" \t") != '{')
	return CModel::Error ("syntax error");
if (!(psm = new CSubModel))
	return CModel::Error ("out of memory");
psm->m_nSubModel = m_nSubModels++;
psm->m_next = m_subModels;
m_subModels = psm;
return psm->Read (cf, m_nFaces, m_nVerts);
}

//------------------------------------------------------------------------------

int CModel::FindSubModel (const char* pszName)
{
	CSubModel *psm;

for (psm = m_subModels; psm; psm = psm->m_next)
	if (!strcmp (psm->m_szName, pszName))
		return psm->m_nSubModel;
return -1;
}

//------------------------------------------------------------------------------

void CModel::LinkSubModels (void)
{
	CSubModel	*psm;

for (psm = m_subModels; psm; psm = psm->m_next)
	psm->m_nParent = FindSubModel (psm->m_szParent);
}

//------------------------------------------------------------------------------

int CModel::Read (const char* filename, short nModel, short nType, int bCustom)
{
	CFile			cf;
	int			nResult = 1;

if (!cf.Open (filename, gameFolders.szModelDir [nType], "rb", 0)) {
	return 0;
	}
bErrMsg = 0;
aseFile = &cf;
Init ();
m_nModel = nModel;
m_nType = nType;
#if DBG
nLine = 0;
#endif
while ((pszToken = ReadLine (cf))) {
	if (!strcmp (pszToken, "*MATERIAL_LIST")) {
		if (!(nResult = ReadMaterialList (cf, nType, bCustom)))
			break;
		}
	else if (!strcmp (pszToken, "*GEOMOBJECT")) {
		if (!(nResult = ReadSubModel (cf)))
			break;
		}
	}
cf.Close ();
if (!nResult)
	Destroy ();
else {
	LinkSubModels ();
	gameData.models.bHaveHiresModel [this - gameData.models.aseModels [bCustom]] = 1;
	}
aseFile = NULL;
return nResult;
}


//------------------------------------------------------------------------------
//eof


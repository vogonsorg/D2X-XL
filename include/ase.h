#ifndef _ASE_H
#define _ASE_H

typedef struct tASEVertex {
	fVector3					vertex;
	fVector3					normal;
} tASEVertex;

typedef struct tASEFace {
	fVector3					vNormal;
	short						nVerts [3];	// indices of vertices 
	short						nTexCoord [3];
	short						nBitmap;
} tASEFace;

typedef struct tASESubModel {
	char						szName [256];
	char						szParent [256];
	short						nId;
	short						nParent;
	short						nBitmap;
	short						nFaces;
	short						nVerts;
	short						nTexCoord;
	short						nIndex;
	ubyte						bRender;
	ubyte						bGlow;
	ubyte						bThruster;
	ubyte						bWeapon;
	char						nGun;
	char						nBomb;
	char						nMissile;
	char						nType;
	char						nWeaponPos;
	char						nGunPoint;
	char						nBullets;
	char						bBarrel;
	fVector3					vOffset;
	CArray<tASEFace>		faces;
	CArray<tASEVertex>	verts;
	CArray<tTexCoord2f>	texCoord;
} tASESubModel;

typedef struct tASESubModelList {
	struct tASESubModelList *pNextModel;
	tASESubModel				sm;
} tASESubModelList;

typedef struct tASEModel {
	CModelTextures			textures;
	tASESubModelList		subModels;
	int						nModel;
	int						nType;
	int						nSubModels;
	int						nVerts;
	int						nFaces;
} tASEModel;

void ASE_FreeModel (tASEModel *pm);
int ASE_ReadFile (char *pszFile, tASEModel *pm, short nModel, short nType, int bCustom);
int ASE_ReloadTextures (void);
int ASE_ReleaseTextures (void);
int ASE_FreeTextures (tASEModel *pm);

#endif //_ASE_H


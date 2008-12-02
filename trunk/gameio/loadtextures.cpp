/* $Id: piggy.c,v 1.51 2004/01/08 19:02:53 schaffner Exp $ */
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

#ifdef _WIN32
#	include <windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pstypes.h"
#include "strutil.h"
#include "inferno.h"
#include "gr.h"
#include "u_mem.h"
#include "iff.h"
#include "mono.h"
#include "error.h"
#include "sounds.h"
#include "songs.h"
#include "bm.h"
#include "bmread.h"
#include "gamemine.h"
#include "hash.h"
#include "args.h"
#include "palette.h"
#include "gamefont.h"
#include "rle.h"
#include "screens.h"
#include "piggy.h"
#include "gamemine.h"
#include "textures.h"
#include "texmerge.h"
#include "paging.h"
#include "game.h"
#include "text.h"
#include "cfile.h"
#include "newmenu.h"
#include "byteswap.h"
#include "findfile.h"
#include "makesig.h"
#include "effects.h"
#include "wall.h"
#include "weapon.h"
#include "error.h"
#include "grdef.h"
#include "gamepal.h"

//#define NO_DUMP_SOUNDS        1   //if set, dump bitmaps but not sounds

#if DBG
#	define PIGGY_MEM_QUOTA	4
#else
#	define PIGGY_MEM_QUOTA	8
#endif

const char *szAddonTextures [MAX_ADDON_BITMAP_FILES] = {
	"slowmotion#0",
	"bullettime#0"
	};

static short d2OpaqueDoors [] = {
	440, 451, 463, 477, /*483,*/ 488, 
	500, 508, 523, 550, 556, 564, 572, 579, 585, 593, 536, 
	600, 608, 615, 628, 635, 6
, 649, 664, /*672,*/ 687, 
	702, 717, 725, 731, 738, 745, 754, 763, 772, 780, 790,
	806, 817, 827, 838, 849, /*858,*/ 863, 871, 886,
	901,
	-1};

#define RLE_REMAP_MAX_INCREASE 132 /* is enough for d1 pc registered */

static int bLowMemory = 0;

#if DBG
#	define PIGGY_BUFFER_SIZE ((unsigned int) (512*1024*1024))
#else
#	define PIGGY_BUFFER_SIZE ((unsigned int) 0x7fffffff)
#endif
#define PIGGY_SMALL_BUFFER_SIZE (16*1024*1024)		// size of buffer when bLowMemory is set

#define DBM_FLAG_ABM    64 // animated bitmap
#define DBM_NUM_FRAMES  63

#define BM_FLAGS_TO_COPY (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT \
                         | BM_FLAG_NO_LIGHTING | BM_FLAG_RLE | BM_FLAG_RLE_BIG)

extern char szLastPalettePig [];
extern ubyte bBigPig;

//------------------------------------------------------------------------------

#if DBG
typedef struct tTrackedBitmaps {
	CBitmap	*bmP;
	int			nSize;
} tTrackedBitmaps;

tTrackedBitmaps	trackedBitmaps [1000000];
int					nTrackedBitmaps = 0;
#endif

void UseBitmapCache (CBitmap *bmP, int nSize)
{
bitmapCacheUsed += nSize;
if (0x7fffffff < bitmapCacheUsed)
	bitmapCacheUsed = 0;
#if DBG
if (nSize < 0) {
	int i;
	for (i = 0; i < nTrackedBitmaps; i++)
		if (trackedBitmaps [i].bmP == bmP) {
			//CBP (trackedBitmaps [i].nSize != -nSize);
			if (i < --nTrackedBitmaps)
				trackedBitmaps [i] = trackedBitmaps [nTrackedBitmaps];
			trackedBitmaps [nTrackedBitmaps].bmP = NULL;
			trackedBitmaps [nTrackedBitmaps].nSize = 0;
			break;
			}
	}
else {
	trackedBitmaps [nTrackedBitmaps].bmP = bmP;
	trackedBitmaps [nTrackedBitmaps++].nSize = nSize;
	}
#endif
}

//------------------------------------------------------------------------------

int IsOpaqueDoor (int i)
{
	short	*p;

if (i >= 0)
	for (p = d2OpaqueDoors; *p >= 0; p++)
		if (i == gameData.pig.tex.pBmIndex [*p].index)
			return 1;
return 0;
}

//------------------------------------------------------------------------------

#if TEXTURE_COMPRESSION

int SaveS3TC (CBitmap *bmP, char *pszFolder, char *pszFilename)
{
	CFile		cf;
	char		szFilename [FILENAME_LEN], szFolder [FILENAME_LEN];

if (!bmP->bmCompressed)
	return 0;
CFSplitPath (pszFilename, szFolder, szFilename + 1, NULL);
strcat (szFilename + 1, ".s3tc");
*szFilename = '\x02';	//don't search lib (hog) files
if (*szFolder)
	pszFolder = szFolder;
if (cf.Exist (szFilename, pszFolder, 0))
	return 1;
if (!cf.Open (szFilename + 1, pszFolder, "wb", 0))
	return 0;
if ((cf.Write (&bmP->props.w, sizeof (bmP->props.w), 1) != 1) ||
	 (cf.Write (&bmP->props.h, sizeof (bmP->props.h), 1) != 1) ||
	 (cf.Write (&bmP->bmFormat, sizeof (bmP->bmFormat), 1) != 1) ||
    (cf.Write (&bmP->bmBufSize, sizeof (bmP->bmBufSize), 1) != 1) ||
    (cf.Write (bmP->texBuf, bmP->bmBufSize, 1) != 1)) {
	cf.Close ();
	return 0;
	}
return !cf.Close ();
}

//------------------------------------------------------------------------------

int ReadS3TC (CBitmap *bmP, char *pszFolder, char *pszFilename)
{
	CFile		cf;
	char		szFilename [FILENAME_LEN], szFolder [FILENAME_LEN];

if (!gameStates.ogl.bHaveTexCompression)
	return 0;
CFSplitPath (pszFilename, szFolder, szFilename, NULL);
if (!*szFilename)
	CFSplitPath (pszFilename, szFolder, szFilename, NULL);
strcat (szFilename, ".s3tc");
if (*szFolder)
	pszFolder = szFolder;
if (!cf.Open (szFilename, pszFolder, "rb", 0))
	return 0;
if ((cf.Read (&bmP->props.w, sizeof (bmP->props.w), 1) != 1) ||
	 (cf.Read (&bmP->props.h, sizeof (bmP->props.h), 1) != 1) ||
	 (cf.Read (&bmP->bmFormat, sizeof (bmP->bmFormat), 1) != 1) ||
	 (cf.Read (&bmP->bmBufSize, sizeof (bmP->bmBufSize), 1) != 1)) {
	cf.Close ();
	return 0;
	}
if (!(bmP->texBuf = (ubyte *) (ubyte *) D2_ALLOC (bmP->bmBufSize))) {
	cf.Close ();
	return 0;
	}
if (cf.Read (bmP->texBuf, bmP->bmBufSize, 1) != 1) {
	cf.Close ();
	return 0;
	}
cf.Close ();
bmP->bmCompressed = 1;
return 1;
}

#endif

//------------------------------------------------------------------------------

int FindTextureByIndex (int nIndex)
{
	int	i, j = gameData.pig.tex.nBitmaps [gameStates.app.bD1Mission];

for (i = 0; i < j; i++)
	if (gameData.pig.tex.pBmIndex [i].index == nIndex)
		return i;
return -1;
}

//------------------------------------------------------------------------------

#define	CHANGING_TEXTURE(_eP)	\
			(((_eP)->changingWallTexture >= 0) ? \
			 (_eP)->changingWallTexture : \
			 (_eP)->changingObjectTexture)

tEffectClip *FindEffect (tEffectClip *ecP, int tNum)
{
	int				h, i, j;
	tBitmapIndex	*frameP;

if (ecP)
	i = (int) (++ecP - gameData.eff.pEffects);
else {
	ecP = gameData.eff.pEffects;
	i = 0;
	}
for (h = gameData.eff.nEffects [gameStates.app.bD1Data]; i < h; i++, ecP++) {
	for (j = ecP->vc.nFrameCount, frameP = ecP->vc.frames; j > 0; j--, frameP++)
		if (frameP->index == tNum) {
#if 0
			int t = FindTextureByIndex (tNum);
			if (t >= 0)
				ecP->changingWallTexture = t;
#endif
			return ecP;
			}
	}
return NULL;
}

//------------------------------------------------------------------------------

tVideoClip *FindVClip (int tNum)
{
	int	h, i, j;
	tVideoClip *vcP = gameData.eff.vClips [0];

for (i = gameData.eff.nClips [0]; i; i--, vcP++) {
	for (h = vcP->nFrameCount, j = 0; j < h; j++)
		if (vcP->frames [j].index == tNum) {
			return vcP;
			}
	}
return NULL;
}

//------------------------------------------------------------------------------

tWallClip *FindWallAnim (int tNum)
{
	int	h, i, j;
	tWallClip *wcP = gameData.walls.pAnims;

for (i = gameData.walls.nAnims [gameStates.app.bD1Data]; i; i--, wcP++)
	for (h = wcP->nFrameCount, j = 0; j < h; j++)
		if (gameData.pig.tex.pBmIndex [wcP->frames [j]].index == tNum)
			return wcP;
return NULL;
}

//------------------------------------------------------------------------------

inline void PiggyFreeBitmapData (CBitmap *bmP)
{
if (bmP->texBuf) {
	D2_FREE (bmP->texBuf);
	UseBitmapCache (bmP, (int) -bmP->props.h * (int) bmP->props.rowSize);
	}
}

//------------------------------------------------------------------------------

void PiggyFreeMask (CBitmap *bmP)
{
	CBitmap	*mask;

if ((mask = bmP->Mask ())) {
	PiggyFreeBitmapData (mask);
	bmP->DestroyMask ();
	}
}

//------------------------------------------------------------------------------

int PiggyFreeHiresFrame (CBitmap *bmP, int bD1)
{

gameData.pig.tex.bitmaps [bD1][bmP->nId].SetOverride (NULL);
bmP->FreeTexture ();
PiggyFreeMask (bmP);
bmP->nType = 0;
bmP->SetTexBuf (NULL);
return 1;
}

//------------------------------------------------------------------------------

int PiggyFreeHiresAnimation (CBitmap *bmP, int bD1)
{
	CBitmap	*altBmP, *bmfP;
	int			i;

if (!(altBmP = bmP->Override ()))
	return 0;
bmP->SetOverride (NULL);
if (altBmP->Type () == BM_TYPE_FRAME)
	if (!(altBmP = altBmP->Parent ()))
		return 1;
if (altBmP->Type () != BM_TYPE_ALT)
	return 0;	//actually this would be an error
if ((bmfP = altBmP->Frames ()))
	for (i = altBmP->FrameCount (); i; i--, bmfP++)
		PiggyFreeHiresFrame (bmfP, bD1);
else
	PiggyFreeMask (altBmP);
altBmP->FreeTexture ();
altBmP->DestroyFrames ();
PiggyFreeBitmapData (altBmP);
altBmP->SetPalette (NULL);
altBmP->SetType (0);
return 1;
}

//------------------------------------------------------------------------------

void PiggyFreeHiresAnimations (void)
{
	int		i, bD1;
	CBitmap	*bmP;

for (bD1 = 0, bmP = gameData.pig.tex.bitmaps [bD1]; bD1 < 2; bD1++)
	for (i = gameData.pig.tex.nBitmaps [bD1]; i; i--, bmP++)
		PiggyFreeHiresAnimation (bmP, bD1);
}

//------------------------------------------------------------------------------

void PiggyFreeBitmap (CBitmap *bmP, int i, int bD1)
{
if (!bmP)
	bmP = gameData.pig.tex.bitmaps [bD1] + i;
else if (i < 0)
	i = (int) (bmP - gameData.pig.tex.bitmaps [bD1]);
PiggyFreeMask (bmP);
if (!PiggyFreeHiresAnimation (bmP, 0))
	bmP->FreeTexture ();
if (bitmapOffsets [bD1][i] > 0)
	bmP->SetFlags (bmP->Flags () | BM_FLAG_PAGED_OUT);
bmP->SetFromPog (0);
bmP->SetPalette (NULL);
PiggyFreeBitmapData (bmP);
}

//------------------------------------------------------------------------------

void PiggyBitmapPageOutAll (int bAll)
{
	int			i, bD1;
	CBitmap	*bmP;

#if TRACE			
con_printf (CON_VERBOSE, "Flushing piggy bitmap cache\n");
#endif
gameData.pig.tex.bPageFlushed++;
TexMergeFlush ();
RLECacheFlush ();
for (bD1 = 0; bD1 < 2; bD1++) {
	bitmapCacheNext [bD1] = 0;
	for (i = 0, bmP = gameData.pig.tex.bitmaps [bD1]; 
		  i < gameData.pig.tex.nBitmaps [bD1]; 
		  i++, bmP++) {
		if (bmP->TexBuf () && (bitmapOffsets [bD1][i] > 0)) { // only page out bitmaps read from disk
			bmP->SetFlags (bmP->Flags () | BM_FLAG_PAGED_OUT);
			PiggyFreeBitmap (bmP, i, bD1);
			}
		}
	}
for (i = 0; i < MAX_ADDON_BITMAP_FILES; i++)
	PiggyFreeBitmap (gameData.pig.tex.addonBitmaps + i, i, 0);
}

//------------------------------------------------------------------------------

void GetFlagData (const char *bmName, int nIndex)
{
	int			i;
	tFlagData	*pf;

if (strstr (bmName, "flag01#0") == bmName)
	i = 0;
else if (strstr (bmName, "flag02#0") == bmName)
	i = 1;
else
	return;
pf = gameData.pig.flags + i;
pf->bmi.index = nIndex;
pf->vcP = FindVClip (nIndex);
pf->vci.nClipIndex = gameData.objs.pwrUp.info [46 + i].nClipIndex;	//46 is the blue flag powerup
pf->vci.xFrameTime = gameData.eff.vClips [0][pf->vci.nClipIndex].xFrameTime;
pf->vci.nCurFrame = 0;
}

//------------------------------------------------------------------------------

int IsAnimatedTexture (short nTexture)
{
return (nTexture > 0) && (strchr (gameData.pig.tex.bitmapFiles [gameStates.app.bD1Mission][gameData.pig.tex.pBmIndex [nTexture].index].name, '#') != NULL);
}

//------------------------------------------------------------------------------

static int BestShrinkFactor (CBitmap *bmP, int nShrinkFactor)
{
	int	nBaseSize, nTargetSize, nBaseFactor;

#if 0
if (bmP->props.h >= 2 * bmP->props.w) {
#endif
	nBaseSize = bmP->props.w;
	nTargetSize = 512 / nShrinkFactor;
	nBaseFactor = (3 * nBaseSize / 2) / nTargetSize;
#if 0
	}
else {
	nBaseSize = bmP->props.w * bmP->props.h;
	nTargetSize = (512 * 512) / (nShrinkFactor * nShrinkFactor);
	nBaseFactor = (int) sqrt ((double) (3 * nBaseSize / 2) / nTargetSize);
	}
#endif
if (!nBaseFactor)
	return 1;
if (nBaseFactor > nShrinkFactor)
	return nShrinkFactor;
for (nShrinkFactor = 1; nShrinkFactor <= nBaseFactor; nShrinkFactor *= 2)
	;
return nShrinkFactor / 2;
}

//------------------------------------------------------------------------------

int PageInBitmap (CBitmap *bmP, const char *bmName, int nIndex, int bD1)
{
	CBitmap		*altBmP = NULL;
	int				temp, nSize, nOffset, nFrames, nShrinkFactor, nBestShrinkFactor,
						bRedone = 0, bTGA, bDefault = 0;
	time_t			tBase, tShrunk;
	CFile				cf, *cfP = &cf;
	char				fn [FILENAME_LEN], fnShrunk [FILENAME_LEN];
	tTgaHeader		h;

#if DBG
if (!bmName)
	return 0;
#endif
if (!bmP->texBuf) {
	StopTime ();
	nShrinkFactor = 8 >> min (gameOpts->render.textures.nQuality, gameStates.render.nMaxTextureQuality);
	nSize = (int) bmP->props.h * (int) bmP->props.rowSize;
	if (nIndex >= 0)
		GetFlagData (bmName, nIndex);
#if DBG
	if (strstr (bmName, "door13")) {
		sprintf (fn, "%s%s%s.tga", gameFolders.szTextureDir [bD1], 
					*gameFolders.szTextureDir [bD1] ? "/" : "", bmName);
		}
	else
#endif
	if (gameStates.app.bNostalgia)
		gameOpts->render.textures.bUseHires = 0;
	else {
		sprintf (fn, "%s%s%s.tga", gameFolders.szTextureDir [bD1], 
					*gameFolders.szTextureDir [bD1] ? "/" : "", bmName);
		tBase = cfP->Date (fn, "", 0);
		if (tBase < 0) 
			*fnShrunk = '\0';
		else {
			sprintf (fnShrunk, "%s%s%d/%s.tga", gameFolders.szTextureCacheDir [bD1], 
						*gameFolders.szTextureCacheDir [bD1] ? "/" : "", 512 / nShrinkFactor, bmName);
			tShrunk = cfP->Date (fnShrunk, "", 0);
			if (tShrunk < tBase)
				*fnShrunk = '\0';
			}
		}
	bTGA = 0;
	bmP->nBPP = 1;
	if (*bmName && ((nIndex < 0) || (gameOpts->render.textures.bUseHires && (!gameOpts->ogl.bGlTexMerge || gameStates.render.textures.bGlsTexMergeOk)))) {
#if 0
		if ((nIndex >= 0) && ReadS3TC (gameData.pig.tex.altBitmaps [bD1] + nIndex, gameFolders.szTextureCacheDir [bD1], bmName)) {
			altBmP = gameData.pig.tex.altBitmaps [bD1] + nIndex;
			altBmP->nType = BM_TYPE_ALT;
			bmP->Override ( = altBmP;
			BM_FRAMECOUNT (altBmP) = 1;
			gameData.pig.tex.bitmapFlags [bD1][nIndex] &= ~BM_FLAG_RLE;
			gameData.pig.tex.bitmapFlags [bD1][nIndex] |= BM_FLAG_TGA;
			bmP = altBmP;
			altBmP = NULL;
			}
		else 
#endif
		if ((gameStates.app.bCacheTextures && (nShrinkFactor > 1) && *fnShrunk && cfP->Open (fnShrunk, "", "rb", 0)) || 
			 cfP->Open (fn, "", "rb", 0)) {
			PrintLog ("loading hires texture '%s' (quality: %d)\n", fn, gameOpts->render.nTextureQuality);
			bTGA = 1;
			if (nIndex < 0)
				altBmP = gameData.pig.tex.addonBitmaps - nIndex - 1;
			else
				altBmP = gameData.pig.tex.altBitmaps [bD1] + nIndex;
			altBmP->nType = BM_TYPE_ALT;
			bmP->SetOverride (altBmP);
			bmP = altBmP;
			ReadTGAHeader (*cfP, &h, bmP);
			nSize = (int) h.width * (int) h.height * bmP->nBPP;
			nFrames = (h.height % h.width) ? 1 : h.height / h.width;
			bmP->SetFrameCount ((ubyte) nFrames);
			nOffset = cfP->Tell ();
			if (nIndex >= 0) {
				gameData.pig.tex.bitmapFlags [bD1][nIndex] &= ~(BM_FLAG_RLE | BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT);
				gameData.pig.tex.bitmapFlags [bD1][nIndex] |= BM_FLAG_TGA;
				if (bmP->props.h > bmP->props.w) {
					tEffectClip	*ecP = NULL;
					tWallClip *wcP;
					tVideoClip *vcP;
					while ((ecP = FindEffect (ecP, nIndex))) {
						//e->vc.nFrameCount = nFrames;
						ecP->flags |= EF_ALTFMT;
						//ecP->vc.flags |= WCF_ALTFMT;
						}
					if (!ecP) {
						if ((wcP = FindWallAnim (nIndex))) {
						//w->nFrameCount = nFrames;
							wcP->flags |= WCF_ALTFMT;
							}
						else if ((vcP = FindVClip (nIndex))) {
							//v->nFrameCount = nFrames;
							vcP->flags |= WCF_ALTFMT;
							}
						else {
							PrintLog ("   couldn't find animation for '%s'\n", bmName);
							}
						}
					}
				}
			}
		}
	if (!altBmP) {
		if (nIndex < 0) {
			StartTime (0);
			return 0;
			}
		cfP = cfPiggy + bD1;
		nOffset = bitmapOffsets [bD1][nIndex];
		bDefault = 1;
		}

reloadTextures:

	if (bRedone) {
		Error ("Not enough memory for textures.\nTry to decrease texture quality\nin the advanced render options menu.");
#if !DBG
		StartTime (0);
		if (!bDefault)
			cfP->Close ();
		return 0;
#endif
		}

	bRedone = 1;
	if (cfP->Seek (nOffset, SEEK_SET)) {
		PiggyCriticalError ();
		goto reloadTextures;
		}
#if 1//def _DEBUG
	strncpy (bmP->szName, bmName, sizeof (bmP->szName));
#endif
#if TEXTURE_COMPRESSION
	if (bmP->bmCompressed)
		UseBitmapCache (bmP, bmP->bmBufSize);
	else 
#endif
		{
		bmP->texBuf = (ubyte *) D2_ALLOC (nSize);
		if (bmP->texBuf) 
			UseBitmapCache (bmP, nSize);
		}
	if (!bmP->texBuf || (bitmapCacheUsed > bitmapCacheSize)) {
		Int3 ();
		PiggyBitmapPageOutAll (0);
		goto reloadTextures;
		}
	if (nIndex >= 0)
		bmP->props.flags = gameData.pig.tex.bitmapFlags [bD1][nIndex];
	bmP->nId = nIndex;
	if (bmP->props.flags & BM_FLAG_RLE) {
		nDescentCriticalError = 0;
		int zSize = cfP->ReadInt ();
		if (nDescentCriticalError) {
			PiggyCriticalError ();
			goto reloadTextures;
			}
		temp = (int) cfP->Read (bmP->texBuf + 4, 1, zSize-4);
		if (nDescentCriticalError) {
			PiggyCriticalError ();
			goto reloadTextures;
			}
		zSize = rle_expand (bmP, NULL, 0);
		if (bD1)
			bmP->Remap (paletteManager.D1 (), TRANSPARENCY_COLOR, SUPER_TRANSP_COLOR);
		else
			bmP->Remap (paletteManager.Game (), TRANSPARENCY_COLOR, SUPER_TRANSP_COLOR);
		}
	else 
#if TEXTURE_COMPRESSION
		if (!bmP->bmCompressed) 
#endif
		{
		nDescentCriticalError = 0;
		if (bDefault) {
			temp = (int) cfP->Read (bmP->texBuf, 1, nSize);
			if (bD1)
				bmP->Remap (paletteManager.D1 (), TRANSPARENCY_COLOR, SUPER_TRANSP_COLOR);
			else
				bmP->Remap (paletteManager.Game (), TRANSPARENCY_COLOR, SUPER_TRANSP_COLOR);
			}
		else {
			ReadTGAImage (*cfP, &h, bmP, -1, 1.0, 0, 0);
			if (bmP->Flags () & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT))
				bmP->SetFlags (bmP->Flags () | BM_FLAG_SEE_THRU);
			bmP->SetType (BM_TYPE_ALT);
			if (IsOpaqueDoor (nIndex)) {
				bmP->SetFlags (bmP->Flags () & ~BM_FLAG_TRANSPARENT);
				bmP->transparentFrames [0] &= ~1;
				}
#if TEXTURE_COMPRESSION
			if (CompressTGA (bmP))
				SaveS3TC (bmP, gameFolders.szTextureCacheDir [bD1], bmName);
			else {
#endif
			nBestShrinkFactor = BestShrinkFactor (bmP, nShrinkFactor);
			if ((nBestShrinkFactor > 1) && ShrinkTGA (bmP, nBestShrinkFactor, nBestShrinkFactor, 1)) {
				nSize /= (nBestShrinkFactor * nBestShrinkFactor);
				if (gameStates.app.bCacheTextures)
					SaveTGA (bmName, gameFolders.szTextureCacheDir [bD1], &h, bmP);
				}
			}
#if TEXTURE_COMPRESSION
		}
#endif
		if (nDescentCriticalError) {
			PiggyCriticalError ();
			goto reloadTextures;
			}
		}
#ifndef MACDATA
	if (!bTGA && IsMacDataFile (cfP, bD1))
		swap_0_255 (bmP);
#endif
	StartTime (0);
	}
if (!bDefault)
	cfP->Close ();
return 1;
}

//------------------------------------------------------------------------------

int PiggyBitmapPageIn (int bmi, int bD1)
{
	CBitmap		*bmP;
	int				i, bmiSave;

	//bD1 = gameStates.app.bD1Mission;
bmiSave = 0;
#if 0
Assert (bmi >= 0);
Assert (bmi < MAX_BITMAP_FILES);
Assert (bmi < gameData.pig.tex.nBitmaps [bD1]);
Assert (bitmapCacheSize > 0);
#endif
if (bmi < 1) 
	return 0;
if (bmi >= MAX_BITMAP_FILES) 
	return 0;
if (bmi >= gameData.pig.tex.nBitmaps [bD1]) 
	return 0;
if (bitmapOffsets [bD1][bmi] == 0) 
	return 0;		// A read-from-disk bmi!!!

if (bLowMemory) {
	bmiSave = bmi;
	bmi = gameData.pig.tex.bitmapXlat [bmi];          // Xlat for low-memory settings!
	}
bmP = gameData.pig.tex.bitmaps [bD1][bmi].Override (-1);
while (0 > (i = PageInBitmap (bmP, gameData.pig.tex.bitmapFiles [bD1][bmi].name, bmi, bD1)))
	G3_SLEEP (0);
if (!i)
	return 0;
if (bLowMemory) {
	if (bmiSave != bmi) {
		gameData.pig.tex.bitmaps [bD1][bmiSave] = gameData.pig.tex.bitmaps [bD1][bmi];
		bmi = bmiSave;
		}
	}
gameData.pig.tex.bitmaps [bD1][bmi].props.flags &= ~BM_FLAG_PAGED_OUT;
return 1;
}

//------------------------------------------------------------------------------

int PiggyBitmapExistsSlow (char * name)
{
	int i, j;

	for (i=0, j=gameData.pig.tex.nBitmaps [gameStates.app.bD1Data]; i<j; i++) {
		if (!strcmp (gameData.pig.tex.pBitmapFiles[i].name, name))
			return 1;
	}
	return 0;
}

//------------------------------------------------------------------------------

void LoadBitmapReplacements (const char *pszLevelName)
{
	char		szFilename [SHORT_FILENAME_LEN];
	CFile		cf;
	int		i, j;
	CBitmap	bm;

	//first, D2_FREE up data allocated for old bitmaps
PrintLog ("   loading replacement textures\n");
CFile::ChangeFilenameExtension (szFilename, pszLevelName, ".pog");
if (cf.Open (szFilename, gameFolders.szDataDir, "rb", 0)) {
	int					id, version, nBitmapNum, bTGA;
	int					bmDataSize, bmDataOffset, bmOffset;
	ushort				*indices;
	tPIGBitmapHeader	*bmh;

	id = cf.ReadInt ();
	version = cf.ReadInt ();
	if (id != MAKE_SIG ('G','O','P','D') || version != 1) {
		cf.Close ();
		return;
		}
	nBitmapNum = cf.ReadInt ();
	MALLOC (indices, ushort, nBitmapNum);
	MALLOC (bmh, tPIGBitmapHeader, nBitmapNum);
#if 0
	cf.Read (indices, nBitmapNum * sizeof (ushort), 1);
	cf.Read (bmh, nBitmapNum * sizeof (tPIGBitmapHeader), 1);
#else
	for (i = 0; i < nBitmapNum; i++)
		indices [i] = cf.ReadShort ();
	for (i = 0; i < nBitmapNum; i++)
		PIGBitmapHeaderRead (bmh + i, cf);
#endif
	bmDataOffset = cf.Tell ();
	bmDataSize = cf.Length () - bmDataOffset;

	for (i = 0; i < nBitmapNum; i++) {
		bmOffset = bmh [i].offset;
		memset (&bm, 0, sizeof (CBitmap));
		bm.props.flags |= bmh [i].flags & (BM_FLAGS_TO_COPY | BM_FLAG_TGA);
		bm.props.w = bm.props.rowSize = bmh [i].width + ((short) (bmh [i].wh_extra & 0x0f) << 8);
		if ((bTGA = (bm.props.flags & BM_FLAG_TGA)) && (bm.props.w > 256))
			bm.SetHeight (bm.props.w * bmh [i].height);
		else
			bm.SetHeight (bmh [i].height + ((short) (bmh [i].wh_extra & 0xf0) << 4));
		bm.nBPP = bTGA ? 4 : 1;
		if (!(bm.props.w * bm.props.h))
			continue;
		bm.avgColor = bmh [i].avgColor;
		bm.nType = BM_TYPE_ALT;
		if (bm.CreateTexBuf ())
			break;
		cf.Seek (bmDataOffset + bmOffset, SEEK_SET);
		if (bTGA) {
			int			nFrames = bm.props.h / bm.props.w;
			tTgaHeader	h;

			h.width = bm.props.w;
			h.height = bm.props.h;
			h.bits = 32;
			if (!ReadTGAImage (cf, &h, &bm, -1, 1.0, 0, 1)) {
				D2_FREE (bm.texBuf);
				break;
				}
			bm.SetRowSize (bm.RowSize () * bm.BPP ());
			bm.SetFrameCount ((ubyte) nFrames);
			if (nFrames > 1) {
				tEffectClip	*ecP = NULL;
				tWallClip *wcP;
				tVideoClip *vcP;
				while ((ecP = FindEffect (ecP, indices [i]))) {
					//e->vc.nFrameCount = nFrames;
					ecP->flags |= EF_ALTFMT | EF_FROMPOG;
					}
				if (!ecP) {
					if ((wcP = FindWallAnim (indices [i]))) {
						//w->nFrameCount = nFrames;
						wcP->flags |= WCF_ALTFMT | WCF_FROMPOG;
						}
					else if ((vcP = FindVClip (i))) {
						//v->nFrameCount = nFrames;
						vcP->flags |= WCF_ALTFMT | WCF_FROMPOG;
						}
					}
				}
			j = indices [i];
			bm.nId = j;
			}
		else {
			int nSize = (int) bm.props.w * (int) bm.props.h;
			cf.Read (bm.texBuf, 1, nSize);
			bm.SetPalette (paletteManager.Game ());
			j = indices [i];
			bm.nId = j;
			rle_expand (&bm, NULL, 0);
			bm.props = gameData.pig.tex.pBitmaps [j].props;
			bm.Remap (paletteManager.Game (), TRANSPARENCY_COLOR, SUPER_TRANSP_COLOR);
			}
		PiggyFreeBitmap (NULL, j, 0);
		bm.bFromPog = 1;
		if (*gameData.pig.tex.pBitmaps [j].szName)
			sprintf (bm.szName, "[%s]", gameData.pig.tex.pBitmaps [j].szName);
		else
			sprintf (bm.szName, "POG#%04d", j);
		gameData.pig.tex.pAltBitmaps [j] = bm;
		gameData.pig.tex.pBitmaps [j].SetOverride (gameData.pig.tex.pAltBitmaps + j);
		{
		tRgbColorf	c = {0,0,0};
		CBitmap	*	bmP = gameData.pig.tex.pAltBitmaps + j;

		;
		if (!(c = bmP->AverageRGB (NULL))
			bmP->SetAverageColor (bmP->Palette ()->ClosestColor ((int) c->red, (int) c->green, (int) c->blue));
		}
		UseBitmapCache (gameData.pig.tex.pAltBitmaps + j, (int) bm.props.h * (int) bm.props.rowSize);
		}
	D2_FREE (indices);
	D2_FREE (bmh);
	cf.Close ();
	szLastPalettePig [0] = 0;  //force pig re-load
	TexMergeFlush ();       //for re-merging with new textures
	}
}

//------------------------------------------------------------------------------

void LoadTextureColors (const char *pszLevelName, tFaceColor *colorP)
{
	char			szFilename [SHORT_FILENAME_LEN];
	CFile			cf;
	int			i;

	//first, D2_FREE up data allocated for old bitmaps
PrintLog ("   loading texture colors\n");
CFile::ChangeFilenameExtension (szFilename, pszLevelName, ".clr");
if (cf.Open (szFilename, gameFolders.szDataDir, "rb", 0)) {
	if (!colorP)
		colorP = gameData.render.color.textures;
	for (i = MAX_WALL_TEXTURES; i; i--, colorP++) {
		ReadColor (cf, colorP, 0, 0);
		colorP->index = 0;
		}
	cf.Close ();
	}
}

//------------------------------------------------------------------------------
//eof

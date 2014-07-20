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
#include <stdarg.h>
#include <string.h> // for memset
#ifndef _WIN32_WCE
#include <errno.h>
#endif
#include <ctype.h>      /* for isdigit */
#if defined (__unix__) || defined (__macosx__)
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "descent.h"
#include "u_mem.h"
#include "stdlib.h"
#include "texmap.h"
#include "key.h"
#include "segmath.h"
#include "objrender.h"
#include "physics.h"
#include "slew.h"
#include "rendermine.h"
#include "error.h"
#include "hostage.h"
#include "collide.h"
#include "light.h"
#include "newdemo.h"
#include "cockpit.h"
#include "text.h"
#include "timer.h"
#include "objsmoke.h"
#include "menu.h"
#include "findfile.h"
#include "createmesh.h"
#include "strutil.h"

void SetFunctionMode (int32_t);

void DoJasonInterpolate (fix xRecordedTime);

//#include "nocfile.h"

//Does demo start automatically?

static int8_t	bNDBadRead;
static int32_t		bRevertFormat = -1;

void DemoError (void)
{
PrintLog (0, "Error in demo playback\n");
}

#define	CATCH_BAD_READ				if (bNDBadRead) {DemoError (); bDone = -1; break;}

#define ND_EVENT_EOF                0   // EOF
#define ND_EVENT_START_DEMO         1   // Followed by 16 character, NULL terminated filename of .SAV file to use
#define ND_EVENT_START_FRAME        2   // Followed by integer frame number, then a fix gameData.time.xFrame
#define ND_EVENT_VIEWER_OBJECT      3   // Followed by an CObject structure
#define ND_EVENT_RENDER_OBJECT      4   // Followed by an CObject structure
#define ND_EVENT_SOUND              5   // Followed by int32_t soundum
#define ND_EVENT_SOUND_ONCE         6   // Followed by int32_t soundum
#define ND_EVENT_SOUND_3D           7   // Followed by int32_t soundum, int32_t angle, int32_t volume
#define ND_EVENT_WALL_HIT_PROCESS   8   // Followed by int32_t nSegment, int32_t nSide, fix damage
#define ND_EVENT_TRIGGER            9   // Followed by int32_t nSegment, int32_t nSide, int32_t nObject
#define ND_EVENT_HOSTAGE_RESCUED    10  // Followed by int32_t hostageType
#define ND_EVENT_SOUND_3D_ONCE      11  // Followed by int32_t soundum, int32_t angle, int32_t volume
#define ND_EVENT_MORPH_FRAME        12  // Followed by ? data
#define ND_EVENT_WALL_TOGGLE        13  // Followed by int32_t seg, int32_t nSide
#define ND_EVENT_HUD_MESSAGE        14  // Followed by char size, char * string (+null)
#define ND_EVENT_CONTROL_CENTER_DESTROYED 15 // Just a simple flag
#define ND_EVENT_PALETTE_EFFECT     16  // Followed by int16_t r, g, b
#define ND_EVENT_PLAYER_ENERGY      17  // followed by byte energy
#define ND_EVENT_PLAYER_SHIELD      18  // followed by byte shield
#define ND_EVENT_PLAYER_FLAGS       19  // followed by player flags
#define ND_EVENT_PLAYER_WEAPON      20  // followed by weapon nType and weapon number
#define ND_EVENT_EFFECT_BLOWUP      21  // followed by CSegment, nSide, and pnt
#define ND_EVENT_HOMING_DISTANCE    22  // followed by homing distance
#define ND_EVENT_LETTERBOX          23  // letterbox mode for death playerSyncData.
#define ND_EVENT_RESTORE_COCKPIT    24  // restore cockpit after death
#define ND_EVENT_REARVIEW           25  // going to rear view mode
#define ND_EVENT_WALL_SET_TMAP_NUM1 26  // Wall changed
#define ND_EVENT_WALL_SET_TMAP_NUM2 27  // Wall changed
#define ND_EVENT_NEW_LEVEL          28  // followed by level number
#define ND_EVENT_MULTI_CLOAK        29  // followed by player num
#define ND_EVENT_MULTI_DECLOAK      30  // followed by player num
#define ND_EVENT_RESTORE_REARVIEW   31  // restore cockpit after rearview mode
#define ND_EVENT_MULTI_DEATH        32  // with player number
#define ND_EVENT_MULTI_KILL         33  // with player number
#define ND_EVENT_MULTI_CONNECT      34  // with player number
#define ND_EVENT_MULTI_RECONNECT    35  // with player number
#define ND_EVENT_MULTI_DISCONNECT   36  // with player number
#define ND_EVENT_MULTI_SCORE        37  // playernum / score
#define ND_EVENT_PLAYER_SCORE       38  // followed by score
#define ND_EVENT_PRIMARY_AMMO       39  // with old/new ammo count
#define ND_EVENT_SECONDARY_AMMO     40  // with old/new ammo count
#define ND_EVENT_DOOR_OPENING       41  // with CSegment/nSide
#define ND_EVENT_LASER_LEVEL        42
  // no data
#define ND_EVENT_PLAYER_AFTERBURNER 43  // followed by byte old ab, current ab
#define ND_EVENT_CLOAKING_WALL      44  // info changing while CWall cloaking
#define ND_EVENT_CHANGE_COCKPIT     45  // change the cockpit
#define ND_EVENT_START_GUIDED       46  // switch to guided view
#define ND_EVENT_END_GUIDED         47  // stop guided view/return to ship
#define ND_EVENT_SECRET_THINGY      48  // 0/1 = secret exit functional/non-functional
#define ND_EVENT_LINK_SOUND_TO_OBJ  49  // record audio.CreateObjectSound
#define ND_EVENT_KILL_SOUND_TO_OBJ  50  // record audio.DestroyObjectSound


#define NORMAL_PLAYBACK         0
#define SKIP_PLAYBACK           1
#define INTERPOLATE_PLAYBACK    2
#define INTERPOL_FACTOR         (I2X (1) + (I2X (1)/5))

#define DEMO_VERSION_D2X        20      // last D1 version was 13
#define DEMO_GAME_TYPE          3       // 1 was shareware, 2 registered

#define DEMO_FILENAME           "tmpdemo.dem"

CFile ndInFile;
CFile ndOutFile;

//	-----------------------------------------------------------------------------

int32_t NDErrorMsg (const char *pszMsg1, const char *pszMsg2, const char *pszMsg3)
{
	CMenu	m (3);

m.AddText (pszMsg1, 0);
if (pszMsg2 && *pszMsg2) 
	m.AddText (pszMsg2, 0);
if (pszMsg3 && *pszMsg3)
	m.AddText (pszMsg3, 0);
m.Menu (NULL, NULL);
return 1;
}

//	-----------------------------------------------------------------------------

void InitDemoData (void)
{
gameData.demo.bAuto = 0;
gameData.demo.nState = 0;
gameData.demo.nVcrState = 0;
gameData.demo.nStartFrame = -1;
gameData.demo.bInterpolate = 0; // 1
gameData.demo.bWarningGiven = 0;
gameData.demo.bCtrlcenDestroyed = 0;
gameData.demo.nFrameBytesWritten = 0;
gameData.demo.bFirstTimePlayback = 1;
gameData.demo.xJasonPlaybackTotal = 0;
}

//	-----------------------------------------------------------------------------

float NDGetPercentDone (void) 
{
if (gameData.demo.nState == ND_STATE_PLAYBACK)
	return float (ndInFile.Tell ()) * 100.0f / float (gameData.demo.nSize);
if (gameData.demo.nState == ND_STATE_RECORDING)
	return float (ndOutFile.Tell ());
return 0;
}

//	-----------------------------------------------------------------------------

#define VEL_PRECISION 12

void my_extract_shortpos (CObject *objP, tShortPos *spp)
{
	int32_t nSegment;
	int8_t *sp;

sp = spp->orient;
objP->info.position.mOrient.m.dir.r.v.coord.x = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.u.v.coord.x = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.f.v.coord.x = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.r.v.coord.y = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.u.v.coord.y = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.f.v.coord.y = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.r.v.coord.z = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.u.v.coord.z = *sp++ << MATRIX_PRECISION;
objP->info.position.mOrient.m.dir.f.v.coord.z = *sp++ << MATRIX_PRECISION;
nSegment = spp->nSegment;
objP->info.nSegment = nSegment;
const CFixVector& v = gameData.segs.vertices [SEGMENTS [nSegment].m_vertices [0]];
objP->info.position.vPos.v.coord.x = (spp->pos [0] << RELPOS_PRECISION) + v.v.coord.x;
objP->info.position.vPos.v.coord.y = (spp->pos [1] << RELPOS_PRECISION) + v.v.coord.y;
objP->info.position.vPos.v.coord.z = (spp->pos [2] << RELPOS_PRECISION) + v.v.coord.z;
objP->mType.physInfo.velocity.v.coord.x = (spp->vel [0] << VEL_PRECISION);
objP->mType.physInfo.velocity.v.coord.y = (spp->vel [1] << VEL_PRECISION);
objP->mType.physInfo.velocity.v.coord.z = (spp->vel [2] << VEL_PRECISION);
}

//	-----------------------------------------------------------------------------

int32_t NDFindObject (int32_t nSignature)
{
	int32_t 		i;
	CObject 	*objP = OBJECTS.Buffer ();

FORALL_OBJSi (objP, i)
	if ((objP->info.nType != OBJ_NONE) && (objP->info.nSignature == nSignature))
		return objP->Index ();
return -1;
}

//	-----------------------------------------------------------------------------

#if DBG

void CHK (void)
{
Assert (&ndOutFile.File () != NULL);
if (gameData.demo.nWritten >= 750)
	BRP;
}

#else

#define	CHK()

#endif

//	-----------------------------------------------------------------------------

int32_t NDWrite (void *buffer, int32_t elsize, int32_t nelem)
{
	int32_t nWritten, nTotalSize = elsize * nelem;

gameData.demo.nFrameBytesWritten += nTotalSize;
gameData.demo.nWritten += nTotalSize;
Assert (&ndOutFile.File () != NULL);
CHK();
nWritten = (int32_t) ndOutFile.Write (buffer, elsize, nelem);
if ((bRevertFormat < 1) && (gameData.demo.nWritten > gameData.demo.nSize) && !gameData.demo.bNoSpace)
	gameData.demo.bNoSpace = 1;
if ((nWritten == nelem) && !gameData.demo.bNoSpace)
	return nWritten;
gameData.demo.bNoSpace = 2;
NDStopRecording ();
return -1;
}

/*
 *  The next bunch of files taken from Matt's gamesave.c.  We have to modify
 *  these since the demo must save more information about OBJECTS that
 *  just a gamesave
*/

//	-----------------------------------------------------------------------------

static inline void NDWriteByte (int8_t b)
{
gameData.demo.nFrameBytesWritten += sizeof (b);
gameData.demo.nWritten += sizeof (b);
CHK();
ndOutFile.WriteByte (b);
}

//	-----------------------------------------------------------------------------

static inline void NDWriteShort (int16_t s)
{
gameData.demo.nFrameBytesWritten += sizeof (s);
gameData.demo.nWritten += sizeof (s);
CHK();
ndOutFile.WriteShort (s);
}

//	-----------------------------------------------------------------------------

static void NDWriteInt (int32_t i)
{
gameData.demo.nFrameBytesWritten += sizeof (i);
gameData.demo.nWritten += sizeof (i);
CHK();
ndOutFile.WriteInt (i);
}

//	-----------------------------------------------------------------------------

static inline void NDWriteString (char *str)
{
	int8_t l = (int32_t) strlen (str) + 1;

NDWriteByte (l);
NDWrite (str, (int32_t) l, 1);
}

//	-----------------------------------------------------------------------------

static inline void NDWriteFix (fix f)
{
gameData.demo.nFrameBytesWritten += sizeof (f);
gameData.demo.nWritten += sizeof (f);
CHK();
ndOutFile.WriteFix (f);
}

//	-----------------------------------------------------------------------------

static inline void NDWriteFixAng (fixang f)
{
gameData.demo.nFrameBytesWritten += sizeof (f);
gameData.demo.nWritten += sizeof (f);
ndOutFile.WriteFixAng (f);
}

//	-----------------------------------------------------------------------------

static inline void NDWriteVector (const CFixVector& v)
{
gameData.demo.nFrameBytesWritten += sizeof (v);
gameData.demo.nWritten += sizeof (v);
CHK();
ndOutFile.WriteVector (v);
}

//	-----------------------------------------------------------------------------

static inline void NDWriteAngVec (const CAngleVector& v)
{
gameData.demo.nFrameBytesWritten += sizeof (v);
gameData.demo.nWritten += sizeof (v);
CHK();
ndOutFile.WriteAngVec (v);
}

//	-----------------------------------------------------------------------------

static inline void NDWriteMatrix (const CFixMatrix& m)
{
gameData.demo.nFrameBytesWritten += sizeof (m);
gameData.demo.nWritten += sizeof (m);
CHK();
ndOutFile.WriteMatrix(m);
}

//	-----------------------------------------------------------------------------

void NDWritePosition (CObject *objP)
{
	uint8_t			renderType = objP->info.renderType;
	tShortPos	sp;
	int32_t			bOldFormat = gameStates.app.bNostalgia || gameOpts->demo.bOldFormat || (bRevertFormat > 0);

if (bOldFormat)
	CreateShortPos (&sp, objP, 0);
if ((renderType == RT_POLYOBJ) || (renderType == RT_HOSTAGE) || (renderType == RT_MORPH) || 
	 (objP->info.nType == OBJ_CAMERA) ||
	 (!bOldFormat && (renderType == RT_POWERUP))) {
	if (bOldFormat)
		NDWrite (sp.orient, sizeof (sp.orient [0]), sizeof (sp.orient) / sizeof (sp.orient [0]));
	else
		NDWriteMatrix(objP->info.position.mOrient);
	}
if (bOldFormat) {
	NDWriteShort (sp.pos [0]);
	NDWriteShort (sp.pos [1]);
	NDWriteShort (sp.pos [2]);
	NDWriteShort (sp.nSegment);
	NDWriteShort (sp.vel [0]);
	NDWriteShort (sp.vel [1]);
	NDWriteShort (sp.vel [2]);
	}
else {
	NDWriteVector (objP->info.position.vPos);
	NDWriteShort (objP->info.nSegment);
	NDWriteVector (objP->mType.physInfo.velocity);
	}
}

//	-----------------------------------------------------------------------------

int32_t NDRead (void *buffer, int32_t elsize, int32_t nelem)
{
int32_t nRead = (int32_t) ndInFile.Read (buffer, elsize, nelem);
if (ndInFile.Error () || ndInFile.EoF ())
	bNDBadRead = -1;
else if (bRevertFormat > 0)
	NDWrite (buffer, elsize, nelem);
return nRead;
}

//	-----------------------------------------------------------------------------

static inline uint8_t NDReadByte (void)
{
if (bRevertFormat > 0) {
	uint8_t	h = (int32_t) ndInFile.ReadByte ();
	NDWriteByte (h);
	return h;
	}
return ndInFile.ReadByte ();
}

//	-----------------------------------------------------------------------------

static inline int16_t NDReadShort (void)
{
if (bRevertFormat > 0) {
	int16_t	h = (int32_t) ndInFile.ReadShort ();
	NDWriteShort (h);
	return h;
	}
return ndInFile.ReadShort ();
}

//	-----------------------------------------------------------------------------

static inline int32_t NDReadInt ()
{
if (bRevertFormat > 0) {
	int32_t h = (int32_t) ndInFile.ReadInt ();
	NDWriteInt (h);
	return h;
	}
return ndInFile.ReadInt ();
}

//	-----------------------------------------------------------------------------

static inline char *NDReadString (char *str)
{
	int8_t len;

len = NDReadByte ();
NDRead (str, len, 1);
return str;
}

//	-----------------------------------------------------------------------------

static inline fix NDReadFix (void)
{
if (bRevertFormat > 0) {
	fix h = (int32_t) ndInFile.ReadFix ();
	NDWriteFix (h);
	return h;
	}
return ndInFile.ReadFix ();
}

//	-----------------------------------------------------------------------------

static inline fixang NDReadFixAng (void)
{
if (bRevertFormat > 0) {
	fixang h = (int32_t) ndInFile.ReadFixAng ();
	NDWriteFixAng (h);
	return h;
	}
return ndInFile.ReadFixAng ();
}

//	-----------------------------------------------------------------------------

static inline void NDReadVector (CFixVector& v)
{
ndInFile.ReadVector (v);
if (bRevertFormat > 0)
	NDWriteVector (v);
}

//	-----------------------------------------------------------------------------

static inline void NDReadAngVec (CAngleVector& v)
{
ndInFile.ReadAngVec (v);
if (bRevertFormat > 0)
	NDWriteAngVec (v);
}

//	-----------------------------------------------------------------------------

static inline void NDReadMatrix (CFixMatrix& m)
{
ndInFile.ReadMatrix(m);
if (bRevertFormat > 0)
	NDWriteMatrix (m);
}

//	-----------------------------------------------------------------------------

static void NDReadPosition (CObject *objP, int32_t bSkip)
{
	tShortPos sp;
	uint8_t renderType;

if (bRevertFormat > 0)
	bRevertFormat = 0;	//temporarily suppress writing back data
renderType = objP->info.renderType;
if ((renderType == RT_POLYOBJ) || (renderType == RT_HOSTAGE) || (renderType == RT_MORPH) || 
	 (objP->info.nType == OBJ_CAMERA) ||
	 ((renderType == RT_POWERUP) && (gameData.demo.nVersion > DEMO_VERSION + 1))) {
	if (gameData.demo.bUseShortPos)
		NDRead (sp.orient, sizeof (sp.orient [0]), sizeof (sp.orient) / sizeof (sp.orient [0]));
	else
		ndInFile.ReadMatrix(objP->info.position.mOrient);
	}
if (gameData.demo.bUseShortPos) {
	sp.pos [0] = NDReadShort ();
	sp.pos [1] = NDReadShort ();
	sp.pos [2] = NDReadShort ();
	sp.nSegment = NDReadShort ();
	sp.vel [0] = NDReadShort ();
	sp.vel [1] = NDReadShort ();
	sp.vel [2] = NDReadShort ();
	my_extract_shortpos (objP, &sp);
	}
else {
	NDReadVector (objP->info.position.vPos);
	objP->info.nSegment = NDReadShort ();
	NDReadVector (objP->mType.physInfo.velocity);
	}
if ((objP->info.nId == VCLIP_MORPHING_ROBOT) && 
	 (renderType == RT_FIREBALL) && 
	 (objP->info.controlType == CT_EXPLOSION))
	ExtractOrientFromSegment (&objP->info.position.mOrient, SEGMENTS + objP->info.nSegment);
if (!(bRevertFormat || bSkip)) {
	bRevertFormat = 1;
	NDWritePosition (objP);
	}
}

//	-----------------------------------------------------------------------------

CObject *prevObjP = NULL;      //ptr to last CObject read in

void NDReadObject (CObject *objP)
{
	int32_t		bSkip = 0;

memset (objP, 0, sizeof (CObject));
/*
 * Do render nType first, since with renderType == RT_NONE, we
 * blow by all other CObject information
 */
if (bRevertFormat > 0)
	bRevertFormat = 0;
objP->info.renderType = NDReadByte ();
if (!bRevertFormat) {
	if (objP->info.renderType <= RT_WEAPON_VCLIP) {
		bRevertFormat = gameOpts->demo.bRevertFormat;
		NDWriteByte ((int8_t) objP->info.renderType);
		}
	else {
		bSkip = 1;
		ndOutFile.Seek (-1, SEEK_CUR);
		gameData.demo.nFrameBytesWritten--;
		gameData.demo.nWritten--;
		}
	}
objP->info.nType = NDReadByte ();
if ((objP->info.renderType == RT_NONE) && (objP->info.nType != OBJ_CAMERA)) {
	if (!bRevertFormat)
		bRevertFormat = gameOpts->demo.bRevertFormat;
	return;
	}
objP->info.nId = NDReadByte ();
if (gameData.demo.nVersion > DEMO_VERSION + 1) {
	if (bRevertFormat > 0)
		bRevertFormat = 0;
	objP->SetShield (NDReadFix ());
	if (!(bRevertFormat || bSkip))
		bRevertFormat = gameOpts->demo.bRevertFormat;
	}
objP->info.nFlags = NDReadByte ();
objP->info.nSignature = NDReadShort ();
NDReadPosition (objP, bSkip);
#if DBG
if ((objP->info.nType == OBJ_ROBOT) && (objP->info.nId == SPECIAL_REACTOR_ROBOT))
	Int3 ();
#endif
objP->info.nAttachedObj = -1;
switch (objP->info.nType) {
	case OBJ_HOSTAGE:
		objP->info.controlType = CT_POWERUP;
		objP->info.movementType = MT_NONE;
		objP->SetSize (HOSTAGE_SIZE);
		break;

	case OBJ_ROBOT:
		objP->info.controlType = CT_AI;
		// (MarkA and MikeK said we should not do the crazy last secret stuff with multiple reactors...
		// This necessary code is our vindication. --MK, 2/15/96)
		if (objP->info.nId != SPECIAL_REACTOR_ROBOT)
			objP->info.movementType = MT_PHYSICS;
		else
			objP->info.movementType = MT_NONE;
		objP->rType.polyObjInfo.nModel = ROBOTINFO (objP->info.nId).nModel;
		objP->AdjustSize ();
		objP->rType.polyObjInfo.nSubObjFlags = 0;
		objP->cType.aiInfo.CLOAKED = (ROBOTINFO (objP->info.nId).cloakType?1:0);
		break;

	case OBJ_POWERUP:
		objP->info.controlType = CT_POWERUP;
		objP->info.movementType = NDReadByte ();        // might have physics movement
		objP->SetSizeFromPowerup ();
		break;

	case OBJ_PLAYER:
		objP->info.controlType = CT_NONE;
		objP->info.movementType = MT_PHYSICS;
		objP->rType.polyObjInfo.nModel = gameData.pig.ship.player->nModel;
		objP->AdjustSize ();
		objP->rType.polyObjInfo.nSubObjFlags = 0;
		break;

	case OBJ_CLUTTER:
		objP->info.controlType = CT_NONE;
		objP->info.movementType = MT_NONE;
		objP->rType.polyObjInfo.nModel = objP->info.nId;
		objP->AdjustSize ();
		objP->rType.polyObjInfo.nSubObjFlags = 0;
		break;

	default:
		objP->info.controlType = NDReadByte ();
		objP->info.movementType = NDReadByte ();
		objP->SetSize (NDReadFix ());
		break;
	}

NDReadVector (objP->info.vLastPos);
if ((objP->info.nType == OBJ_WEAPON) && (objP->info.renderType == RT_WEAPON_VCLIP))
	objP->SetLife (NDReadFix ());
else {
	fix lifeLeft = (fix) NDReadByte ();
	objP->SetLife (lifeLeft << 12);
	}
if (objP->info.nType == OBJ_ROBOT) {
	if (ROBOTINFO (objP->info.nId).bossFlag) {
		int8_t cloaked = NDReadByte ();
		objP->cType.aiInfo.CLOAKED = cloaked;
		}
	}

switch (objP->info.movementType) {
	case MT_PHYSICS:
		NDReadVector (objP->mType.physInfo.velocity);
		NDReadVector (objP->mType.physInfo.thrust);
		break;

	case MT_SPINNING:
		NDReadVector (objP->mType.spinRate);
		break;

	case MT_NONE:
		break;

	default:
		Int3 ();
	}

switch (objP->info.controlType) {
	case CT_EXPLOSION:
		objP->cType.explInfo.nSpawnTime = NDReadFix ();
		objP->cType.explInfo.nDeleteTime = NDReadFix ();
		objP->cType.explInfo.nDeleteObj = NDReadShort ();
		objP->cType.explInfo.attached.nNext = 
		objP->cType.explInfo.attached.nPrev = 
		objP->cType.explInfo.attached.nParent = -1;
		if (objP->info.nFlags & OF_ATTACHED) {     //attach to previous CObject
			Assert (prevObjP != NULL);
			if (prevObjP->info.controlType == CT_EXPLOSION) {
				if ((prevObjP->info.nFlags & OF_ATTACHED) && (prevObjP->cType.explInfo.attached.nParent != -1))
					AttachObject (OBJECTS + prevObjP->cType.explInfo.attached.nParent, objP);
				else
					objP->info.nFlags &= ~OF_ATTACHED;
				}
			else
				AttachObject (prevObjP, objP);
			}
		break;

	case CT_LIGHT:
		objP->cType.lightInfo.intensity = NDReadFix ();
		break;

	case CT_AI:
	case CT_WEAPON:
	case CT_NONE:
	case CT_FLYING:
	case CT_DEBRIS:
	case CT_POWERUP:
	case CT_SLEW:
	case CT_CNTRLCEN:
	case CT_REMOTE:
	case CT_MORPH:
		break;

	case CT_FLYTHROUGH:
	case CT_REPAIRCEN:
	default:
		Int3 ();
	}

switch (objP->info.renderType) {
	case RT_NONE:
		break;

	case RT_MORPH:
	case RT_POLYOBJ: {
		int32_t i, tmo;
		if ((objP->info.nType != OBJ_ROBOT) && (objP->info.nType != OBJ_PLAYER) && (objP->info.nType != OBJ_CLUTTER)) {
			objP->rType.polyObjInfo.nModel = NDReadInt ();
			objP->rType.polyObjInfo.nSubObjFlags = NDReadInt ();
			}
		if ((objP->info.nType != OBJ_PLAYER) && (objP->info.nType != OBJ_DEBRIS))
		for (i = 0; i < gameData.models.polyModels [0][objP->ModelId ()].ModelCount (); i++)
			NDReadAngVec (objP->rType.polyObjInfo.animAngles[i]);
		tmo = NDReadInt ();
		objP->rType.polyObjInfo.nTexOverride = tmo;
		break;
		}

	case RT_POWERUP:
	case RT_FIREBALL:
	case RT_HOSTAGE:
	case RT_WEAPON_VCLIP:
	case RT_THRUSTER:
		objP->rType.vClipInfo.nClipIndex = NDReadInt ();
		objP->rType.vClipInfo.xFrameTime = NDReadFix ();
		objP->rType.vClipInfo.nCurFrame = NDReadByte ();
		break;

	case RT_LASER:
		break;

	case RT_SMOKE:
		objP->rType.particleInfo.nLife = NDReadInt ();
		objP->rType.particleInfo.nSize [0] = NDReadInt ();
		objP->rType.particleInfo.nParts = NDReadInt ();
		objP->rType.particleInfo.nSpeed = NDReadInt ();
		objP->rType.particleInfo.nDrift = NDReadInt ();
		objP->rType.particleInfo.nBrightness = NDReadInt ();
		objP->rType.particleInfo.color.Red () = NDReadByte ();
		objP->rType.particleInfo.color.Green () = NDReadByte ();
		objP->rType.particleInfo.color.Blue () = NDReadByte ();
		objP->rType.particleInfo.color.Alpha () = NDReadByte ();
		objP->rType.particleInfo.nSide = NDReadByte ();
		objP->rType.particleInfo.nType = NDReadByte ();
		objP->rType.particleInfo.bEnabled = NDReadByte ();
		break;

	case RT_LIGHTNING:
		objP->rType.lightningInfo.nLife = NDReadInt ();
		objP->rType.lightningInfo.nDelay = NDReadInt ();
		objP->rType.lightningInfo.nLength = NDReadInt ();
		objP->rType.lightningInfo.nAmplitude = NDReadInt ();
		objP->rType.lightningInfo.nOffset = NDReadInt ();
		objP->rType.lightningInfo.nBolts = NDReadShort ();
		objP->rType.lightningInfo.nId = NDReadShort ();
		objP->rType.lightningInfo.nTarget = NDReadShort ();
		objP->rType.lightningInfo.nNodes = NDReadShort ();
		objP->rType.lightningInfo.nChildren = NDReadShort ();
		objP->rType.lightningInfo.nFrames = NDReadShort ();
		objP->rType.lightningInfo.nWidth = (gameData.demo.nVersion <= 19) ? 3 : NDReadByte ();
		objP->rType.lightningInfo.nAngle = NDReadByte ();
		objP->rType.lightningInfo.nStyle = NDReadByte ();
		objP->rType.lightningInfo.nSmoothe = NDReadByte ();
		objP->rType.lightningInfo.bClamp = NDReadByte ();
		objP->rType.lightningInfo.bGlow = NDReadByte ();
		objP->rType.lightningInfo.bSound = NDReadByte ();
		objP->rType.lightningInfo.bRandom = NDReadByte ();
		objP->rType.lightningInfo.bInPlane = NDReadByte ();
		objP->rType.lightningInfo.color.Red () = NDReadByte ();
		objP->rType.lightningInfo.color.Green () = NDReadByte ();
		objP->rType.lightningInfo.color.Blue () = NDReadByte ();
		objP->rType.lightningInfo.color.Alpha () = NDReadByte ();
		objP->rType.lightningInfo.bEnabled = NDReadByte ();
		break;

	case RT_SOUND:
		NDRead (objP->rType.soundInfo.szFilename, 1, sizeof (objP->rType.soundInfo.szFilename));
		objP->rType.soundInfo.szFilename [sizeof (objP->rType.soundInfo.szFilename) - 1] = '\0';
		strlwr (objP->rType.soundInfo.szFilename);
		objP->rType.soundInfo.nVolume = (int32_t) FRound (float (NDReadInt ()) * float (I2X (1)) / 10.0f);
		objP->rType.soundInfo.bEnabled = NDReadByte ();
		break;

	default:
		Int3 ();
	}
prevObjP = objP;
if (!bRevertFormat)
	bRevertFormat = gameOpts->demo.bRevertFormat;
}

//------------------------------------------------------------------------------
//process this powerup for this frame
void NDSetPowerupClip (CObject *objP)
{
//if (gameStates.app.tick40fps.bTick) 
tVideoClipInfo	*vciP = &objP->rType.vClipInfo;
if (vciP->nClipIndex >= 0) {
	tVideoClip	*vcP = gameData.effects.vClips [0] + vciP->nClipIndex;
	vciP->nCurFrame = vcP->xFrameTime ? ((gameData.time.xGame - gameData.demo.xStartTime) / vcP->xFrameTime) % vcP->nFrameCount : 0;
	}
}

//	-----------------------------------------------------------------------------

int32_t NDWriteObject (CObject *objP)
{
	int32_t		life;
	CObject	o = *objP;

if ((o.info.renderType > RT_WEAPON_VCLIP) && ((gameStates.app.bNostalgia || gameOpts->demo.bOldFormat)))
	return 0;
#if DBG
if ((o.info.nType == OBJ_ROBOT) && (o.info.nId == SPECIAL_REACTOR_ROBOT))
	Int3 ();
#endif
if (o.info.nType == OBJ_EFFECT)
	return 0;
if (o.IsStatic ())
	o.info.movementType = MT_PHYSICS;
// Do renderType first so on read, we can make determination of
// what else to read in
if ((o.info.nType == OBJ_POWERUP) && (o.info.renderType == RT_POLYOBJ)) {
	ConvertWeaponToPowerup (&o);
	NDSetPowerupClip (&o);
	}
NDWriteByte (o.info.renderType);
NDWriteByte (o.info.nType);
if ((o.info.renderType == RT_NONE) && (o.info.nType != OBJ_CAMERA))
	return 1;
NDWriteByte (o.info.nId);
if (!(gameStates.app.bNostalgia || gameOpts->demo.bOldFormat))
	NDWriteFix (o.info.xShield);
NDWriteByte (o.info.nFlags);
NDWriteShort ((int16_t) o.info.nSignature);
NDWritePosition (&o);
if ((o.info.nType != OBJ_HOSTAGE) && (o.info.nType != OBJ_ROBOT) && (o.info.nType != OBJ_PLAYER) && 
	 (o.info.nType != OBJ_POWERUP) && (o.info.nType != OBJ_CLUTTER)) {
	NDWriteByte (o.info.controlType);
	NDWriteByte (o.info.movementType);
	NDWriteFix (o.info.xSize);
	}
else if (o.info.nType == OBJ_POWERUP)
	NDWriteByte (o.info.movementType);
NDWriteVector (o.info.vLastPos);
if ((o.info.nType == OBJ_WEAPON) && (o.info.renderType == RT_WEAPON_VCLIP))
	NDWriteFix (o.info.xLifeLeft);
else {
	life = ((int32_t) o.info.xLifeLeft) >> 12;
	if (life > 255)
		life = 255;
	NDWriteByte ((uint8_t)life);
	}
if (o.info.nType == OBJ_ROBOT) {
	if (ROBOTINFO (o.info.nId).bossFlag) {
		int32_t i = gameData.bosses.Find (objP->Index ());
		if ((i >= 0) &&
			 (gameData.time.xGame > gameData.bosses [i].m_nCloakStartTime) && 
			 (gameData.time.xGame < gameData.bosses [i].m_nCloakEndTime))
			NDWriteByte (1);
		else
			NDWriteByte (0);
		}
	}
switch (o.info.movementType) {
	case MT_PHYSICS:
		NDWriteVector (o.mType.physInfo.velocity);
		NDWriteVector (o.mType.physInfo.thrust);
		break;

	case MT_SPINNING:
		NDWriteVector (o.mType.spinRate);
		break;

	case MT_NONE:
		break;

	default:
		Int3 ();
	}

switch (o.info.controlType) {
	case CT_AI:
		break;

	case CT_EXPLOSION:
		NDWriteFix (o.cType.explInfo.nSpawnTime);
		NDWriteFix (o.cType.explInfo.nDeleteTime);
		NDWriteShort (o.cType.explInfo.nDeleteObj);
		break;

	case CT_WEAPON:
		break;

	case CT_LIGHT:
		NDWriteFix (o.cType.lightInfo.intensity);
		break;

	case CT_NONE:
	case CT_FLYING:
	case CT_DEBRIS:
	case CT_POWERUP:
	case CT_SLEW:       //the player is generally saved as slew
	case CT_CNTRLCEN:
	case CT_REMOTE:
	case CT_MORPH:
		break;

	case CT_REPAIRCEN:
	case CT_FLYTHROUGH:
	default:
		Int3 ();
	}

switch (o.info.renderType) {
	case RT_NONE:
		break;

	case RT_MORPH:
	case RT_POLYOBJ: {
		int32_t i;
		if ((o.info.nType != OBJ_ROBOT) && (o.info.nType != OBJ_PLAYER) && (o.info.nType != OBJ_CLUTTER)) {
			NDWriteInt (o.rType.polyObjInfo.nModel);
			NDWriteInt (o.rType.polyObjInfo.nSubObjFlags);
			}
		if ((o.info.nType != OBJ_PLAYER) && (o.info.nType != OBJ_DEBRIS))
#if 0
			for (i = 0; i < MAX_SUBMODELS; i++)
				NDWriteAngVec (o.polyObjInfo.animAngles + i);
#endif
		for (i = 0; i < gameData.models.polyModels [0][o.ModelId ()].ModelCount (); i++)
			NDWriteAngVec (o.rType.polyObjInfo.animAngles[i]);
		NDWriteInt (o.rType.polyObjInfo.nTexOverride);
		break;
		}

	case RT_POWERUP:
	case RT_FIREBALL:
	case RT_HOSTAGE:
	case RT_WEAPON_VCLIP:
	case RT_THRUSTER:
		NDWriteInt (o.rType.vClipInfo.nClipIndex);
		NDWriteFix (o.rType.vClipInfo.xFrameTime);
		NDWriteByte (o.rType.vClipInfo.nCurFrame);
		break;

	case RT_LASER:
		break;

	case RT_SMOKE:
		NDWriteInt (o.rType.particleInfo.nLife);
		NDWriteInt (o.rType.particleInfo.nSize [0]);
		NDWriteInt (o.rType.particleInfo.nParts);
		NDWriteInt (o.rType.particleInfo.nSpeed);
		NDWriteInt (o.rType.particleInfo.nDrift);
		NDWriteInt (o.rType.particleInfo.nBrightness);
		NDWriteByte (o.rType.particleInfo.color.Red ());
		NDWriteByte (o.rType.particleInfo.color.Green ());
		NDWriteByte (o.rType.particleInfo.color.Blue ());
		NDWriteByte (o.rType.particleInfo.color.Alpha ());
		NDWriteByte (o.rType.particleInfo.nSide);
		NDWriteByte (o.rType.particleInfo.nType);
		NDWriteByte (o.rType.particleInfo.bEnabled);
		break;

	case RT_LIGHTNING:
		NDWriteInt (o.rType.lightningInfo.nLife);
		NDWriteInt (o.rType.lightningInfo.nDelay);
		NDWriteInt (o.rType.lightningInfo.nLength);
		NDWriteInt (o.rType.lightningInfo.nAmplitude);
		NDWriteInt (o.rType.lightningInfo.nOffset);
		NDWriteShort (o.rType.lightningInfo.nBolts);
		NDWriteShort (o.rType.lightningInfo.nId);
		NDWriteShort (o.rType.lightningInfo.nTarget);
		NDWriteShort (o.rType.lightningInfo.nNodes);
		NDWriteShort (o.rType.lightningInfo.nChildren);
		NDWriteShort (o.rType.lightningInfo.nFrames);
		NDWriteByte (o.rType.lightningInfo.nWidth);
		NDWriteByte (o.rType.lightningInfo.nAngle);
		NDWriteByte (o.rType.lightningInfo.nStyle);
		NDWriteByte (o.rType.lightningInfo.nSmoothe);
		NDWriteByte (o.rType.lightningInfo.bClamp);
		NDWriteByte (o.rType.lightningInfo.bGlow);
		NDWriteByte (o.rType.lightningInfo.bSound);
		NDWriteByte (o.rType.lightningInfo.bRandom);
		NDWriteByte (o.rType.lightningInfo.bInPlane);
		NDWriteByte (o.rType.lightningInfo.color.Red ());
		NDWriteByte (o.rType.lightningInfo.color.Green ());
		NDWriteByte (o.rType.lightningInfo.color.Blue ());
		NDWriteByte (o.rType.lightningInfo.color.Alpha ());
		NDWriteByte (o.rType.lightningInfo.bEnabled);
		break;

	case RT_SOUND:
		NDWrite (o.rType.soundInfo.szFilename, 1, sizeof (o.rType.soundInfo.szFilename));
		NDWriteInt (int32_t (float (o.rType.soundInfo.nVolume) * 10.f / float (I2X (1))));
		NDWriteByte (o.rType.soundInfo.bEnabled);
		break;

	default:
		Int3 ();
	}
return 1;
}

//	-----------------------------------------------------------------------------

int32_t	bJustStartedRecording = 0, 
		bJustStartedPlayback = 0;

void NDRecordStartDemo (void)
{
	int32_t i;

StopTime ();
NDWriteByte (ND_EVENT_START_DEMO);
NDWriteByte ((gameStates.app.bNostalgia || gameOpts->demo.bOldFormat) ? DEMO_VERSION : DEMO_VERSION_D2X);
NDWriteByte (DEMO_GAME_TYPE);
NDWriteFix (gameData.time.xGame);
if (IsMultiGame)
	NDWriteInt (gameData.app.nGameMode | (N_LOCALPLAYER << 16));
else
	// NOTE LINK TO ABOVE!!!
	NDWriteInt (gameData.app.nGameMode);
if (IsTeamGame) {
	NDWriteByte (int8_t (netGameInfo.m_info.GetTeamVector ()));
	NDWriteString (netGameInfo.m_info.szTeamName [0]);
	NDWriteString (netGameInfo.m_info.szTeamName [1]);
	}
if (IsMultiGame) {
	NDWriteByte (int8_t (N_PLAYERS));
	for (i = 0; i < N_PLAYERS; i++) {
		NDWriteString (PLAYER (i).callsign);
		NDWriteByte (PLAYER (i).connected);
		if (IsCoopGame)
			NDWriteInt (PLAYER (i).score);
		else {
			NDWriteShort ((int16_t)PLAYER (i).netKilledTotal);
			NDWriteShort ((int16_t)PLAYER (i).netKillsTotal);
			}
		}
	} 
else
	// NOTE LINK TO ABOVE!!!
	NDWriteInt (LOCALPLAYER.score);
for (i = 0; i < MAX_PRIMARY_WEAPONS; i++)
	NDWriteShort ((int16_t)LOCALPLAYER.primaryAmmo [i]);
for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
	NDWriteShort ((int16_t)LOCALPLAYER.secondaryAmmo [i]);
NDWriteByte ((int8_t)LOCALPLAYER.LaserLevel ());
//  Support for missions added here
NDWriteString (gameStates.app.szCurrentMissionFile);
NDWriteByte ((int8_t) (X2IR (LOCALPLAYER.Energy ())));
NDWriteByte ((int8_t) (X2IR (LOCALPLAYER.Shield ())));
NDWriteInt (LOCALPLAYER.flags);        // be sure players flags are set
NDWriteByte ((int8_t)gameData.weapons.nPrimary);
NDWriteByte ((int8_t)gameData.weapons.nSecondary);
gameData.demo.nStartFrame = gameData.app.nFrameCount;
bJustStartedRecording = 1;
NDSetNewLevel (missionManager.nCurrentLevel);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordStartFrame (int32_t nFrameNumber, fix xFrameTime)
{
if (gameStates.render.cameras.bActive)
	return;
if (gameData.demo.bNoSpace) {
	NDStopPlayback ();
	return;
	}
StopTime ();
gameData.demo.bWasRecorded.Clear ();
gameData.demo.bViewWasRecorded.Clear ();
memset (gameData.demo.bRenderingWasRecorded, 0, sizeof (gameData.demo.bRenderingWasRecorded));
nFrameNumber -= gameData.demo.nStartFrame;
Assert (nFrameNumber >= 0);
NDWriteByte (ND_EVENT_START_FRAME);
NDWriteShort ((int16_t) (gameData.demo.nFrameBytesWritten - 1));        // from previous frame
gameData.demo.nFrameBytesWritten = 3;
NDWriteInt (nFrameNumber);
NDWriteInt (xFrameTime);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordRenderObject (CObject * objP)
{
if (gameStates.render.cameras.bActive)
	return;
if (gameData.demo.bViewWasRecorded [objP->Index ()])
	return;
//if (obj==&LOCALOBJECT && !gameStates.app.bPlayerIsDead)
//	return;
StopTime ();
NDWriteByte (ND_EVENT_RENDER_OBJECT);
if (!NDWriteObject (objP))
	ndOutFile.Seek (-1, SEEK_CUR);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordViewerObject (CObject * objP)
{
if (gameStates.render.cameras.bActive)
	return;

	int32_t	i = objP->Index ();
	int32_t	h = gameData.demo.bViewWasRecorded [i];

if (h && (h - 1 == gameStates.render.nRenderingType))
	return;
//if (gameData.demo.bWasRecorded [objP->Index ()])
//	return;
if (gameData.demo.bRenderingWasRecorded [gameStates.render.nRenderingType])
	return;
gameData.demo.bViewWasRecorded [i] = gameStates.render.nRenderingType + 1;
gameData.demo.bRenderingWasRecorded [gameStates.render.nRenderingType] = 1;
StopTime ();
NDWriteByte (ND_EVENT_VIEWER_OBJECT);
NDWriteByte (gameStates.render.nRenderingType);
if (!NDWriteObject (objP))
	ndOutFile.Seek (-2, SEEK_CUR);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordSound (int32_t soundno)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_SOUND);
NDWriteInt (soundno);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordCockpitChange (int32_t mode)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_CHANGE_COCKPIT);
NDWriteInt (mode);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordSound3D (int32_t soundno, int32_t angle, int32_t volume)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_SOUND_3D);
NDWriteInt (soundno);
NDWriteInt (angle);
NDWriteInt (volume);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordSound3DOnce (int32_t soundno, int32_t angle, int32_t volume)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_SOUND_3D_ONCE);
NDWriteInt (soundno);
NDWriteInt (angle);
NDWriteInt (volume);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordCreateObjectSound (int32_t soundno, int16_t nObject, fix maxVolume, fix  maxDistance, int32_t loop_start, int32_t loop_end)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_LINK_SOUND_TO_OBJ);
NDWriteInt (soundno);
NDWriteInt (OBJECTS [nObject].info.nSignature);
NDWriteInt (maxVolume);
NDWriteInt (maxDistance);
NDWriteInt (loop_start);
NDWriteInt (loop_end);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordDestroySoundObject (int32_t nObject)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_KILL_SOUND_TO_OBJ);
NDWriteInt (OBJECTS [nObject].info.nSignature);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordWallHitProcess (int32_t nSegment, int32_t nSide, int32_t damage, int32_t playernum)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
//BRP;
//nSide = nSide;
//damage = damage;
//playernum = playernum;
NDWriteByte (ND_EVENT_WALL_HIT_PROCESS);
NDWriteInt (nSegment);
NDWriteInt (nSide);
NDWriteInt (damage);
NDWriteInt (playernum);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordGuidedStart (void)
{
if (gameStates.render.cameras.bActive)
	return;
NDWriteByte (ND_EVENT_START_GUIDED);
}

//	-----------------------------------------------------------------------------

void NDRecordGuidedEnd (void)
{
if (gameStates.render.cameras.bActive)
	return;
NDWriteByte (ND_EVENT_END_GUIDED);
}

//	-----------------------------------------------------------------------------

void NDRecordSecretExitBlown (int32_t truth)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_SECRET_THINGY);
NDWriteInt (truth);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordTrigger (int32_t nSegment, int32_t nSide, int32_t nObject, int32_t bShot)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_TRIGGER);
NDWriteInt (nSegment);
NDWriteInt (nSide);
NDWriteInt (nObject);
NDWriteInt (bShot);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordHostageRescued (int32_t hostage_number) 
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_HOSTAGE_RESCUED);
NDWriteInt (hostage_number);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMorphFrame (tMorphInfo *md)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MORPH_FRAME);
NDWriteObject (md->objP);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordWallToggle (int32_t nSegment, int32_t nSide)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_WALL_TOGGLE);
NDWriteInt (nSegment);
NDWriteInt (nSide);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordControlCenterDestroyed ()
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_CONTROL_CENTER_DESTROYED);
NDWriteInt (gameData.reactor.countdown.nSecsLeft);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordHUDMessage (char * message)
{
StopTime ();
NDWriteByte (ND_EVENT_HUD_MESSAGE);
NDWriteString (message);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPaletteEffect (int16_t r, int16_t g, int16_t b)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PALETTE_EFFECT);
NDWriteShort (r);
NDWriteShort (g);
NDWriteShort (b);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPlayerEnergy (int32_t old_energy, int32_t energy)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PLAYER_ENERGY);
NDWriteByte ((int8_t) old_energy);
NDWriteByte ((int8_t) energy);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPlayerAfterburner (fix old_afterburner, fix afterburner)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PLAYER_AFTERBURNER);
NDWriteByte ((int8_t) (old_afterburner>>9));
NDWriteByte ((int8_t) (afterburner>>9));
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPlayerShield (int32_t old_shield, int32_t shield)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PLAYER_SHIELD);
NDWriteByte ((int8_t)old_shield);
NDWriteByte ((int8_t)shield);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPlayerFlags (uint32_t oflags, uint32_t flags)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PLAYER_FLAGS);
NDWriteInt (( (int16_t)oflags << 16) | (int16_t)flags);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPlayerWeapon (int32_t nWeaponType, int32_t weapon_num)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PLAYER_WEAPON);
NDWriteByte ((int8_t)nWeaponType);
NDWriteByte ((int8_t)weapon_num);
if (nWeaponType)
	NDWriteByte ((int8_t)gameData.weapons.nSecondary);
else
	NDWriteByte ((int8_t)gameData.weapons.nPrimary);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordEffectBlowup (int16_t CSegment, int32_t nSide, CFixVector& vPos)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_EFFECT_BLOWUP);
NDWriteShort (CSegment);
NDWriteByte ((int8_t)nSide);
NDWriteVector (vPos);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordHomingDistance (fix distance)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_HOMING_DISTANCE);
NDWriteShort ((int16_t) (distance>>16));
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordLetterbox (void)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_LETTERBOX);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordRearView (void)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_REARVIEW);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordRestoreCockpit (void)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_RESTORE_COCKPIT);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordRestoreRearView (void)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_RESTORE_REARVIEW);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordWallSetTMapNum1 (int16_t nSegment, uint8_t nSide, int16_t nConnSeg, uint8_t nConnSide, int16_t tmap, int32_t nAnim, int32_t nFrame)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_WALL_SET_TMAP_NUM1);
NDWriteShort (nSegment);
NDWriteByte (nSide);
NDWriteShort (nConnSeg);
NDWriteByte (nConnSide);
if (gameStates.app.bNostalgia || gameOpts->demo.bOldFormat || (bRevertFormat > 0))
	NDWriteShort (tmap);
else {
	NDWriteShort (nAnim);
	NDWriteShort (nFrame);
	}
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordWallSetTMapNum2 (int16_t nSegment, uint8_t nSide, int16_t nConnSeg, uint8_t nConnSide, int16_t tmap, int32_t nAnim, int32_t nFrame)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_WALL_SET_TMAP_NUM2);
NDWriteShort (nSegment);
NDWriteByte (nSide);
NDWriteShort (nConnSeg);
NDWriteByte (nConnSide);
if (gameStates.app.bNostalgia || gameOpts->demo.bOldFormat || (bRevertFormat > 0))
	NDWriteShort (tmap);
else {
	NDWriteShort (nAnim);
	NDWriteShort (nFrame);
	}
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiCloak (int32_t nPlayer)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_CLOAK);
NDWriteByte ((int8_t)nPlayer);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiDeCloak (int32_t nPlayer)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_DECLOAK);
NDWriteByte ((int8_t)nPlayer);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiDeath (int32_t nPlayer)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_DEATH);
NDWriteByte ((int8_t)nPlayer);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiKill (int32_t nPlayer, int8_t kill)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_KILL);
NDWriteByte ((int8_t)nPlayer);
NDWriteByte (kill);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiConnect (int32_t nPlayer, int32_t nNewPlayer, char *pszNewCallsign)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_CONNECT);
NDWriteByte ((int8_t)nPlayer);
NDWriteByte ((int8_t)nNewPlayer);
if (!nNewPlayer) {
	NDWriteString (PLAYER (nPlayer).callsign);
	NDWriteInt (PLAYER (nPlayer).netKilledTotal);
	NDWriteInt (PLAYER (nPlayer).netKillsTotal);
	}
NDWriteString (pszNewCallsign);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiReconnect (int32_t nPlayer)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_RECONNECT);
NDWriteByte ((int8_t)nPlayer);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiDisconnect (int32_t nPlayer)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_DISCONNECT);
NDWriteByte ((int8_t)nPlayer);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPlayerScore (int32_t score)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PLAYER_SCORE);
NDWriteInt (score);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordMultiScore (int32_t nPlayer, int32_t score)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_MULTI_SCORE);
NDWriteByte ((int8_t)nPlayer);
NDWriteInt (score - PLAYER (nPlayer).score);      // called before score is changed!!!!
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordPrimaryAmmo (int32_t nOldAmmo, int32_t nNewAmmo)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_PRIMARY_AMMO);
if (nOldAmmo < 0)
	NDWriteShort ((int16_t)nNewAmmo);
else
	NDWriteShort ((int16_t)nOldAmmo);
NDWriteShort ((int16_t)nNewAmmo);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordSecondaryAmmo (int32_t nOldAmmo, int32_t nNewAmmo)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_SECONDARY_AMMO);
if (nOldAmmo < 0)
	NDWriteShort ((int16_t)nNewAmmo);
else
	NDWriteShort ((int16_t)nOldAmmo);
NDWriteShort ((int16_t)nNewAmmo);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordDoorOpening (int32_t nSegment, int32_t nSide)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_DOOR_OPENING);
NDWriteShort ((int16_t)nSegment);
NDWriteByte ((int8_t)nSide);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordLaserLevel (int8_t oldLevel, int8_t newLevel)
{
if (gameStates.render.cameras.bActive)
	return;
StopTime ();
NDWriteByte (ND_EVENT_LASER_LEVEL);
NDWriteByte (oldLevel);
NDWriteByte (newLevel);
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDRecordCloakingWall (int32_t nFrontWall, int32_t nBackWall, uint8_t nType, uint8_t state, fix cloakValue, fix l0, fix l1, fix l2, fix l3)
{
if (gameStates.render.cameras.bActive)
	return;
Assert (IS_WALL (nFrontWall) && IS_WALL (nBackWall));
StopTime ();
NDWriteByte (ND_EVENT_CLOAKING_WALL);
NDWriteByte ((int8_t) nFrontWall);
NDWriteByte ((int8_t) nBackWall);
NDWriteByte ((int8_t) nType);
NDWriteByte ((int8_t) state);
NDWriteByte ((int8_t) cloakValue);
NDWriteShort ((int16_t) (l0>>8));
NDWriteShort ((int16_t) (l1>>8));
NDWriteShort ((int16_t) (l2>>8));
NDWriteShort ((int16_t) (l3>>8));
StartTime (0);
}

//	-----------------------------------------------------------------------------

void NDSetNewLevel (int32_t level_num)
{
	int32_t i;
	int32_t nSide;
	CSegment *segP;

StopTime ();
NDWriteByte (ND_EVENT_NEW_LEVEL);
NDWriteByte ((int8_t)level_num);
NDWriteByte ((int8_t)missionManager.nCurrentLevel);

if (bJustStartedRecording == 1) {
	NDWriteInt (gameData.walls.nWalls);
	for (i = 0; i < gameData.walls.nWalls; i++) {
		NDWriteByte (WALLS [i].nType);
		if (gameStates.app.bNostalgia || gameOpts->demo.bOldFormat)
			NDWriteByte (uint8_t (WALLS [i].flags));
		else
			NDWriteShort (WALLS [i].flags);
		NDWriteByte (WALLS [i].state);
		segP = &SEGMENTS [WALLS [i].nSegment];
		nSide = WALLS [i].nSide;
		NDWriteShort (segP->m_sides [nSide].m_nBaseTex);
		NDWriteShort (segP->m_sides [nSide].m_nOvlTex | (segP->m_sides [nSide].m_nOvlOrient << 14));
		bJustStartedRecording = 0;
		}
	}
StartTime (0);
}

//	-----------------------------------------------------------------------------

int32_t NDReadDemoStart (int32_t bRandom)
{
	int8_t	i, gameType, laserLevel;
	char	c, energy, shield;
	int32_t	nVersionFilter;
	char	szMsg [128], szCurrentMission [FILENAME_LEN];

c = NDReadByte ();
if ((c != ND_EVENT_START_DEMO) || bNDBadRead) {
	sprintf (szMsg, "%s\n%s", TXT_CANT_PLAYBACK, TXT_DEMO_CORRUPT);
	return NDErrorMsg (szMsg, NULL, NULL);
	}
if (bRevertFormat > 0)
	bRevertFormat = 0;
gameData.demo.nVersion = NDReadByte ();
if (gameOpts->demo.bRevertFormat && !bRevertFormat) {
	if (gameData.demo.nVersion == DEMO_VERSION)
		bRevertFormat = -1;
	else {
		NDWriteByte (DEMO_VERSION);
		bRevertFormat = 1;
		}
	}
gameType = NDReadByte ();
if (gameType < DEMO_GAME_TYPE) {
	sprintf (szMsg, "%s %s", TXT_CANT_PLAYBACK, TXT_RECORDED);
	return NDErrorMsg (szMsg, "    In Descent: First Strike", NULL);
	}
if (gameType != DEMO_GAME_TYPE) {
	sprintf (szMsg, "%s %s", TXT_CANT_PLAYBACK, TXT_RECORDED);
	return NDErrorMsg (szMsg, "   In Unknown Descent version", NULL);
	}
if (gameData.demo.nVersion < DEMO_VERSION) {
	if (bRandom)
		return 1;
	sprintf (szMsg, "%s %s", TXT_CANT_PLAYBACK, TXT_DEMO_OLD);
	return NDErrorMsg (szMsg, NULL, NULL);
	}
gameData.demo.bUseShortPos = (gameData.demo.nVersion == DEMO_VERSION);
gameData.time.xGame = NDReadFix ();
gameData.bosses.ResetCloakTimes ();
gameData.demo.xJasonPlaybackTotal = 0;
gameData.demo.nGameMode = NDReadInt ();
ChangePlayerNumTo ((gameData.demo.nGameMode >> 16) & 0x7);
if (gameData.demo.nGameMode & GM_TEAM) {
	netGameInfo.m_info.SetTeamVector (NDReadByte ());
	NDReadString (netGameInfo.m_info.szTeamName [0]);
	NDReadString (netGameInfo.m_info.szTeamName [1]);
	}
if (gameData.demo.nGameMode & GM_MULTI) {
	MultiNewGame ();
	N_PLAYERS = int32_t (NDReadByte ());
	// changed this to above two lines -- breaks on the mac because of
	// endian issues
	//		NDReadByte (reinterpret_cast<int8_t*> (&N_PLAYERS);
	for (i = 0; i < N_PLAYERS; i++) {
		PLAYER (i).cloakTime = 0;
		PLAYER (i).invulnerableTime = 0;
		NDReadString (PLAYER (i).callsign);
		CONNECT (i, (int8_t) NDReadByte ());
		if (IsCoopGame)
			PLAYER (i).score = NDReadInt ();
		else {
			PLAYER (i).netKilledTotal = NDReadShort ();
			PLAYER (i).netKillsTotal = NDReadShort ();
			}
		}
	gameData.app.SetGameMode (gameData.demo.nGameMode);
	MultiSortKillList ();
	gameData.app.SetGameMode (GM_NORMAL);
	}
else
	LOCALPLAYER.score = NDReadInt ();      // Note link to above if!
for (i = 0; i < MAX_PRIMARY_WEAPONS; i++)
	LOCALPLAYER.primaryAmmo [i] = NDReadShort ();
for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
	LOCALPLAYER.secondaryAmmo [i] = NDReadShort ();
laserLevel = NDReadByte ();
if (laserLevel != LOCALPLAYER.LaserLevel ()) {
	LOCALPLAYER.ComputeLaserLevels (laserLevel);
	cockpit->UpdateLaserWeaponInfo ();
	}
// Support for missions
NDReadString (szCurrentMission);
nVersionFilter = gameOpts->app.nVersionFilter;
gameOpts->app.nVersionFilter = 3;	//make sure mission will be loaded
i = missionManager.LoadByName (szCurrentMission, -1);
gameOpts->app.nVersionFilter = nVersionFilter;
if (!i) {
	if (bRandom)
		return 1;
	sprintf (szMsg, TXT_NOMISSION4DEMO, szCurrentMission);
	return NDErrorMsg (szMsg, NULL, NULL);
	}
gameData.demo.xRecordedTotal = 0;
gameData.demo.xPlaybackTotal = 0;
energy = NDReadByte ();
shield = NDReadByte ();
LOCALPLAYER.flags = NDReadInt ();
if (LOCALPLAYER.flags & PLAYER_FLAGS_CLOAKED) {
	LOCALPLAYER.cloakTime = gameData.time.xGame - (CLOAK_TIME_MAX / 2);
	gameData.demo.bPlayersCloaked |= (1 << N_LOCALPLAYER);
	}
if (LOCALPLAYER.flags & PLAYER_FLAGS_INVULNERABLE)
	LOCALPLAYER.invulnerableTime = gameData.time.xGame - (INVULNERABLE_TIME_MAX / 2);
gameData.weapons.nPrimary = NDReadByte ();
gameData.weapons.nSecondary = NDReadByte ();
// Next bit of code to fix problem that I introduced between 1.0 and 1.1
// check the next byte -- it _will_ be a load_newLevel event.  If it is
// not, then we must shift all bytes up by one.
LOCALPLAYER.SetEnergy (I2X (energy));
LOCALPLAYER.SetShield (I2X (shield));
bJustStartedPlayback = 1;
return 0;
}

//	-----------------------------------------------------------------------------

void NDPopCtrlCenTriggers ()
{
	int16_t		anim_num, n, i;
	int16_t		side, nConnSide;
	CSegment *segP, *connSegP;

for (i = 0; i < gameData.reactor.triggers.m_nLinks; i++) {
	segP = SEGMENTS + gameData.reactor.triggers.m_segments [i];
	side = gameData.reactor.triggers.m_sides [i];
	connSegP = SEGMENTS + segP->m_children [side];
	nConnSide = segP->ConnectedSide (connSegP);
	anim_num = segP->Wall (side)->nClip;
	n = gameData.walls.animP [anim_num].nFrameCount;
	if (gameData.walls.animP [anim_num].flags & WCF_TMAP1)
		segP->m_sides [side].m_nBaseTex = 
		connSegP->m_sides [nConnSide].m_nBaseTex = gameData.walls.animP [anim_num].frames [n-1];
	else
		segP->m_sides [side].m_nOvlTex = 
		connSegP->m_sides [nConnSide].m_nOvlTex = gameData.walls.animP [anim_num].frames [n-1];
	}
}

//	-----------------------------------------------------------------------------

int32_t NDUpdateSmoke (void)
{
if (!EGI_FLAG (bUseParticles, 0, 1, 0))
	return 0;
else {
		int32_t					nObject;
		CParticleSystem*	systemP;

	int32_t nCurrent = -1;
	for (systemP = particleManager.GetFirst (nCurrent); systemP; systemP = particleManager.GetNext (nCurrent)) {
		nObject = NDFindObject (systemP->m_nSignature);
		if (nObject < 0) {
			particleManager.SetObjectSystem (systemP->m_nObject, -1);
			systemP->SetLife (0);
			}
		else {
			particleManager.SetObjectSystem (nObject, systemP->Id ());
			systemP->m_nObject = nObject;
			}
		}
	return 1;
	}
}

//	-----------------------------------------------------------------------------

void NDRenderExtras (uint8_t, CObject *); 
void MultiApplyGoalTextures ();

int32_t NDReadFrameInfo (void)
{
	int32_t bDone, nSegment, nTexture, nSide, nObject, soundno, angle, volume, i, bShot;
	CObject *objP;
	uint8_t nTag, WhichWindow;
	static int8_t saved_letter_cockpit;
	static int8_t saved_rearview_cockpit;
	CObject extraobj;
	CSegment *segP;

bDone = 0;
nTag = 255;
#if 0
for (int32_t nObject = 1; nObject < gameData.objs.nLastObject [0]; nObject++)
	if ((OBJECTS [nObject].info.nType != OBJ_NONE) && (OBJECTS [nObject].info.nType != OBJ_EFFECT))
		ReleaseObject (nObject);
#else
if (gameData.demo.nVcrState != ND_STATE_PAUSED)
	ResetSegObjLists ();
ResetObjects (1);
#endif
/*
cameraManager.Destroy ();
cameraManager.Create ();
*/
LOCALPLAYER.homingObjectDist = -I2X (1);
prevObjP = NULL;
while (!bDone) {
	nTag = NDReadByte ();
	CATCH_BAD_READ
	switch (nTag) {
		case ND_EVENT_START_FRAME: {        // Followed by an integer frame number, then a fix gameData.time.xFrame
			bDone = 1;
			if (bRevertFormat > 0)
				bRevertFormat = 0;
			NDReadShort ();
			if (!bRevertFormat) {
				NDWriteShort (gameData.demo.nFrameBytesWritten - 1);
				bRevertFormat = 1;
				}
			if (bRevertFormat > 0)
				gameData.demo.nFrameBytesWritten = 3;
			gameData.demo.nFrameCount = NDReadInt ();
			gameData.demo.xRecordedTime = NDReadInt ();
			if (gameData.demo.nVcrState == ND_STATE_PLAYBACK)
				gameData.demo.xRecordedTotal += gameData.demo.xRecordedTime;
			gameData.demo.nFrameCount--;
			CATCH_BAD_READ
			break;
			}

		case ND_EVENT_VIEWER_OBJECT:        // Followed by an CObject structure
			WhichWindow = NDReadByte ();
			if (WhichWindow&15) {
				NDReadObject (&extraobj);
				if (gameData.demo.nVcrState != ND_STATE_PAUSED) {
					CATCH_BAD_READ
					NDRenderExtras (WhichWindow, &extraobj);
					}
				}
			else {
				gameData.objs.viewerP = OBJECTS.Buffer ();
				NDReadObject (gameData.objs.viewerP);
				if (gameData.demo.nVcrState != ND_STATE_PAUSED) {
					CATCH_BAD_READ
					nSegment = gameData.objs.viewerP->info.nSegment;
					gameData.objs.viewerP->info.nNextInSeg = 
					gameData.objs.viewerP->info.nPrevInSeg = 
					gameData.objs.viewerP->info.nSegment = -1;

					// HACK HACK HACK -- since we have multiple level recording, it can be the case
					// HACK HACK HACK -- that when rewinding the demo, the viewer is in a CSegment
					// HACK HACK HACK -- that is greater than the highest index of segments.  Bash
					// HACK HACK HACK -- the viewer to CSegment 0 for bogus view.

					if (nSegment > gameData.segs.nLastSegment)
						nSegment = 0;
					gameData.objs.viewerP->LinkToSeg (nSegment);
					}
				}
			break;

		case ND_EVENT_RENDER_OBJECT:       // Followed by an CObject structure
			nObject = AllocObject ();
			if (nObject == -1)
				break;
#if DBG
			if (nObject == nDbgObj)
				BRP;
#endif
			objP = OBJECTS + nObject;
			NDReadObject (objP);
			CATCH_BAD_READ
			if (objP->info.controlType == CT_POWERUP)
				objP->DoPowerupFrame ();
			if (gameData.demo.nVcrState != ND_STATE_PAUSED) {
				nSegment = objP->info.nSegment;
				objP->info.nNextInSeg = objP->info.nPrevInSeg = objP->info.nSegment = -1;
				// HACK HACK HACK -- don't render OBJECTS is segments greater than gameData.segs.nLastSegment
				// HACK HACK HACK -- (see above)
				if (nSegment > gameData.segs.nLastSegment)
					break;
				objP->LinkToSeg (nSegment);
				if ((objP->info.nType == OBJ_PLAYER) && (gameData.demo.nGameMode & GM_MULTI)) {
					int32_t nPlayer = (gameData.demo.nGameMode & GM_TEAM) ? GetTeam (objP->info.nId) : (objP->info.nId % MAX_PLAYER_COLORS) + 1;
					if (nPlayer == 0)
						break;
					nPlayer--;
					for (i = 0; i < N_PLAYER_SHIP_TEXTURES; i++)
						mpTextureIndex [nPlayer][i] = gameData.pig.tex.objBmIndex [gameData.pig.tex.objBmIndexP [gameData.models.polyModels [0][objP->ModelId ()].FirstTexture () + i]];
					mpTextureIndex [nPlayer][4] = gameData.pig.tex.objBmIndex [gameData.pig.tex.objBmIndexP [gameData.pig.tex.nFirstMultiBitmap + nPlayer * 2]];
					mpTextureIndex [nPlayer][5] = gameData.pig.tex.objBmIndex [gameData.pig.tex.objBmIndexP [gameData.pig.tex.nFirstMultiBitmap + nPlayer * 2 + 1]];
					objP->rType.polyObjInfo.nAltTextures = nPlayer+1;
					}
				}
			break;

		case ND_EVENT_SOUND:
			soundno = NDReadInt ();
			CATCH_BAD_READ
			if (gameData.demo.nVcrState == ND_STATE_PLAYBACK)
				audio.PlaySound ((int16_t) soundno);
			break;

			//--unused		case ND_EVENT_SOUND_ONCE:
			//--unused			NDReadInt (&soundno);
			//--unused			if (bNDBadRead) { bDone = -1; break; }
			//--unused			if (gameData.demo.nVcrState == ND_STATE_PLAYBACK)
			//--unused				audio.PlaySound (soundno);
			//--unused			break;

		case ND_EVENT_SOUND_3D:
			soundno = NDReadInt ();
			angle = NDReadInt ();
			volume = NDReadInt ();
			CATCH_BAD_READ
			if (gameData.demo.nVcrState == ND_STATE_PLAYBACK)
				audio.PlaySound ((int16_t) soundno, SOUNDCLASS_GENERIC, volume, angle);
			break;

		case ND_EVENT_SOUND_3D_ONCE:
			soundno = NDReadInt ();
			angle = NDReadInt ();
			volume = NDReadInt ();
			CATCH_BAD_READ
			if (gameData.demo.nVcrState == ND_STATE_PLAYBACK)
				audio.PlaySound ((int16_t) soundno, SOUNDCLASS_GENERIC, volume, angle, 1);
			break;

		case ND_EVENT_LINK_SOUND_TO_OBJ: {
				int32_t soundno, nObject, maxVolume, maxDistance, loop_start, loop_end;
				int32_t nSignature;

			soundno = NDReadInt ();
			nSignature = NDReadInt ();
			maxVolume = NDReadInt ();
			maxDistance = NDReadInt ();
			loop_start = NDReadInt ();
			loop_end = NDReadInt ();
			nObject = NDFindObject (nSignature);
			if (nObject > -1)   //  @mk, 2/22/96, John told me to.
				audio.CreateObjectSound ((int16_t) soundno, OBJECTS [nObject].SoundClass (), (int16_t) nObject, 1, maxVolume, maxDistance, loop_start, loop_end);
			}
			break;

		case ND_EVENT_KILL_SOUND_TO_OBJ: {
				int32_t nObject, nSignature;

			nSignature = NDReadInt ();
			nObject = NDFindObject (nSignature);
			if (nObject > -1)   //  @mk, 2/22/96, John told me to.
				audio.DestroyObjectSound (nObject);
			}
			break;

		case ND_EVENT_WALL_HIT_PROCESS: {
				int32_t player, nSegment;
				fix damage;

			nSegment = NDReadInt ();
			nSide = NDReadInt ();
			damage = NDReadFix ();
			player = NDReadInt ();
			CATCH_BAD_READ
			if (gameData.demo.nVcrState != ND_STATE_PAUSED)
				SEGMENTS [nSegment].ProcessWallHit ((int16_t) nSide, damage, player, &(OBJECTS [0]));
			break;
		}

		case ND_EVENT_TRIGGER:
			nSegment = NDReadInt ();
			nSide = NDReadInt ();
			nObject = NDReadInt ();
			bShot = NDReadInt ();
			CATCH_BAD_READ
			if (gameData.demo.nVcrState != ND_STATE_PAUSED) {
				CSegment*	segP = SEGMENTS + nSegment;
				CTrigger*	trigP = segP->Trigger (nSide);
				if (trigP && (trigP->m_info.nType == TT_SECRET_EXIT)) {
					int32_t truth;

					nTag = NDReadByte ();
					Assert (nTag == ND_EVENT_SECRET_THINGY);
					truth = NDReadInt ();
					if (!truth)
						segP->OperateTrigger ((int16_t) nSide, OBJECTS + nObject, bShot);
					} 
				else
					segP->OperateTrigger ((int16_t) nSide, OBJECTS + nObject, bShot);
				}
			break;

		case ND_EVENT_HOSTAGE_RESCUED: {
				int32_t hostage_number;

			hostage_number = NDReadInt ();
			CATCH_BAD_READ
			if (gameData.demo.nVcrState != ND_STATE_PAUSED)
				RescueHostage (hostage_number);
			}
			break;

		case ND_EVENT_MORPH_FRAME: {
			nObject = AllocObject ();
			if (nObject==-1)
				break;
			objP = OBJECTS + nObject;
			NDReadObject (objP);
			objP->info.renderType = RT_POLYOBJ;
			if (gameData.demo.nVcrState != ND_STATE_PAUSED) {
				CATCH_BAD_READ
				if (gameData.demo.nVcrState != ND_STATE_PAUSED) {
					nSegment = objP->info.nSegment;
					objP->info.nNextInSeg = objP->info.nPrevInSeg = objP->info.nSegment = -1;
					objP->LinkToSeg (nSegment);
					}
				}
			}
			break;

		case ND_EVENT_WALL_TOGGLE:
			nSegment = NDReadInt ();
			nSide = NDReadInt ();
			CATCH_BAD_READ
			if (gameData.demo.nVcrState != ND_STATE_PAUSED)
				SEGMENTS [nSegment].ToggleWall (nSide);
			break;

		case ND_EVENT_CONTROL_CENTER_DESTROYED:
			gameData.reactor.countdown.nSecsLeft = NDReadInt ();
			gameData.reactor.bDestroyed = 1;
			CATCH_BAD_READ
			if (!gameData.demo.bCtrlcenDestroyed) {
				NDPopCtrlCenTriggers ();
				gameData.demo.bCtrlcenDestroyed = 1;
				//DoReactorDestroyedStuff (NULL);
				}
			break;

		case ND_EVENT_HUD_MESSAGE: {
			char hud_msg [60];

			NDReadString (hud_msg);
			CATCH_BAD_READ
			HUDInitMessage (hud_msg);
			}
			break;

		case ND_EVENT_START_GUIDED:
			gameData.demo.bFlyingGuided=1;
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				gameData.demo.bFlyingGuided = 0;
			break;

		case ND_EVENT_END_GUIDED:
			gameData.demo.bFlyingGuided=0;
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				gameData.demo.bFlyingGuided = 1;
			break;

		case ND_EVENT_PALETTE_EFFECT: {
			int16_t r, g, b;

			r = NDReadShort ();
			g = NDReadShort ();
			b = NDReadShort ();
			CATCH_BAD_READ
			paletteManager.SetEffect (r, g, b);
			paletteManager.SetFadeDelay (I2X (1));
			}
			break;

		case ND_EVENT_PLAYER_ENERGY: {
			uint8_t energy;
			uint8_t old_energy;

			old_energy = NDReadByte ();
			energy = NDReadByte ();
			CATCH_BAD_READ
			if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
				 (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				LOCALPLAYER.SetEnergy (I2X (energy));
			else if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				if (old_energy != 255)
					LOCALPLAYER.SetEnergy (I2X (old_energy));
				}
			}
			break;

		case ND_EVENT_PLAYER_AFTERBURNER: {
			uint8_t afterburner;
			uint8_t old_afterburner;

			old_afterburner = NDReadByte ();
			afterburner = NDReadByte ();
			CATCH_BAD_READ
			if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
				 (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				gameData.physics.xAfterburnerCharge = afterburner<<9;
			else if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				if (old_afterburner != 255)
					gameData.physics.xAfterburnerCharge = old_afterburner<<9;
				}
			break;
		}

		case ND_EVENT_PLAYER_SHIELD: {
			uint8_t shield;
			uint8_t old_shield;

			old_shield = NDReadByte ();
			shield = NDReadByte ();
			CATCH_BAD_READ
			if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
				 (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				LOCALPLAYER.SetShield (I2X (shield));
			else if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				if (old_shield != 255)
					LOCALPLAYER.SetShield (I2X (old_shield));
				}
			}
			break;

		case ND_EVENT_PLAYER_FLAGS: {
			uint32_t oflags;

			LOCALPLAYER.flags = NDReadInt ();
			CATCH_BAD_READ
			oflags = LOCALPLAYER.flags >> 16;
			LOCALPLAYER.flags &= 0xffff;

			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 ((gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD) && (oflags != 0xffff))) {
				if (!(oflags & PLAYER_FLAGS_CLOAKED) && (LOCALPLAYER.flags & PLAYER_FLAGS_CLOAKED)) {
					LOCALPLAYER.cloakTime = 0;
					gameData.demo.bPlayersCloaked &= ~ (1 << N_LOCALPLAYER);
					}
				if ((oflags & PLAYER_FLAGS_CLOAKED) && !(LOCALPLAYER.flags & PLAYER_FLAGS_CLOAKED)) {
					LOCALPLAYER.cloakTime = gameData.time.xGame - (CLOAK_TIME_MAX / 2);
					gameData.demo.bPlayersCloaked |= (1 << N_LOCALPLAYER);
					}
				if (!(oflags & PLAYER_FLAGS_INVULNERABLE) && (LOCALPLAYER.flags & PLAYER_FLAGS_INVULNERABLE))
					LOCALPLAYER.invulnerableTime = 0;
				if ((oflags & PLAYER_FLAGS_INVULNERABLE) && !(LOCALPLAYER.flags & PLAYER_FLAGS_INVULNERABLE))
					LOCALPLAYER.invulnerableTime = gameData.time.xGame - (INVULNERABLE_TIME_MAX / 2);
				LOCALPLAYER.flags = oflags;
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				if (!(oflags & PLAYER_FLAGS_CLOAKED) && (LOCALPLAYER.flags & PLAYER_FLAGS_CLOAKED)) {
					LOCALPLAYER.cloakTime = gameData.time.xGame - (CLOAK_TIME_MAX / 2);
					gameData.demo.bPlayersCloaked |= (1 << N_LOCALPLAYER);
					}
				if ((oflags & PLAYER_FLAGS_CLOAKED) && !(LOCALPLAYER.flags & PLAYER_FLAGS_CLOAKED)) {
					LOCALPLAYER.cloakTime = 0;
					gameData.demo.bPlayersCloaked &= ~ (1 << N_LOCALPLAYER);
					}
				if (!(oflags & PLAYER_FLAGS_INVULNERABLE) && (LOCALPLAYER.flags & PLAYER_FLAGS_INVULNERABLE))
					LOCALPLAYER.invulnerableTime = gameData.time.xGame - (INVULNERABLE_TIME_MAX / 2);
				if ((oflags & PLAYER_FLAGS_INVULNERABLE) && !(LOCALPLAYER.flags & PLAYER_FLAGS_INVULNERABLE))
					LOCALPLAYER.invulnerableTime = 0;
				}
			cockpit->UpdateLaserWeaponInfo ();     // in case of quad laser change
			}
			break;

		case ND_EVENT_PLAYER_WEAPON: {
			int8_t nWeaponType, weapon_num;
			int8_t old_weapon;

			nWeaponType = NDReadByte ();
			weapon_num = NDReadByte ();
			old_weapon = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
				 (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				if (nWeaponType == 0)
					gameData.weapons.nPrimary = (int32_t)weapon_num;
				else
					gameData.weapons.nSecondary = (int32_t)weapon_num;
				}
			else if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				if (nWeaponType == 0)
					gameData.weapons.nPrimary = (int32_t)old_weapon;
				else
					gameData.weapons.nSecondary = (int32_t)old_weapon;
				}
			}
			break;

		case ND_EVENT_EFFECT_BLOWUP: {
			int16_t nSegment;
			int8_t nSide;
			CFixVector pnt;
			CObject dummy;

			//create a dummy CObject which will be the weapon that hits
			//the monitor. the blowup code wants to know who the parent of the
			//laser is, so create a laser whose parent is the player
			dummy.cType.laserInfo.parent.nType = OBJ_PLAYER;
			nSegment = NDReadShort ();
			nSide = NDReadByte ();
			NDReadVector (pnt);
			if (gameData.demo.nVcrState != ND_STATE_PAUSED)
				SEGMENTS [nSegment].BlowupTexture (nSide, pnt, &dummy, 0);
			}
			break;

		case ND_EVENT_HOMING_DISTANCE: {
			int16_t distance;

			distance = NDReadShort ();
			LOCALPLAYER.homingObjectDist = 
				I2X ((int32_t) distance << 16);
			}
			break;

		case ND_EVENT_LETTERBOX:
			if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
				 (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				saved_letter_cockpit = gameStates.render.cockpit.nType;
				cockpit->Activate (CM_LETTERBOX);
				}
			else if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				cockpit->Activate (saved_letter_cockpit);
			break;

		case ND_EVENT_CHANGE_COCKPIT: {
				int32_t dummy;

			dummy = NDReadInt ();
			cockpit->Activate (dummy);
			}
			break;

		case ND_EVENT_REARVIEW:
			if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
				 (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				saved_rearview_cockpit = gameStates.render.cockpit.nType;
				if (gameStates.render.cockpit.nType == CM_FULL_COCKPIT)
					cockpit->Activate (CM_REAR_VIEW);
				gameStates.render.bRearView=1;
				}
			else if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				if (saved_rearview_cockpit == CM_REAR_VIEW)     // hack to be sure we get a good cockpit on restore
					saved_rearview_cockpit = CM_FULL_COCKPIT;
				cockpit->Activate (saved_rearview_cockpit);
				gameStates.render.bRearView = 0;
				}
			break;

		case ND_EVENT_RESTORE_COCKPIT:
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				saved_letter_cockpit = gameStates.render.cockpit.nType;
				cockpit->Activate (CM_LETTERBOX);
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				cockpit->Activate (saved_letter_cockpit);
			break;


		case ND_EVENT_RESTORE_REARVIEW:
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				saved_rearview_cockpit = gameStates.render.cockpit.nType;
				if (gameStates.render.cockpit.nType == CM_FULL_COCKPIT)
					cockpit->Activate (CM_REAR_VIEW);
				gameStates.render.bRearView=1;
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				if (saved_rearview_cockpit == CM_REAR_VIEW)     // hack to be sure we get a good cockpit on restore
					saved_rearview_cockpit = CM_FULL_COCKPIT;
				cockpit->Activate (saved_rearview_cockpit);
				gameStates.render.bRearView=0;
				}
			break;


		case ND_EVENT_WALL_SET_TMAP_NUM1: {
			int16_t nSegment, nConnSeg, tmap = 0;
			uint8_t nSide, nConnSide;
			int16_t nAnim = 0, nFrame = 0;

			nSegment = NDReadShort ();
			nSide = NDReadByte ();
			nConnSeg = NDReadShort ();
			nConnSide = NDReadByte ();
			if (gameData.demo.nVersion < 18)
				tmap = NDReadShort ();
			else {
				nAnim = NDReadShort ();
				nFrame = NDReadShort ();
				}
			if ((nConnSeg >= 0) &&
				 (gameData.demo.nVcrState != ND_STATE_PAUSED) && 
				 (gameData.demo.nVcrState != ND_STATE_REWINDING) &&
				 (gameData.demo.nVcrState != ND_STATE_ONEFRAMEBACKWARD)) {
				if (gameData.demo.nVersion >= 18)
					SEGMENTS [nSegment].SetTexture (nSide, SEGMENTS + nConnSeg, nConnSide, nAnim, nFrame);
				else
					SEGMENTS [nSegment].m_sides [nSide].m_nBaseTex = SEGMENTS [nConnSeg].m_sides [nConnSide].m_nBaseTex = tmap;
				}
			}
			break;

		case ND_EVENT_WALL_SET_TMAP_NUM2: {
			int16_t nSegment, nConnSeg, tmap = 0;
			uint8_t nSide, nConnSide;
			int32_t nAnim = 0, nFrame = 0;

			nSegment = NDReadShort ();
			nSide = NDReadByte ();
			nConnSeg = NDReadShort ();
			nConnSide = NDReadByte ();
			if (gameData.demo.nVersion < 18)
				tmap = NDReadShort ();
			else {
				nAnim = NDReadShort ();
				nFrame = NDReadShort ();
				}
			if ((gameData.demo.nVcrState != ND_STATE_PAUSED) &&
				 (gameData.demo.nVcrState != ND_STATE_REWINDING) &&
				 (gameData.demo.nVcrState != ND_STATE_ONEFRAMEBACKWARD)) {
				Assert (tmap!=0 && SEGMENTS [nSegment].m_sides [nSide].m_nOvlTex!=0);
				if (gameData.demo.nVersion >= 18)
					SEGMENTS [nSegment].SetTexture (nSide, SEGMENTS + nConnSeg, nConnSide, nAnim, nFrame);
				else {
					SEGMENTS [nSegment].m_sides [nSide].m_nOvlTex = tmap & 0x3fff;
					SEGMENTS [nSegment].m_sides [nSide].m_nOvlOrient = (tmap >> 14) & 3;
					if (nConnSide < 6) {
						SEGMENTS [nConnSeg].m_sides [nConnSide].m_nOvlTex = tmap & 0x3fff;
						SEGMENTS [nConnSeg].m_sides [nConnSide].m_nOvlOrient = (tmap >> 14) & 3;
						}
					}
				}
			}
			break;

		case ND_EVENT_MULTI_CLOAK: {
			int8_t nPlayer = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				PLAYER (nPlayer).flags &= ~PLAYER_FLAGS_CLOAKED;
				PLAYER (nPlayer).cloakTime = 0;
				gameData.demo.bPlayersCloaked &= ~(1 << nPlayer);
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				PLAYER (nPlayer).flags |= PLAYER_FLAGS_CLOAKED;
				PLAYER (nPlayer).cloakTime = gameData.time.xGame  - (CLOAK_TIME_MAX / 2);
				gameData.demo.bPlayersCloaked |= (1 << nPlayer);
				}
			}
			break;

		case ND_EVENT_MULTI_DECLOAK: {
			int8_t nPlayer = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				PLAYER (nPlayer).flags |= PLAYER_FLAGS_CLOAKED;
				PLAYER (nPlayer).cloakTime = gameData.time.xGame  - (CLOAK_TIME_MAX / 2);
				gameData.demo.bPlayersCloaked |= (1 << nPlayer);
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				PLAYER (nPlayer).flags &= ~PLAYER_FLAGS_CLOAKED;
				PLAYER (nPlayer).cloakTime = 0;
				gameData.demo.bPlayersCloaked &= ~(1 << nPlayer);
				}
			}
			break;

		case ND_EVENT_MULTI_DEATH: {
			int8_t nPlayer = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				PLAYER (nPlayer).netKilledTotal--;
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				PLAYER (nPlayer).netKilledTotal++;
			}
			break;

		case ND_EVENT_MULTI_KILL: {
			int8_t nPlayer = NDReadByte ();
			int8_t kill = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				PLAYER (nPlayer).netKillsTotal -= kill;
				if (gameData.demo.nGameMode & GM_TEAM)
					gameData.multigame.score.nTeam [GetTeam (nPlayer)] -= kill;
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				PLAYER (nPlayer).netKillsTotal += kill;
				if (gameData.demo.nGameMode & GM_TEAM)
					gameData.multigame.score.nTeam [GetTeam (nPlayer)] += kill;
				}
			gameData.app.SetGameMode (gameData.demo.nGameMode);
			MultiSortKillList ();
			gameData.app.SetGameMode (GM_NORMAL);
			}
			break;

		case ND_EVENT_MULTI_CONNECT: {
			int8_t nPlayer, nNewPlayer;
			int32_t killedTotal = 0, killsTotal = 0;
			char pszNewCallsign [CALLSIGN_LEN+1], old_callsign [CALLSIGN_LEN+1];

			nPlayer = NDReadByte ();
			nNewPlayer = NDReadByte ();
			if (!nNewPlayer) {
				NDReadString (old_callsign);
				killedTotal = NDReadInt ();
				killsTotal = NDReadInt ();
				}
			NDReadString (pszNewCallsign);
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				CONNECT (nPlayer, CONNECT_DISCONNECTED);
				if (!nNewPlayer) {
					memcpy (PLAYER (nPlayer).callsign, old_callsign, CALLSIGN_LEN+1);
					PLAYER (nPlayer).netKilledTotal = killedTotal;
					PLAYER (nPlayer).netKillsTotal = killsTotal;
					}
				else
					N_PLAYERS--;
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				CONNECT (nPlayer, CONNECT_PLAYING);
				PLAYER (nPlayer).netKillsTotal = 0;
				PLAYER (nPlayer).netKilledTotal = 0;
				memcpy (PLAYER (nPlayer).callsign, pszNewCallsign, CALLSIGN_LEN+1);
				if (nNewPlayer)
					N_PLAYERS++;
				}
			}
			break;

		case ND_EVENT_MULTI_RECONNECT: {
			int8_t nPlayer = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				CONNECT (nPlayer, CONNECT_DISCONNECTED);
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				CONNECT (nPlayer, CONNECT_PLAYING);
			}
			break;

		case ND_EVENT_MULTI_DISCONNECT: {
			int8_t nPlayer = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				CONNECT (nPlayer, CONNECT_PLAYING);
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				CONNECT (nPlayer, CONNECT_DISCONNECTED);
			}
			break;

		case ND_EVENT_MULTI_SCORE: {
			int8_t nPlayer = NDReadByte ();
			int32_t score = NDReadInt ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				PLAYER (nPlayer).score -= score;
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				PLAYER (nPlayer).score += score;
			gameData.app.SetGameMode (gameData.demo.nGameMode);
			MultiSortKillList ();
			gameData.app.SetGameMode (GM_NORMAL);
			}
			break;

		case ND_EVENT_PLAYER_SCORE: {
			int32_t score = NDReadInt ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				LOCALPLAYER.score -= score;
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				LOCALPLAYER.score += score;
			}
			break;

		case ND_EVENT_PRIMARY_AMMO: {
			int16_t nOldAmmo = NDReadShort ();
			int16_t nNewAmmo = NDReadShort ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				LOCALPLAYER.primaryAmmo [gameData.weapons.nPrimary] = nOldAmmo;
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				LOCALPLAYER.primaryAmmo [gameData.weapons.nPrimary] = nNewAmmo;
			}
			break;

		case ND_EVENT_SECONDARY_AMMO: {
			int16_t nOldAmmo = NDReadShort ();
			int16_t nNewAmmo = NDReadShort ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				LOCALPLAYER.secondaryAmmo [gameData.weapons.nSecondary] = nOldAmmo;
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD))
				LOCALPLAYER.secondaryAmmo [gameData.weapons.nSecondary] = nNewAmmo;
			}
			break;

		case ND_EVENT_DOOR_OPENING: {
			int16_t nSegment = NDReadShort ();
			uint8_t nSide = NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				int32_t anim_num;
				int32_t nConnSide;
				CSegment *segP, *oppSegP;

				segP = SEGMENTS + nSegment;
				oppSegP = SEGMENTS + segP->m_children [nSide];
				nConnSide = segP->ConnectedSide (oppSegP);
				anim_num = segP->Wall (nSide)->nClip;
				if (gameData.walls.animP [anim_num].flags & WCF_TMAP1)
					segP->m_sides [nSide].m_nBaseTex = oppSegP->m_sides [nConnSide].m_nBaseTex =
						gameData.walls.animP [anim_num].frames [0];
				else
					segP->m_sides [nSide].m_nOvlTex = 
					oppSegP->m_sides [nConnSide].m_nOvlTex = gameData.walls.animP [anim_num].frames [0];
				}
			else
				SEGMENTS [nSegment].OpenDoor (nSide);
			}
			break;

		case ND_EVENT_LASER_LEVEL: {
			int8_t oldLevel, newLevel;

			oldLevel = (int8_t) NDReadByte ();
			newLevel = (int8_t) NDReadByte ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
				LOCALPLAYER.ComputeLaserLevels (oldLevel);
				cockpit->UpdateLaserWeaponInfo ();
				}
			else if ((gameData.demo.nVcrState == ND_STATE_PLAYBACK) || 
						(gameData.demo.nVcrState == ND_STATE_FASTFORWARD) || 
						(gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD)) {
				LOCALPLAYER.ComputeLaserLevels (newLevel);
				cockpit->UpdateLaserWeaponInfo ();
				}
			}
			break;

		case ND_EVENT_CLOAKING_WALL: {
			uint8_t nBackWall, nFrontWall, nType, state, cloakValue;
			int16_t l0, l1, l2, l3;
			CSegment *segP;
			int32_t nSide;

			nFrontWall = NDReadByte ();
			nBackWall = NDReadByte ();
			nType = NDReadByte ();
			state = NDReadByte ();
			cloakValue = NDReadByte ();
			l0 = NDReadShort ();
			l1 = NDReadShort ();
			l2 = NDReadShort ();
			l3 = NDReadShort ();
			WALLS [nFrontWall].nType = nType;
			WALLS [nFrontWall].state = state;
			WALLS [nFrontWall].cloakValue = cloakValue;
			segP = SEGMENTS + WALLS [nFrontWall].nSegment;
			nSide = WALLS [nFrontWall].nSide;
			segP->m_sides [nSide].m_uvls [0].l = ((int32_t) l0) << 8;
			segP->m_sides [nSide].m_uvls [1].l = ((int32_t) l1) << 8;
			segP->m_sides [nSide].m_uvls [2].l = ((int32_t) l2) << 8;
			segP->m_sides [nSide].m_uvls [3].l = ((int32_t) l3) << 8;
			WALLS [nBackWall].nType = nType;
			WALLS [nBackWall].state = state;
			WALLS [nBackWall].cloakValue = cloakValue;
			segP = &SEGMENTS [WALLS [nBackWall].nSegment];
			nSide = WALLS [nBackWall].nSide;
			segP->m_sides [nSide].m_uvls [0].l = ((int32_t) l0) << 8;
			segP->m_sides [nSide].m_uvls [1].l = ((int32_t) l1) << 8;
			segP->m_sides [nSide].m_uvls [2].l = ((int32_t) l2) << 8;
			segP->m_sides [nSide].m_uvls [3].l = ((int32_t) l3) << 8;
			}
			break;

		case ND_EVENT_NEW_LEVEL: {
			int8_t newLevel, oldLevel, loadedLevel;

			newLevel = NDReadByte ();
			oldLevel = NDReadByte ();
			if (gameData.demo.nVcrState == ND_STATE_PAUSED)
				break;
			StopTime ();
			if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
				 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD))
				loadedLevel = oldLevel;
			else {
				loadedLevel = newLevel;
				for (i = 0; i < MAX_PLAYERS; i++) {
					PLAYER (i).cloakTime = 0;
					PLAYER (i).flags &= ~PLAYER_FLAGS_CLOAKED;
					}
				}
			if ((loadedLevel < missionManager.nLastSecretLevel) || 
				 (loadedLevel > missionManager.nLastLevel)) {
				NDErrorMsg (TXT_CANT_PLAYBACK, TXT_LEVEL_CANT_LOAD, TXT_DEMO_OLD_CORRUPT);
				return -1;
				}
			if (!LoadLevel ((int32_t) loadedLevel, true, false))
				return -1;
			meshBuilder.ComputeFaceKeys ();
			gameData.demo.bCtrlcenDestroyed = 0;
			if (bJustStartedPlayback) {
				gameData.walls.nWalls = NDReadInt ();
				for (i = 0; i < gameData.walls.nWalls; i++) {   // restore the walls
					WALLS [i].nType = NDReadByte ();
					if (gameData.demo.nVersion < 19)
						WALLS [i].flags = uint16_t (NDReadByte ());
					else
						WALLS [i].flags = uint16_t (NDReadShort ());
					WALLS [i].state = NDReadByte ();
					segP = SEGMENTS + WALLS [i].nSegment;
					nSide = WALLS [i].nSide;
					segP->m_sides [nSide].m_nBaseTex = NDReadShort ();
					nTexture = NDReadShort ();
					segP->m_sides [nSide].m_nOvlTex = nTexture & 0x3fff;
					segP->m_sides [nSide].m_nOvlOrient = (nTexture >> 14) & 3;
					}
				if (gameData.demo.nGameMode & GM_CAPTURE)
					MultiApplyGoalTextures ();
				bJustStartedPlayback = 0;
				}
			paletteManager.ResetEffect ();                // get palette back to Normal
			StartTime (0);
			}
			break;

		case ND_EVENT_EOF:
			bDone = -1;
			ndInFile.Seek (-1, SEEK_CUR);        // get back to the EOF marker
			gameData.demo.bEof = 1;
			gameData.demo.nFrameCount++;
			break;

		default:
			bDone = -1;
			ndInFile.Seek (-1, SEEK_CUR);        // get back to the EOF marker
			gameData.demo.bEof = 1;
			gameData.demo.nFrameCount++;
			break;
		}
	}
if (bNDBadRead)
	NDErrorMsg (TXT_DEMO_ERR_READING, TXT_DEMO_OLD_CORRUPT, NULL);
else
	NDUpdateSmoke ();
return bDone;
}

//	-----------------------------------------------------------------------------

void NDGotoBeginning ()
{
ndInFile.Seek (0, SEEK_SET);
gameData.demo.nVcrState = ND_STATE_PLAYBACK;
if (NDReadDemoStart (0))
	NDStopPlayback ();
if (NDReadFrameInfo () == -1)
	NDStopPlayback ();
if (NDReadFrameInfo () == -1)
	NDStopPlayback ();
gameData.demo.nVcrState = ND_STATE_PAUSED;
gameData.demo.bEof = 0;
}

//	-----------------------------------------------------------------------------

void NDGotoEnd ()
{
	int16_t nFrameLength, byteCount;
	int8_t level, laserLevel;
	uint8_t energy, shield, c;
	int32_t i, loc;

ndInFile.Seek (-2, SEEK_END);
level = NDReadByte ();
if ((level < missionManager.nLastSecretLevel) || (level > missionManager.nLastLevel)) {
	NDErrorMsg (TXT_CANT_PLAYBACK, TXT_LEVEL_CANT_LOAD, TXT_DEMO_OLD_CORRUPT);
	NDStopPlayback ();
	return;
	}
if (level != missionManager.nCurrentLevel)
	LoadLevel (level, 1, 0);
ndInFile.Seek (-4, SEEK_END);
byteCount = NDReadShort ();
ndInFile.Seek (-2 - byteCount, SEEK_CUR);

nFrameLength = NDReadShort ();
loc = (int32_t) ndInFile.Tell ();
if (gameData.demo.nGameMode & GM_MULTI)
	gameData.demo.bPlayersCloaked = NDReadByte ();
else
	 NDReadByte ();
NDReadByte ();
NDReadShort ();
NDReadInt ();
energy = NDReadByte ();
shield = NDReadByte ();
LOCALPLAYER.SetEnergy (I2X (energy));
LOCALPLAYER.SetShield (I2X (shield));
LOCALPLAYER.flags = NDReadInt ();
if (LOCALPLAYER.flags & PLAYER_FLAGS_CLOAKED) {
	LOCALPLAYER.cloakTime = gameData.time.xGame - (CLOAK_TIME_MAX / 2);
	gameData.demo.bPlayersCloaked |= (1 << N_LOCALPLAYER);
	}
if (LOCALPLAYER.flags & PLAYER_FLAGS_INVULNERABLE)
	LOCALPLAYER.invulnerableTime = gameData.time.xGame - (INVULNERABLE_TIME_MAX / 2);
gameData.weapons.nPrimary = NDReadByte ();
gameData.weapons.nSecondary = NDReadByte ();
for (i = 0; i < MAX_PRIMARY_WEAPONS; i++)
	LOCALPLAYER.primaryAmmo [i] = NDReadShort ();
for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
	LOCALPLAYER.secondaryAmmo [i] = NDReadShort ();
laserLevel = NDReadByte ();
if (laserLevel != LOCALPLAYER.LaserLevel ()) {
	LOCALPLAYER.ComputeLaserLevels (laserLevel);
	cockpit->UpdateLaserWeaponInfo ();
	}
if (gameData.demo.nGameMode & GM_MULTI) {
	c = NDReadByte ();
	N_PLAYERS = (int32_t)c;
	// see newdemo_read_start_demo for explanation of
	// why this is commented out
	//		NDReadByte (reinterpret_cast<int8_t*> (&N_PLAYERS);
	for (i = 0; i < N_PLAYERS; i++) {
		NDReadString (PLAYER (i).callsign);
		CONNECT (i, NDReadByte ());
		if (IsCoopGame)
			PLAYER (i).score = NDReadInt ();
		else {
			PLAYER (i).netKilledTotal = NDReadShort ();
			PLAYER (i).netKillsTotal = NDReadShort ();
			}
		}
	}
else
	LOCALPLAYER.score = NDReadInt ();
ndInFile.Seek (loc - nFrameLength, SEEK_SET);
gameData.demo.nFrameCount = NDReadInt ();            // get the frame count
gameData.demo.nFrameCount--;
ndInFile.Seek (4, SEEK_CUR);
gameData.demo.nVcrState = ND_STATE_PLAYBACK;
NDReadFrameInfo ();           // then the frame information
gameData.demo.nVcrState = ND_STATE_PAUSED;
return;
}

//	-----------------------------------------------------------------------------

inline void NDBackOneFrame (void)
{
	int16_t nPrevFrameLength;

ndInFile.Seek (-10, SEEK_CUR);
nPrevFrameLength = NDReadShort ();
ndInFile.Seek (8 - nPrevFrameLength, SEEK_CUR);
}

//	-----------------------------------------------------------------------------

void NDBackFrames (int32_t nFrames)
{
	int32_t	i;

for (i = nFrames; i; i--) {
	NDBackOneFrame ();
	if (!gameData.demo.bEof && (NDReadFrameInfo () == -1)) {
		NDStopPlayback ();
		return;
		}
	if (gameData.demo.bEof)
		gameData.demo.bEof = 0;
	NDBackOneFrame ();
	}
}

//	-----------------------------------------------------------------------------

/*
 *  routine to interpolate the viewer position.  the current position is
 *  stored in the gameData.objs.viewerP CObject.  Save this position, and read the next
 *  frame to get all OBJECTS read in.  Calculate the delta playback and
 *  the delta recording frame times between the two frames, then intepolate
 *  the viewers position accordingly.  gameData.demo.xRecordedTime is the time that it
 *  took the recording to render the frame that we are currently looking
 *  at.
*/

void NDInterpolateFrame (fix d_play, fix d_recorded)
{
	int32_t			nCurObjs;
	fix			factor;
	CFixVector  fvec1, fvec2, rvec1, rvec2;
	fix         mag1;
	fix			delta_x, delta_y, delta_z;
	uint8_t			renderType;
	CObject		*curObjP, *objP, *i;

	static CObject curObjs [MAX_OBJECTS_D2X];

factor = FixDiv (d_play, d_recorded);
if (factor > I2X (1))
	factor = I2X (1);
nCurObjs = gameData.objs.nLastObject [0];
#if 1
memcpy (curObjs, OBJECTS.Buffer (), OBJECTS.Size ());
#else
for (i = 0; i <= nCurObjs; i++)
	memcpy (&(curObjs [i]), &(OBJECTS [i]), sizeof (CObject));
#endif
gameData.demo.nVcrState = ND_STATE_PAUSED;
if (NDReadFrameInfo () == -1) {
	NDStopPlayback ();
	return;
	}
for (i = curObjs + nCurObjs, curObjP = curObjs; curObjP < i; curObjP++) {
	int32_t h;
	FORALL_OBJSi (objP, h) {
		if (curObjP->info.nSignature == objP->info.nSignature) {
			renderType = curObjP->info.renderType;
			//fix delta_p, delta_h, delta_b;
			//CAngleVector cur_angles, dest_angles;
			//  Extract the angles from the CObject orientation matrix.
			//  Some of this code taken from AITurnTowardsVector
			//  Don't do the interpolation on certain render types which don't use an orientation matrix
			if ((renderType != RT_LASER) &&
				 (renderType != RT_FIREBALL) && 
					(renderType != RT_THRUSTER) && 
					(renderType != RT_POWERUP)) {

				fvec1 = curObjP->info.position.mOrient.m.dir.f;
				fvec1 *= (I2X (1)-factor);
				fvec2 = objP->info.position.mOrient.m.dir.f;
				fvec2 *= factor;
				fvec1 += fvec2;
				mag1 = CFixVector::Normalize (fvec1);
				if (mag1 > I2X (1)/256) {
					rvec1 = curObjP->info.position.mOrient.m.dir.r;
					rvec1 *= (I2X (1)-factor);
					rvec2 = objP->info.position.mOrient.m.dir.r;
					rvec2 *= factor;
					rvec1 += rvec2;
					CFixVector::Normalize (rvec1); // Note: Doesn't matter if this is null, if null, VmVector2Matrix will just use fvec1
					curObjP->info.position.mOrient = CFixMatrix::CreateFR(fvec1, rvec1);
					//curObjP->info.position.mOrient = CFixMatrix::CreateFR(fvec1, NULL, &rvec1);
					}
				}
			// Interpolate the CObject position.  This is just straight linear
			// interpolation.
			delta_x = objP->info.position.vPos.v.coord.x - curObjP->info.position.vPos.v.coord.x;
			delta_y = objP->info.position.vPos.v.coord.y - curObjP->info.position.vPos.v.coord.y;
			delta_z = objP->info.position.vPos.v.coord.z - curObjP->info.position.vPos.v.coord.z;
			delta_x = FixMul (delta_x, factor);
			delta_y = FixMul (delta_y, factor);
			delta_z = FixMul (delta_z, factor);
			curObjP->info.position.vPos.v.coord.x += delta_x;
			curObjP->info.position.vPos.v.coord.y += delta_y;
			curObjP->info.position.vPos.v.coord.z += delta_z;
				// -- old fashioned way --// stuff the new angles back into the CObject structure
				// -- old fashioned way --				VmAngles2Matrix (&(curObjs [i].info.position.mOrient), &cur_angles);
			}
		}
	}

// get back to original position in the demo file.  Reread the current
// frame information again to reset all of the CObject stuff not covered
// with gameData.objs.nLastObject [0] and the CObject array (previously rendered
// OBJECTS, etc....)
NDBackFrames (1);
NDBackFrames (1);
if (NDReadFrameInfo () == -1)
	NDStopPlayback ();
gameData.demo.nVcrState = ND_STATE_PLAYBACK;
OBJECTS = curObjs;
gameData.objs.nLastObject [0] = nCurObjs;
}

//	-----------------------------------------------------------------------------

void NDPlayBackOneFrame (void)
{
	int32_t nFramesBack, i, level;
	static fix base_interpolTime = 0;
	static fix d_recorded = 0;

gameStates.render.nFrameFlipFlop = !gameStates.render.nFrameFlipFlop;
for (i = 0; i < MAX_PLAYERS; i++)
	if (gameData.demo.bPlayersCloaked &(1 << i))
		PLAYER (i).cloakTime = gameData.time.xGame - (CLOAK_TIME_MAX / 2);
if (LOCALPLAYER.flags & PLAYER_FLAGS_INVULNERABLE)
	LOCALPLAYER.invulnerableTime = gameData.time.xGame - (INVULNERABLE_TIME_MAX / 2);
if (gameData.demo.nVcrState == ND_STATE_PAUSED)       // render a frame or not
	return;
if (gameData.demo.nVcrState == ND_STATE_PLAYBACK)
	DoJasonInterpolate (gameData.demo.xRecordedTime);
gameData.reactor.bDestroyed = 0;
gameData.reactor.countdown.nSecsLeft = -1;
paletteManager.SetEffect (0, 0, 0);       //clear flash
if ((gameData.demo.nVcrState == ND_STATE_REWINDING) || 
	 (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD)) {
	level = missionManager.nCurrentLevel;
	if (gameData.demo.nFrameCount == 0)
		return;
	else if ((gameData.demo.nVcrState == ND_STATE_REWINDING) && (gameData.demo.nFrameCount < 10)) {
		NDGotoBeginning ();
		return;
		}
	nFramesBack = (gameData.demo.nVcrState == ND_STATE_REWINDING) ? 10 : 1;
	if (gameData.demo.bEof) {
		ndInFile.Seek (11, SEEK_CUR);
		}
	NDBackFrames (nFramesBack);
	if (level != missionManager.nCurrentLevel)
		NDPopCtrlCenTriggers ();
	if (gameData.demo.nVcrState == ND_STATE_ONEFRAMEBACKWARD) {
		if (level != missionManager.nCurrentLevel)
			NDBackFrames (1);
		gameData.demo.nVcrState = ND_STATE_PAUSED;
		}
	}
else if (gameData.demo.nVcrState == ND_STATE_FASTFORWARD) {
	if (!gameData.demo.bEof) {
		for (i = 0; i < 10; i++) {
			if (NDReadFrameInfo () == -1) {
				if (gameData.demo.bEof)
					gameData.demo.nVcrState = ND_STATE_PAUSED;
				else
					NDStopPlayback ();
				break;
				}
			}
		}
	else
		gameData.demo.nVcrState = ND_STATE_PAUSED;
	}
else if (gameData.demo.nVcrState == ND_STATE_ONEFRAMEFORWARD) {
	if (!gameData.demo.bEof) {
		level = missionManager.nCurrentLevel;
		if (NDReadFrameInfo () == -1) {
			if (!gameData.demo.bEof)
				NDStopPlayback ();
			}
		if (level != missionManager.nCurrentLevel) {
			if (NDReadFrameInfo () == -1) {
				if (!gameData.demo.bEof)
					NDStopPlayback ();
				}
			}
		gameData.demo.nVcrState = ND_STATE_PAUSED;
		} 
	else
		gameData.demo.nVcrState = ND_STATE_PAUSED;
	}
else {
	//  First, uptate the total playback time to date.  Then we check to see
	//  if we need to change the playback style to interpolate frames or
	//  skip frames based on where the playback time is relative to the
	//  recorded time.
	if (gameData.demo.nFrameCount <= 0)
		gameData.demo.xPlaybackTotal = gameData.demo.xRecordedTotal;      // baseline total playback time
	else
		gameData.demo.xPlaybackTotal += gameData.time.xFrame;
	if ((gameData.demo.nPlaybackStyle == NORMAL_PLAYBACK) && (gameData.demo.nFrameCount > 10))
		if ((gameData.demo.xPlaybackTotal * INTERPOL_FACTOR) < gameData.demo.xRecordedTotal) {
			gameData.demo.nPlaybackStyle = INTERPOLATE_PLAYBACK;
			gameData.demo.xPlaybackTotal = gameData.demo.xRecordedTotal + gameData.time.xFrame;  // baseline playback time
			base_interpolTime = gameData.demo.xRecordedTotal;
			d_recorded = gameData.demo.xRecordedTime;                      // baseline delta recorded
			}
	if ((gameData.demo.nPlaybackStyle == NORMAL_PLAYBACK) && (gameData.demo.nFrameCount > 10))
		if (gameData.demo.xPlaybackTotal > gameData.demo.xRecordedTotal)
			gameData.demo.nPlaybackStyle = SKIP_PLAYBACK;
	if ((gameData.demo.nPlaybackStyle == INTERPOLATE_PLAYBACK) && gameData.demo.bInterpolate) {
		fix d_play = 0;

		if (gameData.demo.xRecordedTotal - gameData.demo.xPlaybackTotal < gameData.time.xFrame) {
			d_recorded = gameData.demo.xRecordedTotal - gameData.demo.xPlaybackTotal;
			while (gameData.demo.xRecordedTotal - gameData.demo.xPlaybackTotal < gameData.time.xFrame) {
				CObject *curObjs, *objP;
				int32_t i, j, nObjects, nLevel, nSig;

				nObjects = gameData.objs.nLastObject [0];
				if (!(curObjs = new CObject [nObjects + 1])) {
					Warning (TXT_INTERPOLATE_BOTS, sizeof (CObject) * nObjects);
					break;
					}
				memcpy (curObjs, OBJECTS.Buffer (), (nObjects + 1) * sizeof (CObject));
				nLevel = missionManager.nCurrentLevel;
				if (NDReadFrameInfo () == -1) {
					delete[] curObjs;
					NDStopPlayback ();
					return;
					}
				if (nLevel != missionManager.nCurrentLevel) {
					delete[] curObjs;
					if (NDReadFrameInfo () == -1)
						NDStopPlayback ();
					break;
					}
				//  for each new CObject in the frame just read in, determine if there is
				//  a corresponding CObject that we have been interpolating.  If so, then
				//  copy that interpolated CObject to the new OBJECTS array so that the
				//  interpolated position and orientation can be preserved.
				for (i = 0; i <= nObjects; i++) {
					nSig = curObjs [i].info.nSignature;
					FORALL_OBJSi (objP, j) {
						if (nSig == objP->info.nSignature) {
							objP->info.position.mOrient = curObjs [i].info.position.mOrient;
							objP->info.position.vPos = curObjs [i].info.position.vPos;
							break;
							}
						}
					}
				delete[] curObjs;
				d_recorded += gameData.demo.xRecordedTime;
				base_interpolTime = gameData.demo.xPlaybackTotal - gameData.time.xFrame;
				}
			}
		d_play = gameData.demo.xPlaybackTotal - base_interpolTime;
		NDInterpolateFrame (d_play, d_recorded);
		return;
		}
	else {
		if (NDReadFrameInfo () == -1) {
			NDStopPlayback ();
			return;
			}
		if (gameData.demo.nPlaybackStyle == SKIP_PLAYBACK) {
			while (gameData.demo.xPlaybackTotal > gameData.demo.xRecordedTotal) {
				if (NDReadFrameInfo () == -1) {
					NDStopPlayback ();
					return;
					}
				}
			}
		}
	}
}

//	-----------------------------------------------------------------------------

void NDStartRecording (void)
{
#if 1
gameData.demo.nSize = 0;
#else
// free disk space shouldn't be an issue these days ...
gameData.demo.nSize = GetDiskFree ();
gameData.demo.nSize -= 100000;
if ((gameData.demo.nSize+100000) <  2000000000) {
	if (( (int32_t) (gameData.demo.nSize)) < 500000) {
		TextBox (NULL, BG_STANDARD, 1, TXT_OK, TXT_DEMO_NO_SPACE);
		return;
		}
	}
#endif
InitDemoData ();
gameData.demo.nWritten = 0;
gameData.demo.bNoSpace = 0;
gameData.demo.xStartTime = gameData.time.xGame;
gameData.demo.nState = ND_STATE_RECORDING;
ndOutFile.Open (DEMO_FILENAME, gameFolders.user.szDemos, "wb", 0);
#ifndef _WIN32_WCE
if (!ndOutFile.File () && (errno == ENOENT)) {   //dir doesn't exist?
#else
if (&ndOutFile.File ()) {                      //dir doesn't exist and no errno on mac!
#endif
	CFile::MkDir (gameFolders.user.szDemos); //try making directory
	ndOutFile.Open (DEMO_FILENAME, gameFolders.user.szDemos, "wb", 0);
	}
if (!ndOutFile.File ()) {
	TextBox (NULL, BG_STANDARD, 1, TXT_OK, "Cannot open demo temp file");
	gameData.demo.nState = ND_STATE_NORMAL;
	}
else
	NDRecordStartDemo ();
}

//	-----------------------------------------------------------------------------

void NDFinishRecording (void)
{
	uint8_t cloaked = 0;
	uint16_t byteCount = 0;
	int32_t l;

NDWriteByte (ND_EVENT_EOF);
NDWriteShort ((int16_t) (gameData.demo.nFrameBytesWritten - 1));
if (gameData.demo.nGameMode & GM_MULTI) {
	for (l = 0; l < N_PLAYERS; l++) {
		if (PLAYER (l).flags & PLAYER_FLAGS_CLOAKED)
			cloaked |= (1 << l);
		}
	NDWriteByte (cloaked);
	NDWriteByte (ND_EVENT_EOF);
	}
else {
	NDWriteShort (ND_EVENT_EOF);
	}
NDWriteShort (ND_EVENT_EOF);
NDWriteInt (ND_EVENT_EOF);
byteCount += 10;       // from gameData.demo.nFrameBytesWritten
NDWriteByte ((int8_t) (X2IR (LOCALPLAYER.Energy ())));
NDWriteByte ((int8_t) (X2IR (LOCALPLAYER.Shield ())));
NDWriteInt (LOCALPLAYER.flags);        // be sure players flags are set
NDWriteByte ((int8_t)gameData.weapons.nPrimary);
NDWriteByte ((int8_t)gameData.weapons.nSecondary);
byteCount += 8;
for (l = 0; l < MAX_PRIMARY_WEAPONS; l++)
	NDWriteShort ((int16_t)LOCALPLAYER.primaryAmmo [l]);
for (l = 0; l < MAX_SECONDARY_WEAPONS; l++)
	NDWriteShort ((int16_t)LOCALPLAYER.secondaryAmmo [l]);
byteCount += (sizeof (int16_t) * (MAX_PRIMARY_WEAPONS + MAX_SECONDARY_WEAPONS));
NDWriteByte (LOCALPLAYER.LaserLevel ());
byteCount++;
if (gameData.demo.nGameMode & GM_MULTI) {
	NDWriteByte ((int8_t)N_PLAYERS);
	byteCount++;
	for (l = 0; l < N_PLAYERS; l++) {
		NDWriteString (PLAYER (l).callsign);
		byteCount += ((int32_t) strlen (PLAYER (l).callsign) + 2);
		NDWriteByte ((int8_t) PLAYER (l).connected);
		if (IsCoopGame) {
			NDWriteInt (PLAYER (l).score);
			byteCount += 5;
			}
		else {
			NDWriteShort ((int16_t)PLAYER (l).netKilledTotal);
			NDWriteShort ((int16_t)PLAYER (l).netKillsTotal);
			byteCount += 5;
			}
		}
	} 
else {
	NDWriteInt (LOCALPLAYER.score);
	byteCount += 4;
	}
NDWriteShort (byteCount);
NDWriteByte ((int8_t) missionManager.nCurrentLevel);
NDWriteByte (ND_EVENT_EOF);
gameData.demo.nState = ND_STATE_NORMAL;
ndOutFile.Close ();
}

//	-----------------------------------------------------------------------------

char szAllowedDemoNameChars [] = "azAZ09_-";

static const char* demoMenuTitles [] = { TXT_SAVE_DEMO_AS, TXT_DEMO_SAVE_BAD, TXT_DEMO_SAVE_NOSPACE };

void NDStopRecording (void)
{
	static char szSaveName [FILENAME_LEN] = "";
	static uint8_t nAnonymous = 0;

	CMenu	m (6);
	int32_t	nChoice = 0;
	char	szFullSaveName [FILENAME_LEN] = "";
	char	*s;

NDFinishRecording ();
//paletteManager.ResumeEffect ();
if (szSaveName [0] != '\0') {
	int32_t num, i = (int32_t) strlen (szSaveName) - 1;
	char newfile [15];

	while (::isdigit (szSaveName [i])) {
		i--;
		if (i == -1)
			break;
		}
	i++;
	num = atoi (&(szSaveName [i]));
	num++;
	szSaveName [i] = '\0';
	sprintf (newfile, "%s%d", szSaveName, num);
	strncpy (szSaveName, newfile, 8);
	szSaveName [8] = '\0';
	}

m.Create (1);
m.AddInput ("", szSaveName, 8);

do {
	do {
		nChoice = m.Menu (NULL, demoMenuTitles [gameData.demo.bNoSpace]);

		if (nChoice != 0) {
			CFile::Delete (DEMO_FILENAME, gameFolders.user.szDemos);
			return;
			}

		if (!*szSaveName) 
			sprintf (szFullSaveName, "tmp%d.dem", nAnonymous++);
		else {
			//check to make sure name is ok
			for (s = szSaveName; *s; s++) {
				if (!isalnum (*s) && (*s != '_') && (*s != '-')) {
					TextBox (NULL, BG_STANDARD, 1, TXT_CONTINUE, TXT_DEMO_USE_LETTERS);
					*szSaveName = '\0';
					continue;
					}
				} 
			sprintf (szFullSaveName, "%s.dem", szSaveName);
			}

		nChoice = CFile::Exist (szFullSaveName, gameFolders.user.szDemos, 0)
					 ? InfoBox (NULL, NULL, BG_STANDARD, 2, TXT_YES, TXT_NO, TXT_CONFIRM_OVERWRITE)
					 : 0;
		} while (nChoice != 0);

	} while (!*szSaveName);

gameData.demo.nState = ND_STATE_NORMAL;
if (CFile::Exist (szFullSaveName, gameFolders.user.szDemos, 0))
	CFile::Delete (szFullSaveName, gameFolders.user.szDemos);
CFile::Rename (DEMO_FILENAME, szFullSaveName, gameFolders.user.szDemos);
#if DBG
int32_t nError = errno;
char* szError = strerror (nError);
#endif
}

//	-----------------------------------------------------------------------------
//returns the number of demo files on the disk
int32_t NDCountDemos (void)
{
	FFS	ffs;
	int32_t 	nFiles=0;
	char	searchName [FILENAME_LEN];

sprintf (searchName, "%s*.dem", gameFolders.user.szDemos);
if (!FFF (searchName, &ffs, 0)) {
	do {
		nFiles++;
		} while (!FFN (&ffs, 0));
	FFC (&ffs);
	}
if (gameFolders.bAltHogDirInited) {
	sprintf (searchName, "%s%s*.dem", gameFolders.game.szAltHogs, gameFolders.user.szDemos);
	if (!FFF (searchName, &ffs, 0)) {
		do {
			nFiles++;
			} while (!FFN (&ffs, 0));
		FFC (&ffs);
		}
	}
return nFiles;
}

//	-----------------------------------------------------------------------------

void NDStartPlayback (char * filename)
{
	FFS ffs;
	int32_t bRandom = 0;
	char filename2 [PATH_MAX+FILENAME_LEN], searchName [FILENAME_LEN];

ChangePlayerNumTo (0);
*filename2 = '\0';
InitDemoData ();
gameData.demo.bFirstTimePlayback = 1;
gameData.demo.xJasonPlaybackTotal = 0;
if (!filename) {
	// Randomly pick a filename
	int32_t nFiles = 0, nRandFiles;
	bRandom = 1;
	nFiles = NDCountDemos ();
	if (nFiles == 0) {
		return;     // No files found!
		}
	nRandFiles = RandShort () % nFiles;
	nFiles = 0;
	sprintf (searchName, "%s*.dem", gameFolders.user.szDemos);
	if (!FFF (searchName, &ffs, 0)) {
		do {
			if (nFiles == nRandFiles) {
				filename = reinterpret_cast<char*> (&ffs.name);
				break;
				}
			nFiles++;
			} while (!FFN (&ffs, 0));
		FFC (&ffs);
		}
	if (!filename && gameFolders.bAltHogDirInited) {
		sprintf (searchName, "%s%s*.dem", gameFolders.game.szAltHogs, gameFolders.user.szDemos);
		if (!FFF (searchName, &ffs, 0)) {
			do {
				if (nFiles==nRandFiles) {
					filename = reinterpret_cast<char*> (&ffs.name);
					break;
					}
				nFiles++;
				} while (!FFN (&ffs, 0));
			FFC (&ffs);
			}
		}
		if (!filename) 
			return;
	}
if (!filename)
	return;
strcpy (filename2, filename);
bRevertFormat = gameOpts->demo.bRevertFormat ? 1 : -1;
if (!ndInFile.Open (filename2, gameFolders.user.szDemos, "rb", 0)) {
#if TRACE			
	console.printf (CON_DBG, "Error reading '%s'\n", filename);
#endif
	return;
	}
if (bRevertFormat > 0) {
	strcat (filename2, ".v15");
	if (!ndOutFile.Open (filename2, gameFolders.user.szDemos, "wb", 0))
		bRevertFormat = -1;
	}
else
	ndOutFile.File () = NULL;
bNDBadRead = 0;
ChangePlayerNumTo (0);                 // force playernum to 0
strncpy (gameData.demo.callSignSave, LOCALPLAYER.callsign, CALLSIGN_LEN);
gameData.objs.viewerP = gameData.objs.consoleP = OBJECTS.Buffer ();   // play properly as if console player
if (NDReadDemoStart (bRandom)) {
	ndInFile.Close ();
	ndOutFile.Close ();
	return;
	}
if (gameOpts->demo.bRevertFormat && ndOutFile.File () && (bRevertFormat < 0)) {
	ndOutFile.Close ();
	CFile::Delete (filename2, gameFolders.user.szDemos);
	}
gameData.app.SetGameMode (GM_NORMAL);
gameData.demo.nState = ND_STATE_PLAYBACK;
gameData.demo.nVcrState = ND_STATE_PLAYBACK;
gameData.demo.nOldCockpit = gameStates.render.cockpit.nType;
gameData.demo.nSize = (int32_t) ndInFile.Length ();
bNDBadRead = 0;
gameData.demo.bEof = 0;
gameData.demo.nFrameCount = 0;
gameData.demo.bPlayersCloaked = 0;
gameData.demo.nPlaybackStyle = NORMAL_PLAYBACK;
SetFunctionMode (FMODE_GAME);
gameStates.render.cockpit.n3DView [0] = CV_NONE;       //turn off 3d views on cockpit
gameStates.render.cockpit.n3DView [1] = CV_NONE;       //turn off 3d views on cockpit
SDL_ShowCursor (0);
NDPlayBackOneFrame ();       // this one loads new level
ResetTime ();
NDPlayBackOneFrame ();       // get all of the OBJECTS to renderb game
}

//	-----------------------------------------------------------------------------

void NDStopPlayback ()
{
if (bRevertFormat > 0) {
	int32_t h = (int32_t) (ndInFile.Length () - ndInFile.Tell ());
	char *p = new char [h];
	if (p) {
		bRevertFormat = 0;
		NDRead (p, h, 1);
		NDWriteShort ((int16_t) (gameData.demo.nFrameBytesWritten - 1));
		NDWrite (p + 3, h - 3, 1);
		delete[] p;
		}
	ndOutFile.Close ();
	bRevertFormat = -1;
	}
ndInFile.Close ();
gameData.demo.nState = ND_STATE_NORMAL;
ChangePlayerNumTo (0);             //this is reality
strncpy (LOCALPLAYER.callsign, gameData.demo.callSignSave, CALLSIGN_LEN);
cockpit->Activate (gameData.demo.nOldCockpit);
gameData.app.SetGameMode (GM_GAME_OVER);
SetFunctionMode (FMODE_MENU);
SDL_ShowCursor (1);
longjmp (gameExitPoint, 0);               // Exit game loop
}

//	-----------------------------------------------------------------------------

#if DBG

#define BUF_SIZE 16384

void NDStripFrames (char *outname, int32_t bytes_to_strip)
{
	CFile	ndOutFile;
	char	*buf;
	int32_t	nTotalSize, bytes_done, read_elems, bytes_back;
	int32_t	trailer_start, loc1, loc2, stop_loc, bytes_to_read;
	int16_t	nPrevFrameLength;

bytes_done = 0;
nTotalSize = (int32_t) ndInFile.Length ();
if (!ndOutFile.Open (outname, "", "wb", 0)) {
	NDErrorMsg ("Can't open output file", NULL, NULL);
	NDStopPlayback ();
	return;
	}
if (!(buf = new char [BUF_SIZE])) {
	NDErrorMsg ("Mot enough memory for output buffer", NULL, NULL);
	ndOutFile.Close ();
	NDStopPlayback ();
	return;
	}
NDGotoEnd ();
trailer_start = (int32_t) ndInFile.Tell ();
ndInFile.Seek (11, SEEK_CUR);
bytes_back = 0;
while (bytes_back < bytes_to_strip) {
	loc1 = (int32_t) ndInFile.Tell ();
	//ndInFile.Seek (-10, SEEK_CUR);
	//NDReadShort (&nPrevFrameLength);
	//ndInFile.Seek (8 - nPrevFrameLength, SEEK_CUR);
	NDBackFrames (1);
	loc2 = (int32_t) ndInFile.Tell ();
	bytes_back += (loc1 - loc2);
	}
ndInFile.Seek (-10, SEEK_CUR);
nPrevFrameLength = NDReadShort ();
ndInFile.Seek (-3, SEEK_CUR);
stop_loc = (int32_t) ndInFile.Tell ();
ndInFile.Seek (0, SEEK_SET);
while (stop_loc > 0) {
	if (stop_loc < BUF_SIZE)
		bytes_to_read = stop_loc;
	else
		bytes_to_read = BUF_SIZE;
	read_elems = (int32_t) ndInFile.Read (buf, 1, bytes_to_read);
	ndOutFile.Write (buf, 1, read_elems);
	stop_loc -= read_elems;
	}
stop_loc = (int32_t) ndOutFile.Tell ();
ndInFile.Seek (trailer_start, SEEK_SET);
while ((read_elems = (int32_t) ndInFile.Read (buf, 1, BUF_SIZE)))
	ndOutFile.Write (buf, 1, read_elems);
ndOutFile.Seek (stop_loc, SEEK_SET);
ndOutFile.Seek (1, SEEK_CUR);
ndOutFile.Write (&nPrevFrameLength, 2, 1);
ndOutFile.Close ();
NDStopPlayback ();
particleManager.Shutdown ();
}

#endif

//	-----------------------------------------------------------------------------

CObject demoRightExtra, demoLeftExtra;
uint8_t nDemoDoRight = 0, nDemoDoLeft = 0;
uint8_t nDemoDoingRight = 0, nDemoDoingLeft = 0;

char nDemoWBUType [] = {0, WBUMSL, WBUMSL, WBU_REAR, WBU_ESCORT, WBU_MARKER, WBUMSL};
char bDemoRearCheck [] = {0, 0, 0, 1, 0, 0, 0};
const char *szDemoExtraMessage [] = {"PLAYER", "GUIDED", "MISSILE", "REAR", "GUIDE-BOT", "MARKER", "SHIP"};

void NDRenderExtras (uint8_t which, CObject *objP)
{
	uint8_t w = which >> 4;
	uint8_t nType = which & 15;

if (which == 255) {
	Int3 (); // how'd we get here?
	cockpit->RenderWindow (w, NULL, 0, WBU_WEAPON, NULL);
	return;
	}
if (w) {
	memcpy (&demoRightExtra, objP, sizeof (CObject));  
	nDemoDoRight = nType;
	}
else {
	memcpy (&demoLeftExtra, objP, sizeof (CObject)); 
	nDemoDoLeft = nType;
	}
}

//	-----------------------------------------------------------------------------

void DoJasonInterpolate (fix xRecordedTime)
{
	fix xDelay;

gameData.demo.xJasonPlaybackTotal += gameData.time.xFrame;
if (!gameData.demo.bFirstTimePlayback) {
	// get the difference between the recorded time and the playback time
	xDelay = (xRecordedTime - gameData.time.xFrame);
	if (xDelay >= 0) {
		StopTime ();
		TimerDelay (xDelay);
		StartTime (0);
		}
	else {
		while (gameData.demo.xJasonPlaybackTotal > gameData.demo.xRecordedTotal)
			if (NDReadFrameInfo () == -1) {
				NDStopPlayback ();
				return;
				}
		//xDelay = gameData.demo.xRecordedTotal - gameData.demo.xJasonPlaybackTotal;
		//if (xDelay > 0)
		//	TimerDelay (xDelay);
		}
	}
gameData.demo.bFirstTimePlayback = 0;
}

//	-----------------------------------------------------------------------------
//eof

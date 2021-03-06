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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "descent.h"
#include "objrender.h"
#include "lightning.h"
#include "trackobject.h"
#include "omega.h"
#include "segpoint.h"
#include "error.h"
#include "key.h"
#include "texmap.h"
#include "textures.h"
#include "rendermine.h"
#include "fireball.h"
#include "newdemo.h"
#include "timer.h"
#include "physics.h"
#include "segmath.h"
#include "input.h"
#include "dropobject.h"
#include "lightcluster.h"
#include "visibility.h"
#include "postprocessing.h"

//	--------------------------------------------------------------------------------------------------

#if defined(_WIN32) && !DBG
typedef int32_t ( __fastcall * pWeaponHandler) (CObject *, int32_t, int32_t&, int32_t);
#else
typedef int32_t (* pWeaponHandler) (CObject *, int32_t, int32_t&, int32_t);
#endif

//-------------------------------------------

int32_t LaserHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
	uint8_t	nLaser = (nLevel <= MAX_LASER_LEVEL) ? LASER_ID + nLevel : SUPERLASER_ID + (nLevel - MAX_LASER_LEVEL - 1);
	int16_t	nLightObj = lightClusterManager.Create (pObj);
	int16_t nFired = 0;

gameData.laserData.nOffset = (I2X (2) * Rand (8)) / 8;
if (0 <= LaserPlayerFire (pObj, nLaser, 0, 1, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFire (pObj, nLaser, 1, 0, 0, nLightObj))
	nFired++;
if (nFlags & LASER_QUAD) {
	nFired += 2;
	//	hideous system to make quad laser 1.5x powerful as Normal laser, make every other quad laser bolt bHarmless
	if (0 <= LaserPlayerFire (pObj, nLaser, 2, 0, 0, nLightObj))
		nFired++;
	if (0 <= LaserPlayerFire (pObj, nLaser, 3, 0, 0, nLightObj))
		nFired++;
	}
if (!nFired && OBJECT (nLightObj))
	OBJECT (nLightObj)->Die ();
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t VulcanHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
#	define VULCAN_SPREAD	(SRandShort () / 8)

	tFiringData *pFiringData = gameData.multiplayer.weaponStates [pObj->info.nId].firing;
	int16_t			nFired = 0;

if (pFiringData->nDuration <= GATLING_DELAY)
	return 0;
//	Only make sound for 1/4 of vulcan bullets.
if (0 <= LaserPlayerFireSpread (pObj, VULCAN_ID, 6, VULCAN_SPREAD, VULCAN_SPREAD, 1, 0, -1))
	nFired++;
if (nRoundsPerShot > 1) {
	if (0 <= LaserPlayerFireSpread (pObj, VULCAN_ID, 6, VULCAN_SPREAD, VULCAN_SPREAD, 0, 0, -1))
	nFired++;
	if (nRoundsPerShot > 2)
		if (0 <= LaserPlayerFireSpread (pObj, VULCAN_ID, 6, VULCAN_SPREAD, VULCAN_SPREAD, 0, 0, -1))
	nFired++;
	}
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t SpreadfireHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
	int16_t	nLightObj = lightClusterManager.Create (pObj);
	int16_t	nFired = 0;

if (nFlags & LASER_SPREADFIRE_TOGGLED) {
	if (0 <= LaserPlayerFireSpread (pObj, SPREADFIRE_ID, 6, I2X (1) / 16, 0, 0, 0, nLightObj))
		nFired++;
	if (0 <= LaserPlayerFireSpread (pObj, SPREADFIRE_ID, 6, -I2X (1) / 16, 0, 0, 0, nLightObj))
		nFired++;
	}
else {
	if (0 <= LaserPlayerFireSpread (pObj, SPREADFIRE_ID, 6, 0, I2X (1) / 16, 0, 0, nLightObj))
		nFired++;
	if (0 <= LaserPlayerFireSpread (pObj, SPREADFIRE_ID, 6, 0, -I2X (1) / 16, 0, 0, nLightObj))
		nFired++;
	}
if (0 <= LaserPlayerFireSpread (pObj, SPREADFIRE_ID, 6, 0, 0, 1, 0, nLightObj))
	nFired++;
if (!nFired && OBJECT (nLightObj))
	OBJECT (nLightObj)->Die ();
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t PlasmaHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
	int16_t	nLightObj = lightClusterManager.Create (pObj);
	int16_t	nFired = 0;

if (0 <= LaserPlayerFire (pObj, PLASMA_ID, 0, 1, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFire (pObj, PLASMA_ID, 1, 0, 0, nLightObj))
	nFired++;
if (!nFired && (nLightObj >= 0))
	OBJECT (nLightObj)->Die ();
if (nRoundsPerShot > 1) {
	nLightObj = lightClusterManager.Create (pObj);
	if (0 <= FireWeaponDelayedWithSpread (pObj, PLASMA_ID, 0, 0, 0, gameData.timeData.xFrame / 2, 1, 0, nLightObj))
		nFired++;
	if (0 <= FireWeaponDelayedWithSpread (pObj, PLASMA_ID, 1, 0, 0, gameData.timeData.xFrame / 2, 0, 0, nLightObj))
		nFired++;
	if (!nFired && OBJECT (nLightObj))
		OBJECT (nLightObj)->Die ();
	}
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t FusionHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
	CFixVector	vForce;
	int16_t		nLightObj = lightClusterManager.Create (pObj);
	int16_t		nFired = 0;

if (0 <= LaserPlayerFire (pObj, FUSION_ID, 0, 1, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFire (pObj, FUSION_ID, 1, 1, 0, nLightObj))
	nFired++;
if (EGI_FLAG (bTripleFusion, 0, 0, 0) && gameData.multiplayer.weaponStates [pObj->info.nId].bTripleFusion)
	if (0 <= LaserPlayerFire (pObj, FUSION_ID, 6, 1, 0, nLightObj))
		nFired++;
nFlags = int8_t (gameData.FusionCharge () >> 12);
gameData.SetFusionCharge (0);
if (!nFired) {
	if (OBJECT (nLightObj))
		OBJECT (nLightObj)->Die ();
	return 0;
	}
postProcessManager.Add (NEW CPostEffectShockwave (SDL_GetTicks (), I2X (1) / 3, pObj->info.xSize, 1, 
																  OBJPOS (pObj)->vPos + OBJPOS (pObj)->mOrient.m.dir.f * pObj->info.xSize, pObj->Index ()));
vForce.v.coord.x = -(pObj->info.position.mOrient.m.dir.f.v.coord.x << 7);
vForce.v.coord.y = -(pObj->info.position.mOrient.m.dir.f.v.coord.y << 7);
vForce.v.coord.z = -(pObj->info.position.mOrient.m.dir.f.v.coord.z << 7);
pObj->ApplyForce (vForce);
vForce.v.coord.x = (vForce.v.coord.x >> 4) + SRandShort ();
vForce.v.coord.y = (vForce.v.coord.y >> 4) + SRandShort ();
vForce.v.coord.z = (vForce.v.coord.z >> 4) + SRandShort ();
pObj->ApplyRotForce (vForce);
return nRoundsPerShot;
}

//-------------------------------------------

int32_t SuperlaserHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
	uint8_t	nSuperLevel = 3;		//make some new kind of laser eventually
	int16_t	nLightObj = lightClusterManager.Create (pObj);
	int16_t	nFired = 0;

if (0 <= LaserPlayerFire (pObj, nSuperLevel, 0, 1, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFire (pObj, nSuperLevel, 1, 0, 0, nLightObj))
	nFired++;

if (nFlags & LASER_QUAD) {
	//	hideous system to make quad laser 1.5x powerful as Normal laser, make every other quad laser bolt bHarmless
	if (0 <= LaserPlayerFire (pObj, nSuperLevel, 2, 0, 0, nLightObj))
		nFired++;
	if (0 <= LaserPlayerFire (pObj, nSuperLevel, 3, 0, 0, nLightObj))
		nFired++;
	}
if (!nFired && OBJECT (nLightObj))
	OBJECT (nLightObj)->Die ();
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t GaussHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
#	define GAUSS_SPREAD		(VULCAN_SPREAD / 5)

	tFiringData *pFiringData = gameData.multiplayer.weaponStates [pObj->info.nId].firing;
	int16_t			nFired = 0;

if (pFiringData->nDuration <= GATLING_DELAY)
	return 0;
//	Only make sound for 1/4 of vulcan bullets.
if (0 <= LaserPlayerFireSpread (pObj, GAUSS_ID, 6, GAUSS_SPREAD, GAUSS_SPREAD,
										  (pObj->info.nId != N_LOCALPLAYER) || (gameData.laserData.xNextFireTime > gameData.timeData.xGame), 0, -1))
	nFired++;
if (nRoundsPerShot > 1) {
	if (0 <= LaserPlayerFireSpread (pObj, GAUSS_ID, 6, GAUSS_SPREAD, GAUSS_SPREAD, 0, 0, -1))
		nFired++;
	if (nRoundsPerShot > 2)
		if (0 <= LaserPlayerFireSpread (pObj, GAUSS_ID, 6, GAUSS_SPREAD, GAUSS_SPREAD, 0, 0, -1))
			nFired++;
	}
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t HelixHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
	typedef struct tSpread {
		fix	r, u;
		} tSpread;

	static tSpread spreadTable [8] = {
	 {I2X (1) / 16, 0},
	 {I2X (1) / 17, I2X (1) / 42},
	 {I2X (1) / 22, I2X (1) / 22},
	 {I2X (1) / 42, I2X (1) / 17},
	 {0, I2X (1) / 16},
	 {-I2X (1) / 42, I2X (1) / 17},
	 {-I2X (1) / 22, I2X (1) / 22},
	 {-I2X (1) / 17, I2X (1) / 42}
		};

	tSpread	spread = spreadTable [(nFlags >> LASER_HELIX_SHIFT) & LASER_HELIX_MASK];
	int16_t		nLightObj = lightClusterManager.Create (pObj);
	int16_t		nFired = 0;

if (0 <= LaserPlayerFireSpread (pObj, HELIX_ID, 6,  0,  0, 1, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFireSpread (pObj, HELIX_ID, 6,  spread.r,  spread.u, 0, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFireSpread (pObj, HELIX_ID, 6, -spread.r, -spread.u, 0, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFireSpread (pObj, HELIX_ID, 6,  spread.r * 2,  spread.u * 2, 0, 0, nLightObj))
	nFired++;
if (0 <= LaserPlayerFireSpread (pObj, HELIX_ID, 6, -spread.r * 2, -spread.u * 2, 0, 0, nLightObj))
	nFired++;
if (!nFired && OBJECT (nLightObj))
	OBJECT (nLightObj)->Die ();
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t PhoenixHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
	int16_t	nLightObj = lightClusterManager.Create (pObj);
	int16_t	nFired = 0;

if (0 <= LaserPlayerFire (pObj, PHOENIX_ID, 0, 1, 0, nLightObj))
	nFired++;;
if (0 <= LaserPlayerFire (pObj, PHOENIX_ID, 1, 0, 0, nLightObj))
	nFired++;
if (!nFired && (nLightObj >= 0))
	OBJECT (nLightObj)->Die ();
if (nRoundsPerShot > 1) {
	nLightObj = lightClusterManager.Create (pObj);
	if (0 <= FireWeaponDelayedWithSpread (pObj, PHOENIX_ID, 0, 0, 0, gameData.timeData.xFrame / 2, 1, 0, nLightObj))
	nFired++;
	if (0 <= FireWeaponDelayedWithSpread (pObj, PHOENIX_ID, 1, 0, 0, gameData.timeData.xFrame / 2, 0, 0, nLightObj))
	nFired++;
	if (!nFired && OBJECT (nLightObj))
		OBJECT (nLightObj)->Die ();
	}
return nFired ? nRoundsPerShot : 0;
}

//-------------------------------------------

int32_t OmegaHandler (CObject *pObj, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
LaserPlayerFire (pObj, OMEGA_ID, 6, 1, 0, -1);
return nRoundsPerShot;
}

//-------------------------------------------

pWeaponHandler weaponHandlers [] = {
	LaserHandler,
	VulcanHandler,
	SpreadfireHandler,
	PlasmaHandler,
	FusionHandler,
	SuperlaserHandler,
	GaussHandler,
	HelixHandler,
	PhoenixHandler,
	OmegaHandler
	};



//	--------------------------------------------------------------------------------------------------
//	Object "nObject" fires weapon "weapon_num" of level "level". (Right now (9/24/94) level is used only for nType 0 laser.
//	Flags are the player flags.  For network mode, set to 0.
//	It is assumed that this is a player CObject (as in multiplayer), and therefore the gun positions are known.
//	Returns number of times a weapon was fired.  This is typically 1, but might be more for low frame rates.
//	More than one shot is fired with a pseudo-delay so that players on show machines can fire (for themselves
//	or other players) often enough for things like the vulcan cannon.

int32_t FireWeapon (int16_t nObject, uint8_t nWeapon, int32_t nLevel, int32_t& nFlags, int32_t nRoundsPerShot)
{
if (nWeapon > OMEGA_INDEX) {
	gameData.weaponData.nPrimary = 0;
	nRoundsPerShot = 0;
	}
else {
	gameData.multigame.weapon.nFired [0] = 0;
	nRoundsPerShot = weaponHandlers [nWeapon] (OBJECT (nObject), nLevel, nFlags, nRoundsPerShot);
	}
if (IsMultiGame && (nObject == LOCALPLAYER.nObject)) {
	gameData.multigame.weapon.bFired = nRoundsPerShot;
	gameData.multigame.weapon.nGun = nWeapon;
	gameData.multigame.weapon.nFlags = nFlags;
	gameData.multigame.weapon.nLevel = nLevel;
	MultiSendFire ();
	}
return nRoundsPerShot;
}

//	-------------------------------------------------------------------------------------------
// eof

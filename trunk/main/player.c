/* $Id: player.c,v 1.3 2003/10/10 09:36:35 btb Exp $ */

/*
 *
 * Player Stuff
 *
 */

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include "inferno.h"
#include "multi.h"
#include "input.h"

#ifdef RCS
static char rcsid[] = "$Id: player.c,v 1.3 2003/10/10 09:36:35 btb Exp $";
#endif

//-------------------------------------------------------------------------
// reads a tPlayerShip structure from a CFILE
 
void PlayerShipRead (tPlayerShip *ps, CFILE *fp)
{
	int i;

ps->nModel = CFReadInt (fp);
ps->nExplVClip = CFReadInt (fp);
ps->mass = CFReadFix (fp);
ps->drag = CFReadFix (fp);
ps->maxThrust = CFReadFix (fp);
ps->reverseThrust = CFReadFix (fp);
ps->brakes = CFReadFix (fp);
ps->wiggle = CFReadFix (fp);
ps->maxRotThrust = CFReadFix (fp);
for (i = 0; i < N_PLAYER_GUNS; i++)
	CFReadVector (ps->gunPoints + i, fp);
}

//-------------------------------------------------------------------------

int EquippedPlayerGun (tObject *objP)
{
if (objP->nType == OBJ_PLAYER) {
		int		nPlayer = objP->id;
		int		nWeapon = gameData.multiplayer.weaponStates [nPlayer].nPrimary;

	return (nWeapon || (gameData.multiplayer.weaponStates [nPlayer].nLaserLevel <= MAX_LASER_LEVEL)) ? nWeapon : SUPER_LASER_INDEX;
	}
return 0;
}

//-------------------------------------------------------------------------

static int nBombIds [] = {SMART_INDEX, MEGA_INDEX, EARTHSHAKER_INDEX};

int EquippedPlayerBomb (tObject *objP)
{
if (objP->nType == OBJ_PLAYER) {
		int		nPlayer = objP->id;
		int		i, nWeapon = gameData.multiplayer.weaponStates [nPlayer].nSecondary;

	for (i = 0; i < sizeofa (nBombIds); i++)
		if (nWeapon == nBombIds [i])
			return i + 1;
	}
return 0;
}

//-------------------------------------------------------------------------

static int nMissileIds [] = {CONCUSSION_INDEX, HOMING_INDEX, FLASHMSL_INDEX, GUIDED_INDEX, MERCURY_INDEX};

int EquippedPlayerMissile (tObject *objP, int *nMissiles)
{
if (objP->nType == OBJ_PLAYER) {
		int		nPlayer = objP->id;
		int		i, nWeapon = gameData.multiplayer.weaponStates [nPlayer].nSecondary;

	for (i = 0; i < sizeofa (nMissileIds); i++)
		if (nWeapon == nMissileIds [i]) {
			*nMissiles = gameData.multiplayer.weaponStates [nPlayer].nMissiles;
			return i + 1;
			}
	}
*nMissiles = 0;
return 0;
}

//-------------------------------------------------------------------------

void UpdatePlayerWeaponInfo (void)
{
	int	bUpdate = 0;

gameData.weapons.bFiring [0] = (Controls [0].firePrimaryState != 0) || (Controls [0].firePrimaryDownCount != 0);
gameData.weapons.bFiring [1] = (Controls [0].fireSecondaryState != 0) || (Controls [0].fireSecondaryDownCount != 0);
if (gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nPrimary != gameData.weapons.nPrimary) {
	gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nPrimary = gameData.weapons.nPrimary;
	bUpdate = 1;
	}
if (gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nSecondary != gameData.weapons.nSecondary) {
	gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nSecondary = gameData.weapons.nSecondary;
	bUpdate = 1;
	}
if (gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].bFiring [0] != gameData.weapons.bFiring [0]) {
	gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].bFiring [0] = gameData.weapons.bFiring [0];
	bUpdate = 1;
	}
if (gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].bFiring [1] != gameData.weapons.bFiring [1]) {
	gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].bFiring [1] = gameData.weapons.bFiring [1];
	bUpdate = 1;
	}
if (gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nMissiles != LOCALPLAYER.secondaryAmmo [gameData.weapons.nSecondary]) {
	gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nMissiles = (char) LOCALPLAYER.secondaryAmmo [gameData.weapons.nSecondary];
	bUpdate = 1;
	}
if (gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nLaserLevel != LOCALPLAYER.laserLevel) {
	gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].nLaserLevel = LOCALPLAYER.laserLevel;
	bUpdate = 1;
	}
if (gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].bTripleFusion != gameData.weapons.bTripleFusion) {
	gameData.multiplayer.weaponStates [gameData.multiplayer.nLocalPlayer].bTripleFusion = gameData.weapons.bTripleFusion;
	bUpdate = 1;
	}
if (bUpdate)
	MultiSendPlayerWeapons (gameData.multiplayer.nLocalPlayer);
}

//-------------------------------------------------------------------------

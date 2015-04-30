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

#ifndef _LASER_H
#define _LASER_H

#define LASER_ID        				0   //0..3 are lasers
#define ROBOT_LIGHT_FIREBALL_ID		5
#define REACTOR_BLOB_ID					6
#define CONCUSSION_ID   				8
#define FLARE_ID        				9   //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define ROBOT_BLUE_LASER_ID			10
#define VULCAN_ID       				11  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define SPREADFIRE_ID   				12  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define PLASMA_ID       				13  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define FUSION_ID       				14  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define HOMINGMSL_ID       			15
#define PROXMINE_ID    					16
#define SMARTMSL_ID        			17
#define MEGAMSL_ID         			18
#define SMARTMSL_BLOB_ID 				19
#define ROBOT_BLUE_ENERGY_ID			20
#define ROBOT_HOMINGMSL_ID      		21
#define ROBOT_CONCUSSION_ID    		22
#define SILENT_SPREADFIRE_ID    		23
#define ROBOT_RED_LASER_ID				24
#define ROBOT_GREEN_LASER_ID			25
#define ROBOT_PLASMA_ID					26
#define ROBOT_MEDIUM_FIREBALL_ID		27
#define ROBOT_MEGAMSL_ID		  		28
#define ROBOT_SMARTMSL_BLOB_ID  		29
#define SUPERLASER_ID          		30  // 30,31 are super lasers (level 5,6)
#define GAUSS_ID                		32  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define HELIX_ID                		33  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define PHOENIX_ID              		34  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define OMEGA_ID                		35  //  NOTE: This MUST correspond to the ID generated at bitmaps.tbl read time.
#define FLASHMSL_ID                	36
#define GUIDEDMSL_ID           		37
#define SMARTMINE_ID            		38
#define MERCURYMSL_ID              	39
#define EARTHSHAKER_ID          		40
#define ROBOT_VULCAN_ID					41
#define ROBOT_TI_STREAM_ID		      42

#define ROBOT_WHITE_LASER_ID			43
#define ROBOT_PHOENIX_ID				44
#define ROBOT_FAST_PHOENIX_ID			45
#define ROBOT_HELIX_ID					46
#define SMARTMINE_BLOB_ID        	47
#define ROBOT_PHASE_ENERGY_ID			48
#define ROBOT_SMARTMINE_BLOB_ID  	49
#define ROBOT_FLASHMSL_ID				50
#define SMALLMINE_ID                51  //the mine that the designers can place
#define ROBOT_GAUSS_ID              52
#define ROBOT_SMARTMINE_ID   			53
#define EARTHSHAKER_MEGA_ID  			54
#define ROBOT_MERCURYMSL_ID			55
#define ROBOT_SMARTMSL_ID				57
#define ROBOT_EARTHSHAKER_ID        58
#define ROBOT_SHAKER_MEGA_ID        59
#define ROBOT_WHITE_ENERGY_ID			60
#define ROBOT_MEGA_FLASHMSL_ID	   61
#define ROBOT_VERTIGO_FLASHMSL_ID	62
#define ROBOT_VERTIGO_FIREBALL_ID	63
#define ROBOT_VERTIGO_PHOENIX_ID		64

#define EXTRA_OBJ_IDS					65
#define CONCUSSION4_ID   				65
#define HOMINGMSL4_ID       			66
#define FLASHMSL4_ID       			67
#define MERCURYMSL4_ID       			68
#define SMINEPACK_ID						69
#define PMINEPACK_ID						70

#define MAX_WEAPON_ID					70

#define OMEGA_MULTI_LIFELEFT    (I2X (1)/6)

// These are new defines for the value of 'flags' passed to FireWeapon.
// The purpose is to collect other flags like QUAD_LASER and Spreadfire_toggle
// into a single 8-bit quantity so it can be easily used in network mode.

#define LASER_QUAD                  1
#define LASER_SPREADFIRE_TOGGLED    2
#define LASER_HELIX_FLAG0           4   // helix uses 3 bits for angle
#define LASER_HELIX_FLAG1           8   // helix uses 3 bits for angle
#define LASER_HELIX_FLAG2           16  // helix uses 3 bits for angle

#define LASER_HELIX_SHIFT				2   // how far to shift count to put in flags
#define LASER_HELIX_MASK				7   // must match number of bits in flags

#define MAX_LASER_BITMAPS				6

// For muzzle firing casting light.
#define MUZZLE_QUEUE_MAX				8

void RenderLaser (CObject *obj);
void find_goal_texture (CObject * obj, uint8_t nType, int32_t gun_num, int32_t makeSound, int32_t harmlessFlag);
void CreateFlare (CObject *obj);
int32_t LasersAreRelated (int32_t o1, int32_t o2);
int32_t FireWeaponDelayedWithSpread (CObject *objP, uint8_t laserType, int32_t gun_num, fix spreadr, 
												 fix spreadu, fix delayTime, int32_t makeSound, int32_t harmless, int16_t nLightObj);
int32_t LocalPlayerFireGun (void);
void DoMissileFiring (int32_t do_autoselect);
void NetMissileFiring (int32_t CPlayerData, int32_t weapon, int32_t flags);
void DoMuzzleStuff (int32_t nSegment, CFixVector *pos);

int32_t CreateNewWeapon (CFixVector * direction, CFixVector * position, int16_t nSegment, int16_t nParent, uint8_t nType, int32_t makeSound);

// Fires a laser-nType weapon (a Primary weapon)
// Fires from CObject nObject, weapon nType weapon_id.
// Assumes that it is firing from a player CObject, so it knows which
// gun to fire from.
// Returns the number of shots actually fired, which will typically be
// 1, but could be higher for low frame rates when rapidfire weapons,
// such as vulcan or plasma are fired.
int32_t FireWeapon (int16_t nObject, uint8_t weapon_id, int32_t level, int32_t& flags, int32_t nfires);
void FireGun (void);

// Easier to call than CreateNewWeapon because it determines the
// CSegment containing the firing point and deals with it being stuck
// in an CObject or through a CWall.
// Fires a laser of nType "weaponType" from an CObject (parent) in the
// direction "direction" from the position "position"
// Returns CObject number of laser fired or -1 if not possible to fire
// laser.
int16_t CreateClusterLight (CObject *objP);

int32_t CreateNewWeaponSimple(CFixVector * direction, CFixVector * position, int16_t parent, uint8_t weaponType, int32_t makeSound);

// creates a weapon CObject
int32_t CreateWeaponObject (uint8_t weaponType, int16_t nSegment,CFixVector *position, int16_t nParent);

// give up control of the guided missile
void ReleaseGuidedMissile(int32_t player_num);

void CreateSmartChildren (CObject *objp, int32_t count);
int32_t UpdateOmegaLightnings (CObject *parentObjP, CObject *targetObjP);
void StopPrimaryFire (void);
void StopSecondaryFire (void);
float MissileSpeedScale (CObject *objP);

void ChargeFusion (void);
int32_t FusionBump (void);

int32_t GetPlayerGun (int32_t nPlayer, int32_t *bFiring);

void GetPlayerMslLock (void);
CFixVector *GetGunPoints (CObject *objP, int32_t nGun);
CFixVector *TransformGunPoint (CObject *objP, CFixVector *vGunPoints, int32_t nGun, 
										fix xDelay, uint8_t nLaserType, CFixVector *vMuzzle, CFixMatrix *mP);
typedef struct tMuzzleInfo {
	fix         createTime;
	int16_t       nSegment;
	CFixVector  pos;
} __pack__ tMuzzleInfo;

// Omega cannon stuff.
#define DEFAULT_MAX_OMEGA_CHARGE    (I2X (1))  //  Maximum charge level for omega cannonw
extern int32_t nOmegaDuration [7];

// NOTE: OMEGA_CHARGE_SCALE moved to laser.c to avoid long rebuilds if changed

//	-----------------------------------------------------------------------------------------------------------

static inline int32_t LaserPlayerFireSpread (CObject *objP, uint8_t laserType, int32_t nGun, fix spreadr, fix spreadu, 
													  int32_t makeSound, int32_t harmless, int16_t nLightObj)
{
return FireWeaponDelayedWithSpread (objP, laserType, nGun, spreadr, spreadu, 0, makeSound, harmless, nLightObj);
}

//	-----------------------------------------------------------------------------------------------------------

static int32_t inline LaserPlayerFire (CObject *objP, uint8_t laserType, int32_t nGun, int32_t makeSound, int32_t harmless, int16_t nLightObj)
{
return LaserPlayerFireSpread (objP, laserType, nGun, 0, 0, makeSound, harmless, nLightObj);
}

// ---------------------------------------------------------------------------------

#define MAX_OMEGA_CHARGE (gameStates.app.bHaveExtraGameInfo [IsMultiGame] ? gameData.omega.xMaxCharge : DEFAULT_MAX_OMEGA_CHARGE)

//	-----------------------------------------------------------------------------------------------------------

#define HOMING_MSL_FPS			gameData.physics.nHomingMslFPS [extraGameInfo [IsMultiGame].nMslTurnSpeed]
#define HOMING_MSL_FRAMETIME	I2X (1) / HOMING_MSL_FPS

#endif /* _LASER_H */

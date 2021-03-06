#ifdef HAVE_CONFIG_H
#	include <conf.h>
#endif

#include "descent.h"
#include "error.h"
#include "segmath.h"
#include "network.h"
#include "lightning.h"
#include "omega.h"
#include "slowmotion.h"

//	-------------------------------------------------------------------------------------------------------------------------------
//	***** HEY ARTISTS!!*****
//	Here are the constants you're looking for!--MK

//	Change the following constants to affect the look of the omega cannon.
//	Changing these constants will not affect the damage done.
//	WARNING: If you change DESIRED_OMEGA_DIST and MAX_OMEGA_BLOBS, you don't merely change the look of the cannon,
//	you change its range.  If you decrease DESIRED_OMEGA_DIST, you decrease how far the gun can fire.

#define OMEGA_ENERGY_RATE		(I2X (190) / 17)
//	Note, you don't need to change these constants.  You can control damage and energy consumption by changing the
//	usual bitmaps.tbl parameters.
#define	OMEGA_DAMAGE_SCALE			32				//	Controls how much damage is done.  This gets multiplied by gameData.timeData.xFrame and then again by the damage specified in bitmaps.tbl in the $WEAPON line.
#define	OMEGA_ENERGY_CONSUMPTION	16				//	Controls how much energy is consumed.  This gets multiplied by gameData.timeData.xFrame and then again by the energy parameter from bitmaps.tbl.

#define	MIN_OMEGA_CHARGE	 (DEFAULT_MAX_OMEGA_CHARGE/8)
#define	OMEGA_CHARGE_SCALE	4			//	gameData.timeData.xFrame / OMEGA_CHARGE_SCALE added to gameData.omegaData.xCharge [IsMultiGame] every frame.

// ---------------------------------------------------------------------------------

fix OmegaEnergy (fix xDeltaCharge)
{
	fix xEnergyUsed;

if (xDeltaCharge < 0)
	return -1;
xEnergyUsed = FixMul (OMEGA_ENERGY_RATE, xDeltaCharge);
if (gameStates.app.nDifficultyLevel < 2)
	xEnergyUsed = FixMul (xEnergyUsed, I2X (gameStates.app.nDifficultyLevel + 2) / 4);
return xEnergyUsed;
}

// ---------------------------------------------------------------------------------
//	*pObj is the CObject firing the omega cannon
//	*pos is the location from which the omega bolt starts

int32_t nOmegaDuration [7] = {1, 2, 3, 5, 7, 10, 15};

void SetMaxOmegaCharge (void)
{
gameData.omegaData.xMaxCharge = DEFAULT_MAX_OMEGA_CHARGE * nOmegaDuration [int32_t (extraGameInfo [0].nOmegaRamp)];
if (gameData.omegaData.xCharge [IsMultiGame] > gameData.omegaData.xMaxCharge) {
	LOCALPLAYER.UpdateEnergy (OmegaEnergy (gameData.omegaData.xCharge [IsMultiGame] - gameData.omegaData.xMaxCharge));
	gameData.omegaData.xCharge [IsMultiGame] = gameData.omegaData.xMaxCharge;
	}
}

//	-------------------------------------------------------------------------------------------------------------------------------

void DeleteOldOmegaBlobs (CObject *pParentObj)
{
	int32_t	nParentObj = pParentObj->cType.laserInfo.parent.nObject;
	CObject*	pObj;

FORALL_WEAPON_OBJS (pObj)
	if ((pObj->info.nId == OMEGA_ID) && (pObj->cType.laserInfo.parent.nObject == nParentObj))
		ReleaseObject (pObj->Index ());
}

// ---------------------------------------------------------------------------------

void CreateOmegaBlobs (int16_t nFiringSeg, CFixVector *vMuzzle, CFixVector *vTargetPos, CObject *pParentObj, CObject *pTargetObj)
{
	int16_t		nLastSeg, nLastCreatedObj = -1;
	CFixVector	vGoal;
	fix			xGoalDist;
	int32_t		nOmegaBlobs;
	fix			xOmegaBlobDist;
	CFixVector	vOmegaDelta;
	CFixVector	vBlobPos, vPerturb;
	fix			xPerturbArray [MAX_OMEGA_BLOBS];
	int32_t		i;

if (IsMultiGame)
	DeleteOldOmegaBlobs (pParentObj);
omegaLightning.Create (vTargetPos, pParentObj, pTargetObj);
vGoal = *vTargetPos - *vMuzzle;
xGoalDist = CFixVector::Normalize (vGoal);
if (xGoalDist < MIN_OMEGA_BLOBS * MIN_OMEGA_BLOB_DIST) {
	xOmegaBlobDist = MIN_OMEGA_BLOB_DIST;
	nOmegaBlobs = xGoalDist / xOmegaBlobDist;
	if (nOmegaBlobs == 0)
		nOmegaBlobs = 1;
	}
else {
	xOmegaBlobDist = MAX_OMEGA_BLOB_DIST;
	nOmegaBlobs = xGoalDist / xOmegaBlobDist;
	if (nOmegaBlobs > gameData.MaxOmegaBlobs ()) {
		nOmegaBlobs = gameData.MaxOmegaBlobs ();
		xOmegaBlobDist = xGoalDist / nOmegaBlobs;
		}
	else if (nOmegaBlobs < MIN_OMEGA_BLOBS) {
		nOmegaBlobs = MIN_OMEGA_BLOBS;
		xOmegaBlobDist = xGoalDist / nOmegaBlobs;
		}
	}
vOmegaDelta = vGoal;
vOmegaDelta *= xOmegaBlobDist;
//	Now, create all the blobs
vBlobPos = *vMuzzle;
nLastSeg = nFiringSeg;

//	If nearby, don't perturb vector.  If not nearby, start halfway out.
if (xGoalDist < MIN_OMEGA_BLOB_DIST * 4) {
	for (i = 0; i < nOmegaBlobs; i++)
		xPerturbArray [i] = 0;
	}
else {
	vBlobPos += vOmegaDelta * (I2X (1) / 2);	//	Put first blob half way out.
	for (i = 0; i < nOmegaBlobs / 2; i++) {
		xPerturbArray [i] = I2X (i) + I2X (1) / 4;
		xPerturbArray [nOmegaBlobs - 1 - i] = I2X (i);
		}
	}

//	Create Random perturbation vector, but favor _not_ going up in CPlayerData's reference.
vPerturb = CFixVector::Random ();
vPerturb += pParentObj->info.position.mOrient.m.dir.u * (-I2X (1) / 2);
for (i = 0; i < nOmegaBlobs; i++) {
	CFixVector	vTempPos;
	int16_t		nBlobObj, nSegment;

	//	This will put the last blob right at the destination CObject, causing damage.
	if (i == nOmegaBlobs - 1)
		vBlobPos += vOmegaDelta * (I2X (15) / 32);	//	Move last blob another (almost) half section
	//	Every so often, re-perturb blobs
	if (i % 4 == 3) {
		CFixVector	vTemp;

		vTemp = CFixVector::Random ();
		vPerturb += vTemp * (I2X (1) / 4);
		}
	vTempPos = vBlobPos + vPerturb * xPerturbArray[i];
	nSegment = FindSegByPos (vTempPos, nLastSeg, 1, 0);
	if (nSegment != -1) {
		CObject		*pObj;

		nLastSeg = nSegment;
		nBlobObj = CreateWeapon (OMEGA_ID, -1, nSegment, vTempPos, 0, RT_WEAPON_VCLIP);
		if (nBlobObj == -1)
			break;
		nLastCreatedObj = nBlobObj;
		pObj = OBJECT (nBlobObj);
		pObj->SetLife (ONE_FRAME_TIME);
		pObj->mType.physInfo.velocity = vGoal;
		//	Only make the last one move fast, else multiple blobs might collide with target.
		pObj->mType.physInfo.velocity *= (I2X (4));
		pObj->SetSize (WI_BlobSize (pObj->info.nId));
		pObj->info.xShield = FixMul (OMEGA_DAMAGE_SCALE * gameData.timeData.xFrame, WI_Strength (pObj->info.nId, gameStates.app.nDifficultyLevel));
		if (gameData.BoostOmega ())
			pObj->info.xShield *= 4;
		pObj->cType.laserInfo.parent.nType = pParentObj->info.nType;
		pObj->cType.laserInfo.parent.nSignature = pParentObj->info.nSignature;
		pObj->cType.laserInfo.parent.nObject = OBJ_IDX (pParentObj);
		pObj->info.movementType = MT_NONE;	//	Only last one moves, that will get bashed below.
		}
	vBlobPos += vOmegaDelta;
	}

	//	Make last one move faster, but it's already moving at speed = I2X (4).
if (nLastCreatedObj != -1) {
	CObject* pObj = OBJECT (nLastCreatedObj);
	pObj->mType.physInfo.velocity *= WI_Speed (OMEGA_ID, gameStates.app.nDifficultyLevel) / 4;
	pObj->info.movementType = MT_PHYSICS;
	}
}

// ---------------------------------------------------------------------------------
//	Call this every frame to recharge the Omega Cannon.
void OmegaChargeFrame (void)
{
	fix	xOldOmegaCharge;

if (gameData.omegaData.xCharge [IsMultiGame] == MAX_OMEGA_CHARGE) {
	omegaLightning.Destroy (LOCALPLAYER.nObject);
	return;
	}
if (!(PlayerHasWeapon (OMEGA_INDEX, 0, -1, 0) & HAS_WEAPON_FLAG)) {
	omegaLightning.Destroy (LOCALPLAYER.nObject);
	return;
	}
if (gameStates.app.bPlayerIsDead) {
	omegaLightning.Destroy (LOCALPLAYER.nObject);
	return;
	}
if ((gameData.weaponData.nPrimary == OMEGA_INDEX) && !gameData.omegaData.xCharge [IsMultiGame] && !LOCALPLAYER.Energy ()) {
	omegaLightning.Destroy (LOCALPLAYER.nObject);
	gameData.weaponData.nPrimary--;
	AutoSelectWeapon (0, 1);
	}
//	Don't charge while firing.
if ((gameData.omegaData.nLastFireFrame == gameData.appData.nFrameCount) ||
	 (gameData.omegaData.nLastFireFrame == gameData.appData.nFrameCount - 1))
	return;

omegaLightning.Destroy (LOCALPLAYER.nObject);
if (LOCALPLAYER.Energy ()) {
	xOldOmegaCharge = gameData.omegaData.xCharge [IsMultiGame];
	gameData.omegaData.xCharge [IsMultiGame] += (fix) (gameData.timeData.xFrame / OMEGA_CHARGE_SCALE / gameStates.gameplay.slowmo [0].fSpeed);
	if (gameData.omegaData.xCharge [IsMultiGame] > MAX_OMEGA_CHARGE)
		gameData.omegaData.xCharge [IsMultiGame] = MAX_OMEGA_CHARGE;
	LOCALPLAYER.UpdateEnergy (-OmegaEnergy (gameData.omegaData.xCharge [IsMultiGame] - xOldOmegaCharge));
	}
}

// -- fix	Last_omega_muzzle_flashTime;

// ---------------------------------------------------------------------------------

void DoOmegaStuff (CObject *pParentObj, CFixVector *vMuzzle, CObject *pWeaponObj)
{
	int16_t		nTargetObj, nFiringSeg, nParentSeg;
	CFixVector	vTargetPos;
	int32_t		nPlayer = (pParentObj && (pParentObj->info.nType == OBJ_PLAYER)) ? pParentObj->info.nId : -1;
	int32_t		bSpectate = SPECTATOR (pParentObj);
	static		int32_t nDelay = 0;

#if 1
if (gameStates.gameplay.bMineMineCheat && (gameData.omegaData.xCharge [IsMultiGame] < MAX_OMEGA_CHARGE))
	gameData.omegaData.xCharge [IsMultiGame] = MAX_OMEGA_CHARGE - 1;
#endif
if (nPlayer == N_LOCALPLAYER) {
	//	If charge >= min, or (some charge and zero energy), allow to fire.
	if (((RandShort () > pParentObj->GunDamage ()) || (gameData.omegaData.xCharge [IsMultiGame] < MIN_OMEGA_CHARGE)) &&
		 (!gameData.omegaData.xCharge [IsMultiGame] || PLAYER (nPlayer).energy)) {
		ReleaseObject (OBJ_IDX (pWeaponObj));
		omegaLightning.Destroy (LOCALPLAYER.nObject);
		return;
		}
	gameData.omegaData.xCharge [IsMultiGame] -= gameData.timeData.xFrame;
	if (gameData.omegaData.xCharge [IsMultiGame] < 0)
		gameData.omegaData.xCharge [IsMultiGame] = 0;
	//	Ensure that the lightning cannon can be fired next frame.
	gameData.laserData.xNextFireTime = gameData.timeData.xGame + 1;
	gameData.omegaData.nLastFireFrame = gameData.appData.nFrameCount;
	}

pWeaponObj->cType.laserInfo.parent.nType = pParentObj->info.nType;
pWeaponObj->cType.laserInfo.parent.nObject = pParentObj ? OBJ_IDX (pParentObj) : -1;
pWeaponObj->cType.laserInfo.parent.nSignature = pParentObj->info.nSignature;

if (gameStates.limitFPS.bOmega && !gameStates.app.tick40fps.bTick)
#if 1
	return;
if (SlowMotionActive ()) {
	if (nDelay > 0) {
		nDelay -= 2;
		if	(nDelay > 0)
			return;
		}
	nDelay += gameOpts->gameplay.nSlowMotionSpeedup;
	}
#else
	nTargetObj = -1;
else
#endif
	nTargetObj = pWeaponObj->FindVisibleHomingTarget (*vMuzzle, MAX_THREADS);
nParentSeg = bSpectate ? gameStates.app.nPlayerSegment : pParentObj->info.nSegment;

if (0 > (nFiringSeg = FindSegByPos (*vMuzzle, nParentSeg, 1, 0))) {
	omegaLightning.Destroy (OBJ_IDX (pParentObj));
	return;
	}
//	Play sound.
CWeaponInfo *pWeaponInfo = WEAPONINFO (pWeaponObj);
if (pWeaponInfo) {
	if (pParentObj == gameData.objData.pViewer)
		audio.PlaySound (pWeaponInfo->flashSound);
	else
		audio.CreateSegmentSound (pWeaponInfo->flashSound, pWeaponObj->info.nSegment, 0, pWeaponObj->info.position.vPos, 0, I2X (1));
	}
//	Delete the original CObject.  Its only purpose in life was to determine which CObject to home in on.
ReleaseObject (OBJ_IDX (pWeaponObj));
if (nTargetObj != -1)
	vTargetPos = OBJECT (nTargetObj)->info.position.vPos;
else {	//	If couldn't lock on anything, fire straight ahead.
	CFixVector	vPerturb, perturbed_fvec;

	vPerturb = CFixVector::Random();
	perturbed_fvec = bSpectate ? gameStates.app.playerPos.mOrient.m.dir.f : pParentObj->info.position.mOrient.m.dir.f + vPerturb * (I2X (1) / 16);
	vTargetPos = *vMuzzle + perturbed_fvec * gameData.MaxOmegaRange ();
	CHitQuery	hitQuery (FQ_IGNORE_POWERUPS | FQ_TRANSPOINT | FQ_CHECK_OBJS, vMuzzle, &vTargetPos, nFiringSeg, OBJ_IDX (pParentObj), 0, 0, ++gameData.physicsData.bIgnoreObjFlag);
	CHitResult	hitResult;
	int32_t fate = FindHitpoint (hitQuery, hitResult);
	if (fate != HIT_NONE) {
		if (hitResult.nSegment != -1)		//	How can this be?  We went from inside the mine to outside without hitting anything?
			vTargetPos = hitResult.vPoint;
		else {
#if DBG
			fate = FindHitpoint (hitQuery, hitResult);
#endif
			return;
			}
		}
	}
//	This is where we create a pile of omega blobs!
CreateOmegaBlobs (nFiringSeg, vMuzzle, &vTargetPos, pParentObj, (nTargetObj < 0) ? NULL : OBJECT (nTargetObj));
}

// ---------------------------------------------------------------------------------

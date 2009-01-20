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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "inferno.h"
#include "error.h"
#include "state.h"

#ifdef EDITOR
#include "editor/editor.h"
#endif

#include "string.h"
//#define _DEBUG
#if DBG
#include <time.h>
#endif

#undef DBG
#if DBG
#	define DBG(_expr)	_expr
#else
#	define DBG(_expr)
#endif

//	-------------------------------------------------------------------------------------------------

int CSaveGameManager::LoadAIBinFormat (void)
{
	int	i;

gameData.ai.localInfo.Clear ();
gameData.ai.routeSegs.Clear ();
m_cf.Read (&gameData.ai.bInitialized, sizeof (int), 1);
m_cf.Read (&gameData.ai.nOverallAgitation, sizeof (int), 1);
gameData.ai.localInfo.Read (m_cf, (m_nVersion > 39) ? LEVEL_OBJECTS : (m_nVersion > 22) ? MAX_OBJECTS : MAX_OBJECTS_D2);
gameData.ai.routeSegs.Read (m_cf, (m_nVersion > 39) ? LEVEL_POINT_SEGS : (m_nVersion > 22) ? MAX_POINT_SEGS : MAX_POINT_SEGS_D2);
gameData.ai.cloakInfo.Read (m_cf);
if (m_nVersion < 29) {
	m_cf.Read (&gameData.bosses [0].nCloakStartTime, sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nCloakEndTime , sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nLastTeleportTime , sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nTeleportInterval, sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nCloakInterval, sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nCloakDuration, sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nLastGateTime, sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nGateInterval, sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nDyingStartTime, sizeof (fix), 1);
	m_cf.Read (&gameData.bosses [0].nDying, sizeof (int), 1);
	m_cf.Read (&gameData.bosses [0].bDyingSoundPlaying, sizeof (int), 1);
	m_cf.Read (&gameData.bosses [0].nHitTime, sizeof (fix), 1);
	for (i = 1; i < MAX_BOSS_COUNT; i++) {
		gameData.bosses [i].nCloakStartTime = 0;
		gameData.bosses [i].nCloakEndTime = 0;
		gameData.bosses [i].nLastTeleportTime = 0;
		gameData.bosses [i].nTeleportInterval = 0;
		gameData.bosses [i].nCloakInterval = 0;
		gameData.bosses [i].nCloakDuration = 0;
		gameData.bosses [i].nLastGateTime = 0;
		gameData.bosses [i].nGateInterval = 0;
		gameData.bosses [i].nDyingStartTime = 0;
		gameData.bosses [i].nDying = 0;
		gameData.bosses [i].bDyingSoundPlaying = 0;
		gameData.bosses [i].nHitTime = 0;
		}
	}
else {
	for (i = 0; i < MAX_BOSS_COUNT; i++) {
		m_cf.Read (&gameData.bosses [i].nCloakStartTime, sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nCloakEndTime , sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nLastTeleportTime , sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nTeleportInterval, sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nCloakInterval, sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nCloakDuration, sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nLastGateTime, sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nGateInterval, sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nDyingStartTime, sizeof (fix), 1);
		m_cf.Read (&gameData.bosses [i].nDying, sizeof (int), 1);
		m_cf.Read (&gameData.bosses [i].bDyingSoundPlaying, sizeof (int), 1);
		m_cf.Read (&gameData.bosses [i].nHitTime, sizeof (fix), 1);
		}
	}
// -- MK, 10/21/95, unused!-- m_cf.Read (&Boss_been_hit, sizeof (int), 1);

if (m_nVersion >= 8) {
	m_cf.Read (&gameData.escort.nKillObject, sizeof (gameData.escort.nKillObject), 1);
	m_cf.Read (&gameData.escort.xLastPathCreated, sizeof (gameData.escort.xLastPathCreated), 1);
	m_cf.Read (&gameData.escort.nGoalObject, sizeof (gameData.escort.nGoalObject), 1);
	m_cf.Read (&gameData.escort.nSpecialGoal, sizeof (gameData.escort.nSpecialGoal), 1);
	m_cf.Read (&gameData.escort.nGoalIndex, sizeof (gameData.escort.nGoalIndex), 1);
	gameData.thief.stolenItems.Read (m_cf);
	}
else {
	gameData.escort.nKillObject = -1;
	gameData.escort.xLastPathCreated = 0;
	gameData.escort.nGoalObject = ESCORT_GOAL_UNSPECIFIED;
	gameData.escort.nSpecialGoal = -1;
	gameData.escort.nGoalIndex = -1;
	gameData.thief.stolenItems.Clear (0xff);
	}
if (m_nVersion >= 15) {
	int temp;
	m_cf.Read (&temp, sizeof (int), 1);
	gameData.ai.freePointSegs = gameData.ai.routeSegs + temp;
	}
else
	AIResetAllPaths ();
if (m_nVersion >= 24) {
	for (i = 0; i < MAX_BOSS_COUNT; i++)
		m_cf.Read (&gameData.bosses [i].nTeleportSegs, sizeof (gameData.bosses [i].nTeleportSegs), 1);
	for (i = 0; i < MAX_BOSS_COUNT; i++)
		m_cf.Read (&gameData.bosses [i].nGateSegs, sizeof (gameData.bosses [i].nGateSegs), 1);
	for (i = 0; i < MAX_BOSS_COUNT; i++) {
		if (gameData.bosses [i].nGateSegs)
			m_cf.Read (gameData.bosses [i].gateSegs.Buffer (), sizeof (gameData.bosses [i].gateSegs [0]), gameData.bosses [i].nGateSegs);
		if (gameData.bosses [i].nTeleportSegs)
			m_cf.Read (gameData.bosses [i].teleportSegs.Buffer (), sizeof (gameData.bosses [i].teleportSegs [0]), gameData.bosses [i].nTeleportSegs);
		}
	}
else if (m_nVersion >= 21) {
	short nTeleportSegs, nGateSegs;

	m_cf.Read (&nTeleportSegs, sizeof (nTeleportSegs), 1);
	m_cf.Read (&nGateSegs, sizeof (nGateSegs), 1);

	if (nGateSegs) {
		gameData.bosses [0].nGateSegs = nGateSegs;
		m_cf.Read (gameData.bosses [0].gateSegs.Buffer (), sizeof (gameData.bosses [0].gateSegs [0]), nGateSegs);
		}
	if (nTeleportSegs) {
		gameData.bosses [0].nTeleportSegs = nTeleportSegs;
		m_cf.Read (gameData.bosses [0].teleportSegs.Buffer (), sizeof (gameData.bosses [0].teleportSegs [0]), nTeleportSegs);
		}
} else {
	// -- gameData.bosses.nTeleportSegs = 1;
	// -- gameData.bosses.nGateSegs = 1;
	// -- gameData.bosses.teleportSegs [0] = 0;
	// -- gameData.bosses.gateSegs [0] = 0;
	// Note: Maybe better to leave alone...will probably be ok.
#if TRACE
	console.printf (1, "Warning: If you fight the boss, he might teleport to CSegment #0!\n");
#endif
	}
return 1;
}
//	-------------------------------------------------------------------------------------------------

void CSaveGameManager::SaveAILocalInfo (tAILocalInfo *ailP)
{
	int	i;

m_cf.WriteInt (ailP->playerAwarenessType);
m_cf.WriteInt (ailP->nRetryCount);
m_cf.WriteInt (ailP->nConsecutiveRetries);
m_cf.WriteInt (ailP->mode);
m_cf.WriteInt (ailP->nPrevVisibility);
m_cf.WriteInt (ailP->nRapidFireCount);
m_cf.WriteInt (ailP->nGoalSegment);
m_cf.WriteFix (ailP->nextActionTime);
m_cf.WriteFix (ailP->nextPrimaryFire);
m_cf.WriteFix (ailP->nextSecondaryFire);
m_cf.WriteFix (ailP->playerAwarenessTime);
m_cf.WriteFix (ailP->timePlayerSeen);
m_cf.WriteFix (ailP->timePlayerSoundAttacked);
m_cf.WriteFix (ailP->nextMiscSoundTime);
m_cf.WriteFix (ailP->timeSinceProcessed);
for (i = 0; i < MAX_SUBMODELS; i++) {
	m_cf.WriteAngVec (ailP->goalAngles[i]);
	m_cf.WriteAngVec (ailP->deltaAngles[i]);
	}
m_cf.Write (ailP->goalState, sizeof (ailP->goalState [0]), 1);
m_cf.Write (ailP->achievedState, sizeof (ailP->achievedState [0]), 1);
}

//	-------------------------------------------------------------------------------------------------

void CSaveGameManager::SaveAIPointSeg (tPointSeg *psegP)
{
m_cf.WriteInt (psegP->nSegment);
m_cf.WriteVector (psegP->point);
}

//	-------------------------------------------------------------------------------------------------

void CSaveGameManager::SaveAICloakInfo (tAICloakInfo *ciP)
{
m_cf.WriteFix (ciP->lastTime);
m_cf.WriteInt (ciP->nLastSeg);
m_cf.WriteVector (ciP->vLastPos);
}

//	-------------------------------------------------------------------------------------------------

int CSaveGameManager::SaveAI (void)
{
	int	h, i, j;

m_cf.WriteInt (gameData.ai.bInitialized);
m_cf.WriteInt (gameData.ai.nOverallAgitation);
for (i = 0; i < LEVEL_OBJECTS; i++)
	SaveAILocalInfo (gameData.ai.localInfo + i);
for (i = 0; i < LEVEL_POINT_SEGS; i++)
	SaveAIPointSeg (gameData.ai.routeSegs + i);
for (i = 0; i < MAX_AI_CLOAK_INFO; i++)
	SaveAICloakInfo (gameData.ai.cloakInfo + i);
for (i = 0; i < MAX_BOSS_COUNT; i++) {
	m_cf.WriteShort (gameData.bosses [i].nObject);
	m_cf.WriteFix (gameData.bosses [i].nCloakStartTime);
	m_cf.WriteFix (gameData.bosses [i].nCloakEndTime );
	m_cf.WriteFix (gameData.bosses [i].nLastTeleportTime );
	m_cf.WriteFix (gameData.bosses [i].nTeleportInterval);
	m_cf.WriteFix (gameData.bosses [i].nCloakInterval);
	m_cf.WriteFix (gameData.bosses [i].nCloakDuration);
	m_cf.WriteFix (gameData.bosses [i].nLastGateTime);
	m_cf.WriteFix (gameData.bosses [i].nGateInterval);
	m_cf.WriteFix (gameData.bosses [i].nDyingStartTime);
	m_cf.WriteInt (gameData.bosses [i].nDying);
	m_cf.WriteInt (gameData.bosses [i].bDyingSoundPlaying);
	m_cf.WriteFix (gameData.bosses [i].nHitTime);
	}
m_cf.WriteInt (gameData.escort.nKillObject);
m_cf.WriteFix (gameData.escort.xLastPathCreated);
m_cf.WriteInt (gameData.escort.nGoalObject);
m_cf.WriteInt (gameData.escort.nSpecialGoal);
m_cf.WriteInt (gameData.escort.nGoalIndex);
gameData.thief.stolenItems.Write (m_cf);
#if DBG
i = CFTell ();
#endif
m_cf.WriteInt ((int) (gameData.ai.freePointSegs - gameData.ai.routeSegs.Buffer ()));
for (i = 0; i < MAX_BOSS_COUNT; i++) {
	m_cf.WriteShort (gameData.bosses [i].nTeleportSegs);
	m_cf.WriteShort (gameData.bosses [i].nGateSegs);
	}
for (i = 0; i < MAX_BOSS_COUNT; i++) {
	if ((h = gameData.bosses [i].nGateSegs))
		for (j = 0; j < h; j++)
			m_cf.WriteShort (gameData.bosses [i].gateSegs [j]);
	if ((h = gameData.bosses [i].nTeleportSegs))
		for (j = 0; j < h; j++)
			m_cf.WriteShort (gameData.bosses [i].teleportSegs [j]);
	}
return 1;
}

//	-------------------------------------------------------------------------------------------------

void CSaveGameManager::LoadAILocalInfo (tAILocalInfo *ailP)
{
	int	i;

ailP->playerAwarenessType = m_cf.ReadInt ();
ailP->nRetryCount = m_cf.ReadInt ();
ailP->nConsecutiveRetries = m_cf.ReadInt ();
ailP->mode = m_cf.ReadInt ();
ailP->nPrevVisibility = m_cf.ReadInt ();
ailP->nRapidFireCount = m_cf.ReadInt ();
ailP->nGoalSegment = m_cf.ReadInt ();
ailP->nextActionTime = m_cf.ReadFix ();
ailP->nextPrimaryFire = m_cf.ReadFix ();
ailP->nextSecondaryFire = m_cf.ReadFix ();
ailP->playerAwarenessTime = m_cf.ReadFix ();
ailP->timePlayerSeen = m_cf.ReadFix ();
ailP->timePlayerSoundAttacked = m_cf.ReadFix ();
ailP->nextMiscSoundTime = m_cf.ReadFix ();
ailP->timeSinceProcessed = m_cf.ReadFix ();
for (i = 0; i < MAX_SUBMODELS; i++) {
	m_cf.ReadAngVec (ailP->goalAngles [i]);
	m_cf.ReadAngVec (ailP->deltaAngles [i]);
	}
m_cf.Read (ailP->goalState, sizeof (ailP->goalState [0]), 1);
m_cf.Read (ailP->achievedState, sizeof (ailP->achievedState [0]), 1);
}

//	-------------------------------------------------------------------------------------------------

void CSaveGameManager::LoadAIPointSeg (tPointSeg *psegP)
{
psegP->nSegment = m_cf.ReadInt ();
m_cf.ReadVector (psegP->point);
}

//	-------------------------------------------------------------------------------------------------

void CSaveGameManager::LoadAICloakInfo (tAICloakInfo *ciP)
{
ciP->lastTime = m_cf.ReadFix ();
ciP->nLastSeg = m_cf.ReadInt ();
m_cf.ReadVector (ciP->vLastPos);
}

//	-------------------------------------------------------------------------------------------------

int CSaveGameManager::LoadAIUniFormat (void)
{
	int	h, i, j, nMaxBossCount, nMaxPointSegs;

gameData.ai.localInfo.Clear ();
gameData.ai.bInitialized = m_cf.ReadInt ();
gameData.ai.nOverallAgitation = m_cf.ReadInt ();
h = (m_nVersion > 39) ? LEVEL_OBJECTS : (m_nVersion > 22) ? MAX_OBJECTS : MAX_OBJECTS_D2;
DBG (i = CFTell (fp));
for (i = 0; i < h; i++)
	LoadAILocalInfo (gameData.ai.localInfo + i);
nMaxPointSegs = (m_nVersion > 39) ? LEVEL_POINT_SEGS : (m_nVersion > 22) ? MAX_POINT_SEGS : MAX_POINT_SEGS_D2;
gameData.ai.routeSegs.Clear ();
DBG (i = CFTell (fp));
for (i = 0; i < nMaxPointSegs; i++)
	LoadAIPointSeg (gameData.ai.routeSegs + i);
DBG (i = CFTell (fp));
for (i = 0; i < MAX_AI_CLOAK_INFO; i++)
	LoadAICloakInfo (gameData.ai.cloakInfo + i);
DBG (i = CFTell (fp));
if (m_nVersion < 29) {
	gameData.bosses [0].nCloakStartTime = m_cf.ReadFix ();
	gameData.bosses [0].nCloakEndTime = m_cf.ReadFix ();
	gameData.bosses [0].nLastTeleportTime = m_cf.ReadFix ();
	gameData.bosses [0].nTeleportInterval = m_cf.ReadFix ();
	gameData.bosses [0].nCloakInterval = m_cf.ReadFix ();
	gameData.bosses [0].nCloakDuration = m_cf.ReadFix ();
	gameData.bosses [0].nLastGateTime = m_cf.ReadFix ();
	gameData.bosses [0].nGateInterval = m_cf.ReadFix ();
	gameData.bosses [0].nDyingStartTime = m_cf.ReadFix ();
	gameData.bosses [0].nDying = m_cf.ReadInt ();
	gameData.bosses [0].bDyingSoundPlaying = m_cf.ReadInt ();
	gameData.bosses [0].nHitTime = m_cf.ReadFix ();
	for (i = 1; i < MAX_BOSS_COUNT; i++) {
		gameData.bosses [i].nCloakStartTime = 0;
		gameData.bosses [i].nCloakEndTime = 0;
		gameData.bosses [i].nLastTeleportTime = 0;
		gameData.bosses [i].nTeleportInterval = 0;
		gameData.bosses [i].nCloakInterval = 0;
		gameData.bosses [i].nCloakDuration = 0;
		gameData.bosses [i].nLastGateTime = 0;
		gameData.bosses [i].nGateInterval = 0;
		gameData.bosses [i].nDyingStartTime = 0;
		gameData.bosses [i].nDying = 0;
		gameData.bosses [i].bDyingSoundPlaying = 0;
		gameData.bosses [i].nHitTime = 0;
		}
	}
else {
	for (i = 0; i < MAX_BOSS_COUNT; i++) {
		if (m_nVersion > 31)
			gameData.bosses [i].nObject = m_cf.ReadShort ();
		gameData.bosses [i].nCloakStartTime = m_cf.ReadFix ();
		gameData.bosses [i].nCloakEndTime = m_cf.ReadFix ();
		gameData.bosses [i].nLastTeleportTime = m_cf.ReadFix ();
		gameData.bosses [i].nTeleportInterval = m_cf.ReadFix ();
		gameData.bosses [i].nCloakInterval = m_cf.ReadFix ();
		gameData.bosses [i].nCloakDuration = m_cf.ReadFix ();
		gameData.bosses [i].nLastGateTime = m_cf.ReadFix ();
		gameData.bosses [i].nGateInterval = m_cf.ReadFix ();
		gameData.bosses [i].nDyingStartTime = m_cf.ReadFix ();
		gameData.bosses [i].nDying = m_cf.ReadInt ();
		gameData.bosses [i].bDyingSoundPlaying = m_cf.ReadInt ();
		gameData.bosses [i].nHitTime = m_cf.ReadFix ();
		}
	}
if (m_nVersion >= 8) {
	gameData.escort.nKillObject = m_cf.ReadInt ();
	gameData.escort.xLastPathCreated = m_cf.ReadFix ();
	gameData.escort.nGoalObject = m_cf.ReadInt ();
	gameData.escort.nSpecialGoal = m_cf.ReadInt ();
	gameData.escort.nGoalIndex = m_cf.ReadInt ();
	gameData.thief.stolenItems.Read (m_cf);
	}
else {
	gameData.escort.nKillObject = -1;
	gameData.escort.xLastPathCreated = 0;
	gameData.escort.nGoalObject = ESCORT_GOAL_UNSPECIFIED;
	gameData.escort.nSpecialGoal = -1;
	gameData.escort.nGoalIndex = -1;
	gameData.thief.stolenItems.Clear (char (0xff));
	}

if (m_nVersion >= 15) {
	DBG (i = CFTell (fp));
	i = m_cf.ReadInt ();
	if ((i >= 0) && (i < nMaxPointSegs))
		gameData.ai.freePointSegs = gameData.ai.routeSegs + i;
	else
		AIResetAllPaths ();
	}
else
	AIResetAllPaths ();

if (m_nVersion < 21) {
	#if TRACE
	console.printf (1, "Warning: If you fight the boss, he might teleport to CSegment #0!\n");
	#endif
	}
else {
	nMaxBossCount = (m_nVersion >= 24) ? MAX_BOSS_COUNT : 1;
	for (i = 0; i < MAX_BOSS_COUNT; i++) {
		gameData.bosses [i].nTeleportSegs = m_cf.ReadShort ();
		gameData.bosses [i].nGateSegs = m_cf.ReadShort ();
		}
	for (i = 0; i < MAX_BOSS_COUNT; i++) {
		if ((h = gameData.bosses [i].nGateSegs))
			for (j = 0; j < h; j++)
				gameData.bosses [i].gateSegs [j] = m_cf.ReadShort ();
		if ((h = gameData.bosses [i].nTeleportSegs))
			for (j = 0; j < h; j++)
				gameData.bosses [i].teleportSegs [j] = m_cf.ReadShort ();
		}
	}
return 1;
}

//	-------------------------------------------------------------------------------------------------

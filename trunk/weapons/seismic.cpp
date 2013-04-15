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
#include <string.h>

#include "descent.h"
#include "error.h"
#include "text.h"
#include "network.h"

//	-----------------------------------------------------------------------------

#define	ESHAKER_SHAKE_TIME		(I2X (2))
#define	MAX_ESHAKER_DETONATES	4

fix	eshakerDetonateTimes [MAX_ESHAKER_DETONATES];
float	eshakerDetonateScales [MAX_ESHAKER_DETONATES];

//	Call this to initialize for a new level.
//	Sets all super mega missile detonation times to 0 which means there aren't any.
void InitShakerDetonates (void)
{
memset (eshakerDetonateTimes, 0, sizeof (eshakerDetonateTimes));
memset (eshakerDetonateScales, 0, sizeof (eshakerDetonateScales));
}

//	-----------------------------------------------------------------------------

static void RockPlayerShip (int fc, float fScaleMod = 1.0f)
{
if (fc > 16)
	fc = 16;
else if (fc == 0)
	fc = 1;
gameStates.gameplay.seismic.nVolume += fc;
float fScale = X2F (I2X (3) / 16 + (I2X (16 - fc)) / 32);
if (fScaleMod > 0.0f)
	fScale *= fScaleMod;
//HUDMessage (0, "shaker rock scale %d: %1.2f", i, fScale);
int rx = fix (SRandShort () * fScale + 0.5f);
int rz = fix (SRandShort () * fScale + 0.5f);
gameData.objs.consoleP->mType.physInfo.rotVel.v.coord.x += rx;
gameData.objs.consoleP->mType.physInfo.rotVel.v.coord.z += rz;
//	Shake the buddy!
if (gameData.escort.nObjNum != -1) {
	OBJECTS [gameData.escort.nObjNum].mType.physInfo.rotVel.v.coord.x += rx * 4;
	OBJECTS [gameData.escort.nObjNum].mType.physInfo.rotVel.v.coord.z += rz * 4;
	}
//	Shake a guided missile!
gameStates.gameplay.seismic.nMagnitude += rx;
} 

//	-----------------------------------------------------------------------------

//	If a smega missile been detonated, rock the mine!
//	This should be called every frame.
//	Maybe this should affect all robots, being called when they get their physics done.
void RockTheMineFrame (void)
{
for (int i = 0; i < MAX_ESHAKER_DETONATES; i++) {
	if (eshakerDetonateTimes [i] != 0) {
		fix deltaTime = gameData.time.xGame - eshakerDetonateTimes [i];
		if (!gameStates.gameplay.seismic.bSound) {
			audio.PlayLoopingSound ((short) gameStates.gameplay.seismic.nSound, I2X (1), -1, -1);
			gameStates.gameplay.seismic.bSound = 1;
			gameStates.gameplay.seismic.nNextSoundTime = gameData.time.xGame + RandShort () / 2;
			}
		if (deltaTime >= ESHAKER_SHAKE_TIME) {
			eshakerDetonateTimes [i] = 0;
			eshakerDetonateScales [i] = 0.0f;
			}
		else {
#if 1
			RockPlayerShip ((ESHAKER_SHAKE_TIME - deltaTime) / (ESHAKER_SHAKE_TIME / 16), eshakerDetonateScales [i]);
#else
			//	Control center destroyed, rock the player's ship.
			int	fc, rx, rz;
			// -- fc = abs(deltaTime - ESHAKER_SHAKE_TIME/2);
			//	Changed 10/23/95 to make decreasing for super mega missile.
			fc = (ESHAKER_SHAKE_TIME - deltaTime) / (ESHAKER_SHAKE_TIME / 16);
			if (fc > 16)
				fc = 16;
			else if (fc == 0)
				fc = 1;
			gameStates.gameplay.seismic.nVolume += fc;
			float fScale = X2F (I2X (3) / 16 + (I2X (16 - fc)) / 32);
			if (eshakerDetonateScales [i] > 0.0f)
				fScale *= eshakerDetonateScales [i];
			//HUDMessage (0, "shaker rock scale %d: %1.2f", i, fScale);
			rx = fix (SRandShort () * fScale + 0.5f);
			rz = fix (SRandShort () * fScale + 0.5f);
			gameData.objs.consoleP->mType.physInfo.rotVel.v.coord.x += rx;
			gameData.objs.consoleP->mType.physInfo.rotVel.v.coord.z += rz;
			//	Shake the buddy!
			if (gameData.escort.nObjNum != -1) {
				OBJECTS [gameData.escort.nObjNum].mType.physInfo.rotVel.v.coord.x += rx * 4;
				OBJECTS [gameData.escort.nObjNum].mType.physInfo.rotVel.v.coord.z += rz * 4;
				}
			//	Shake a guided missile!
			gameStates.gameplay.seismic.nMagnitude += rx;
#endif
			} 
		}
	}
//	Hook in the rumble sound effect here.
}

//	-----------------------------------------------------------------------------

#define	SEISMIC_DISTURBANCE_DURATION	(I2X (5))

int SeismicLevel (void)
{
return gameStates.gameplay.seismic.nLevel;
}

//	-----------------------------------------------------------------------------

void InitSeismicDisturbances (void)
{
gameStates.gameplay.seismic.nStartTime = 0;
gameStates.gameplay.seismic.nEndTime = 0;
}

//	-----------------------------------------------------------------------------
//	Return true if time to start a seismic disturbance.
int StartSeismicDisturbance (void)
{
if (gameStates.gameplay.seismic.nShakeDuration < 1)
	return 0;
#if 0
int rval = (2 * FixMul (RandShort (), gameStates.gameplay.seismic.nShakeFrequency)) < gameData.time.xFrame;
if (rval) 
#endif
	{
	gameStates.gameplay.seismic.nStartTime = gameData.time.xGame;
	gameStates.gameplay.seismic.nEndTime = gameData.time.xGame + gameStates.gameplay.seismic.nShakeDuration;
	gameStates.gameplay.seismic.nShakeDuration = 0;
	if (!gameStates.gameplay.seismic.bSound) {
		audio.PlayLoopingSound ((short) gameStates.gameplay.seismic.nSound, I2X (1), -1, -1);
		gameStates.gameplay.seismic.bSound = 1;
		gameStates.gameplay.seismic.nNextSoundTime = gameData.time.xGame + RandShort () / 2;
		}
	if (IsMultiGame)
		MultiSendSeismic (gameStates.gameplay.seismic.nStartTime, gameStates.gameplay.seismic.nEndTime);
	}
return 1/*rval*/;
}

//	-----------------------------------------------------------------------------

void SeismicDisturbanceFrame (void)
{
if (gameStates.gameplay.seismic.nShakeFrequency) {
	if (((gameStates.gameplay.seismic.nStartTime < gameData.time.xGame) && (gameStates.gameplay.seismic.nEndTime > gameData.time.xGame)) || StartSeismicDisturbance ()) {
		fix	deltaTime = gameData.time.xGame - gameStates.gameplay.seismic.nStartTime;
#if 1
		RockPlayerShip (abs (deltaTime - (gameStates.gameplay.seismic.nEndTime - gameStates.gameplay.seismic.nStartTime) / 2), X2F (gameStates.gameplay.seismic.nShakeFrequency));
#else
		int	fc, rx, rz;
		fix	h;

		fc = abs (deltaTime - (gameStates.gameplay.seismic.nEndTime - gameStates.gameplay.seismic.nStartTime) / 2);
		fc /= I2X (1) / 16;
		if (fc > 16)
			fc = 16;
		else if (fc == 0)
			fc = 1;
		gameStates.gameplay.seismic.nVolume += fc;
		h = I2X (3) / 16 + (I2X (16 - fc)) / 32;
		rx = FixMul (SRandShort (), h);
		rz = FixMul (SRandShort (), h);
		gameData.objs.consoleP->mType.physInfo.rotVel.v.coord.x += rx;
		gameData.objs.consoleP->mType.physInfo.rotVel.v.coord.z += rz;
		//	Shake the buddy!
		if (gameData.escort.nObjNum != -1) {
			OBJECTS [gameData.escort.nObjNum].mType.physInfo.rotVel.v.coord.x += rx * 4;
			OBJECTS [gameData.escort.nObjNum].mType.physInfo.rotVel.v.coord.z += rz * 4;
			}
		//	Shake a guided missile!
		gameStates.gameplay.seismic.nMagnitude += rx;
#endif
		}
	}
}


//	-----------------------------------------------------------------------------
//	Call this when a smega detonates to start the process of rocking the mine.
void ShakerRockStuff (CFixVector* vPos)
{
#if 1 //!DBG
	int	i;

for (i = 0; i < MAX_ESHAKER_DETONATES; i++)
	if (eshakerDetonateTimes [i] + ESHAKER_SHAKE_TIME < gameData.time.xGame)
		eshakerDetonateTimes [i] = 0;

float fScale;
if (gameStates.app.bNostalgia || COMPETITION || (vPos == NULL))
	fScale = 1.0f;
else {
	float fDist = X2F (CFixVector::Dist (*vPos, gameData.objs.consoleP->Position ()));
	fScale = (fDist <= 200.0f) ? 1.0f : 200.0f / fDist;
	}

for (i = 0; i < MAX_ESHAKER_DETONATES; i++)
	if (eshakerDetonateTimes [i] == 0) {
		eshakerDetonateTimes [i] = gameData.time.xGame;
		eshakerDetonateScales [i] = fScale;
		//HUDMessage (0, "shaker rock scale %d: %1.2f (dist %1.2f)", i, fScale, fDist);
		break;
		}
#endif
}

//	---------------------------------------------------------------------------------------
//	Do seismic disturbance stuff including the looping sounds with changing volume.

void DoSeismicStuff (void)
{
	int		stv_save;

if (gameStates.limitFPS.bSeismic && !gameStates.app.tick40fps.bTick)
	return;
stv_save = gameStates.gameplay.seismic.nVolume;
gameStates.gameplay.seismic.nMagnitude = 0;
gameStates.gameplay.seismic.nVolume = 0;
RockTheMineFrame ();
SeismicDisturbanceFrame ();
if (stv_save != 0) {
	if (gameStates.gameplay.seismic.nVolume == 0) {
		audio.StopLoopingSound ();
		gameStates.gameplay.seismic.bSound = 0;
		}

	if ((gameData.time.xGame > gameStates.gameplay.seismic.nNextSoundTime) && gameStates.gameplay.seismic.nVolume) {
		int volume = gameStates.gameplay.seismic.nVolume * 2048;
		if (volume > I2X (1))
			volume = I2X (1);
		audio.ChangeLoopingVolume (volume);
		gameStates.gameplay.seismic.nNextSoundTime = gameData.time.xGame + RandShort () / 4 + 8192;
		}
	}
}

//	-----------------------------------------------------------------------------
//eof

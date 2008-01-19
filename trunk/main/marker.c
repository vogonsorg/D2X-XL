/* $Id: automap.c,v 1.16 2003/11/15 00:37:48 btb Exp $ */
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
#	include <conf.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "error.h"
#include "key.h"
#include "text.h"
#include "error.h"
#include "newmenu.h"
#include "marker.h"

#define	MAX_DROP_MULTI		2
#define	MAX_DROP_SINGLE	9

// -------------------------------------------------------------

static inline tObject *MarkerObj (int nMarker)
{
short nObject = gameData.marker.objects [gameData.multiplayer.nLocalPlayer * 2 + nMarker];
return (nObject < 0) ? NULL : OBJECTS + nObject;
}

// -------------------------------------------------------------

void DrawMarkerNumber (int nMarker)
{
  int i;
  g3sPoint basePoint, fromPoint, toPoint;

  float xCoord [10][20]={{-.25f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f},
                         {-1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f},
                         {-1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f},
                         {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f},
                         {-1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f},
                         {-1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f},
                         {-1.0f, 1.0f, 1.0f, 1.0f},
                         {-1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f},
                         {-1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f}

                       };
  float yCoord [10][20]={{.75f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f},
                         {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, -1.0f},
                         {1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f},
                         {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f},
                         {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, -1.0f},
                         {1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f, -1.0f},
                         {1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f}
                       };
  int nPoints [] = {6, 10, 8, 6, 10, 10, 4, 10, 8};

for (i = 0; i < nPoints [nMarker]; i++) {
	xCoord [nMarker][i] *= gameData.marker.fScale;
	yCoord [nMarker][i] *= gameData.marker.fScale;
	}
if (nMarker == gameData.marker.nHighlight)
	GrSetColorRGB (255, 255, 255, 255);
else
	GrSetColorRGBi (RGBA_PAL2 (48, 0, 0));
G3TransformAndEncodePoint (&basePoint, &MarkerObj (nMarker)->position.vPos);
fromPoint.p3_index =
toPoint.p3_index =
basePoint.p3_index = -1;
for (i = 0; i < nPoints [nMarker]; i += 2) {
	fromPoint = basePoint;
	toPoint = basePoint;
	fromPoint.p3_x += fl2f (xCoord [nMarker][i]); 
	fromPoint.p3_y += fl2f (yCoord [nMarker][i]); 
	G3EncodePoint (&fromPoint);
	G3ProjectPoint (&fromPoint);
	toPoint.p3_x += fl2f (xCoord [nMarker][i+1]); 
	toPoint.p3_y += fl2f (yCoord [nMarker][i+1]); 
	G3EncodePoint (&toPoint);
	G3ProjectPoint (&toPoint);
	G3DrawLine (&fromPoint, &toPoint);
	}
}

//------------------------------------------------------------------------------

#define MARKER_SPHERE_SIZE 0x58000

void DrawMarkers (void)
{
	g3sPoint spherePoint;
	int		i, nMaxDrop;
	tObject	*objP;

	static int cyc = 10, cycdir = 1;

nMaxDrop = IsMultiGame ? 2 : 9;
spherePoint.p3_index = -1;
for (i = 0; i < nMaxDrop; i++)
	if ((objP = MarkerObj (i))) {
		G3TransformAndEncodePoint (&spherePoint, &objP->position.vPos);
		GrSetColorRGB (PAL2RGBA (10), 0, 0, 255);
		G3DrawSphere (&spherePoint, MARKER_SPHERE_SIZE, 1);
		GrSetColorRGB (PAL2RGBA (20), 0, 0, 255);
		G3DrawSphere (&spherePoint, MARKER_SPHERE_SIZE / 2, 1);
		GrSetColorRGB (PAL2RGBA (30), 0, 0, 255);
		G3DrawSphere (&spherePoint, MARKER_SPHERE_SIZE / 4, 1);
		DrawMarkerNumber (i);
		}
if (cycdir) {
	cyc += 2;
	if (cyc > 43) {
		cyc = 43;
		cycdir = 0;
		}
	}
else {
	cyc -= 2;
	if (cyc < 10) {
		cyc = 10;
		cycdir = 1;
		}
	}
}

//------------------------------------------------------------------------------

void DropMarker (char nPlayerMarker)
{
	ubyte nMarker = (gameData.multiplayer.nLocalPlayer * 2) + nPlayerMarker;
	tObject *playerP = gameData.objs.objects + LOCALPLAYER.nObject;

gameData.marker.point [nMarker] = playerP->position.vPos;
if (gameData.marker.objects [nMarker] != -1)
	ReleaseObject (gameData.marker.objects [nMarker]);
gameData.marker.objects [nMarker] = 
	DropMarkerObject (&playerP->position.vPos, (short) playerP->nSegment, &playerP->position.mOrient, nMarker);
	if (gameData.app.nGameMode & GM_MULTI)
		MultiSendDropMarker (gameData.multiplayer.nLocalPlayer, playerP->position.vPos, nPlayerMarker, gameData.marker.szMessage [nMarker]);
}

//------------------------------------------------------------------------------

void DropBuddyMarker (tObject *objP)
{
	ubyte	nMarker;

	//	Find spare marker slot.  "if" code below should be an assert, but what if someone changes NUM_MARKERS or MAX_CROP_SINGLE and it never gets hit?
nMarker = MAX_DROP_SINGLE + 1;
if (nMarker > NUM_MARKERS - 1)
	nMarker = NUM_MARKERS - 1;
sprintf (gameData.marker.szMessage [nMarker], "RIP: %s",gameData.escort.szName);
gameData.marker.point [nMarker] = objP->position.vPos;
if (gameData.marker.objects [nMarker] != -1 && gameData.marker.objects [nMarker] !=0)
	ReleaseObject (gameData.marker.objects [nMarker]);
gameData.marker.objects [nMarker] = DropMarkerObject (&objP->position.vPos, (short) objP->nSegment, &objP->position.mOrient, nMarker);
}

//------------------------------------------------------------------------------

void ClearMarkers (void)
{
int i;

for (i = 0; i < NUM_MARKERS; i++) {
	gameData.marker.szMessage [i][0] = 0;
	gameData.marker.objects [i] = -1;
	}
}

//------------------------------------------------------------------------------

int LastMarker (void)
{
	int nMaxDrop, h, i;

//find free marker slot
nMaxDrop = IsMultiGame ? MAX_DROP_MULTI : MAX_DROP_SINGLE;
h = gameData.multiplayer.nLocalPlayer * 2 + nMaxDrop;
for (i = nMaxDrop; i; i--)
	if (gameData.marker.objects [--h] > -1)		//found free slot!
		return i - 1;
return -1;
}

//------------------------------------------------------------------------------

void DeleteMarker (void)
{
if (gameData.marker.nHighlight > -1 && gameData.marker.objects [gameData.marker.nHighlight] != -1) {
	if (!ExecMessageBox (NULL, NULL, 2, TXT_YES, TXT_NO, "Delete Marker?")) {
		int	h, i;
		ReleaseObject (gameData.marker.objects [gameData.marker.nHighlight]);
		i = LastMarker ();
		if (i == gameData.marker.nHighlight) {
			gameData.marker.objects [gameData.marker.nHighlight] = -1;
			gameData.marker.szMessage [gameData.marker.nHighlight][0] = '\0';
			gameData.marker.nHighlight = i ? 0 : -1;
			}
		else {
			h = i - gameData.marker.nHighlight;
			memcpy (gameData.marker.objects + gameData.marker.nHighlight,
						gameData.marker.objects + gameData.marker.nHighlight + 1,
						h * sizeof (gameData.marker.objects [0]));
			memcpy (gameData.marker.szMessage [gameData.marker.nHighlight],
						gameData.marker.szMessage [gameData.marker.nHighlight + 1],
						h * sizeof (gameData.marker.szMessage [0]));
			gameData.marker.objects [i] = -1;
			gameData.marker.szMessage [i][0] = '\0';
			}
		}				
	}
}

//------------------------------------------------------------------------------

static int	 nMarkerIndex=0;
static ubyte nDefiningMarker;
static ubyte nLastMarkerDropped;

void InitMarkerInput (void)
{
	int nMaxDrop, i;

//find free marker slot
i = LastMarker () + 1;
nMaxDrop = (gameData.app.nGameMode & GM_MULTI) ? MAX_DROP_MULTI : MAX_DROP_SINGLE;
if (i == nMaxDrop) {		//no free slot
	if (gameData.app.nGameMode & GM_MULTI)
		i = !nLastMarkerDropped;		//in multi, replace older of two
	else {
		HUDInitMessage (TXT_MARKER_SLOTS);
		return;
		}
	}
//got a D2_FREE slot.  start inputting marker message
gameData.marker.szInput [0] = '\0';
nMarkerIndex = 0;
gameData.marker.nDefiningMsg = 1;
nDefiningMarker = i;
}

//------------------------------------------------------------------------------

void MarkerInputMessage (int key)
{
switch (key) {
	case KEY_F8:
	case KEY_ESC:
		gameData.marker.nDefiningMsg = 0;
		GameFlushInputs ();
		break;

	case KEY_LEFT:
	case KEY_BACKSP:
	case KEY_PAD4:
		if (nMarkerIndex > 0)
			nMarkerIndex--;
		gameData.marker.szInput [nMarkerIndex] = 0;
		break;

	case KEY_ENTER:
		strcpy (gameData.marker.szMessage [(gameData.multiplayer.nLocalPlayer*2)+nDefiningMarker], gameData.marker.szInput);
		if (IsMultiGame)
		 strcpy (gameData.marker.nOwner [ (gameData.multiplayer.nLocalPlayer*2)+nDefiningMarker],LOCALPLAYER.callsign);
		DropMarker (nDefiningMarker);
		nLastMarkerDropped = nDefiningMarker;
		GameFlushInputs ();
		gameData.marker.nDefiningMsg = 0;
		break;

	default:
		if (key > 0) {
			int ascii = KeyToASCII (key);
			if ((ascii < 255) && (nMarkerIndex < 38)) {
				gameData.marker.szInput [nMarkerIndex++] = ascii;
				gameData.marker.szInput [nMarkerIndex] = 0;
				}
			}
		break;
		}
}

//------------------------------------------------------------------------------
//eof

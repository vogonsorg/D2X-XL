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

#include <stdio.h>		// for printf ()
#include <stdlib.h>		// for rand () and qsort ()
#include <string.h>		// for memset ()

#include "inferno.h"
#include "error.h"
#include "key.h"
#include "screens.h"
#include "text.h"
#include "newmenu.h"
#include "escort.h"
#include "playsave.h"
#include "network.h"
#include "gameseg.h"

#ifdef EDITOR
#include "editor/editor.h"
#endif

void SayEscortGoal (int goal_num);
void ShowEscortMenu (char *msg);

int nEscortGoalText [MAX_ESCORT_GOALS] = {
	702,
	703,
	704,
	705,
	706,
	707,
	708,
	709,
	710,
	711,
	712,
	713,
	714,
	715,
	716,
	717,
	718,
	719,
	720,
	721,
	722,
	723,
	724
// -- too much work -- 	"KAMIKAZE  "
};

//	-------------------------------------------------------------------------------------------------

void InitBuddyForLevel (void)
{
	int		i;
	tObject	*objP;

gameData.escort.bMayTalk = 0;
gameData.escort.nObjNum = -1;
gameData.escort.nGoalObject = ESCORT_GOAL_UNSPECIFIED;
gameData.escort.nSpecialGoal = -1;
gameData.escort.nGoalIndex = -1;
gameData.escort.bMsgsSuppressed = 0;
FORALL_ROBOT_OBJS (objP, i)
	if (IS_GUIDEBOT (objP))
		break;
if (IS_OBJECT (objP, i))
	gameData.escort.nObjNum = OBJ_IDX (objP);
gameData.escort.xSorryTime = -F1_0;
gameData.escort.bSearchingMarker = -1;
gameData.escort.nLastKey = -1;
}

//	-----------------------------------------------------------------------------
//	See if tSegment from curseg through nSide is reachable.
//	Return true if it is reachable, else return false.
int SegmentIsReachable (int curseg, short nSide)
{
return AIDoorIsOpenable (NULL, gameData.segs.segments + curseg, nSide);
}


//	-----------------------------------------------------------------------------
//	Create a breadth-first list of segments reachable from current tSegment.
//	max_segs is maximum number of segments to search.  Use MAX_SEGMENTS to search all.
//	On exit, *length <= max_segs.
//	Input:
//		start_seg
//	Output:
//		bfs_list:	array of shorts, each reachable tSegment.  Includes start tSegment.
//		length:		number of elements in bfs_list
void CreateBfsList (int start_seg, short bfs_list [], int *length, int max_segs)
{
	int		head, tail;
	sbyte		bVisited [MAX_SEGMENTS_D2X];

	memset (bVisited, 0, MAX_SEGMENTS * sizeof (sbyte));
	head = 0;
	tail = 0;

	bfs_list [head++] = start_seg;
	bVisited [start_seg] = 1;

while ((head != tail) && (head < max_segs)) {
	int		i;
	short		curseg;
	tSegment	*cursegp;

	curseg = bfs_list [tail++];
	cursegp = gameData.segs.segments + curseg;

	for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) {
		int	connected_seg = cursegp->children [i];
		if (!IS_CHILD (connected_seg))
			continue;
		if (bVisited [connected_seg])
			continue;
		if (!SegmentIsReachable (curseg, (short) i))
			continue;
		bfs_list [head++] = connected_seg;
		if (head >= max_segs)
			break;
		bVisited [connected_seg] = 1;
		Assert (head < MAX_SEGMENTS);
		}
	}
*length = head;
}

//	-----------------------------------------------------------------------------
//	Return true if ok for buddy to talk, else return false.
//	Buddy is allowed to talk if the tSegment he is in does not contain a blastable tWall that has not been blasted
//	AND he has never yet, since being initialized for level, been allowed to talk.
int BuddyMayTalk (void)
{
	int		i;
	tSegment	*segP;
	tObject	*objP;

if ((gameData.escort.nObjNum < 0) || (OBJECTS [gameData.escort.nObjNum].info.nType != OBJ_ROBOT)) {
	gameData.escort.bMayTalk = 0;
	return 0;
	}
if (gameData.escort.bMayTalk)
	return 1;
if ((OBJECTS [gameData.escort.nObjNum].info.nType == OBJ_ROBOT) &&
	 (gameData.escort.nObjNum <= gameData.objs.nLastObject [0]) &&
	!ROBOTINFO (OBJECTS [gameData.escort.nObjNum].info.nId).companion) {
	FORALL_ROBOT_OBJS (objP, i)
		if (IS_GUIDEBOT (objP))
			break;
	if (!IS_OBJECT (objP, i))
		return 0;
	gameData.escort.nObjNum = OBJ_IDX (objP);
	}
segP = gameData.segs.segments + OBJECTS [gameData.escort.nObjNum].info.nSegment;
for (i = 0; i < MAX_SIDES_PER_SEGMENT; i++) {
	short	nWall = WallNumP (segP, (short) i);
	if (IS_WALL (nWall) &&
		 (gameData.walls.walls [nWall].nType == WALL_BLASTABLE) &&
		 !(gameData.walls.walls [nWall].flags & WALL_BLASTED))
		return 0;
	//	Check one level deeper.
	if (IS_CHILD (segP->children [i])) {
		int		j;
		tSegment	*connSegP = gameData.segs.segments + segP->children [i];

		for (j = 0; j<MAX_SIDES_PER_SEGMENT; j++) {
			short	wall2 = WallNumP (connSegP, (short) j);
			if (IS_WALL (wall2) &&
				 (gameData.walls.walls [wall2].nType == WALL_BLASTABLE) &&
				 !(gameData.walls.walls [wall2].flags & WALL_BLASTED))
				return 0;
			}
		}
	}
gameData.escort.bMayTalk = 1;
return 1;
}

//	--------------------------------------------------------------------------------------------

void DetectEscortGoalAccomplished (int index)
{
	int		i, j;
	int		bDetected = 0;
	tObject	*objP;
	short		*childI, *childJ;

//	If goal is to go away, how can it be achieved?
if (gameData.escort.nSpecialGoal == ESCORT_GOAL_SCRAM)
	return;

//	See if goal found was a key.  Need to handle default goals differently.
//	Note, no buddy_met_goal sound when blow up reactor or exit.  Not great, but ok
//	since for reactor, noisy, for exit, buddy is disappearing.
if ((gameData.escort.nSpecialGoal == -1) && (gameData.escort.nGoalIndex == index)) {
	bDetected = 1;
	goto dega_ok;
	}

if ((gameData.escort.nGoalIndex <= ESCORT_GOAL_RED_KEY) && (index >= 0) && (index <= gameData.objs.nLastObject [0])) {
	objP = OBJECTS + index;
	if (objP->info.nType == OBJ_POWERUP)  {
		ubyte id = objP->info.nId;
		if (id == POW_KEY_BLUE) {
			if (gameData.escort.nGoalIndex == ESCORT_GOAL_BLUE_KEY) {
				bDetected = 1;
				goto dega_ok;
				}
			}
		else if (id == POW_KEY_GOLD) {
			if (gameData.escort.nGoalIndex == ESCORT_GOAL_GOLD_KEY) {
				bDetected = 1;
				goto dega_ok;
				}
			}
		else if (id == POW_KEY_RED) {
			if (gameData.escort.nGoalIndex == ESCORT_GOAL_RED_KEY) {
				bDetected = 1;
				goto dega_ok;
				}
			}
		}
	}
if (gameData.escort.nSpecialGoal != -1) {
	if (gameData.escort.nSpecialGoal == ESCORT_GOAL_ENERGYCEN) {
		if (index == -4)
			bDetected = 1;
		else if ((index >= 0) && (index <= gameData.segs.nLastSegment) &&
					(gameData.escort.nGoalIndex >= 0) && (gameData.escort.nGoalIndex <= gameData.segs.nLastSegment)) {
			childI = gameData.segs.segments [index].children;
			for (i = MAX_SIDES_PER_SEGMENT; i; i--, childI++) {
				if (*childI == gameData.escort.nGoalIndex) {
					bDetected = 1;
					goto dega_ok;
					}
				else {
					childJ = gameData.segs.segments [*childI].children;
					for (j = MAX_SIDES_PER_SEGMENT; j; j--, childJ++) {
						if (*childJ == gameData.escort.nGoalIndex) {
							bDetected = 1;
							goto dega_ok;
							}
						}
					}
				}
			}
		}
	else if ((index >= 0) && (index <= gameData.objs.nLastObject [0])) {
		objP = OBJECTS + index;
		if ((objP->info.nType == OBJ_POWERUP) && (gameData.escort.nSpecialGoal == ESCORT_GOAL_POWERUP))
			bDetected = 1;	//	Any nType of powerup picked up will do.
		else if ((gameData.escort.nGoalIndex >= 0) && (gameData.escort.nGoalIndex <= gameData.objs.nLastObject [0]) &&
					(objP->info.nType == OBJECTS [gameData.escort.nGoalIndex].info.nType) &&
					(objP->info.nId == OBJECTS [gameData.escort.nGoalIndex].info.nId)) {
		//	Note: This will help a little bit in making the buddy believe a goal is satisfied.  Won't work for a general goal like "find any powerup"
		// because of the insistence of both nType and id matching.
			bDetected = 1;
			}
		}
	}

dega_ok: ;
if (bDetected) {
	if (!BuddyMayTalk ())
		DigiPlaySampleOnce (SOUND_BUDDY_MET_GOAL, F1_0);
	gameData.escort.nGoalIndex = -1;
	gameData.escort.nGoalObject = ESCORT_GOAL_UNSPECIFIED;
	gameData.escort.nSpecialGoal = -1;
	gameData.escort.bSearchingMarker = -1;
	}
}

//	-------------------------------------------------------------------------------------------------

void ChangeGuidebotName ()
{
	tMenuItem m;
	char text [GUIDEBOT_NAME_LEN+1]="";
	int item;

	strcpy (text,gameData.escort.szName);

	memset (&m, 0, sizeof (m));
	m.nType=NM_TYPE_INPUT;
	m.text_len = GUIDEBOT_NAME_LEN;
	m.text = text;
	item = ExecMenu (NULL, "Enter Guide-bot name:", 1, &m, NULL, NULL);

	if (item != -1) {
		strcpy (gameData.escort.szName,text);
		strcpy (gameData.escort.szRealName,text);
		WritePlayerFile ();
	}
}

//	-----------------------------------------------------------------------------

void _CDECL_ BuddyMessage (const char * format, ... )
{
if (gameData.escort.bMsgsSuppressed)
	return;
if (gameData.app.nGameMode & GM_MULTI)
	return;
if ((gameData.escort.xLastMsgTime + F1_0 < gameData.time.xGame) ||
	 (gameData.escort.xLastMsgTime > gameData.time.xGame)) {
	if (BuddyMayTalk ()) {
		char		szMsg [200];
		va_list	args;
		int		l = (int) strlen (gameData.escort.szName);

		va_start (args, format);
		vsprintf (szMsg + l + 10, format, args);
		va_end (args);

		szMsg [0] = (char) 1;
		szMsg [1] = (char) (127 + 128);
		szMsg [2] = (char) (127 + 128);
		szMsg [3] = (char) (0 + 128);
		memcpy (szMsg + 4, gameData.escort.szName, l);
		szMsg [l+4] = ':';
		szMsg [l+5] = ' ';
		szMsg [l+6] = (char) 1;
		szMsg [l+7] = (char) (0 + 128);
		szMsg [l+8] = (char) (127 + 128);
		szMsg [l+9] = (char) (0 + 128);

		HUDInitMessage (szMsg);
		gameData.escort.xLastMsgTime = gameData.time.xGame;
		}
	}
}

//	-----------------------------------------------------------------------------
//	Return true if marker #id has been placed.
int MarkerExistsInMine (int id)
{
	int		i;
	tObject	*objP;

FORALL_OBJS (objP, i)
	if ((objP->info.nType == OBJ_MARKER) && (objP->info.nId == id))
		return 1;
return 0;
}

//	-----------------------------------------------------------------------------
void EscortSetSpecialGoal (int special_key)
{
	int marker_key;

gameData.escort.bMsgsSuppressed = 0;
if (!gameData.escort.bMayTalk) {
	BuddyMayTalk ();
	if (!gameData.escort.bMayTalk) {
		int		i;
		tObject	*objP;

		FORALL_ROBOT_OBJS (objP, i)
			if (IS_GUIDEBOT (objP)) {
				HUDInitMessage (TXT_GB_RELEASE, gameData.escort.szName);
				return;
				}
		HUDInitMessage (TXT_GB_NONE);
		return;
		}
	}

special_key = special_key & (~KEY_SHIFTED);
marker_key = special_key;

if (gameData.escort.nLastKey == special_key) {
	if ((gameData.escort.bSearchingMarker == -1) && (special_key != KEY_0)) {
		if (MarkerExistsInMine (marker_key - KEY_1))
			gameData.escort.bSearchingMarker = marker_key - KEY_1;
		else {
			gameData.escort.xLastMsgTime = 0;	//	Force this message to get through.
			BuddyMessage ("Marker %i not placed.", marker_key - KEY_1 + 1);
			gameData.escort.bSearchingMarker = -1;
			}
		} 
	else {
		gameData.escort.bSearchingMarker = -1;
		}
	}
gameData.escort.nLastKey = special_key;
if (special_key == KEY_0)
	gameData.escort.bSearchingMarker = -1;
	if (gameData.escort.bSearchingMarker != -1)
		gameData.escort.nSpecialGoal = ESCORT_GOAL_MARKER1 + marker_key - KEY_1;
else {
	switch (special_key) {
		case KEY_1:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_ENERGY;
			break;
		case KEY_2:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_ENERGYCEN;
			break;
		case KEY_3:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_SHIELD;
			break;
		case KEY_4:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_POWERUP;
			break;
		case KEY_5:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_ROBOT;
			break;
		case KEY_6:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_HOSTAGE;
			break;
		case KEY_7:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_SCRAM;
			break;
		case KEY_8:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_PLAYER_SPEW;
			break;
		case KEY_9:
			gameData.escort.nSpecialGoal = ESCORT_GOAL_EXIT;
			break;
		case KEY_0:
			gameData.escort.nSpecialGoal = -1;
			break;
		default:
			Int3 ();		//	Oops, called with illegal key value.
		}
	}
gameData.escort.xLastMsgTime = gameData.time.xGame - 2*F1_0;	//	Allow next message to come through.
SayEscortGoal (gameData.escort.nSpecialGoal);
gameData.escort.nGoalObject = ESCORT_GOAL_UNSPECIFIED;
}

//	-----------------------------------------------------------------------------
//	Return id of boss.
int GetBossId (void)
{
	int	h, i;
	int	nObject;

	for (h = BOSS_COUNT, i = 0; i < BOSS_COUNT; i++) {
		if (0 <= (nObject = gameData.boss [i].nObject))
			return OBJECTS [nObject].info.nId;
		}
	return -1;
}

//	-----------------------------------------------------------------------------
//	Return tObject index if tObject of objtype, objid exists in mine, else return -1
//	"special" is used to find OBJECTS spewed by tPlayer which is hacked into flags field of powerup.
int ExistsInMine2 (int nSegment, int objtype, int objid, int special)
{
	int nObject = SEGMENTS [nSegment].objects;
	
if (nObject != -1) {
	int		id;

	while (nObject != -1) {
		tObject	*curObjP = OBJECTS + nObject;

		if (special == ESCORT_GOAL_PLAYER_SPEW) {
			if (curObjP->info.nFlags & OF_PLAYER_DROPPED)
				return nObject;
			}
		if (curObjP->info.nType == objtype) {
			//	Don't find escort robots if looking for robot!
			if (IS_GUIDEBOT (curObjP))
				;
			else if (objid == -1) {
				id = curObjP->info.nId;
				if ((objtype == OBJ_POWERUP) && (id != POW_KEY_BLUE) && (id != POW_KEY_GOLD) && (id != POW_KEY_RED))
					return nObject;
				else
					return nObject;
			} else if (curObjP->info.nId == objid)
				return nObject;
		}

		if (objtype == OBJ_POWERUP)
			if (curObjP->info.contains.nCount)
				if (curObjP->info.contains.nType == OBJ_POWERUP)
					if (curObjP->info.contains.nId == objid)
						return nObject;

		nObject = curObjP->info.nNextInSeg;
		}
	}
return -1;
}

//	-----------------------------------------------------------------------------
//	Return nearest tObject of interest.
//	If special == ESCORT_GOAL_PLAYER_SPEW, then looking for any tObject spewed by player.
//	-1 means tObject does not exist in mine.
//	-2 means tObject does exist in mine, but buddy-bot can't reach it (eg, behind triggered tWall)
int ExistsInMine (int start_seg, int objtype, int objid, int special)
{
	int	nSegIdx, nSegment;
	short	bfs_list [MAX_SEGMENTS_D2X];
	int	length;
	int	nObject;

CreateBfsList (start_seg, bfs_list, &length, MAX_SEGMENTS);
if (objtype == FUELCEN_CHECK) {
	for (nSegIdx = 0; nSegIdx < length; nSegIdx++) {
		nSegment = bfs_list [nSegIdx];
		if (gameData.segs.segment2s [nSegment].special == SEGMENT_IS_FUELCEN)
			return nSegment;
		}
	}
else {
	for (nSegIdx = 0; nSegIdx < length; nSegIdx++) {
		nSegment = bfs_list [nSegIdx];
		nObject = ExistsInMine2 (nSegment, objtype, objid, special);
		if (nObject != -1)
			return nObject;
		}
	}
//	Couldn't find what we're looking for by looking at connectivity.
//	See if it's in the mine.  It could be hidden behind a tTrigger or switch
//	which the buddybot doesn't understand.
if (objtype == FUELCEN_CHECK) {
	for (nSegment = 0; nSegment <= gameData.segs.nLastSegment; nSegment++)
		if (gameData.segs.segment2s [nSegment].special == SEGMENT_IS_FUELCEN)
			return -2;
	}
else {
	for (nSegment = 0; nSegment <= gameData.segs.nLastSegment; nSegment++) {
		nObject = ExistsInMine2 (nSegment, objtype, objid, special);
		if (nObject != -1)
			return -2;
		}
	}
return -1;
}

//	-----------------------------------------------------------------------------
//	Return true if it happened, else return false.
int FindExitSegment (void)
{
	int	i,j;

	//	---------- Find exit doors ----------
	for (i=0; i<=gameData.segs.nLastSegment; i++)
		for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
			if (gameData.segs.segments [i].children [j] == -2) {
				return i;
			}

	return -1;
}

#define	BUDDY_MARKER_TEXT_LEN	25

//	-----------------------------------------------------------------------------

void SayEscortGoal (int goal_num)
{
if (gameStates.app.bPlayerIsDead)
	return;
switch (goal_num) {
	case ESCORT_GOAL_BLUE_KEY:
		BuddyMessage (TXT_FIND_BLUEKEY);
		break;
	case ESCORT_GOAL_GOLD_KEY:
		BuddyMessage (TXT_FIND_GOLDKEY);
		break;
	case ESCORT_GOAL_RED_KEY:
		BuddyMessage (TXT_FIND_REDKEY);
		break;
	case ESCORT_GOAL_CONTROLCEN:
		BuddyMessage (TXT_FIND_REACTOR);
		break;
	case ESCORT_GOAL_EXIT:
		BuddyMessage (TXT_FIND_EXIT);
		break;
	case ESCORT_GOAL_ENERGY:
		BuddyMessage (TXT_FIND_ENERGY);
		break;
	case ESCORT_GOAL_ENERGYCEN:
		BuddyMessage (TXT_FIND_ECENTER);
		break;
	case ESCORT_GOAL_SHIELD:
		BuddyMessage (TXT_FIND_SHIELD);
		break;
	case ESCORT_GOAL_POWERUP:
		BuddyMessage (TXT_FIND_POWERUP);
		break;
	case ESCORT_GOAL_ROBOT:
		BuddyMessage (TXT_FIND_ROBOT);
		break;
	case ESCORT_GOAL_HOSTAGE:
		BuddyMessage (TXT_FIND_HOSTAGE);
		break;
	case ESCORT_GOAL_SCRAM:
		BuddyMessage (TXT_STAYING_AWAY);
		break;
	case ESCORT_GOAL_BOSS:
		BuddyMessage (TXT_FIND_BOSS);
		break;
	case ESCORT_GOAL_PLAYER_SPEW:
		BuddyMessage (TXT_FIND_YOURSTUFF);
		break;
	case ESCORT_GOAL_MARKER1:
	case ESCORT_GOAL_MARKER2:
	case ESCORT_GOAL_MARKER3:
	case ESCORT_GOAL_MARKER4:
	case ESCORT_GOAL_MARKER5:
	case ESCORT_GOAL_MARKER6:
	case ESCORT_GOAL_MARKER7:
	case ESCORT_GOAL_MARKER8:
	case ESCORT_GOAL_MARKER9: {
			char marker_text [BUDDY_MARKER_TEXT_LEN];

		strncpy (marker_text, gameData.marker.szMessage [goal_num-ESCORT_GOAL_MARKER1], BUDDY_MARKER_TEXT_LEN-1);
		marker_text [BUDDY_MARKER_TEXT_LEN-1] = 0;
		BuddyMessage (TXT_FIND_MARKER, goal_num-ESCORT_GOAL_MARKER1+1, marker_text);
		break;
		}
	}
}

//	-----------------------------------------------------------------------------

void EscortCreatePathToGoal (tObject *objP)
{
	short			nGoalSeg = -1;
	short			nObject = OBJ_IDX (objP);
	tAIStaticInfo	*aip = &objP->cType.aiInfo;
	tAILocalInfo		*ailp = gameData.ai.localInfo + nObject;

Assert (nObject >= 0);
if (gameData.escort.nSpecialGoal != -1)
	gameData.escort.nGoalObject = gameData.escort.nSpecialGoal;
gameData.escort.nKillObject = -1;
if (gameData.escort.bSearchingMarker != -1) {
	gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_MARKER, gameData.escort.nGoalObject-ESCORT_GOAL_MARKER1, -1);
	if (gameData.escort.nGoalIndex > -1)
		nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
	}
else {
	switch (gameData.escort.nGoalObject) {
		case ESCORT_GOAL_BLUE_KEY:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_POWERUP, POW_KEY_BLUE, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_GOLD_KEY:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_POWERUP, POW_KEY_GOLD, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_RED_KEY:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_POWERUP, POW_KEY_RED, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_CONTROLCEN:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_REACTOR, -1, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_EXIT:
		case ESCORT_GOAL_EXIT2:
			nGoalSeg = FindExitSegment ();
			gameData.escort.nGoalIndex = nGoalSeg;
			break;
		case ESCORT_GOAL_ENERGY:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_POWERUP, POW_ENERGY, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_ENERGYCEN:
			nGoalSeg = ExistsInMine (objP->info.nSegment, FUELCEN_CHECK, -1, -1);
			gameData.escort.nGoalIndex = nGoalSeg;
			break;
		case ESCORT_GOAL_SHIELD:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_POWERUP, POW_SHIELD_BOOST, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_POWERUP:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_POWERUP, -1, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_ROBOT:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_ROBOT, -1, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_HOSTAGE:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_HOSTAGE, -1, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_PLAYER_SPEW:
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, -1, -1, ESCORT_GOAL_PLAYER_SPEW);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		case ESCORT_GOAL_SCRAM:
			nGoalSeg = -3;		//	Kinda a hack.
			gameData.escort.nGoalIndex = nGoalSeg;
			break;
		case ESCORT_GOAL_BOSS: {
			int	boss_id;

			boss_id = GetBossId ();
			Assert (boss_id != -1);
			gameData.escort.nGoalIndex = ExistsInMine (objP->info.nSegment, OBJ_ROBOT, boss_id, -1);
			if (gameData.escort.nGoalIndex > -1)
				nGoalSeg = OBJECTS [gameData.escort.nGoalIndex].info.nSegment;
			break;
		}
		default:
			Int3 ();	//	Oops, Illegal value in gameData.escort.nGoalObject.
			nGoalSeg = 0;
			break;
		}
	}
if ((gameData.escort.nGoalIndex < 0) && (gameData.escort.nGoalIndex != -3)) {	//	I apologize for this statement -- MK, 09/22/95
	if (gameData.escort.nGoalIndex == -1) {
		gameData.escort.xLastMsgTime = 0;	//	Force this message to get through.
		BuddyMessage (TXT_NOT_IN_MINE, GT (nEscortGoalText [gameData.escort.nGoalObject-1]));
		gameData.escort.bSearchingMarker = -1;
		}
	else if (gameData.escort.nGoalIndex == -2) {
		gameData.escort.xLastMsgTime = 0;	//	Force this message to get through.
		BuddyMessage (TXT_CANT_REACH, GT (nEscortGoalText [gameData.escort.nGoalObject-1]));
		gameData.escort.bSearchingMarker = -1;
		}
	else
		Int3 ();

	gameData.escort.nGoalObject = ESCORT_GOAL_UNSPECIFIED;
	gameData.escort.nSpecialGoal = -1;
	}
else {
	if (nGoalSeg == -3) {
		CreateNSegmentPath (objP, 16 + d_rand () * 16, -1);
		aip->nPathLength = SmoothPath (objP, gameData.ai.pointSegs + aip->nHideIndex, aip->nPathLength);
		}
	else {
		CreatePathToSegment (objP, nGoalSeg, gameData.escort.nMaxLength, 1);	//	MK!: Last parm (safetyFlag) used to be 1!!
		if (aip->nPathLength > 3)
			aip->nPathLength = SmoothPath (objP, gameData.ai.pointSegs + aip->nHideIndex, aip->nPathLength);
		if ((aip->nPathLength > 0) && (gameData.ai.pointSegs [aip->nHideIndex + aip->nPathLength - 1].nSegment != nGoalSeg)) {
			fix	xDistToPlayer;
			gameData.escort.xLastMsgTime = 0;	//	Force this message to get through.
			BuddyMessage (TXT_CANT_REACH, GT (nEscortGoalText [gameData.escort.nGoalObject-1]));
			gameData.escort.bSearchingMarker = -1;
			gameData.escort.nGoalObject = ESCORT_GOAL_SCRAM;
			xDistToPlayer = FindConnectedDistance (&objP->info.position.vPos, objP->info.nSegment, &gameData.ai.vBelievedPlayerPos, gameData.ai.nBelievedPlayerSeg, 100, WID_FLY_FLAG, 0);
			if (xDistToPlayer > MIN_ESCORT_DISTANCE)
				CreatePathToPlayer (objP, gameData.escort.nMaxLength, 1);	//	MK!: Last parm used to be 1!
			else {
				CreateNSegmentPath (objP, 8 + d_rand () * 8, -1);
				aip->nPathLength = SmoothPath (objP, gameData.ai.pointSegs + aip->nHideIndex, aip->nPathLength);
				}
			}
		}
	ailp->mode = AIM_GOTO_OBJECT;
	SayEscortGoal (gameData.escort.nGoalObject);
	}
}

//	-----------------------------------------------------------------------------
//	Escort robot chooses goal tObject based on tPlayer's keys, location.
//	Returns goal tObject.
int EscortSetGoalObject (void)
{
if (gameData.escort.nSpecialGoal != -1)
	return ESCORT_GOAL_UNSPECIFIED;
else if (!(gameData.objs.consoleP->info.nFlags & PLAYER_FLAGS_BLUE_KEY) &&
			 (ExistsInMine (gameData.objs.consoleP->info.nSegment, OBJ_POWERUP, POW_KEY_BLUE, -1) != -1))
	return ESCORT_GOAL_BLUE_KEY;
else if (!(gameData.objs.consoleP->info.nFlags & PLAYER_FLAGS_GOLD_KEY) &&
			 (ExistsInMine (gameData.objs.consoleP->info.nSegment, OBJ_POWERUP, POW_KEY_GOLD, -1) != -1))
	return ESCORT_GOAL_GOLD_KEY;
else if (!(gameData.objs.consoleP->info.nFlags & PLAYER_FLAGS_RED_KEY) &&
			 (ExistsInMine (gameData.objs.consoleP->info.nSegment, OBJ_POWERUP, POW_KEY_RED, -1) != -1))
	return ESCORT_GOAL_RED_KEY;
else if (!gameData.reactor.bDestroyed) {
	int	i;

	for (i = 0; i < extraGameInfo [0].nBossCount; i++)
		if ((gameData.boss [i].nObject >= 0) && gameData.boss [i].nTeleportSegs)
			return ESCORT_GOAL_BOSS;
		return ESCORT_GOAL_CONTROLCEN;
	}
else
	return ESCORT_GOAL_EXIT;
}

#define	MAX_ESCORT_TIME_AWAY		 (F1_0*4)

fix	xBuddyLastSeenPlayer = 0, Buddy_last_player_path_created;

//	-----------------------------------------------------------------------------

int TimeToVisitPlayer (tObject *objP, tAILocalInfo *ailp, tAIStaticInfo *aip)
{
	//	Note: This one has highest priority because, even if already going towards tPlayer,
	//	might be necessary to create a new path, as tPlayer can move.
if (gameData.time.xGame - xBuddyLastSeenPlayer > MAX_ESCORT_TIME_AWAY)
	if (gameData.time.xGame - Buddy_last_player_path_created > F1_0)
		return 1;
if (ailp->mode == AIM_GOTO_PLAYER)
	return 0;
if (objP->info.nSegment == gameData.objs.consoleP->info.nSegment)
	return 0;
if (aip->nCurPathIndex < aip->nPathLength/2)
	return 0;
return 1;
}

fix Last_come_back_messageTime = 0;

fix Buddy_last_missileTime;

//	-----------------------------------------------------------------------------

void BashBuddyWeaponInfo (int nWeaponObj)
{
	tObject	*objP = OBJECTS + nWeaponObj;

objP->cType.laserInfo.parent.nObject = OBJ_IDX (gameData.objs.consoleP);
objP->cType.laserInfo.parent.nType = OBJ_PLAYER;
objP->cType.laserInfo.parent.nSignature = gameData.objs.consoleP->info.nSignature;
}

//	-----------------------------------------------------------------------------

int MaybeBuddyFireMega (short nObject)
{
	tObject		*objP = OBJECTS + nObject;
	tObject		*buddyObjP = OBJECTS + gameData.escort.nObjNum;
	fix			dist, dot;
	vmsVector	vVecToRobot;
	int			nWeaponObj;

vVecToRobot = buddyObjP->info.position.vPos - objP->info.position.vPos;
dist = vmsVector::Normalize(vVecToRobot);
if (dist > F1_0*100)
	return 0;
dot = vmsVector::Dot(vVecToRobot, buddyObjP->info.position.mOrient[FVEC]);
if (dot < F1_0/2)
	return 0;
if (!ObjectToObjectVisibility (buddyObjP, objP, FQ_TRANSWALL))
	return 0;
if (gameData.weapons.info [MEGAMSL_ID].renderType == 0) {
#if TRACE
	con_printf (CON_VERBOSE, "Buddy can't fire mega (shareware)\n");
#endif
	BuddyMessage (TXT_BUDDY_CLICK);
	return 0;
	}
#if TRACE
con_printf (CONDBG, "Buddy firing mega in frame %i\n", gameData.app.nFrameCount);
#endif
BuddyMessage (TXT_BUDDY_GAHOOGA);
nWeaponObj = CreateNewLaserEasy (&buddyObjP->info.position.mOrient[FVEC], &buddyObjP->info.position.vPos, nObject, MEGAMSL_ID, 1);
if (nWeaponObj != -1)
	BashBuddyWeaponInfo (nWeaponObj);
return 1;
}

//-----------------------------------------------------------------------------

int MaybeBuddyFireSmart (short nObject)
{
	tObject	*objP = &OBJECTS [nObject];
	tObject	*buddyObjP = &OBJECTS [gameData.escort.nObjNum];
	fix		dist;
	short		nWeaponObj;

dist = vmsVector::Dist(buddyObjP->info.position.vPos, objP->info.position.vPos);
if (dist > F1_0*80)
	return 0;
if (!ObjectToObjectVisibility (buddyObjP, objP, FQ_TRANSWALL))
	return 0;
#if TRACE
con_printf (CONDBG, "Buddy firing smart missile in frame %i\n", gameData.app.nFrameCount);
#endif
BuddyMessage (TXT_BUDDY_WHAMMO);
nWeaponObj = CreateNewLaserEasy (&buddyObjP->info.position.mOrient[FVEC], &buddyObjP->info.position.vPos, nObject, SMARTMSL_ID, 1);
if (nWeaponObj != -1)
	BashBuddyWeaponInfo (nWeaponObj);
return 1;
}

//	-----------------------------------------------------------------------------

void DoBuddyDudeStuff (void)
{
	short		i;
	tObject	*objP;

if (!BuddyMayTalk ())
	return;

if (Buddy_last_missileTime > gameData.time.xGame)
	Buddy_last_missileTime = 0;

if (Buddy_last_missileTime + F1_0*2 < gameData.time.xGame) {
	//	See if a robot potentially in view cone
	FORALL_ROBOT_OBJS (objP, i)
		if (!ROBOTINFO (objP->info.nId).companion)
			if (MaybeBuddyFireMega (OBJ_IDX (objP))) {
				Buddy_last_missileTime = gameData.time.xGame;
				return;
			}
	//	See if a robot near enough that buddy should fire smart missile
	FORALL_ROBOT_OBJS (objP, i)
		if (!ROBOTINFO (objP->info.nId).companion)
			if (MaybeBuddyFireSmart (OBJ_IDX (objP))) {
				Buddy_last_missileTime = gameData.time.xGame;
				return;
			}
	}
}

//	-----------------------------------------------------------------------------
//	Called every frame (or something).
void DoEscortFrame (tObject *objP, fix xDistToPlayer, int player_visibility)
{
	int			nObject = OBJ_IDX (objP);
	tAIStaticInfo	*aip = &objP->cType.aiInfo;
	tAILocalInfo		*ailp = gameData.ai.localInfo + nObject;

Assert (nObject >= 0);
gameData.escort.nObjNum = nObject;
if (player_visibility) {
	xBuddyLastSeenPlayer = gameData.time.xGame;
	if (PlayerHasHeadlight (-1) && EGI_FLAG (headlight.bDrainPower, 0, 0, 1))	//	DAMN!MK, stupid bug, fixed 12/08/95, changed PLAYER_FLAGS_HEADLIGHT to PLAYER_FLAGS_HEADLIGHT_ON
		if (X2I (LOCALPLAYER.energy) < 40)
			if ((X2I (LOCALPLAYER.energy)/2) & 2)
				if (!gameStates.app.bPlayerIsDead)
					BuddyMessage (TXT_HEADLIGHT_WARN);
	}
if (gameStates.app.cheats.bMadBuddy)
	DoBuddyDudeStuff ();
if (gameData.escort.xSorryTime + F1_0 > gameData.time.xGame) {
	gameData.escort.xLastMsgTime = 0;	//	Force this message to get through.
	if (gameData.escort.xSorryTime < gameData.time.xGame + F1_0*2)
		BuddyMessage (TXT_BUDDY_SORRY);
	gameData.escort.xSorryTime = -F1_0*2;
	}
//	If buddy not allowed to talk, then he is locked in his room.  Make him mostly do nothing unless you're nearby.
if (!gameData.escort.bMayTalk)
	if (xDistToPlayer > F1_0*100)
		aip->SKIP_AI_COUNT = (sbyte) ((F1_0 / 4) / (gameData.time.xFrame ? gameData.time.xFrame : 1));
//	AIM_WANDER has been co-opted for buddy behavior (didn't want to modify aistruct.h)
//	It means the tObject has been told to get lost and has come to the end of its path.
//	If the tPlayer is now visible, then create a path.
if (ailp->mode == AIM_WANDER)
	if (player_visibility) {
		CreateNSegmentPath (objP, 16 + d_rand () * 16, -1);
		aip->nPathLength = SmoothPath (objP, gameData.ai.pointSegs + aip->nHideIndex, aip->nPathLength);
		}
if (gameData.escort.nSpecialGoal == ESCORT_GOAL_SCRAM) {
	if (player_visibility)
		if (gameData.escort.xLastPathCreated + F1_0*3 < gameData.time.xGame) {
#if TRACE
			con_printf (CONDBG, "Frame %i: Buddy creating new scram path.\n", gameData.app.nFrameCount);
#endif
			CreateNSegmentPath (objP, 10 + d_rand () * 16, gameData.objs.consoleP->info.nSegment);
			gameData.escort.xLastPathCreated = gameData.time.xGame;
			}
	// -- Int3 ();
	return;
	}
//	Force checking for new goal every 5 seconds, and create new path, if necessary.
if (((gameData.escort.nSpecialGoal != ESCORT_GOAL_SCRAM) && ((gameData.escort.xLastPathCreated + F1_0*5) < gameData.time.xGame)) ||
	 ((gameData.escort.nSpecialGoal == ESCORT_GOAL_SCRAM) && ((gameData.escort.xLastPathCreated + F1_0*15) < gameData.time.xGame))) {
	gameData.escort.nGoalObject = ESCORT_GOAL_UNSPECIFIED;
	gameData.escort.xLastPathCreated = gameData.time.xGame;
	}
if ((gameData.escort.nSpecialGoal != ESCORT_GOAL_SCRAM) && TimeToVisitPlayer (objP, ailp, aip)) {
	int	nMaxLen;

	Buddy_last_player_path_created = gameData.time.xGame;
	ailp->mode = AIM_GOTO_PLAYER;
	if (!player_visibility) {
		if ((Last_come_back_messageTime + F1_0 < gameData.time.xGame) || (Last_come_back_messageTime > gameData.time.xGame)) {
			BuddyMessage (TXT_COMING_BACK);
			Last_come_back_messageTime = gameData.time.xGame;
			}
		}
	//	No point in Buddy creating very long path if he's not allowed to talk.  Really kills framerate.
	nMaxLen = gameData.escort.nMaxLength;
	if (!gameData.escort.bMayTalk)
		nMaxLen = 3;
	CreatePathToPlayer (objP, nMaxLen, 1);	//	MK!: Last parm used to be 1!
	aip->nPathLength = SmoothPath (objP, gameData.ai.pointSegs + aip->nHideIndex, aip->nPathLength);
	ailp->mode = AIM_GOTO_PLAYER;
	}
else if (gameData.time.xGame - xBuddyLastSeenPlayer > MAX_ESCORT_TIME_AWAY) {
	//	This is to prevent buddy from looking for a goal, which he will do because we only allow path creation once/second.
	return;
	}
else if ((ailp->mode == AIM_GOTO_PLAYER) && (xDistToPlayer < MIN_ESCORT_DISTANCE)) {
	gameData.escort.nGoalObject = EscortSetGoalObject ();
	ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but AIDoorIsOpenable uses mode to determine what doors can be got through
	EscortCreatePathToGoal (objP);
	aip->nPathLength = SmoothPath (objP, gameData.ai.pointSegs + aip->nHideIndex, aip->nPathLength);
	if (aip->nPathLength < 3)
		CreateNSegmentPath (objP, 5, gameData.ai.nBelievedPlayerSeg);
	ailp->mode = AIM_GOTO_OBJECT;
	}
else if (gameData.escort.nGoalObject == ESCORT_GOAL_UNSPECIFIED) {
	if ((ailp->mode != AIM_GOTO_PLAYER) || (xDistToPlayer < MIN_ESCORT_DISTANCE)) {
		gameData.escort.nGoalObject = EscortSetGoalObject ();
		ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but AIDoorIsOpenable uses mode to determine what doors can be got through
		EscortCreatePathToGoal (objP);
		aip->nPathLength = SmoothPath (objP, gameData.ai.pointSegs + aip->nHideIndex, aip->nPathLength);
		if (aip->nPathLength < 3)
			CreateNSegmentPath (objP, 5, gameData.ai.nBelievedPlayerSeg);
		ailp->mode = AIM_GOTO_OBJECT;
		}
	}
}

//	-------------------------------------------------------------------------------------------------

void InvalidateEscortGoal (void)
{
	gameData.escort.nGoalObject = -1;
}

// --------------------------------------------------------------------------------------------------------------

int ShowEscortHelp (char *pszGoal, char *tstr)
{

	int				nItems;
	tMenuItem	m [12];
	char szGoal		 [40], szMsgs [40];
#if 0
	static char *szEscortHelp [12] = {
		"0.  Next Goal: %s",
		"1.  Find Energy Powerup",
		"2.  Find Energy Center",
		"3.  Find Shield Powerup",
		"4.  Find Any Powerup",
		"5.  Find a Robot",
		"6.  Find a Hostage",
		"7.  Stay Away From Me",
		"8.  Find My Powerups",
		"9.  Find the exit",
		"",
		"T.  %s Messages"
		};
#endif

sprintf (szGoal, TXT_GOAL_NEXT, pszGoal);
sprintf (szMsgs, TXT_GOAL_MESSAGES, tstr);
memset (m, 0, sizeof (m));
for (nItems = 1; nItems < 10; nItems++) {
	m [nItems].text = (char*) (GT (343 + nItems));
	m [nItems].nType = *m [nItems].text ? NM_TYPE_MENU : NM_TYPE_TEXT;
	m [nItems].key = -1;
	}
m [0].text = szGoal;
m [10].text = szMsgs;
m [10].key = KEY_T;
return ExecMenutiny2 (NULL, "Guide-Bot Commands", 11, m, NULL);
}

// --------------------------------------------------------------------------------------------------------------

void DoEscortMenu (void)
{
	int		i;
	int		paused;
	int		next_goal;
	char		szGoal [32], tstr [32];
	tObject	*objP;

if (gameData.app.nGameMode & GM_MULTI) {
	HUDInitMessage (TXT_GB_MULTIPLAYER);
	return;
	}

FORALL_ROBOT_OBJS (objP, i) {
	if (ROBOTINFO (objP->info.nId).companion)
		break;
	}

if (!IS_OBJECT (objP, i)) {
#if 1//def _DEBUG - always allow buddy bot creation
		//	If no buddy bot, create one!
		HUDInitMessage (TXT_GB_CREATE);
		CreateBuddyBot ();
#else
		HUDInitMessage (TXT_GB_NONE);
		return;
#endif
	}

	BuddyMayTalk ();	//	Needed here or we might not know buddy can talk when he can.

	if (!gameData.escort.bMayTalk) {
		HUDInitMessage (TXT_GB_RELEASE, gameData.escort.szName);
		return;
	}

	DigiPauseDigiSounds ();
	if (!gameOpts->menus.nStyle)
		StopTime ();

	paletteManager.SaveEffect ();
	paletteManager.ResetEffect ();
	GameFlushInputs ();
	paused = 1;

//	SetScreenMode ( SCREEN_MENU );
	SetPopupScreenMode ();
	paletteManager.LoadEffect ();

	//	This prevents the buddy from coming back if you've told him to scram.
	//	If we don't set next_goal, we get garbage there.
	if (gameData.escort.nSpecialGoal == ESCORT_GOAL_SCRAM) {
		gameData.escort.nSpecialGoal = -1;	//	Else setting next goal might fail.
		next_goal = EscortSetGoalObject ();
		gameData.escort.nSpecialGoal = ESCORT_GOAL_SCRAM;
	} else {
		gameData.escort.nSpecialGoal = -1;	//	Else setting next goal might fail.
		next_goal = EscortSetGoalObject ();
	}

	switch (next_goal) {
	#if DBG
		case ESCORT_GOAL_UNSPECIFIED:
			Int3 ();
			sprintf (szGoal, "ERROR");
			break;
	#endif

		case ESCORT_GOAL_BLUE_KEY:
			sprintf (szGoal, TXT_GB_BLUEKEY);
			break;
		case ESCORT_GOAL_GOLD_KEY:
			sprintf (szGoal, TXT_GB_YELLOWKEY);
			break;
		case ESCORT_GOAL_RED_KEY:
			sprintf (szGoal, TXT_GB_REDKEY);
			break;
		case ESCORT_GOAL_CONTROLCEN:
			sprintf (szGoal, TXT_GB_REACTOR);
			break;
		case ESCORT_GOAL_BOSS:
			sprintf (szGoal, TXT_GB_BOSS);
			break;
		case ESCORT_GOAL_EXIT:
			sprintf (szGoal, TXT_GB_EXIT);
			break;
		case ESCORT_GOAL_MARKER1:
		case ESCORT_GOAL_MARKER2:
		case ESCORT_GOAL_MARKER3:
		case ESCORT_GOAL_MARKER4:
		case ESCORT_GOAL_MARKER5:
		case ESCORT_GOAL_MARKER6:
		case ESCORT_GOAL_MARKER7:
		case ESCORT_GOAL_MARKER8:
		case ESCORT_GOAL_MARKER9:
			sprintf (szGoal, TXT_GB_MARKER, next_goal-ESCORT_GOAL_MARKER1+1);
			break;

	}

	if (!gameData.escort.bMsgsSuppressed)
		sprintf (tstr, TXT_GB_SUPPRESS);
	else
		sprintf (tstr, TXT_GB_ENABLE);

	i = ShowEscortHelp (szGoal, tstr);
	if (i < 11) {
		gameData.escort.bSearchingMarker = -1;
		gameData.escort.nLastKey = -1;
		EscortSetSpecialGoal (i ? KEY_1 + i - 1 : KEY_0);
		gameData.escort.nLastKey = -1;
		paused = 0;
		}
	else if (i == 11) {
		BuddyMessage (gameData.escort.bMsgsSuppressed ? TXT_GB_MSGS_ON : TXT_GB_MSGS_OFF);
		gameData.escort.bMsgsSuppressed = !gameData.escort.bMsgsSuppressed;
		paused = 0;
		}
	GameFlushInputs ();
	paletteManager.LoadEffect ();
	if (!gameOpts->menus.nStyle)
		StartTime (0);
	DigiResumeDigiSounds ();
}

// --------------------------------------------------------------------------------------------------------------------
//	Create a Buddy bot.
//	This automatically happens when you bring up the Buddy menu in a debug version.
//	It is available as a cheat in a non-debug (release) version.
void CreateBuddyBot (void)
{
	ubyte	buddy_id;
	vmsVector	vObjPos;

for (buddy_id = 0; buddy_id < gameData.bots.nTypes [0]; buddy_id++)
	if (gameData.bots.info [0][buddy_id].companion)
		break;

	if (buddy_id == gameData.bots.nTypes [0]) {
#if TRACE
		con_printf (CONDBG, "Can't create Buddy.  No 'companion' bot found in gameData.bots.infoP!\n");
#endif
		return;
	}
	COMPUTE_SEGMENT_CENTER_I (&vObjPos, OBJSEG (gameData.objs.consoleP));
	CreateMorphRobot (gameData.segs.segments + OBJSEG (gameData.objs.consoleP), &vObjPos, buddy_id);
}

//	-------------------------------------------------------------------------------
//	Show the Buddy menu!
#if 0 //obsolete in d2x-xl
void ShowEscortMenu (char *msg)
{
	int	w,h,aw;
	int	x,y;
	bkg	bg;

	memset (&bg, 0, sizeof (bg));
	bg.bIgnoreBg = 1;

	CCanvas::SetCurrent (&gameStates.render.vr.buffers.screenPages [0]);
	fontManager.SetCurrent ( GAME_FONT );
	FONT->StringSize (msg,&w,&h,&aw);
	x = (screen.Width ()-w)/2;
	y = (screen.Height ()-h)/4;
	fontManager.SetColorRGBi (RGBA (0, PAL2RGBA (28), 0, 255), 1, 0, 0);
   NMDrawBackground (NULL,x-15,y-15,x+w+15-1,y+h+15-1);
  	GrUString ( x, y, msg );
	ResetCockpit ();
}
#endif

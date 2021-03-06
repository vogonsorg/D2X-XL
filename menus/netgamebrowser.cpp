#ifdef HAVE_CONFIG_H
#	include <conf.h>
#endif

#define PATCH12

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#	pragma pack(push)
#	pragma pack(8)
#	include <WinSock.h>
#	pragma pack(pop)
#else
#	include <sys/socket.h>
#endif

#include "descent.h"
#include "strutil.h"
#include "args.h"
#include "timer.h"
#include "ipx.h"
#include "ipx_udp.h"
#include "menu.h"
#include "key.h"
#include "error.h"
#include "network.h"
#include "network_lib.h"
#include "menu.h"
#include "text.h"
#include "byteswap.h"
#include "netmisc.h"
#include "kconfig.h"
#include "autodl.h"
#include "tracker.h"
#include "gamefont.h"
#include "menubackground.h"
#include "netmenu.h"
#include "timeout.h"
#include "console.h"

#define LHX(x)      (gameStates.menus.bHires?2* (x):x)
#define LHY(y)      (gameStates.menus.bHires? (24* (y))/10:y)

char szGameList [MAX_ACTIVE_NETGAMES + 5][100];

//------------------------------------------------------------------------------

void InitNetGameMenuOption (CMenu& menu, int32_t j)
{
if (j >= int32_t (menu.ToS ()))
	menu.AddMenu ("", szGameList [j]);
CMenuItem* m = menu + j;
sprintf (m->m_text, "%2d.                                                     ", j - 1 - tracker.m_bUse);
m->Value () = 0;
m->Rebuild ();
}

//------------------------------------------------------------------------------

void InitNetGameMenu (CMenu& menu, int32_t i)
{
	int32_t j;

for (j = i + 2 + tracker.m_bUse; i < MAX_ACTIVE_NETGAMES; i++, j++)
	InitNetGameMenuOption (menu, j);
}

//------------------------------------------------------------------------------

extern int32_t nTabs [];

char *PruneText (char *pszDest, char *pszSrc, int32_t nSize, int32_t nPos, int32_t nVersion)
{
	int32_t		lDots, lMax, l, tx, ty, ta;
	char*		psz;
	CFont*	curFont = CCanvas::Current ()->Font ();

if (gameOpts->menus.bShowLevelVersion && (nVersion >= 0)) {
	if (nVersion)
		sprintf (pszDest, "[%d] ", nVersion);
	else
		strcpy (pszDest, "[?] ");
	strncat (pszDest + 4, pszSrc, nSize - 4);
	}
else
	strncpy (pszDest, pszSrc, nSize);
pszDest [nSize - 1] = '\0';
if ((psz = strchr (pszDest, '\t')))
	*psz = '\0';
fontManager.SetCurrent (SMALL_FONT);
fontManager.Current ()->StringSize ("... ", lDots, ty, ta);
fontManager.Current ()->StringSize (pszDest, tx, ty, ta);
l = (int32_t) strlen (pszDest);
lMax = LHX (nTabs [nPos]) - LHX (nTabs [nPos - 1]);
if (tx > lMax) {
	lMax -= lDots;
	do {
		pszDest [--l] = '\0';
		fontManager.Current ()->StringSize (pszDest, tx, ty, ta);
	} while (tx > lMax);
	strcat (pszDest, "...");
	}
fontManager.SetCurrent (curFont); 
return pszDest;
}

//------------------------------------------------------------------------------

const char *szModeLetters []  = 
 {"ANRCHY", 
	 "TEAM", 
	 "ROBO", 
	 "COOP", 
	 "FLAG", 
	 "HOARD", 
	 "TMHOARD", 
	 "ENTROPY",
	 "MONSTER"};

int32_t NetworkJoinPoll (CMenu& menu, int32_t& key, int32_t nCurItem, int32_t nState)
{
if (nState)
	return nCurItem;

	// Polling loop for Join Game menu
	static fix t1 = 0;
	int32_t	t = SDL_GetTicks ();
	int32_t	i, h = 2 + tracker.m_bUse, osocket, nJoinStatus, bPlaySound = 0;
	const char	*psz;
	char	szOption [200];
	char	szTrackers [100];

if (tracker.m_bUse) {
	i = tracker.ActiveCount (0);
	menu [1].m_color = ORANGE_RGBA;
	sprintf (szTrackers, TXT_TRACKERS_FOUND, i, (i == 1) ? "" : "s");
	if (strcmp (menu [1].m_text, szTrackers)) {
		strcpy (menu [1].m_text, szTrackers);
//			menu [1].x = (int16_t) 0x8000;
		menu [1].m_bRebuild = 1;
		}
	}

if ((gameStates.multi.nGameType >= IPX_GAME) && networkData.bAllowSocketChanges) {
	osocket = networkData.nPortOffset;
	if (key == KEY_PAGEDOWN) { 
		networkData.nPortOffset--; 
		key = 0; 
		}
	else if (key == KEY_PAGEUP) { 
		networkData.nPortOffset++; 
		key = 0; 
		}
	if (networkData.nPortOffset > 99)
		networkData.nPortOffset = -99;
	else if (networkData.nPortOffset < -99)
		networkData.nPortOffset = 99;
	if (networkData.nPortOffset + IPX_DEFAULT_SOCKET > 0x8000)
		networkData.nPortOffset = 0x8000 - IPX_DEFAULT_SOCKET;
	if (networkData.nPortOffset + IPX_DEFAULT_SOCKET < 0)
		networkData.nPortOffset = IPX_DEFAULT_SOCKET;
	if (networkData.nPortOffset != osocket) {
		sprintf (menu [0].m_text, TXT_CURR_SOCK, 
					 (gameStates.multi.nGameType == IPX_GAME) ? "IPX" : "UDP", 
					 networkData.nPortOffset);
		menu [0].m_bRebuild = 1;
#if 1			
		console.printf (0, TXT_CHANGE_SOCK, networkData.nPortOffset);
#endif
		NetworkListen ();
		IpxChangeDefaultSocket ((uint16_t) (IPX_DEFAULT_SOCKET + networkData.nPortOffset));
		RestartNetSearching (menu);
		NetworkSendGameListRequest ();
		return nCurItem;
		}
	}
	// send a request for game info every 3 seconds
#if DBG
if (!networkData.nActiveGames)
#endif
	if (gameStates.multi.nGameType >= IPX_GAME) {
		if (t > t1 + 3000) {
			if (!NetworkSendGameListRequest ())
				return nCurItem;
			t1 = t;
			}
		}
NetworkListen ();
DeleteTimedOutNetGames ();
if (networkData.bGamesChanged || (networkData.nActiveGames != networkData.nLastActiveGames)) {
	networkData.bGamesChanged = 0;
	networkData.nLastActiveGames = networkData.nActiveGames;
#if 1			
	console.printf (CON_DBG, "Found %d netgames.\n", networkData.nActiveGames);
#endif
	// Copy the active games data into the menu options
	for (i = 0; i < networkData.nActiveGames; i++, h++) {
			int32_t gameStatus = activeNetGames [i].m_info.gameStatus;
			int32_t nplayers = 0;
			char szLevelName [100], szMissionName [100], szGameName [100];
			int32_t nLevelVersion = gameOpts->menus.bShowLevelVersion ? missionManager.FindByName (activeNetGames [i].m_info.szMissionName, -1) : -1;

		// These next two loops protect against menu skewing
		// if missiontitle or gamename contain a tab

		PruneText (szMissionName, activeNetGames [i].m_info.szMissionTitle, sizeof (szMissionName), 4, nLevelVersion);
		PruneText (szGameName, activeNetGames [i].m_info.szGameName, sizeof (szGameName), 1, -1);
		nplayers = activeNetGames [i].m_info.nConnected;
		if (activeNetGames [i].m_info.nLevel < 0)
			sprintf (szLevelName, "S%d", -activeNetGames [i].m_info.nLevel);
		else
			sprintf (szLevelName, "%d", activeNetGames [i].m_info.nLevel);
		if (gameStatus == NETSTAT_STARTING)
			psz = "Forming";
		else if (gameStatus == NETSTAT_PLAYING) {
			nJoinStatus = CanJoinNetGame (activeNetGames + i, NULL);

			if (nJoinStatus == 1)
				psz = "Open";
			else if (nJoinStatus == 2)
				psz = "Full";
			else if (nJoinStatus == 3)
				psz = "Restrict";
			else
				psz = "Closed";
			}
		else 
			psz = "Between";
		sprintf (szOption, "%2d.\t%s\t%s\t%d/%d\t%s\t%s\t%s", 
					i + 1, szGameName, szModeLetters [activeNetGames [i].m_info.gameMode], nplayers, 
					activeNetGames [i].m_info.nMaxPlayers, szMissionName, szLevelName, psz);
		Assert (strlen (szOption) < 100);
		if (strcmp (szOption, menu [h].m_text)) {
			memcpy (menu [h].m_text, szOption, 100);
			menu [h].m_bRebuild = 1;
			menu [h].m_bUnavailable = (nLevelVersion == 0);
			bPlaySound = 1;
			}
		}
	for (i = 3 + networkData.nActiveGames; i < MAX_ACTIVE_NETGAMES; i++, h++)
		InitNetGameMenuOption (menu, h);
	}
#if 0
else
	for (i = 3; i < networkData.nActiveGames; i++, h++)
		menu [h].m_value = SDL_GetTicks ();
for (i = 3 + networkData.nActiveGames; i < MAX_ACTIVE_NETGAMES; i++, h++)
	if (menu [h].m_value && (t - menu [h].m_value > 10000)) 
	 {
		InitNetGameMenuOption (menu, h);
		bPlaySound = 1;
		}
#endif
if (bPlaySound)
	audio.PlaySound (SOUND_HUD_MESSAGE);
return nCurItem;
}


//------------------------------------------------------------------------------

int32_t NetworkBrowseGames (void)
{
	CMenu	menu (MAX_ACTIVE_NETGAMES + 5);
	int32_t	choice, bAutoLaunch = (gameData.multiplayer.autoNG.bValid > 0);
	char	callsign [CALLSIGN_LEN+1];

memcpy (callsign, LOCALPLAYER.callsign, sizeof (callsign));
if (gameStates.multi.nGameType >= IPX_GAME) {
	if (!networkData.bActive) {
		TextBox (NULL, BG_STANDARD, 1, TXT_OK, TXT_IPX_NOT_FOUND);
		return 0;
		}
	}
LOCALPLAYER.timeLevel = 0;
NetworkInit ();
N_PLAYERS = 0;
setjmp (gameExitPoint);
networkData.nJoining = 0; 
networkData.nJoinState = 0;
networkData.nStatus = NETSTAT_BROWSING; // We are looking at a game menu
IpxChangeDefaultSocket ((uint16_t) (IPX_DEFAULT_SOCKET + networkData.nPortOffset));
NetworkFlush ();
networkThread.Start ();
NetworkListen ();  // Throw out old info
NetworkFlush ();
NetworkSendGameListRequest (); // broadcast a request for lists
networkData.nActiveGames = 0;
networkData.nLastActiveGames = 0;
memset (activeNetGames, 0, sizeof (activeNetGames));
memset (activeNetPlayers, 0, sizeof (activeNetPlayers));
if (!bAutoLaunch) {
	fontManager.SetColorRGBi (RGB_PAL (15, 15, 23), 1, 0, 0);
	menu.AddText ("", szGameList [0]);
	menu.Top ()->m_bNoScroll = 1;
	menu.Top ()->m_x = (int16_t) 0x8000;	//centered
	if (gameStates.multi.nGameType >= IPX_GAME) {
		if (networkData.bAllowSocketChanges)
			sprintf (menu.Top ()->m_text, TXT_CURR_SOCK, (gameStates.multi.nGameType == IPX_GAME) ? "IPX" : "UDP", networkData.nPortOffset);
		else
			*menu.Top ()->m_text = '\0';
		}
	if (tracker.m_bUse) {
		menu.AddText ("", szGameList [1]);
		strcpy (menu.Top ()->m_text, TXT_0TRACKERS);
		menu.Top ()->m_x = (int16_t) 0x8000;
		menu.Top ()->m_bNoScroll = 1;
		}
	menu.AddText ("", szGameList [1 + tracker.m_bUse]);
	strcpy (menu.Top ()->m_text, TXT_GAME_BROWSER); // skip leading tab
	menu.Top ()->m_bNoScroll = 1;
	InitNetGameMenu (menu, 0);
	}
networkData.bGamesChanged = 1;    

do {
	networkData.nStatus = NETSTAT_BROWSING;
	for (;;) {
		gameStates.app.nExtGameStatus = GAMESTAT_JOIN_NETGAME;
		if (bAutoLaunch) {
			static CTimeout to (1000, true);

			do {
				if (to.Expired ())
					NetworkSendGameListRequest (bAutoLaunch);
				G3_SLEEP (5);
				NetworkListen ();
				if (KeyInKey () == KEY_ESC)
					return 0;
				} while (!networkData.nActiveGames);
			choice = 2 + tracker.m_bUse;
			}
		else {
			gameStates.multi.bSurfingNet = 1;
			choice = menu.Menu (TXT_NETGAMES, NULL, NetworkJoinPoll, NULL, BG_SUBMENU, BG_STANDARD, LHX (340), -1, 1);
			gameStates.multi.bSurfingNet = 0;
			}

		if (choice == -1) {
			ChangePlayerNumTo (0);
			memcpy (LOCALPLAYER.callsign, callsign, sizeof (callsign));
			networkData.nStatus = NETSTAT_MENU;
			return 0; // they cancelled               
			}               
		choice -= (2 + tracker.m_bUse);
		if ((choice < 0) || (choice >= networkData.nActiveGames)) {
			//InfoBox (TXT_SORRY, BG_STANDARD, 1, TXT_OK, TXT_INVALID_CHOICE);
			continue;
			}

		// Choice has been made and looks legit
		if (AGI.m_info.gameStatus == NETSTAT_ENDLEVEL) {
			TextBox (TXT_SORRY, BG_STANDARD, 1, TXT_OK, TXT_NET_GAME_BETWEEN2);
			continue;
			}

		if (AGI.m_info.protocolVersion != MULTI_PROTO_VERSION) {
			if (AGI.m_info.protocolVersion == 3) {
				TextBox (TXT_SORRY, BG_STANDARD, 1, TXT_OK, TXT_INCOMPAT1);
				}
			else if (AGI.m_info.protocolVersion == 4) {
				}
			else {
				char	szFmt [200], szError [200];

				sprintf (szFmt, "%s%s", TXT_VERSION_MISMATCH, TXT_NETGAME_VERSIONS);
				sprintf (szError, szFmt, MULTI_PROTO_VERSION, AGI.m_info.protocolVersion);
				TextBox (TXT_SORRY, BG_STANDARD, 1, TXT_OK, szError);
				}
			continue;
			}

		// Check for valid mission name
		memcpy (&networkData.serverAddress.m_address, activeNetGames [choice].m_server, sizeof (networkData.serverAddress.m_address));
		console.printf (CON_DBG, TXT_LOADING_MSN, AGI.m_info.szMissionName);
		if (!(missionManager.LoadByName (AGI.m_info.szMissionName, 0, gameFolders.missions.szDownloads) || missionManager.LoadByName (AGI.m_info.szMissionName, -1) ||	
				(downloadManager.DownloadMission (AGI.m_info.szMissionName) && missionManager.LoadByName (AGI.m_info.szMissionName, 0, gameFolders.missions.szDownloads)))) {
			PrintLog (0, "Mission '%s' not found\n", AGI.m_info.szMissionName);
			TextBox (NULL, BG_STANDARD, 1, TXT_OK, TXT_MISSION_NOT_FOUND);
			continue;
			}

		if (IS_D2_OEM && (AGI.m_info.nLevel > 8)) {
			TextBox (NULL, BG_STANDARD, 1, TXT_OK, TXT_OEM_ONLY8);
			continue;
			}

		if (IS_MAC_SHARE && (AGI.m_info.nLevel > 4)) {
			TextBox (NULL, BG_STANDARD, 1, TXT_OK, TXT_SHARE_ONLY4);
			continue;
			}

		if (!NetworkWaitForAllInfo (choice)) {
			TextBox (TXT_SORRY, BG_STANDARD, 1, TXT_OK, TXT_JOIN_ERROR);
			networkData.nStatus = NETSTAT_BROWSING; // We are looking at a game menu
			continue;
			}       

		networkData.nStatus = NETSTAT_BROWSING; // We are looking at a game menu
		  if (!CanJoinNetGame (activeNetGames + choice, activeNetPlayers + choice)) {
			if (AGI.m_info.nNumPlayers == AGI.m_info.nMaxPlayers)
				TextBox (TXT_SORRY, BG_STANDARD, 1, TXT_OK, TXT_GAME_FULL);
			else
				TextBox (TXT_SORRY, BG_STANDARD, 1, TXT_OK, TXT_IN_PROGRESS);
			continue;
			}
		break;
		}

	// Choice is valid, prepare to join in
	netGameInfo = activeNetGames [choice];
	netPlayers [0] = activeNetPlayers [choice];
	gameStates.app.nDifficultyLevel = netGameInfo.m_info.difficulty;
	gameData.multiplayer.nMaxPlayers = netGameInfo.m_info.nMaxPlayers;

	if (SetLocalPlayer (&netPlayers [0], netGameInfo.m_info.nNumPlayers, 1) < 0)
		return 0;
	ResetPlayerData (true, false, false, -1);
	memcpy (LOCALPLAYER.callsign, callsign, sizeof (callsign));
	// Handle the extra data for the network driver
	// For the mcast4 driver, this is the game's multicast address, to
	// which the driver subscribes.
	} while (IpxHandleNetGameAuxData (netGameInfo.AuxData ()) < 0);

NetworkSetGameMode (netGameInfo.m_info.gameMode);
//networkThread.Start ();
return StartNewLevel (netGameInfo.m_info.GetLevel (), true);
}

//------------------------------------------------------------------------------


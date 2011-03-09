#ifdef HAVE_CONFIG_H
#	include <conf.h>
#endif

#define PATCH12

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#	include <winsock.h>
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

#define LHX(x)      (gameStates.menus.bHires ? 2 * (x) : x)
#define LHY(y)      (gameStates.menus.bHires? (24* (y)) / 10 : y)

#define AGI	activeNetGames [choice]
#define AXI activeExtraGameInfo [choice]

/* the following are the possible packet identificators.
 * they are stored in the "nType" field of the packet structs.
 * they are offset 4 bytes from the beginning of the raw IPX data
 * because of the "driver's" ipx_packetnum (see linuxnet.c).
 */

void SetFunctionMode (int);
extern ubyte ipx_MyAddress [10];

//------------------------------------------------------------------------------

#if 1
extern CNetGameInfo activeNetGames [];
extern tExtraGameInfo activeExtraGameInfo [];
#endif

char szHighlight [] = {1, (char) 255, (char) 192, (char) 128, 0};

#define FLAGTEXT(_b)	((_b) ? TXT_ON : TXT_OFF)

#define	INITFLAGS(_t) \
		 {sprintf (mTexts [opt], _t); strcat (mTexts [opt], szHighlight); j = 0;}

#define	ADDFLAG(_f,_t) \
	if (_f) {if (j) strcat (mTexts [opt], ", "); strcat (mTexts [opt], _t); if (++j == 5) {opt++; INITFLAGS ("   ")}; }

//------------------------------------------------------------------------------

void ShowNetGameInfo (int choice)
{
	CMenu	m (30);
   char	mTexts [30][200];
	int	i, j, nInMenu, opt = 0;

#if !DBG
if (choice >= networkData.nActiveGames)
	return;
#endif
memset (mTexts, 0, sizeof (mTexts));
sprintf (mTexts [opt], TXT_NGI_GAME, szHighlight, AGI.m_info.szGameName); 
opt++;
sprintf (mTexts [opt], TXT_NGI_MISSION, szHighlight, AGI.m_info.szMissionTitle); 
opt++;
sprintf (mTexts [opt], TXT_NGI_LEVEL, szHighlight, AGI.m_info.nLevel); 
opt++;
sprintf (mTexts [opt], TXT_NGI_SKILL, szHighlight, MENU_DIFFICULTY_TEXT (AGI.m_info.difficulty)); 
opt++;
opt++;
#if !DBG
if (!*AXI.szGameName) {
	sprintf (mTexts [opt], "Gamehost is not using D2X-XL or running in pure mode");
	opt++;
	}
else 
#endif
	{
	if (AXI.bShadows || AXI.bUseParticles || (!AXI.bCompetition && AXI.bUseLightning)) {
		INITFLAGS ("Graphics Fx: "); 
		ADDFLAG (AXI.bShadows, "Shadows");
		ADDFLAG (AXI.bUseParticles, "Smoke");
		if (!AXI.bCompetition)
			ADDFLAG (AXI.bUseLightning, "Lightning");
		}
	else
		strcpy (mTexts [opt], "Graphics Fx: None");
	opt++;
	if (!AXI.bCompetition && (AXI.bLightTrails || AXI.bTracers)) {
		INITFLAGS ("Weapon Fx: ");
		ADDFLAG (AXI.bLightTrails, "Light trails");
		ADDFLAG (AXI.bTracers, "Tracers");
		}
	else
		sprintf (mTexts [opt], "Weapon Fx: None");
	opt++;
	if (!AXI.bCompetition && (AXI.bDamageExplosions || AXI.bPlayerShield)) {
		INITFLAGS ("Ship Fx: ");
		ADDFLAG (AXI.bPlayerShield, "Shield");
		ADDFLAG (AXI.bDamageExplosions, "Damage");
		ADDFLAG (AXI.bShowWeapons, "Weapons");
		ADDFLAG (AXI.bGatlingSpeedUp, "Gatling speedup");
		}
	else
		sprintf (mTexts [opt], "Ship Fx: None");
	opt++;
	if (AXI.nWeaponIcons || (!AXI.bCompetition && (AXI.bTargetIndicators || AXI.bDamageIndicators))) {
		INITFLAGS ("HUD extensions: ");
		ADDFLAG (AXI.nWeaponIcons != 0, "Icons");
		ADDFLAG (!AXI.bCompetition && AXI.bTargetIndicators, "Tgt indicators");
		ADDFLAG (!AXI.bCompetition && AXI.bDamageIndicators, "Dmg indicators");
		ADDFLAG (!AXI.bCompetition && AXI.bMslLockIndicators, "Trk indicators");
		}
	else
		strcat (mTexts [opt], "HUD extensions: None");
	opt++;
	if (!AXI.bCompetition && AXI.bRadarEnabled) {
		INITFLAGS ("Radar: ");
		ADDFLAG ((AGI.m_info.gameFlags & NETGAME_FLAG_SHOW_MAP) != 0, "Players");
		ADDFLAG (AXI.nRadar, "Radar");
		ADDFLAG (AXI.bPowerupsOnRadar, "Powerups");
		ADDFLAG (AXI.bRobotsOnRadar, "Robots");
		}
	else
		strcat (mTexts [opt], "Radar: off");
	opt++;
	if (!AXI.bCompetition && (AXI.bMouseLook || AXI.bFastPitch)) {
		INITFLAGS ("Controls ext.: ");
		ADDFLAG (AXI.bMouseLook, "mouselook");
		ADDFLAG (AXI.bFastPitch, "fast pitch");
		}
	else
		strcat (mTexts [opt], "Controls ext.: None");
	opt++;
	if (!AXI.bCompetition && 
		 (AXI.bDualMissileLaunch || !AXI.bFriendlyFire || AXI.bInhibitSuicide || 
		  AXI.bEnableCheats || AXI.bDarkness || (AXI.nFusionRamp != 2))) {
		INITFLAGS ("Gameplay ext.: ");
		ADDFLAG (AXI.bEnableCheats, "Cheats");
		ADDFLAG (AXI.bDarkness, "Darkness");
		ADDFLAG (AXI.bSmokeGrenades, "Smoke Grens");
		ADDFLAG (AXI.bDualMissileLaunch, "Dual Msls");
		ADDFLAG (AXI.nFusionRamp != 2, "Fusion ramp");
		ADDFLAG (!AXI.bFriendlyFire, "no FF");
		ADDFLAG (AXI.bInhibitSuicide, "no suicide");
		ADDFLAG (AXI.bKillMissiles, "shoot msls");
		ADDFLAG (AXI.bTripleFusion, "tri fusion");
		ADDFLAG (AXI.bEnhancedShakers, "enh shakers");
		ADDFLAG (AXI.nHitboxes, "hit boxes");
		}
	else
		strcat (mTexts [opt], "Gameplay ext.: None");
	opt++;
	}
for (i = 0; i < opt; i++)
	m.AddText (reinterpret_cast<char*> (mTexts + i));
bAlreadyShowingInfo = 1;
nInMenu = gameStates.menus.nInMenu;
gameStates.menus.nInMenu = 0;
gameStates.menus.bNoBackground = 0;
m.TinyMenu (NULL, TXT_NETGAME_INFO);
gameStates.menus.bNoBackground = 0;
gameStates.app.bGameRunning = 0;
gameStates.menus.nInMenu = nInMenu;
bAlreadyShowingInfo = 0;
 }

//------------------------------------------------------------------------------

#undef AGI
#undef AXI

#define AGI netGame.m_info
#define AXI	extraGameInfo [0]

char* XMLGameInfo (void)
{
	static char xmlGameInfo [UDP_PAYLOAD_SIZE];

	static char* szGameType [2][9] = {
		{"Anarchy", "Anarchy", "Anarchy", "Coop", "CTF", "Hoard", "Hoard", "Monsterball", "Entropy"},
		{"Anarchy", "Anarchy", "Anarchy", "Coop", "CTF+", "Hoard", "Hoard", "Monsterball", "Entropy"}
		};
	static char* szGameState [] = {"open", "closed", "restricted"};

sprintf (xmlGameInfo, "<?xml version=\"1.0\"?>\n<GameInfo>\n  <Descent>\n");
sprintf (xmlGameInfo + strlen (xmlGameInfo), "    <Host>%s</Host>\n",gameData.multiplayer.players [gameData.multiplayer.nLocalPlayer].callsign);
sprintf (xmlGameInfo + strlen (xmlGameInfo), "    <Mission name=\"%s\" level=\"%d\" />\n", AGI.szMissionTitle, AGI.nLevel);
sprintf (xmlGameInfo + strlen (xmlGameInfo), "    <Player current=\"%d\" max=\"%d\" />\n", AGI.nNumPlayers, AGI.nMaxPlayers);
if (IsCoopGame || (mpParams.nGameMode & NETGAME_ROBOT_ANARCHY))
	sprintf (xmlGameInfo + strlen (xmlGameInfo), "    <Difficulty>%d<Difficulty>\n", AGI.difficulty);
sprintf (xmlGameInfo + strlen (xmlGameInfo), "    <Mode type=\"%s\" team=\"%d\" robots=\"%d\" />\n",
			szGameType [extraGameInfo [0].bEnhancedCTF][mpParams.nGameMode],
			(mpParams.nGameMode != NETGAME_ANARCHY) && (mpParams.nGameMode != NETGAME_ROBOT_ANARCHY) && (mpParams.nGameMode != NETGAME_HOARD),
			(mpParams.nGameMode == NETGAME_ROBOT_ANARCHY) || (mpParams.nGameMode == NETGAME_COOPERATIVE));
sprintf (xmlGameInfo + strlen (xmlGameInfo), "    <Status>%s</Status>\n",
			(AGI.nNumPlayers == AGI.nMaxPlayers)
			? "full"
			: (networkData.nStatus == NETSTAT_STARTING)
				? "forming"
				: szGameState [mpParams.nGameAccess]);
strcat (xmlGameInfo, "  </Descent>\n  <Extensions>\n    <D2X-XL>\n");
sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <CompetitionMode>%d</CompetitionMode>\n", AXI.bCompetition);
sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <GraphicsFx Shadows=\"%d\" Smoke=\"%d\" Lightning=\"%d\" />\n",
			AXI.bShadows, 
			AXI.bUseParticles, 
			!AXI.bCompetition && AXI.bUseLightning);

sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <WeaponFx LightTrails=\"%d\" Tracers=\"%d\" />\n",
			!AXI.bCompetition && AXI.bLightTrails, 
			!AXI.bCompetition && AXI.bTracers);

sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <ShipFx Shield=\"%d\" Damage=\"%d\" Weapons=\"%d\" GatlingSpeedup=\"%d\" />\n",
			!AXI.bCompetition && AXI.bPlayerShield, 
			!AXI.bCompetition && AXI.bDamageExplosions, 
			!AXI.bCompetition && AXI.bShowWeapons, 
			!AXI.bCompetition && AXI.bGatlingSpeedUp);

sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <HUD Icons=\"%d\" TgtInd=\"%d\" DmgInd=\"%d\" TrkInd=\"%d\" />\n",
			AXI.nWeaponIcons != 0,
			!AXI.bCompetition && AXI.bTargetIndicators, 
			!AXI.bCompetition && AXI.bDamageIndicators, 
			!AXI.bCompetition && AXI.bMslLockIndicators);

sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <Radar Players=\"%d\" Powerups=\"%d\" Robots=\"%d\"  HUD=\"%d\" />\n",
			!AXI.bCompetition && AXI.bRadarEnabled && ((AGI.gameFlags & NETGAME_FLAG_SHOW_MAP) != 0),
			!AXI.bCompetition && AXI.bRadarEnabled && AXI.bPowerupsOnRadar,
			!AXI.bCompetition && AXI.bRadarEnabled && AXI.bRobotsOnRadar,
			!AXI.bCompetition && AXI.bRadarEnabled && (AXI.nRadar != 0));

sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <Controls MouseLook=\"%d\" FastPitch=\"%d\" />\n",
			!AXI.bCompetition && AXI.bMouseLook, 
			!AXI.bCompetition && AXI.bFastPitch);

sprintf (xmlGameInfo + strlen (xmlGameInfo), "      <GamePlay Cheats=\"%d\" Darkness=\"%d\" SmokeGrenades=\"%d\" DualMissiles=\"%d\" FusionRamp=\"%d\" FriendlyFire=\"%d\" Suicide=\"%d\" KillMissiles=\"%d\" TriFusion=\"%d\" BetterShakers=\"%d\" HitBoxes=\"%d\" />\n",
			!AXI.bCompetition && AXI.bEnableCheats,
			!AXI.bCompetition && AXI.bDarkness, 
			!AXI.bCompetition && AXI.bSmokeGrenades,
			!AXI.bCompetition && AXI.bDualMissileLaunch,
			!AXI.bCompetition && (AXI.nFusionRamp != 2), 
			AXI.bCompetition || AXI.bFriendlyFire,
			AXI.bCompetition || !AXI.bInhibitSuicide, 
			!AXI.bCompetition && AXI.bKillMissiles, 
			!AXI.bCompetition && AXI.bTripleFusion, 
			!AXI.bCompetition && AXI.bEnhancedShakers,
			!AXI.bCompetition && AXI.nHitboxes);

strcat (xmlGameInfo, "    </D2X-XL>\n  </Extensions>\n</GameInfo>\n");
PrintLog ("\nXML game info:\n\n");
PrintLog (xmlGameInfo);
PrintLog ("\n");
return xmlGameInfo;
}

//------------------------------------------------------------------------------


#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(__unix__) || defined(__macosx__)
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef __macosx__
#	include "SDL/SDL_main.h"
#	include "SDL/SDL_keyboard.h"
#	include "FolderDetector.h"
#else
#	include "SDL_main.h"
#	include "SDL_keyboard.h"
#endif
#include "descent.h"
#include "findfile.h"
#include "args.h"
#include "text.h"

#ifdef __macosx__
#	include <SDL/SDL.h>
#	if USE_SDL_MIXER
#		include <SDL_mixer/SDL_mixer.h>
#	endif
#else
#	include <SDL.h>
#	if USE_SDL_MIXER
#		include <SDL_mixer.h>
#	endif
#endif
#include "hogfile.h"
#include "menubackground.h"
#include "vers_id.h"

// ----------------------------------------------------------------------------

#ifdef _WIN32
#	define	DEFAULT_GAME_FOLDER		""
#	define	D2X_APPNAME					"d2x-xl.exe"
#elif defined(__macosx__)
#	define	DEFAULT_GAME_FOLDER		"/Applications/Games/D2X-XL"
#	define	D2X_APPNAME					"d2x-xl"
#else
#	define	DEFAULT_GAME_FOLDER		"/usr/local/games/d2x-xl"
#	define	D2X_APPNAME					"d2x-xl"
#endif

#ifndef SHAREPATH
#	define SHAREPATH						""
#endif

#if defined(__macosx__)
#	define	DATA_FOLDER					"Data"
#	define	SHADER_FOLDER				"Shaders"
#	define	MODEL_FOLDER				"Models"
#	define	SOUND_FOLDER				"Sounds"
#	define	SOUND_FOLDER1				"Sounds1"
#	define	SOUND_FOLDER2				"Sounds2"
#	define	SOUND_FOLDER1_D1			"Sounds1/D1"
#	define	SOUND_FOLDER2_D1			"Sounds2/D1"
#	define	SOUND_FOLDER_D2X			"Sounds2/d2x-xl"
#	define	CONFIG_FOLDER				"Config"
#	define	PROF_FOLDER					"Profiles"
#	define	SCRSHOT_FOLDER				"Screenshots"
#	define	MOVIE_FOLDER				"Movies"
#	define	SAVE_FOLDER					"Savegames"
#	define	DEMO_FOLDER					"Demos"
#	define	TEXTURE_FOLDER				"Textures"
#	define	TEXTURE_FOLDER_D2			"Textures"
#	define	TEXTURE_FOLDER_D1			"Textures/D1"
#	define	CACHE_FOLDER				"Cache"
#	define	LIGHTMAP_FOLDER			"Lightmaps"
#	define	LIGHTDATA_FOLDER			"Lights"
#	define	MESH_FOLDER					"Meshes"
#	define	MISSIONSTATE_FOLDER		"Missions"
#	define	MOD_FOLDER					"Mods"
#	define	MUSIC_FOLDER				"Music"
#	define	DOWNLOAD_FOLDER			"Downloads"
#	define	WALLPAPER_FOLDER			"Wallpapers"
#else
#	define	DATA_FOLDER					"data"
#	define	SHADER_FOLDER				"shaders"
#	define	MODEL_FOLDER				"models"
#	define	SOUND_FOLDER				"sounds"
#	define	SOUND_FOLDER1				"sounds1"
#	define	SOUND_FOLDER2				"sounds2"
#	define	SOUND_FOLDER1_D1			"sounds1/D1"
#	define	SOUND_FOLDER2_D1			"sounds2/D1"
#	define	SOUND_FOLDER_D2X			"sounds2/d2x-xl"
#	define	CONFIG_FOLDER				"config"
#	define	PROF_FOLDER					"profiles"
#	define	SCRSHOT_FOLDER				"screenshots"
#	define	MOVIE_FOLDER				"movies"
#	define	SAVE_FOLDER					"savegames"
#	define	DEMO_FOLDER					"demos"
#	define	TEXTURE_FOLDER				"textures"
#	define	TEXTURE_FOLDER_D2			"textures/d2"
#	define	TEXTURE_FOLDER_D1			"textures/d1"
#	define	WALLPAPER_FOLDER			"textures/wallpapers"
#	define	MOD_FOLDER					"mods"
#	define	MUSIC_FOLDER				"music"
#	define	DOWNLOAD_FOLDER			"downloads"
#	define	LIGHTMAP_FOLDER			"lightmaps"
#	define	LIGHTDATA_FOLDER			"lights"
#	define	MESH_FOLDER					"meshes"
#	define	MISSIONSTATE_FOLDER		"missions"
#	if DBG
#		define	CACHE_FOLDER			"cache/debug"
#	else
#		define	CACHE_FOLDER			"cache"
#	endif
#endif

// ----------------------------------------------------------------------------

char* CheckFolder (char* pszAppFolder, char* pszFolder, char* pszFile)
{
if (*pszFolder) {
	CFile::SplitPath (pszFolder, gameFolders.szGameDir, NULL, NULL);
	FlipBackslash (pszAppFolder);
	if (GetAppFolder ("", pszAppFolder, pszAppFolder, "d2x-xl.exe"))
		*pszAppFolder = '\0';
	else
		AppendSlash (pszAppFolder);
	}
return pszAppFolder;
}

// ----------------------------------------------------------------------------

int CheckDataFolder (char* pszDataRootDir)
{
return GetAppFolder ("", gameFolders.szDataDir [0], pszDataRootDir, "descent2.hog") &&
		 GetAppFolder ("", gameFolders.szDataDir [0], pszDataRootDir, "d2demo.hog") &&
		 GetAppFolder (pszDataRootDir, gameFolders.szDataDir [0], DATA_FOLDER, "descent2.hog") &&
		 GetAppFolder (pszDataRootDir, gameFolders.szDataDir [0], DATA_FOLDER, "d2demo.hog");
}

// ----------------------------------------------------------------------------

void MakeFolder (char* pszAppFolder, char* pszFolder = "", char* pszSubFolder = "", char* format = "%s/%s")
{
if (GetAppFolder (pszFolder, pszAppFolder, pszSubFolder, "")) {
	if (pszFolder && *pszFolder)
		sprintf (pszAppFolder, format, pszFolder, pszSubFolder);
	CFile::MkDir (pszAppFolder);
	}
}

// ----------------------------------------------------------------------------

void MakeTexSubFolders (char* pszParentFolder)
{
if (*pszParentFolder) {
		static char szTexSubFolders [4][4] = {"256", "128", "64", "dxt"};

		char	szFolder [FILENAME_LEN];

	for (int i = 0; i < 4; i++) {
		sprintf (szFolder, "%s/%s", pszParentFolder, szTexSubFolders [i]);
		CFile::MkDir (szFolder);
		}
	}
}

// ----------------------------------------------------------------------------

void GetAppFolders (void)
{
	int	i;
	char	szDataRootDir [2][FILENAME_LEN];

*gameFolders.szUserDir =
*gameFolders.szGameDir =
*gameFolders.szDataDir [0] =
*szDataRootDir [0] = 
*szDataRootDir [1] = '\0';

#ifdef _WIN32
if (!*CheckFolder (gameFolders.szGameDir, appConfig.Text ("-gamedir"), D2X_APPNAME) &&
	 !*CheckFolder (gameFolders.szGameDir, getenv ("DESCENT2"), D2X_APPNAME) &&
	 !*CheckFolder (gameFolders.szGameDir, appConfig [1], D2X_APPNAME))
	CheckFolder (gameFolders.szGameDir, DEFAULT_GAME_FOLDER, "");

if (!*CheckFolder (gameFolders.szUserDir, appConfig.Text ("-userdir"), ""))
	strcpy (gameFolders.szUserDir, gameFolders.szGameDir);

#else // Linux, OS X

*gameFolders.szSharePath = '\0';
if (*SHAREPATH) {
	if (strstr (SHAREPATH, "games"))
		sprintf (gameFolders.szSharePath, "%s/d2x-xl", SHAREPATH);
	else
		sprintf (gameFolders.szSharePath, "%s/games/d2x-xl", SHAREPATH);

if (!*CheckFolder (gameFolders.szGameDir, appConfig.Text ("-gamedir"), D2X_APPNAME) &&
	 !*CheckFolder (gameFolders.szameDir, gameFolders.szSharePath, D2X_APPNAME) &&
	 !*CheckFolder (gameFolders.szGameDir, getenv ("DESCENT2"), D2X_APPNAME))
	CheckFolder (gameFolders.szGameDir, DEFAULT_GAME_FOLDER, "");

if (!*CheckFolder (gameFolders.szUserDir, appConfig.Text ("-userdir"), "") &&
	!*CheckFolder (gameFolders.szUserDir, getenv ("HOME"), ""))
	strcpy (gameFolders.szUserDir, gameFolders.szGameDir);
strcat (gameFolders.szUserDir, ".d2x-xl");

if (!*CheckFolder (gameFolders.szDataDir [0], appConfig.Text ("-datadir"), D2X_APPNAME))
	strcpy (gameFolders.szDataDir [0], gameFolders.szGameDir);

if (*gameFolders.szGameDir)
	chdir (gameFolders.szGameDir);

#endif

if (!*CheckFolder (szDataRootDir [0], appConfig.Text ("-datadir"), "descent2.hog") &&
	 !*CheckFolder (szDataRootDir [0], appConfig.Text ("-hogdir"), "descent2.hog"))
#ifdef __macosx__
	GetOSXAppFolder (szDataRootDir [0], gameFolders.szGameDir);
#else
	strcpy (szDataRootDir [0], gameFolders.szGameDir);
#endif //__macosx__

if (CheckDataFolder (szDataRootDir [0])) {
#ifdef __macosx__
	GetOSXAppFolder (szDataRootDir [0], gameFolders.szGameDir);
#else
	strcpy (szDataRootDir [0], gameFolders.szGameDir);
#endif //__macosx__
	if (CheckDataFolder (szDataRootDir [0]))
		Error (TXT_NO_HOG2);
	}

/*---*/PrintLog (0, "expected game app folder = '%s'\n", gameFolders.szGameDir);
/*---*/PrintLog (0, "expected game data folder = '%s'\n", gameFolders.szDataDir [0]);
if (GetAppFolder (szDataRootDir [0], gameFolders.szModelDir [0], MODEL_FOLDER, "*.ase"))
	GetAppFolder (szDataRootDir [0], gameFolders.szModelDir [0], MODEL_FOLDER, "*.oof");
GetAppFolder (szDataRootDir [0], gameFolders.szSoundDir [0], SOUND_FOLDER1, "*.wav");
GetAppFolder (szDataRootDir [0], gameFolders.szSoundDir [1], SOUND_FOLDER2, "*.wav");
GetAppFolder (szDataRootDir [0], gameFolders.szSoundDir [6], SOUND_FOLDER_D2X, "*.wav");
if (GetAppFolder (szDataRootDir [0], gameFolders.szSoundDir [2], SOUND_FOLDER1_D1, "*.wav"))
	*gameFolders.szSoundDir [2] = '\0';
if (GetAppFolder (szDataRootDir [0], gameFolders.szSoundDir [3], SOUND_FOLDER2_D1, "*.wav"))
	*gameFolders.szSoundDir [3] = '\0';
GetAppFolder (szDataRootDir [0], gameFolders.szShaderDir, SHADER_FOLDER, "");
GetAppFolder (szDataRootDir [0], gameFolders.szTextureDir [0], TEXTURE_FOLDER_D2, "*.tga");
GetAppFolder (szDataRootDir [0], gameFolders.szTextureDir [1], TEXTURE_FOLDER_D1, "*.tga");
GetAppFolder (szDataRootDir [0], gameFolders.szModDir [0], MOD_FOLDER, "");
GetAppFolder (szDataRootDir [0], gameFolders.szMovieDir, MOVIE_FOLDER, "*.mvl");

strcpy (szDataRootDir [1], gameFolders.szUserDir);

// create the user folders

MakeFolder (szDataRootDir [1]);
MakeFolder (gameFolders.szCacheDir [1], szDataRootDir [1], CACHE_FOLDER);
MakeFolder (gameFolders.szProfDir, szDataRootDir [1], PROF_FOLDER);
MakeFolder (gameFolders.szSaveDir, szDataRootDir [1], SAVE_FOLDER);
MakeFolder (gameFolders.szScrShotDir, szDataRootDir [1], SCRSHOT_FOLDER);
MakeFolder (gameFolders.szDemoDir, szDataRootDir [1], DEMO_FOLDER);
MakeFolder (gameFolders.szConfigDir, szDataRootDir [1], CONFIG_FOLDER);
MakeFolder (gameFolders.szDownloadDir, gameFolders.szCacheDir [1], DOWNLOAD_FOLDER);
MakeFolder (gameFolders.szMissionStateDir, gameFolders.szCacheDir [1], MISSIONSTATE_FOLDER);

// create the shared folders

#ifdef __macosx__

char *pszOSXCacheDir = GetMacOSXCacheFolder ();
MakeFolder (gameFolders.szCacheDir [0], pszOSXCacheDir, DOWNLOAD_FOLDER);
strcpy (gameFolders.szCacheDir [1], gameFolders.szCacheDir [0]);
MakeFolder (gameFolders.szTextureCacheDir [0], pszOSXCacheDir, TEXTURE_FOLDER_D2);
MakeFolder (gameFolders.szTextureCacheDir [1], pszOSXCacheDir, TEXTURE_FOLDER_D1);
MakeFolder (gameFolders.szModelCacheDir [0], pszOSXCacheDir, MODEL_FOLDER);
MakeFolder (gameFolders.szCacheDir [0], pszOSXCacheDir, CACHE_FOLDER);
MakeFolder (gameFolders.szLightmapDir, pszOSXCacheDir, LIGHTMAP_FOLDER);
MakeFolder (gameFolders.szLightDataDir, pszOSXCacheDir, LIGHTDATA_FOLDER);
MakeFolder (gameFolders.szMeshDir, pszOSXCacheDir, MESH_FOLDER);
MakeFolder (gameFolders.szMissionStateDir, pszOSXCacheDir, MISSIONSTATE_FOLDER);
MakeFolder (gameFolders.szCacheDir [0], pszOSXCacheDir, CACHE_FOLDER, "%s/%s/256");
MakeFolder (gameFolders.szCacheDir [0], pszOSXCacheDir, CACHE_FOLDER, "%s/%s/128");
MakeFolder (gameFolders.szCacheDir [0], pszOSXCacheDir, CACHE_FOLDER, "%s/%s/64");
MakeFolder (gameFolders.szCacheDir [0], pszOSXCacheDir, CACHE_FOLDER);

#else

MakeFolder (szDataRootDir [0]);
MakeFolder (gameFolders.szDataDir [0], szDataRootDir [0], DATA_FOLDER);
MakeFolder (gameFolders.szDataDir [1], gameFolders.szDataDir [0], "d2x-xl");
MakeFolder (gameFolders.szCacheDir [0], szDataRootDir [0], CACHE_FOLDER);
MakeFolder (gameFolders.szTextureDir [0], gameFolders.szTextureDir [0], WALLPAPER_FOLDER);
MakeFolder (gameFolders.szTextureCacheDir [0], szDataRootDir [0], TEXTURE_FOLDER_D2);
MakeFolder (gameFolders.szTextureCacheDir [1], szDataRootDir [0], TEXTURE_FOLDER_D1);
MakeFolder (gameFolders.szModelCacheDir [0], szDataRootDir [0], MODEL_FOLDER);
MakeFolder (gameFolders.szLightmapDir, gameFolders.szCacheDir [0], LIGHTMAP_FOLDER);
MakeFolder (gameFolders.szLightDataDir, gameFolders.szCacheDir [0], LIGHTDATA_FOLDER);
MakeFolder (gameFolders.szMeshDir, gameFolders.szCacheDir [0], MESH_FOLDER);

#endif // __macosx__

for (i = 0; i < 2; i++)
	MakeTexSubFolders (gameFolders.szTextureCacheDir [i]);
MakeTexSubFolders (gameFolders.szModelCacheDir [0]);

if (GetAppFolder (szDataRootDir [1], gameFolders.szConfigDir, CONFIG_FOLDER, "*.ini"))
	strcpy (gameFolders.szConfigDir, gameFolders.szGameDir);

if (GetAppFolder (szDataRootDir [0], gameFolders.szMissionDir, BASE_MISSION_FOLDER, ""))
	GetAppFolder (gameFolders.szGameDir, gameFolders.szMissionDir, BASE_MISSION_FOLDER, "");
MakeFolder (gameFolders.szMissionDownloadDir, gameFolders.szMissionDir, DOWNLOAD_FOLDER);
}

// ----------------------------------------------------------------------------

void ResetModFolders (void)
{
gameStates.app.bHaveMod = 0;
*gameFolders.szModName =
*gameFolders.szMusicDir =
*gameFolders.szSoundDir [4] =
*gameFolders.szSoundDir [5] =
*gameFolders.szModDir [1] =
*gameFolders.szTextureDir [2] =
*gameFolders.szTextureCacheDir [2] =
*gameFolders.szTextureDir [3] =
*gameFolders.szTextureCacheDir [3] =
*gameFolders.szModelDir [1] =
*gameFolders.szModelCacheDir [1] =
*gameFolders.szModelDir [2] =
*gameFolders.szModelCacheDir [2] = 
*gameFolders.szWallpaperDir [1] = '\0';
sprintf (gameOpts->menus.altBg.szName [1], "default.tga");
}

// ----------------------------------------------------------------------------

void MakeModFolders (const char* pszMission, int nLevel)
{

	static int nLoadingScreen = -1;
	static int nShuffledLevels [24];

int bDefault, bBuiltIn;

ResetModFolders ();
if (gameStates.app.bDemoData)
	return;

if ((bDefault = (*pszMission == '\0')))
	pszMission = missionManager [missionManager.nCurrentMission].szMissionName;
else
	CFile::SplitPath (pszMission, NULL, gameFolders.szModName, NULL);

#if 1
if ((bBuiltIn = missionManager.IsBuiltIn (pszMission)))
	strcpy (gameFolders.szModName, (bBuiltIn == 1) ? "descent" : "descent2");
#else
if ((bBuiltIn = (strstr (pszMission, "Descent: First Strike") != NULL) ? 1 : 0))
	strcpy (gameFolders.szModName, "descent");
else if ((bBuiltIn = (strstr (pszMission, "Descent 2: Counterstrike!") != NULL) ? 2 : 0))
	strcpy (gameFolders.szModName, "descent2");
else if ((bBuiltIn = (strstr (pszMission, "d2x.hog") != NULL) ? 3 : 0))
	strcpy (gameFolders.szModName, "descent2");
#endif
else if (bDefault)
	return;

if (bBuiltIn && !gameOpts->app.bEnableMods)
	return;

if (GetAppFolder (gameFolders.szModDir [0], gameFolders.szModDir [1], gameFolders.szModName, "")) 
	*gameFolders.szModName = '\0';
else {
	sprintf (gameFolders.szSoundDir [4], "%s/%s", gameFolders.szModDir [1], SOUND_FOLDER);
	if (GetAppFolder (gameFolders.szModDir [1], gameFolders.szTextureDir [2], TEXTURE_FOLDER, "*.tga"))
		*gameFolders.szTextureDir [2] = '\0';
	else {
		sprintf (gameFolders.szTextureCacheDir [2], "%s/%s", gameFolders.szModDir [1], TEXTURE_FOLDER);
		//gameOpts->render.textures.bUseHires [0] = 1;
		}
	if (GetAppFolder (gameFolders.szModDir [1], gameFolders.szModelDir [1], MODEL_FOLDER, "*.ase") &&
		 GetAppFolder (gameFolders.szModDir [1], gameFolders.szModelDir [1], MODEL_FOLDER, "*.oof"))
		*gameFolders.szModelDir [1] = '\0';
	else {
		sprintf (gameFolders.szModelDir [1], "%s/%s", gameFolders.szModDir [1], MODEL_FOLDER);
		sprintf (gameFolders.szModelCacheDir [1], "%s/%s", gameFolders.szModDir [1], MODEL_FOLDER);
		}
	if (GetAppFolder (gameFolders.szModDir [1], gameFolders.szWallpaperDir [1], WALLPAPER_FOLDER, "*.tga")) {
		*gameFolders.szWallpaperDir [1] = '\0';
		*gameOpts->menus.altBg.szName [1] = '\0';
		}
	else {
		sprintf (gameFolders.szWallpaperDir [1], "%s/%s", gameFolders.szModDir [1], WALLPAPER_FOLDER);
		if (nLevel < 0)
			sprintf (gameOpts->menus.altBg.szName [1], "slevel%02d.tga", -nLevel);
		else if (nLevel > 0) {
			// chose a random custom loading screen for missions other than D2:CS that do not have their own custom loading screens
			if ((bBuiltIn == 2) && (missionManager.IsBuiltIn (hogFileManager.MissionName ()) != 2)) {
				if (nLoadingScreen < 0) { // create a random offset the first time this function is called and use it later on
					int i;
					for (i = 0; i < 24; i++)
						nShuffledLevels [i] = i + 1;
					gameStates.app.SRand ();
					for (i = 0; i < 23; i++) {
						int h = 23 - i;
						int j = h ? rand () % h : 0;
						Swap (nShuffledLevels [i], nShuffledLevels [i + j]);
						}
					}
				nLoadingScreen = (nLoadingScreen + 1) % 24;
				nLevel = nShuffledLevels [nLoadingScreen];
				}
			sprintf (gameOpts->menus.altBg.szName [1], "level%02d.tga", nLevel);
			}
		else 
			sprintf (gameOpts->menus.altBg.szName [1], "default.tga");
		backgroundManager.Rebuild ();
		}
	if (GetAppFolder (gameFolders.szModDir [1], gameFolders.szMusicDir, MUSIC_FOLDER, "*.ogg"))
		*gameFolders.szMusicDir = '\0';
	MakeTexSubFolders (gameFolders.szTextureCacheDir [2]);
	MakeTexSubFolders (gameFolders.szModelCacheDir [1]);
	gameStates.app.bHaveMod = 1;
	}
}

// ----------------------------------------------------------------------------

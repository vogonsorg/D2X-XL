// MIDI stuff follows.
#include <stdio.h>

#include "inferno.h"
#include "error.h"
#include "hmpfile.h"
#include "audio.h"
#include "songs.h"
#include "midi.h"

CMidi midi;

//------------------------------------------------------------------------------

void CMidi::Init (void)
{
m_nVolume = 255;
m_nPaused = 0;
m_music = NULL;
m_hmp = NULL;
}

//------------------------------------------------------------------------------

int CMidi::SetVolume (int nVolume)
{
	int nLastVolume = m_nVolume;

if (nVolume < 0)
	m_nVolume = 0;
else if (nVolume > 127)
	m_nVolume = 127;
else
	m_nVolume = nVolume;

#if USE_SDL_MIXER
if (gameOpts->sound.bUseSDLMixer)
	Mix_VolumeMusic (m_nVolume);
#endif
#if defined (_WIN32)
#	if USE_SDL_MIXER
else 
#	endif
if (m_hmp) {
	int mmVolume;

	// scale up from 0-127 to 0-0xffff
	mmVolume = (m_nVolume << 1) | (m_nVolume & 1);
	mmVolume |= (mmVolume << 8);
	nVolume = midiOutSetVolume ((HMIDIOUT)m_hmp->hmidi, mmVolume | (mmVolume << 16));
	}
#endif
return nLastVolume;
}

//------------------------------------------------------------------------------

int CMidi::PlaySong (char* pszSong, char* melodicBank, char* drumBank, int bLoop, int bD1Song)
{
	int	bCustom;
#if 0
if (!gameStates.sound.digi.bInitialized)
	return 0;
#endif
PrintLog ("DigiPlayMidiSong (%s)\n", pszSong);
audio.StopCurrentSong ();
if (!(pszSong && *pszSong))
	return 0;
if (m_nVolume < 1)
	return 0;
bCustom = ((strstr (pszSong, ".mp3") != NULL) || (strstr (pszSong, ".ogg") != NULL));
if (!(bCustom || (m_hmp = hmp_open (pszSong, bD1Song))))
	return 0;
#if USE_SDL_MIXER
if (gameOpts->sound.bUseSDLMixer) {
	char	fnSong [FILENAME_LEN], *pfnSong;

	if (bCustom) {
		pfnSong = pszSong;
		if (strstr (pszSong, ".mp3") && !songManager.MP3 ()) {
			audio.Shutdown ();
			songManager.SetMP3 (1);
			audio.Setup (1);
			}
		}
	else {
		if (!strstr (pszSong, ".mp3") && songManager.MP3 ()) {
			audio.Shutdown ();
			songManager.SetMP3 (0);
			audio.Setup (1);
			}
		sprintf (fnSong, "%s/d2x-temp.mid", *gameFolders.szCacheDir ? gameFolders.szCacheDir : gameFolders.szHomeDir);
		if (!hmp_to_midi (m_hmp, fnSong)) {
			PrintLog ("SDL_mixer failed to load %s\n(%s)\n", fnSong, Mix_GetError ());
			return 0;
			}
		pfnSong = fnSong;
		}
	if (!(m_music = Mix_LoadMUS (pfnSong))) {
		PrintLog ("SDL_mixer failed to load %s\n(%s)\n", fnSong, Mix_GetError ());
		return 0;
		}
	if (-1 == Mix_FadeInMusicPos (m_music, bLoop ? -1 : 1, songManager.Pos () ? 1000 : 1500, (double) songManager.Pos () / 1000.0)) {
		PrintLog ("SDL_mixer cannot play %s\n(%s)\n", pszSong, Mix_GetError ());
		songManager.SetPos (0);
		return 0;
		}
	PrintLog ("SDL_mixer playing %s\n", pszSong);
	if (songManager.Pos ())
		songManager.SetPos (0);
	else
		songManager.SetStart (SDL_GetTicks ());
	
	songManager.SetPlaying (1);
	SetVolume (m_nVolume);
	return 1;
	}
#endif
#if defined (_WIN32)
if (bCustom) {
	PrintLog ("Cannot play %s - enable SDL_mixer\n", pszSong);
	return 0;
	}
hmp_play (m_hmp, bLoop);
songManager.SetPlaying (1);
SetVolume (m_nVolume);
#endif
return 1;
}

//------------------------------------------------------------------------------

void CMidi::Pause (void)
{
#if 0
	if (!gameStates.sound.digi.bInitialized)
		return;
#endif

if (!m_nPaused) {
#if USE_SDL_MIXER
	if (gameOpts->sound.bUseSDLMixer)
		Mix_PauseMusic ();
#endif
	}
m_nPaused++;
}

//------------------------------------------------------------------------------

void CMidi::Resume (void)
{
Assert(m_nPaused > 0);
if (m_nPaused == 1) {
#if USE_SDL_MIXER
	if (gameOpts->sound.bUseSDLMixer)
		Mix_ResumeMusic ();
#endif
	}
m_nPaused--;
}

//------------------------------------------------------------------------------

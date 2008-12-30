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

#ifndef _SONGS_H
#define _SONGS_H

#define MAX_NUM_SONGS         100

#define SONG_TITLE            0
#define SONG_BRIEFING         1
#define SONG_ENDLEVEL         2
#define SONG_ENDGAME          3
#define SONG_CREDITS          4
#define SONG_FIRST_LEVEL_SONG 5
#define SONG_INTER				2

extern int Num_songs, nD1SongNum, nD2SongNum;   //how many MIDI songs

//------------------------------------------------------------------------------

class CRedbook {
	private:
		int	m_bForceRegister;
		int	m_bEnabled;
		int	m_bPlaying;
		fix	m_xLastCheck;

	public:
		CRedbook () { Init (); }
		~CRedbook () { Destroy (); }
		void Init (void);
		void Destroy (void);
		void Register (void);
		void SetVolume (int volume);
		int PlayTrack (int nTrack, int bKeepPlaying);
		void CheckRepeat (void);
		void ReInit (void);
		void Stop (void);
		int HaveD2CD (void);
		inline void ForceRegister (void) { m_bForceRegister = 1; }
		inline int Enabled (void) { return m_bEnabled; }
		inline int Playing (void) { return m_bPlaying; }
};

extern CRedbook redbook;

//------------------------------------------------------------------------------

class CUserMusicInfo {
	public:
		int					nLevelSongs;
		int					nCurrentSong;
		int					bMP3;
		CArray<char*>		levelSongs;
		char					szIntroSong [FILENAME_LEN];
		char					szBriefingSong [FILENAME_LEN];
		char					szCreditsSong [FILENAME_LEN];
		char					szMenuSong [FILENAME_LEN];
	};

class CSongData {
	public:
		char    filename [16];
		char    melodicBankFile [16];
		char    drumBankFile [16];
	};

class CSongInfo {
	public:
		CSongData			data [MAX_NUM_SONGS];
		int					bInitialized;
		int					bPlaying;
		int					bMP3;
		int					nTotalSongs;
		int					nSongs [2];
		int					nFirstLevelSong [2];
		int					nLevelSongs [2];
		int					nCurrent;
		int					nLevel;
		int					nD1EndLevelSong;
		time_t				tStart;
		time_t				tSlowDown;
		time_t				tPos;
	};

class CSongManager {
	private:
		CSongInfo		m_info;
		CUserMusicInfo	m_user;

	public:
		CSongManager () { Init (); }
		~CSongManager () { Destroy (); }
		void Init (void);
		void Destroy (void);
		void Setup (void);
		void CheckRepeat (void);
		void StopAll (void);
		void Play (int nSong, int repeat);
		void PlayLevel (int nLevel, int bFromHog);
		void PlayCurrent (int repeat);
		void Prev (void);
		void Next (void);
		int LoadPlayList (char *pszPlayList);
		void FreeUserSongs (void);
		inline int Current (void) { return m_info.nCurrent; }
		inline int MP3 (void) { return m_user.bMP3; }
	};

extern CSongManager songManager;

#endif /* _SONGS_H */

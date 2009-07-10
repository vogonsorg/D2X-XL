/* $Id: hmiplay.c,v 1.2 2003/03/13 00:20:21 btb Exp $ */
/*
 * HMI midi playing routines by Jani Frilander
 *
 * External device support by Patrick McCarthy
 *
 * Ported to d1x/sdl_threads by Matthew Mueller
 *
 */

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#if !(defined (USE_SDL_MIXER) && USE_SDL_MIXER)

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "music.h"
#include "cfile.h"
#include "u_mem.h"

#include <SDL_thread.h>

//#define WANT_AWE32 1

#ifdef WANT_AWE32
#include <linux/awe_voice.h>
#endif

//#define WANT_MPU401 1

#ifdef WANT_MPU401
#define MIDI_MESSAGE2(a,b) {		\
	SEQ_MIDIOUT(synth_dev,a);	\
	SEQ_MIDIOUT(synth_dev,b);	\
}

#define MIDI_MESSAGE3(a,b,c) {		\
	SEQ_MIDIOUT(synth_dev,a);	\
	SEQ_MIDIOUT(synth_dev,b);	\
	SEQ_MIDIOUT(synth_dev,c);	\
}
#endif

SEQ_DEFINEBUF(1024);

int drumflag = 1<<9;
int seqfd;
int synth_dev;
int program[16];
int stop;
double volume=1;

int ipc_queue_id = -1;
struct msgbuf *snd;

SDL_Thread *player_thread=NULL;

Voice_info *voices;
ubyte *data=NULL;

struct synth_info card_info;

void seqbuf_dump()
{
	if (_seqbufptr)
	{
		if (write(seqfd, _seqbuf, _seqbufptr) == -1)
		{
			perror ("Error writing sequencer device");
			SDL_KillThread(player_thread);
		}
	}
	_seqbufptr = 0;
}
	      
void my_quit()
{
//	//printf("goodbye\n");//#####
//	exit(0);
}

int seq_init()
{
	int nrmidis,nrsynths,i;

	if ((seqfd = open(SEQ_DEV, O_WRONLY, 0)) < 0)
	{
		perror ("Error opening sequencer device");
		return (-1);
	}

	if (ioctl(seqfd, SNDCTL_SEQ_NRSYNTHS, &nrsynths) == -1)
	{
		perror ("There is no soundcard");
		return (-1);
	}

	if (ioctl(seqfd, SNDCTL_SEQ_NRMIDIS, &nrmidis) == -1)
	{
		perror ("There is no soundcard");
		return (-1);
	}


	if(nrsynths < 1 && nrmidis < 1)
	{
		//printf("No synth or midi device!\n");
		return -1;
	}

	synth_dev = 0;

	//Check if we have wavetable synth device
	for (i=0; i < nrsynths; i++)
	{
		card_info.device = i;
		if (ioctl(seqfd, SNDCTL_SYNTH_INFO, &card_info) == -1)
		{
			perror("cannot get info on soundcard");
			return (-1);
		}
	
		if (card_info.synthType == SYNTH_TYPE_SAMPLE)
		{    
			synth_dev = i;
			break;
		}
	}

#ifdef WANT_AWE32
	for (i=0; i < nrsynths; i++)
	{
		card_info.device = i;
		if (ioctl(seqfd, SNDCTL_SYNTH_INFO, &card_info) == -1)
		{
			perror("cannot get info on soundcard");
			return (-1);
		}
	
		if (card_info.synthType==SYNTH_TYPE_SAMPLE
		    &&card_info.synth_subtype==SAMPLE_TYPE_AWE32) {
			synth_dev = i;
			break;
		}
	}
#endif

#ifdef WANT_MPU401
	for (i=0; i < nrmidis; i++)
	{
		struct midi_info cinfo;
		cinfo.device = i;
		if (ioctl(seqfd, SNDCTL_MIDI_INFO, &cinfo) == -1)
		{
			perror("cannot get info on soundcard");
			return (-1);
		}
	
		// Just take first available for now.
		card_info.synthType=SYNTH_TYPE_MIDI;
		card_info.device=i;
		synth_dev=i;
		break;
	}

	if (card_info.synthType!=SYNTH_TYPE_MIDI) {
#endif
	
		card_info.device = synth_dev;
		if (ioctl(seqfd, SNDCTL_SYNTH_INFO, &card_info) == -1)
		{
			perror("cannot get info on soundcard");
			return (-1);
		}
	
#ifdef WANT_MPU401
	}

	if (card_info.synthType==SYNTH_TYPE_MIDI) {
		// Insert some sort of midi reset here later.
	} else
#endif
#ifdef WANT_AWE32    
	  if (card_info.synthType == SYNTH_TYPE_SAMPLE
	      && card_info.synth_subtype == SAMPLE_TYPE_AWE32)
	{
		AWE_SET_CHANNEL_MODE(synth_dev,1);
		AWE_DRUM_CHANNELS(synth_dev,drumflag);
	}
	else
#endif
	{
		voices = new Voice_info [card_info.nr_voices];
		for (i=0;i<card_info.nr_voices;i++)
		{
			voices[i].note = -1;
			voices[i].channel = -1;
		}
	}

	return(0);
}

void seq_close()
{
	SEQ_DUMPBUF();
	ioctl(seqfd,SNDCTL_SEQ_SYNC);
	close(seqfd);
	delete[] voices;
	voices = NULL;
}

void set_program(int channel, int pgm)
{
#ifdef WANT_AWE32
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)
	{
		SEQ_SET_PATCH(synth_dev,channel,pgm);
	}
	else
#endif
	  program[channel]=pgm;
}

void start_note(int channel, int note, int vel)
{
	int i;

#ifdef WANT_AWE32
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)
	{
		SEQ_START_NOTE(synth_dev,channel,note,vel);
	}
	else
#endif
	{
		for (i=0; i<card_info.nr_voices;i++)
		  if ((voices[i].note == -1) && (voices[i].channel == -1))
		    break;
		if (i != card_info.nr_voices)
		{
			voices[i].note = note;
			voices[i].channel = channel;
			if (((1<<channel) & drumflag))         /* drum note */
			{
				SEQ_SET_PATCH(synth_dev, i, note + 128);
			}
			else
			{
				SEQ_SET_PATCH(synth_dev, i, program[channel]);
			}
			SEQ_START_NOTE(synth_dev, i, note, vel);
		}
	}
}

void stop_note(int channel, int note, int vel)
{
	int i;

#ifdef WANT_AWE32
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)
	{
		SEQ_STOP_NOTE(synth_dev,channel,note,vel);
	}
	else
#endif
	{      
		for (i=0; i<card_info.nr_voices;i++)
		  if ((voices[i].note == note) && (voices[i].channel == channel))
		    break;
		if (i != card_info.nr_voices)
		{
			voices[i].note = -1;
			voices[i].channel = -1;
			SEQ_STOP_NOTE(synth_dev,i,note,vel);
		}
	}
}

void set_control(int channel,int ctrl,int value)
{
	int i;

#ifdef WANT_AWE32
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)
	{
		SEQ_CONTROL(synth_dev,channel,ctrl,value);
	}
	else
#endif
	{
		for (i=0; i<card_info.nr_voices;i++)
		  if (voices[i].channel == channel)
		    if (ctrl == CTL_MAIN_VOLUME)
		      SEQ_MAIN_VOLUME(synth_dev,i,value);
	}
}

void set_pitchbend(int channel, int bend)
{
	int i;

#ifdef WANT_AWE32
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)
	{
		SEQ_BENDER(synth_dev,channel,bend);
	}
	else
#endif
	{
		for (i=0; i<card_info.nr_voices;i++)
		  if (voices[i].channel == channel)
		{
			/*SEQ_BENDER_RANGE(synth_dev, i, 200);*/
			SEQ_BENDER(synth_dev, i, bend);
		}    
	}
}

void set_key_pressure(int channel, int note, int vel)
{
	int i;

#ifdef WANT_AWE32
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)
	{
		AWE_KEY_PRESSURE(synth_dev,channel,note,vel);
	}
	else
#endif
	{
		for (i=0; i<card_info.nr_voices;i++)
		  if ((voices[i].note == note) && (voices[i].channel == channel))
		    SEQ_KEY_PRESSURE(synth_dev,i,note,vel);
	}
}

void set_chn_pressure(int channel, int vel)
{    
	int i;

#ifdef WANT_AWE32
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)    
	{
		AWE_CHN_PRESSURE(synth_dev,channel,vel);
	}
	else
#endif
	{
		for (i=0; i<card_info.nr_voices;i++)
		  if (voices[i].channel == channel)
		    SEQ_CHN_PRESSURE(synth_dev,i,vel);
	}    
}

void stop_all()
{
	int i;

#ifdef WANT_AWE32
	int j;
	if (card_info.synthType == SYNTH_TYPE_SAMPLE
	    && card_info.synth_subtype == SAMPLE_TYPE_AWE32)    
	{
		for (i=0; i<16;i++)
		  for (j=0;j<128;j++)
		    SEQ_STOP_NOTE(synth_dev,i,j,0);
	}
	else
#endif
#ifdef WANT_MPU401
	  if (card_info.synthType==SYNTH_TYPE_MIDI) {
	  } else
#endif
	{
		for (i=0; i<card_info.nr_voices;i++)
		  SEQ_STOP_NOTE(synth_dev,i,voices[i].note,0);
	}    
}

int get_dtime(ubyte *data, int *pos)
{
	char buf;
	int result;
	result =0;

	buf = data[*pos];
	*pos +=1;
	result = (0x7f & buf);

	if (!(buf & 0x80))
	{
		buf = data[*pos];
		*pos +=1;
		result |=(int) (((int) 0x7f & (int) buf)<<7);
		if (!(buf & 0x80))
		{
			buf = data[*pos];
			*pos +=1;
			result |=(int) (((int) 0x7f & (int) buf)<<14);
			if (!(buf & 0x80))
			{
				buf = data[*pos];
				*pos +=1;
				result |=(int) (((int) 0x7f & (int) buf)<<21);
			}
		}
	}

	return result;
}

int do_track_event(ubyte *data, int *pos)
{
	char channel;
	ubyte buf[5];

	buf[0]=data[*pos];
	*pos +=1;
	channel = buf[0] & 0xf;
#ifdef WANT_MPU401
	if (card_info.synthType==SYNTH_TYPE_MIDI) {
		switch((buf[0]&0xf0)) {
		 case 0x80:
		 case 0x90:
		 case 0xa0:
		 case 0xb0:
		 case 0xe0:
			buf[1]=data[*pos]; *pos+=1;
			buf[2]=data[*pos]; *pos+=1;
			MIDI_MESSAGE3(buf[0],buf[1],buf[2]);
			break;
		 case 0xc0:
		 case 0xd0:
			buf[1]=data[*pos]; *pos+=1;
			MIDI_MESSAGE3(buf[0],0,buf[1]);
			break;
		 case 0xf0:
			return 1;
		 default:
			return 3;
		}
		seqbuf_dump();
		return 0;
	}
#endif

	switch((buf[0] & 0xf0))
	{
	 case 0x80:
		buf[1]=data[*pos];
		*pos +=1;
		buf[2]=data[*pos];
		*pos +=1;
		stop_note((int) channel, (int) buf[1], (int) buf[2]);
		break;
	 case 0x90:
		buf[1]=data[*pos];
		*pos +=1;
		buf[2]=data[*pos];
		*pos +=1;
		if(buf[2] == 0)
		{
			stop_note((int) channel, (int) buf[1], (int) buf[2]);
		}
		else
		{
			start_note((int) channel, (int) buf[1], (int) buf[2]);
		}
		break;
	 case 0xa0:
		buf[1]=data[*pos];
		*pos +=1;
		buf[2]=data[*pos];
		*pos +=1;
		set_key_pressure((int) channel, (int) buf[1], (int) buf[2]);
		break;
	 case 0xb0:
		buf[1]=data[*pos];
		*pos +=1;
		buf[2]=data[*pos];
		*pos +=1;
		set_control((int) channel, (int) buf[1], (int) buf[2]);
		break;
	 case 0xe0:
		buf[1]=data[*pos];
		*pos +=1;
		buf[2]=data[*pos];
		*pos +=1;
		set_pitchbend((int) channel, (int) ((buf[2] << 7) + buf[1]);
		break;
	 case 0xc0:
		buf[1]=data[*pos];
		*pos +=1;
		set_program((int) channel, (int) buf[1] );
		break;
	 case 0xd0:
		buf[1]=data[*pos];
		*pos +=1;
		set_chn_pressure((int) channel, (int) buf[1]);
		break;
	 case 0xf0:
		return 1;
	 default:
		return 3;
	}
	seqbuf_dump();
	return 0;
}

void send_ipc(char *message)
{
	//printf ("sendipc %s\n", message);//##########3
	if (ipc_queue_id<0)
	{
		ipc_queue_id=msgget ((key_t) ('l'<<24) | ('d'<<16) | ('e'<<8) | 's', 
				     IPC_CREAT | 0660);
		snd= reinterpret_cast<struct msgbuf*> (new ubyte [sizeof(long) + 32]);
		snd->mType=1;
		player_thread=SDL_CreateThread(play_hmi, NULL);
//		player_pid = play_hmi();
	}    
	if (strlen(message) < 16)
	{
		sprintf(snd->mtext,"%s",message);
		msgsnd(ipc_queue_id,snd,16,0);
	}
}

void kill_ipc()
{
//	send_ipc("q");
//	kill(player_pid,SIGTERM);
	msgctl( ipc_queue_id, IPC_RMID, 0);
	delete[] snd;
	snd = NULL;
	ipc_queue_id = -1;
//	player_pid = 0;
}

int do_ipc(int qid, struct msgbuf *buf, int flags)
{
	int ipc_read;
	CFile cf;
	int l=0;

	ipc_read = msgrcv(qid,buf,16,0,flags | MSG_NOERROR);

	switch (ipc_read)
	{
	 case -1:
		if (errno == ENOMSG)
		  break;
		perror("IPC trouble");
		break;
	 case 0:
		break;
	 default:
		//printf ("do_ipc %s\n", buf->mtext);//##########3
		switch (buf->mtext[0])
		{
		 case 'v':
			volume=(double) ((double) buf->mtext[0]/127.0);
			break;
		 case 'p':
			if (cf.Open(&cf, (buf->mtext+1), gameFolders.szDataDir,"rb", 0)) {
				l = cf.Length(&cf);
				data=realloc(data,(size_t) l);
				cf.Read(data, l, 1);
				cf.Close ();
				//printf ("good. fpr=%p l=%i data=%p\n", fptr, l, data);//##########3
			}
			stop = 0;
			break;
		 case 's':
			stop = 2;
			break;
		 case 'q':
//			SDL_KillThread(player_thread);
			break;  
		}
	}

	return ipc_read;
}

void play_hmi (void * arg)
{
	int i;
	int pos = 0x308;
	int n_chunks = 0;
	int low_dtime;
	int low_chunk;
	int csec;
//	pid_t loc_pid;
	int qid;
	int ipc_read = 0;
	int k=0;

	struct msgbuf *rcv;

	Track_info *t_info;

	//printf ("play_hmi\n");//#########

	stop = 0;
	ipc_read=0;
//	loc_pid=fork();
    
/*	switch (loc_pid)
	{
	 case 0:
		break;
	 case -1:
		return -1;
	 default:
		atexit(kill_ipc);
		return loc_pid;
	}*/

//	signal(SIGTERM, my_quit);
	rcv = reinterpret_cast<struct msgbuf*> (new ubyte [sizeof(long) + 16]);

	rcv->mType=1;
	rcv->mtext[0]='0';

	sleep(2);

	qid=msgget ((key_t) ('l'<<24) | ('d'<<16) | ('e'<<8) | 's', 0660);
	if(qid == -1)
	{
		return;
	}

	do
	{
		ipc_read=do_ipc(qid,rcv,0);
	}
	while(rcv->mtext[0] != 'p');

	stop=0;
	rcv->mtext[0] = '0';

	seq_init();

	n_chunks=data[0x30];

	t_info = new Track_info [n_chunks];

	while(1)
	{
	
		for(i=0;i<n_chunks;i++)
		{
			t_info[i].info.position.vPosition = pos + 12;
			t_info[i].status = PLAYING;
			t_info[i].time = get_dtime(data,&t_info[i].info.position.vPosition);
			pos += (( (0xff & data[pos + 5]) << 8 ) + (0xff & data[pos + 4]);
		}
	
		SEQ_START_TIMER();
		do
		{
			low_chunk = -1;
			k++;
			i=0;
			do
			{
				if (t_info[i].status == PLAYING)
				  low_chunk = i;
				i++;
			}
			while((low_chunk <=0) && (i<n_chunks);
		
			if (low_chunk == -1)
			  break;
		
			low_dtime = t_info[low_chunk].time;
		
			for(i=1;i<n_chunks;i++)
			{
				if ((t_info[i].time < low_dtime) && 
				    (t_info[i].status == PLAYING))
				{
					low_dtime = t_info[i].time;
					low_chunk = i;
				}
			}
		
			//if (low_dtime < 0)
			  //printf("Serious warning: dTime negative!!!!!!\n");
		
			csec = 0.86 * low_dtime;
		
			//flush sequencer buffer after 20 events
			if (k == 20)
			{
				ioctl(seqfd, SNDCTL_SEQ_SYNC);
				k = 0;
			}
		
			SEQ_WAIT_TIME(csec);
		
			t_info[low_chunk].status = do_track_event(data,&t_info[low_chunk].info.position.vPosition);
		
			if (t_info[low_chunk].status == 3)
			{
				//printf("Error playing data in chunk %d\n",low_chunk);
				t_info[low_chunk].status = STOPPED;
			}
		
			if (t_info[low_chunk].status == PLAYING)
			  t_info[low_chunk].time += get_dtime(data,&t_info[low_chunk].info.position.vPosition);
		
			//Check if the song has reached the end
			stop = t_info[0].status;
			for(i=1;i<n_chunks;i++)
			  stop &= t_info[i].status;
		
			if((do_ipc(qid,rcv,IPC_NOWAIT) > 0) && (rcv->mtext[0]=='p'))
			{
				n_chunks=data[0x30];
				t_info = realloc(t_info,sizeof(Track_info)*n_chunks);
				stop = 1;
				rcv->mtext[0] = '0';  
				stop_all();
			}
		}
		while(!stop);
		SEQ_STOP_TIMER();
		if( stop == 2)
		{
			stop_all();	    
			do
			{
				ipc_read=do_ipc(qid,rcv,0);
			}
			while(rcv->mtext[0] != 'p');
			rcv->mtext[0] = '0';
			n_chunks=data[0x30];
			t_info = realloc(t_info,sizeof(Track_info)*n_chunks);
			stop = 0;
		}
		pos=0x308;
	}
	delete[] data;
	delete[] t_info;
	delete[] rcv;
	data = NULL;
	t_info = NULL;
	rcv = NULL;

}

int DigiPlayMidiSong( char * filename, char * melodic_bank, char * drum_bank, int loop ) {
        char buf[128];
    
        sprintf(buf,"p%s",filename);
        send_ipc(buf);
        return 0;
}
void DigiSetMidiVolume( int mvolume ) { 
        char buf[128];
    
        sprintf(buf,"v%i",mvolume);
        send_ipc(buf);
}

#endif //!USE_SDL_MIXER

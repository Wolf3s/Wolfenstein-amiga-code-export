//
//      ID Engine
//      ID_SD.c - Sound Manager for Wolfenstein 3D
//      v1.2
//      By Jason Blochowiak
//

//
//      This module handles dealing with generating sound on the appropriate
//              hardware
//
//      Depends on: User Mgr (for parm checking)
//
//      Globals:
//              For User Mgr:
//                      SoundBlasterPresent - SoundBlaster card present?
//                      AdLibPresent - AdLib card present?
//                      SoundMode - What device is used for sound effects
//                              (Use SM_SetSoundMode() to set)
//                      MusicMode - What device is used for music
//                              (Use SM_SetMusicMode() to set)
//                      DigiMode - What device is used for digitized sound effects
//                              (Use SM_SetDigiDevice() to set)
//
//              For Cache Mgr:
//                      NeedsDigitized - load digitized sounds?
//                      NeedsMusic - load music?
//

#include "wl_def.h"
#undef MaxX
#undef MaxY
#define Point APoint
#define Write AWrite
#define Delay ADelay
#include <graphics/gfxbase.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>
#include <dos/dostags.h>
#include <dos/stdio.h>

#pragma hdrstop

#define ORIGSAMPLERATE 7042

/*typedef struct
{
    uint32_t startpage;
    uint32_t length;
} digiinfo;*/

//globalsoundpos channelSoundPos[MIX_CHANNELS];

//      Global variables
        boolean         AdLibPresent,
                        SoundBlasterPresent,SBProPresent,
                        SoundPositioned;
        SDMode          SoundMode;
        SMMode          MusicMode;
        SDSMode         DigiMode;
static  byte          **SoundTable;
        int             DigiMap[LASTSOUND];
//        int             DigiChannel[STARTMUSIC - STARTDIGISOUNDS];

//      Internal variables
static  boolean                 SD_Started;
static  boolean                 nextsoundpos;
static  soundnames              SoundNumber;
static  soundnames              DigiNumber;
static  word                    SoundPriority;
static  word                    DigiPriority;
static  int                     LeftPosition;
static  int                     RightPosition;

        word                    NumDigi;
//static  digiinfo               *DigiList;
static  boolean                 DigiPlaying;

//      PC Sound variables
//static  volatile byte           pcLastSample;
static  byte * volatile         pcSound;
//static  longword                pcLengthLeft;

//      AdLib variables
//static  byte * volatile         alSound;
static int8_t                  *alSound;
//static  byte                    alBlock;
static  longword                alLengthLeft;
//static  longword                alTimeCount;
//static  Instrument              alZeroInst;

//      Sequencer variables
static  volatile boolean        sqActive;
/*#ifdef __AMIGA__
static  byte                   *sqHack;
static  byte                   *sqHackPtr;
#else
static  word                   *sqHack;
static  word                   *sqHackPtr;
#endif
static  int                     sqHackLen;
static  int                     sqHackSeqLen;*/
//static  longword                sqHackTime;


enum
{
	CHANNEL_LEFT,
	CHANNEL_RIGHT,
	CHANNEL_COUNT
};
#define LEFT_MASK 0x9
#define RIGHT_MASK 0x6

typedef struct
{
	struct MsgPort *port;
	struct IOAudio *ioreq;
	BYTE device;
} audiodev_t;

static boolean audiodev_init(audiodev_t *dev, UBYTE *whichannel, ULONG chancount)
{
	if ((dev->port = CreateMsgPort()))
	{
		if ((dev->ioreq = (struct IOAudio *)CreateIORequest(dev->port, sizeof(struct IOAudio))))
		{
			struct IOAudio *audioReq = dev->ioreq;
			audioReq->ioa_Request.io_Message.mn_Node.ln_Pri = 127; // no stealing
			audioReq->ioa_Request.io_Command = ADCMD_ALLOCATE;
			audioReq->ioa_Request.io_Flags = ADIOF_NOWAIT;
			audioReq->ioa_AllocKey = 0;
			audioReq->ioa_Data = whichannel;
			audioReq->ioa_Length = chancount;
			if (!(dev->device = OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)audioReq, 0)))
			{
				return true;
			}
		}
	}
	return false;
}

static void audiodev_close(audiodev_t *dev)
{
	if (!dev->device)
	{
		struct IOAudio *audioReq = dev->ioreq;
		/*
		if (audioReq->ioa_Request.io_Command != ADCMD_ALLOCATE && !CheckIO((struct IORequest *)audioReq))
		{
			AbortIO((struct IORequest *)audioReq);
			WaitIO((struct IORequest *)audioReq);
		}
		*/
		audioReq->ioa_Request.io_Command = CMD_RESET;
		audioReq->ioa_Request.io_Flags = 0;
		DoIO((struct IORequest *)audioReq);
		CloseDevice((struct IORequest *)audioReq);
		dev->device = -1;
	}

	if (dev->ioreq)
	{
		DeleteIORequest((struct IORequest *)dev->ioreq);
		dev->ioreq = NULL;
	}

	if (dev->port)
	{
		DeleteMsgPort(dev->port);
		dev->port = NULL;
	}
}

static audiodev_t digidev;
//static struct MsgPort *audioPort = NULL;
//static struct IOAudio *audioReq = NULL;
static struct IOAudio digiChannelReq[CHANNEL_COUNT];
//static BYTE audioDevice = -1;
static LONG audioClock;
static UWORD leftvol = 64;
static UWORD rightvol = 64;

void channel_init(struct IOAudio *srcio, struct IOAudio *destio, ULONG chanmask)
{
	CopyMem(srcio, destio, sizeof(struct IOAudio));
	destio->ioa_Request.io_Unit = (struct Unit *)((ULONG)destio->ioa_Request.io_Unit & chanmask);
	destio->ioa_Request.io_Message.mn_ReplyPort = CreateMsgPort();
	destio->ioa_Request.io_Message.mn_Node.ln_Type = 0;
	destio->ioa_Cycles = 1;
	destio->ioa_Request.io_Command = CMD_WRITE;
	destio->ioa_Request.io_Flags = ADIOF_PERVOL /*| IOF_QUICK*/;
	//printf("%s channels %lu msgport %p\n", __FUNCTION__, (ULONG)destio->ioa_Request.io_Unit, destio->ioa_Request.io_Message.mn_ReplyPort);
}

void channel_close(struct IOAudio *destio)
{
	destio->ioa_Request.io_Command = CMD_RESET;
	destio->ioa_Request.io_Flags = 0;
	DoIO((struct IORequest *)destio);
	if (destio->ioa_Request.io_Message.mn_ReplyPort)
	{
		// TODO reply to the message here?
		DeleteMsgPort(destio->ioa_Request.io_Message.mn_ReplyPort);
		destio->ioa_Request.io_Message.mn_ReplyPort = NULL;
	}
}

int sound_init(void)
{
	UBYTE whichannel[] = {3, 5, 10, 12};

	if (audiodev_init(&digidev, whichannel, sizeof(whichannel)))
	{
		channel_init(digidev.ioreq, &digiChannelReq[CHANNEL_LEFT], LEFT_MASK); // left
		channel_init(digidev.ioreq, &digiChannelReq[CHANNEL_RIGHT], RIGHT_MASK); // right
		return 1;
	}

	return 0;
}

void sound_close(void)
{
	channel_close(&digiChannelReq[CHANNEL_LEFT]);
	channel_close(&digiChannelReq[CHANNEL_RIGHT]);
	audiodev_close(&digidev);
}

static void channel_setup(struct IOAudio *audioio, UBYTE *data, ULONG length, UWORD period, UWORD volume)
{
	audioio->ioa_Data = data;
	audioio->ioa_Length = length;
	audioio->ioa_Period = period;
	audioio->ioa_Volume = volume;
}

/*static inline void channel_play(struct IOAudio *audioio)
{
	if (!audioio->ioa_Request.io_Unit || !audioio->ioa_Request.io_Message.mn_ReplyPort)
		return;

	BeginIO((struct IORequest *)audioio);
}*/

void sound_stop(void);

void sound_play(UBYTE *data, ULONG length, UWORD rate, UWORD leftvol, UWORD rightvol)
{
	/*if (audioDevice)
		return;*/

	sound_stop();

	struct IOAudio *audioReq = digidev.ioreq;
	audioReq->ioa_Request.io_Command = CMD_STOP;
	DoIO((struct IORequest *)audioReq);
	channel_setup(&digiChannelReq[CHANNEL_LEFT], data, length, audioClock / rate, leftvol);
	channel_setup(&digiChannelReq[CHANNEL_RIGHT], data, length, audioClock / rate, rightvol);
	BeginIO((struct IORequest *)&digiChannelReq[CHANNEL_LEFT]);
	BeginIO((struct IORequest *)&digiChannelReq[CHANNEL_RIGHT]);
	audioReq->ioa_Request.io_Command = CMD_START;
	DoIO((struct IORequest *)audioReq);
}

static void channel_volume(struct IOAudio *audioio, UWORD volume)
{
	audioio->ioa_Request.io_Command = ADCMD_PERVOL;
	audioio->ioa_Volume = volume;
	DoIO((struct IORequest *)audioio);
}

void sound_setvolume(UWORD leftvol, UWORD rightvol)
{
	/*if (audioDevice)
		return;*/

	struct IOAudio *audioReq = digidev.ioreq;
	struct Unit *oldunit = audioReq->ioa_Request.io_Unit;
	audioReq->ioa_Request.io_Unit = (struct Unit *)((ULONG)oldunit & LEFT_MASK); // left
	audioReq->ioa_Period = digiChannelReq[CHANNEL_LEFT].ioa_Period; // TODO init this
	channel_volume(audioReq, leftvol);
	audioReq->ioa_Request.io_Unit = (struct Unit *)((ULONG)oldunit & RIGHT_MASK); // right
	audioReq->ioa_Period = digiChannelReq[CHANNEL_RIGHT].ioa_Period; // TODO init this
	channel_volume(audioReq, rightvol);
	audioReq->ioa_Request.io_Unit = oldunit;
}

static BOOL channel_playing(struct IOAudio *audioio)
{
	if (audioio->ioa_Request.io_Command != CMD_WRITE)
		return 0;

	if (CheckIO((struct IORequest *)audioio))
	{
		WaitIO((struct IORequest *)audioio);
		return 0;
	}

	return 1;
}

void sound_stop(void)
{
	/*if (audioDevice)
		return;*/

	struct IOAudio *audioReq = digidev.ioreq;
	audioReq->ioa_Request.io_Command = CMD_FLUSH;
	DoIO((struct IORequest *)audioReq);
}

int sound_isplaying(void)
{
	BOOL leftPlaying = channel_playing(&digiChannelReq[CHANNEL_LEFT]);
	BOOL rightPlaying = channel_playing(&digiChannelReq[CHANNEL_RIGHT]);
	return leftPlaying || rightPlaying;
}


static void SDL_SoundFinished(void)
{
	SoundNumber   = (soundnames)0;
	SoundPriority = 0;
}

static void SDL_DigitizedDone(void)
{
	DigiNumber = DigiPriority = 0;
	DigiPlaying = SoundPositioned = false;
}

void
SD_StopDigitized(void)
{
    DigiPlaying = false;
    DigiNumber = (soundnames) 0;
    DigiPriority = 0;
    SoundPositioned = false;
    if ((DigiMode == sds_PC) && (SoundMode == sdm_PC))
        SDL_SoundFinished();

    switch (DigiMode)
    {
        case sds_PC:
//            SDL_PCStopSampleInIRQ();
            break;
        case sds_SoundBlaster:
//            SDL_SBStopSampleInIRQ();
			sound_isplaying(); // reply to pending messages
			sound_stop();
            break;
    }
}

void SD_SetPosition(int channel, int leftpos, int rightpos)
{
    /*if((leftpos < 0) || (leftpos > 15) || (rightpos < 0) || (rightpos > 15)
            || ((leftpos == 15) && (rightpos == 15)))
        Quit("SD_SetPosition: Illegal position");*/

    switch (DigiMode)
    {
        case sds_SoundBlaster:
//            SDL_PositionSBP(leftpos,rightpos);
            // TODO Mix_SetPanning(channel, ((15 - leftpos) << 4) + 15, ((15 - rightpos) << 4) + 15);
			leftvol = ((15 - leftpos) << 2) + 4;
			rightvol = ((15 - rightpos) << 2) + 4;
			//printf("leftpos %d leftvol %u rightpos %d rightvol %u\n", leftpos, leftvol, rightpos, rightvol);
			if (sound_isplaying())
			{
				sound_setvolume(leftvol, rightvol);
			}
            break;
    }
}

void SD_PrepareSound(int which)
{
	// the page manager prepares everything for us
}

int SD_PlayDigitized(word which,int leftpos,int rightpos)
{
    if (!DigiMode)
        return 0;

    if (which >= NumDigi)
        Quit("SD_PlayDigitized: bad sound number %i", which);

    SD_SetPosition(0, leftpos,rightpos);

    DigiPlaying = true;

    word *soundInfoPage = (word *) (void *) PM_GetPage(ChunksInFile-1);
	word page = soundInfoPage[(which * 2) + 0];
	word length = soundInfoPage[(which * 2) + 1];
	uint8_t *data = PM_GetSound(page);
	//printf("%s which %d page %u(%u) length %u\n", __FUNCTION__, which, page, PMSoundStart+page, length);
	sound_play(data, length, ORIGSAMPLERATE, leftvol, rightvol);

    return 0;
}

/*
void SD_ChannelFinished(int channel)
{
    channelSoundPos[channel].valid = 0;
}
*/
void
SD_SetDigiDevice(SDSMode mode)
{
    boolean devicenotpresent;

    if (mode == DigiMode)
        return;

    SD_StopDigitized();

    devicenotpresent = false;
    switch (mode)
    {
        case sds_SoundBlaster:
            if (!SoundBlasterPresent)
                devicenotpresent = true;
            break;
    }

    if (!devicenotpresent)
    {
        DigiMode = mode;
		// free the adlib sounds with digi replacements
		// TODO this can undo the work of the precache if toggled ingame
		/*
		if (mode == sds_SoundBlaster)
		{
			for (int i = 0; i < LASTSOUND; i++)
			{
				//printf("%s DigiMap[%d] = %d\n", __FUNCTION__, i, DigiMap[i]);
				if (DigiMap[i] != -1)
					BEL_ST_FreeSound(&g_adlibSounds[i]);
			}
		}
		*/
    }
}

void
SDL_SetupDigi(void)
{
    //word *soundInfoPage = (word *) (void *) PM_GetPage(ChunksInFile-1);
    NumDigi = (word) PM_GetPageSize(ChunksInFile - 1) / 4;

    int i;
    for(i = 0; i < LASTSOUND; i++)
    {
        DigiMap[i] = -1;
        //DigiChannel[i] = -1;
    }
}

//      AdLib Code

typedef struct
{
	int8_t *data;
	int32_t length;
	int16_t rate;
} sample_t;

static sample_t g_adlibSounds[LASTSOUND+1]; // LASTSOUND is reserved for the music

typedef struct
{
	uint32_t magic; /* magic number */
	uint32_t hdr_size; /* size of this header */ 
	uint32_t data_size; /* length of data (optional) */ 
	uint32_t encoding; /* data encoding format */
	uint32_t sample_rate; /* samples per second */
	uint32_t channels; /* number of interleaved channels */
} Audio_filehdr;

static void read_audio_header(BPTR fp, Audio_filehdr *hdr, LONG length)
{
	Read(fp, hdr, length);
	if (hdr->data_size == -1)
	{
		Seek(fp, 0, OFFSET_END);
		int32_t nbyte = Seek(fp, hdr->hdr_size, OFFSET_BEGINNING);
		nbyte -= hdr->hdr_size;
		hdr->data_size = nbyte;
		//printf("%s oldpos %d hdr_size %d\n", __FUNCTION__, oldpos, hdr->hdr_size);
	}
}

static void BEL_ST_LoadSoundFile(char *filename, sample_t *digi)
{
	BPTR fp;

	if ((fp = Open((STRPTR)filename, MODE_OLDFILE)))
	{
		Audio_filehdr header;
		read_audio_header(fp, &header, sizeof(header));
		digi->data = malloc(header.data_size);
		//CHECKMALLOCRESULT(digi->data);
		/*
		MM_BombOnError (false);
		MM_GetPtr((memptr)&digi->data, nbyte);
		MM_BombOnError (true);
		if (mmerror)
			mmerror = false;
		else
			*/
		if (digi->data)
		{
			Read(fp, digi->data, header.data_size);
			digi->length = header.data_size;
			digi->rate = header.sample_rate;
			//MM_SetPurge((memptr)&digi->data, 3);
		}
		Close(fp);
	}
}

static void BEL_ST_LoadSound(int sound)
{
	char filename[32];
	snprintf(filename, sizeof(filename), "adlib/sound%02d.au", sound);
	//printf("%s loading %s...\n", __FUNCTION__, filename);
	BEL_ST_LoadSoundFile(filename, &g_adlibSounds[sound]);
}

static void BEL_ST_FreeSound(sample_t *digi)
{
	if (!digi->data)
		return;

	//MM_FreePtr((memptr)&digi->data);
	free(digi->data);
	digi->data = NULL;
}

static BPTR alMusicHandle = 0;
static LONG alMusicStart = 0;
static UWORD alMusicRate = 0;

static void BEL_ST_FreeMusic(void)
{
	//BEL_ST_FreeSound(&g_adlibSounds[LASTSOUND]);
	if (!alMusicHandle)
		return;

	Close(alMusicHandle);
	alMusicHandle = 0;
}

static void BEL_ST_LoadMusic(int music)
{
	char filename[32];
	snprintf(filename, sizeof(filename), "adlib/music%02d.au", music);
	//printf("%s loading %s...\n", __FUNCTION__, filename);
	//BEL_ST_LoadSoundFile(filename, &g_adlibSounds[LASTSOUND]);
	/*
	if (!g_adlibSounds[LASTSOUND].data)
	{
		return;
	}
	MM_SetLock((memptr)&g_adlibSounds[LASTSOUND].data, true); // don't let the music to be purged!
	BEL_ST_LoadSample(g_adlibSounds[LASTSOUND].data, g_adlibSounds[LASTSOUND].length, SOUND_MUSIC);
	*/
	alMusicHandle = Open((STRPTR)filename, MODE_OLDFILE);
	if (!alMusicHandle) {
		//printf("%s can't open %s\n", __FUNCTION__, filename);
		return;
	}
	Audio_filehdr header;
	read_audio_header(alMusicHandle, &header, sizeof(header));
	alMusicStart = header.hdr_size;
	alMusicRate = header.sample_rate;
	//SetVBuf(fp, NULL, BUF_FULL, 1024*10);
	//printf("%s music pos %ld\n", __FUNCTION__, Seek(alMusicHandle, 0, OFFSET_CURRENT));
}

static int BEL_ST_TellMusicPos(void)
{
	if (!alMusicHandle)
		return 0;

	return Seek(alMusicHandle, 0, OFFSET_CURRENT) - alMusicStart;
}

static void BEL_ST_SetMusicPos(int offset)
{
	if (!alMusicHandle)
		return;
	//printf("%s(%d) alMusicStart %ld\n", __FUNCTION__, offset, alMusicStart);
	Seek(alMusicHandle, offset + alMusicStart, OFFSET_BEGINNING);
	//printf("%s music pos %ld\n", __FUNCTION__, Seek(alMusicHandle, 0, OFFSET_CURRENT));
}


static struct Task *alThread;
static struct Task *parentThread;
#define BUFFER_SIZE (1024)
static __chip BYTE alMixBuffer[2][BUFFER_SIZE]; // double buffered
#define CLIP_SAMPLES
#ifdef CLIP_SAMPLES
static BYTE MV_HarshClipTable[512];
#endif

#define MUSBUFFER_SIZE (BUFFER_SIZE*10)
static BYTE alMusicBuffer[MUSBUFFER_SIZE];
static WORD alMusicCurrent = MUSBUFFER_SIZE;

static void SDL_FillMusicBuffer(BYTE *buffer)
{
	if (alMusicCurrent >= sizeof(alMusicBuffer))
	{
		BYTE *dest = alMusicBuffer;
		//LONG count = FRead(alMusicHandle, dest, 1, sizeof(alMusicBuffer));
		LONG count = Read(alMusicHandle, dest, sizeof(alMusicBuffer));
		if (count < 0)
			count = 0; // treat the EOF as 0 bytes read
		dest += count;
		count = sizeof(alMusicBuffer) - count;
		if (count > 0)
		{
			// reached the end of the file, rewind and read the remaining
			Seek(alMusicHandle, alMusicStart, OFFSET_BEGINNING);
			//FRead(alMusicHandle, dest, 1, count);
			Read(alMusicHandle, dest, count);
		}
		alMusicCurrent = 0;
	}
	BYTE *music = &alMusicBuffer[alMusicCurrent];
	memcpy(buffer, music, BUFFER_SIZE);
	alMusicCurrent += BUFFER_SIZE;
}

static void SDL_MixBuffer(BYTE *buffer/*, word size*/)
{
	// music
	if (sqActive)
	{
		/*
		word length = size;
		while (length > 0)
		{
			if (length > sqHackLen)
				length = sqHackLen;

			memcpy(buffer, sqHackPtr, length);
			sqHackLen -= length;
			sqHackPtr += length;
			length = size - length;

			if (!sqHackLen)
			{
				sqHackPtr = sqHack;
				sqHackLen = sqHackSeqLen;
			}
		}
		*/
		SDL_FillMusicBuffer(buffer);
		/*
		int8_t *music = alMusicBuffer;
		memcpy(buffer, music, BUFFER_SIZE);
		*/
	}
	else
	{
		memset(buffer, 0, BUFFER_SIZE);
	}
	// sound effect
	if (alSound)
	{
		word length = BUFFER_SIZE;
		if (length > alLengthLeft)
			length = alLengthLeft;

		alLengthLeft -= length;
		int8_t *src = alSound;
		//Forbid();
		if (!alLengthLeft)
		{
			alSound = NULL;
			SoundPriority = 0;
			// SoundNumber = (soundnames) 0; TODO
		}
		else
		{
			alSound += length;
		}
		//Permit();
		int8_t *dest = buffer;
		int8_t *clipTable = MV_HarshClipTable;
		do
		{
			int16_t sample0;
#ifdef CLIP_SAMPLES
			sample0 = (*src++) + *dest;
			*dest++ = clipTable[sample0 + 256];
#else
			sample0 = (int16_t)(*alSound++) + (int16_t)*dest;
			*dest++ = (int8_t)sample0;
#endif
		} while (--length);
	}
}

static void SDL_IMFMusicPlayer(void)
{
	audiodev_t aldev;
	UBYTE whichannel[] = {3, 5, 10, 12};
	struct IOAudio alChannelReq[2][CHANNEL_COUNT]; // double buffered
	UWORD currentRate = 8000/*11025*/;
	WORD currentBuffer;

	if (audiodev_init(&aldev, whichannel, sizeof(whichannel)))
	{
		struct IOAudio *audioReq = aldev.ioreq;
		channel_init(audioReq, &alChannelReq[0][CHANNEL_LEFT], LEFT_MASK);
		channel_init(audioReq, &alChannelReq[1][CHANNEL_LEFT], LEFT_MASK);
		channel_init(audioReq, &alChannelReq[0][CHANNEL_RIGHT], RIGHT_MASK);
		channel_init(audioReq, &alChannelReq[1][CHANNEL_RIGHT], RIGHT_MASK);

		if (alMusicRate && alMusicRate != currentRate)
			alMusicRate = currentRate;
		UWORD period = audioClock / currentRate;

		// mix and queue the first two buffers
		audioReq->ioa_Request.io_Command = CMD_STOP;
		DoIO((struct IORequest *)audioReq);
		for (int i = 0; i < 2; i++)
		{
			BYTE *buffer = alMixBuffer[i];
			struct IOAudio *alLeftReq = &alChannelReq[i][CHANNEL_LEFT];
			struct IOAudio *alRightReq = &alChannelReq[i][CHANNEL_RIGHT];
			channel_setup(alLeftReq, (UBYTE *)buffer, BUFFER_SIZE, period, 64);
			channel_setup(alRightReq, (UBYTE *)buffer, BUFFER_SIZE, period, 64);
			SDL_MixBuffer(buffer);
			alRightReq->ioa_Request.io_Flags |= IOF_QUICK;
			BeginIO((struct IORequest *)alLeftReq);
			BeginIO((struct IORequest *)alRightReq);
		}
		audioReq->ioa_Request.io_Command = CMD_START;
		DoIO((struct IORequest *)audioReq);

		currentBuffer = 0;
		struct MsgPort *port = alChannelReq[currentBuffer][CHANNEL_LEFT].ioa_Request.io_Message.mn_ReplyPort;
		ULONG portmask = 1 << port->mp_SigBit;

		Signal(parentThread, SIGBREAKF_CTRL_E); // init success
		do {
			ULONG signals = Wait(SIGBREAKF_CTRL_C | portmask);

			if (signals & SIGBREAKF_CTRL_C) {
				break;
			}
			if (alMusicRate && currentRate != alMusicRate) {
				// sample rate change
				period = audioClock / alMusicRate;
				for (int i = 0; i < 2; i++)
				{
					struct IOAudio *alLeftReq = &alChannelReq[i][CHANNEL_LEFT];
					struct IOAudio *alRightReq = &alChannelReq[i][CHANNEL_RIGHT];
					alLeftReq->ioa_Period = period;
					alRightReq->ioa_Period = period;
				}
				currentRate = alMusicRate;
			}

			if (signals & portmask) {
				GetMsg(port);

				// reuse the finished requests
				BYTE *buffer = alMixBuffer[currentBuffer];
				struct IOAudio *alLeftReq = &alChannelReq[currentBuffer][CHANNEL_LEFT];
				struct IOAudio *alRightReq = &alChannelReq[currentBuffer][CHANNEL_RIGHT];
				//printf("left %d right %d\n", IsListEmpty(&alLeftReq->ioa_Request.io_Message.mn_ReplyPort->mp_MsgList), IsListEmpty(&alLeftReq->ioa_Request.io_Message.mn_ReplyPort->mp_MsgList));
				SDL_MixBuffer(buffer);
				alRightReq->ioa_Request.io_Flags |= IOF_QUICK;
				BeginIO((struct IORequest *)alLeftReq);
				BeginIO((struct IORequest *)alRightReq);

				// wait for the other one to finish
				currentBuffer ^= 1;
				port = alChannelReq[currentBuffer][CHANNEL_LEFT].ioa_Request.io_Message.mn_ReplyPort;
				portmask = 1 << port->mp_SigBit;
			}

		} while (TRUE);
		channel_close(&alChannelReq[0][CHANNEL_LEFT]);
		channel_close(&alChannelReq[1][CHANNEL_LEFT]);
		channel_close(&alChannelReq[0][CHANNEL_RIGHT]);
		channel_close(&alChannelReq[1][CHANNEL_RIGHT]);
		audiodev_close(&aldev);
	}

	Forbid();
	Signal(parentThread, SIGBREAKF_CTRL_F);
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_ALStopSound() - Turns off any sound effects playing through the
//              AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ALStopSound(void)
{
	//Forbid();
    alSound = NULL;
	//Permit();
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_ALPlaySound() - Plays the specified sound on the AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ALPlaySound(soundnames sound)
{
    SDL_ALStopSound();

	sample_t *digi = &g_adlibSounds[sound];
	if (!digi->data){
		//printf("Missing sound %d\n", sound);
		BEL_ST_LoadSound(sound);
		CHECKMALLOCRESULT(digi->data);
	}
	//printf("%s %d %d\n", __FUNCTION__, alMusicRate, digi->rate);
	if (!alMusicRate)
	{
		// if the music is not running use the sample rate of the first sound
		alMusicRate = digi->rate;
	}
	//Forbid();
    alLengthLeft = digi->length;
    alSound = /*(byte *)*/digi->data;
	//Permit();
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_ShutAL() - Shuts down the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutAL(void)
{
	for (int i = 0; i < LASTSOUND; i++)
	{
		BEL_ST_FreeSound(&g_adlibSounds[i]);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_CleanAL() - Totally shuts down the AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_CleanAL(void)
{
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_StartAL() - Starts up the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartAL(void)
{
   int volume;

#ifdef CLIP_SAMPLES
   for( volume = 0; volume < 128; volume++ )
      {
      MV_HarshClipTable[ volume ] = -128;
      MV_HarshClipTable[ volume + 384 ] = 127;
      }
   for( volume = 0; volume < 256; volume++ )
      {
      MV_HarshClipTable[ volume + 128 ] = volume - 0x80;
      }
#endif
}

////////////////////////////////////////////////////////////////////////////
//
//      SDL_ShutDevice() - turns off whatever device was being used for sound fx
//
////////////////////////////////////////////////////////////////////////////
static void
SDL_ShutDevice(void)
{
    switch (SoundMode)
    {
        case sdm_PC:
//            SDL_ShutPC();
            break;
        case sdm_AdLib:
            SDL_ShutAL();
            break;
    }
    SoundMode = sdm_Off;
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_CleanDevice() - totally shuts down all sound devices
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_CleanDevice(void)
{
    if ((SoundMode == sdm_AdLib) || (MusicMode == smm_AdLib))
        SDL_CleanAL();
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_StartDevice() - turns on whatever device is to be used for sound fx
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartDevice(void)
{
    switch (SoundMode)
    {
        case sdm_AdLib:
            SDL_StartAL();
            break;
    }
    SoundNumber = (soundnames) 0;
    SoundPriority = 0;
}

//      Public routines

///////////////////////////////////////////////////////////////////////////
//
//      SD_SetSoundMode() - Sets which sound hardware to use for sound effects
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetSoundMode(SDMode mode)
{
    boolean result = false;
    word    tableoffset;

    SD_StopSound();

    if ((mode == sdm_AdLib) && !AdLibPresent)
        mode = sdm_PC;

    switch (mode)
    {
        case sdm_Off:
            tableoffset = STARTADLIBSOUNDS;
            result = true;
            break;
        case sdm_PC:
            tableoffset = STARTPCSOUNDS;
            result = true;
            break;
        case sdm_AdLib:
            tableoffset = STARTADLIBSOUNDS;
            if (AdLibPresent)
                result = true;
            break;
        default:
            Quit("SD_SetSoundMode: Invalid sound mode %i", mode);
            return false;
    }
    SoundTable = &audiosegs[tableoffset];

    if (result && (mode != SoundMode))
    {
        SDL_ShutDevice();
        SoundMode = mode;
        SDL_StartDevice();
    }

    return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_SetMusicMode() - sets the device to use for background music
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetMusicMode(SMMode mode)
{
    boolean result = false;

    SD_MusicOff();

    switch (mode)
    {
        case smm_Off:
			BEL_ST_FreeMusic();
            result = true;
            break;
        case smm_AdLib:
            if (AdLibPresent)
                result = true;
            break;
    }

    if (result)
        MusicMode = mode;

//    SDL_SetTimerSpeed();

    return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_Startup() - starts up the Sound Mgr
//              Detects all additional sound hardware and installs my ISR
//
///////////////////////////////////////////////////////////////////////////
void
SD_Startup(void)
{
//    int     i;

	if (SD_Started)
		return;

	if (GfxBase->DisplayFlags & PAL)
		audioClock = 3546895;
	else
		audioClock = 3579545;

	if (sound_init())
		SoundBlasterPresent = true;

	parentThread = FindTask(NULL);
	alThread = (struct Task *)CreateNewProcTags(
		NP_Name, (Tag)"IMF Music Player",
		NP_Priority, 21,
		NP_Entry, (Tag)SDL_IMFMusicPlayer,
		//NP_ExitData, (Tag)voice,
		TAG_END);
	if (alThread)
	{
		ULONG signals = Wait(SIGBREAKF_CTRL_E|SIGBREAKF_CTRL_F);
		if (signals & SIGBREAKF_CTRL_E)
		{
			AdLibPresent = true;
		}
		else
		{
			alThread = NULL;
		}
	}

    //alTimeCount = 0;

    SD_SetSoundMode(sdm_Off);
    SD_SetMusicMode(smm_Off);

    SDL_SetupDigi();

    SD_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_Shutdown() - shuts down the Sound Mgr
//              Removes sound ISR and turns off whatever sound hardware was active
//
///////////////////////////////////////////////////////////////////////////
void
SD_Shutdown(void)
{
    if (!SD_Started)
        return;

    SD_MusicOff();
	BEL_ST_FreeMusic();
    SD_StopSound();
	sound_close();
	if (alThread) {
		Signal(alThread, SIGBREAKF_CTRL_C);
		Wait(SIGBREAKF_CTRL_F);
		alThread = NULL;
	}

    SD_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_PositionSound() - Sets up a stereo imaging location for the next
//              sound to be played. Each channel ranges from 0 to 15.
//
///////////////////////////////////////////////////////////////////////////
void
SD_PositionSound(int leftvol,int rightvol)
{
    LeftPosition = leftvol;
    RightPosition = rightvol;
    nextsoundpos = true;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_PlaySound() - plays the specified sound on the appropriate hardware
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_PlaySound(soundnames sound)
{
    boolean         ispos;
    SoundCommon     *s;
    int             lp,rp;

    lp = LeftPosition;
    rp = RightPosition;
    LeftPosition = 0;
    RightPosition = 0;

    ispos = nextsoundpos;
    nextsoundpos = false;

    if (sound == -1 || (DigiMode == sds_Off && SoundMode == sdm_Off))
        return 0;

    s = (SoundCommon *) SoundTable[sound];

    if ((SoundMode != sdm_Off) && !s)
            Quit("SD_PlaySound() - Uncached sound");

    if ((DigiMode != sds_Off) && (DigiMap[sound] != -1))
    {
        if ((DigiMode == sds_PC) && (SoundMode == sdm_PC))
        {
#ifdef NOTYET
            if (s->priority < SoundPriority)
                return 0;

            SDL_PCStopSound();

            SD_PlayDigitized(DigiMap[sound],lp,rp);
            SoundPositioned = ispos;
            SoundNumber = sound;
            SoundPriority = s->priority;
#else
            return 0;
#endif
        }
        else
        {
			if (!sound_isplaying())
				SDL_DigitizedDone();

            if (s->priority < DigiPriority)
                return(false);

            SD_PlayDigitized(DigiMap[sound], lp, rp);
            SoundPositioned = ispos;
            DigiNumber = sound;
            DigiPriority = s->priority;
            return 1;
        }

        return(true);
    }

    if (SoundMode == sdm_Off)
        return 0;

    if (!s->length)
        Quit("SD_PlaySound() - Zero length sound");
    if (s->priority < SoundPriority)
        return 0;

    switch (SoundMode)
    {
        case sdm_PC:
//            SDL_PCPlaySound((PCSound *)s);
            break;
        case sdm_AdLib:
            SDL_ALPlaySound(sound);
            break;
    }

    SoundNumber = sound;
    SoundPriority = s->priority;

    return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_SoundPlaying() - returns the sound number that's playing, or 0 if
//              no sound is playing
//
///////////////////////////////////////////////////////////////////////////
word
SD_SoundPlaying(void)
{
    boolean result = false;

    switch (SoundMode)
    {
        case sdm_PC:
            result = pcSound? true : false;
            break;
        case sdm_AdLib:
            result = alSound? true : false;
            break;
    }

    if (result)
        return(SoundNumber);
    else
        return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_StopSound() - if a sound is playing, stops it
//
///////////////////////////////////////////////////////////////////////////
void
SD_StopSound(void)
{
    if (DigiPlaying)
        SD_StopDigitized();

    switch (SoundMode)
    {
        case sdm_PC:
//            SDL_PCStopSound();
            break;
        case sdm_AdLib:
            SDL_ALStopSound();
            break;
    }

    SoundPositioned = false;

    SDL_SoundFinished();
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_WaitSoundDone() - waits until the current sound is done playing
//
///////////////////////////////////////////////////////////////////////////
void
SD_WaitSoundDone(void)
{
    while (SD_SoundPlaying())
        SDL_Delay(5);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicOn() - turns on the sequencer
//
///////////////////////////////////////////////////////////////////////////
void
SD_MusicOn(void)
{
    sqActive = true;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicOff() - turns off the sequencer and any playing notes
//      returns the last music offset for music continue
//
///////////////////////////////////////////////////////////////////////////
int
SD_MusicOff(void)
{
    //word    i;

    sqActive = false;
    switch (MusicMode)
    {
        case smm_AdLib:
            // TODO
            break;
    }

    //return (int) (sqHackPtr-sqHack);
	return BEL_ST_TellMusicPos();
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_StartMusic() - starts playing the music pointed to
//
///////////////////////////////////////////////////////////////////////////
void
SD_StartMusic(int chunk)
{
    SD_MusicOff();

    if (MusicMode == smm_AdLib)
    {
		BEL_ST_FreeMusic();
		BEL_ST_LoadMusic(chunk - STARTMUSIC);
		if (!alMusicHandle)
			return;
		alMusicCurrent = MUSBUFFER_SIZE; // reset
		SD_MusicOn();
    }
}

void
SD_ContinueMusic(int chunk, int startoffs)
{
    SD_MusicOff();

    if (MusicMode == smm_AdLib)
    {
		BEL_ST_LoadMusic(chunk - STARTMUSIC);
		if (!alMusicHandle)
			return;
		BEL_ST_SetMusicPos(startoffs);
		alMusicCurrent = MUSBUFFER_SIZE; // reset
        SD_MusicOn();
    }
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_FadeOutMusic() - starts fading out the music. Call SD_MusicPlaying()
//              to see if the fadeout is complete
//
///////////////////////////////////////////////////////////////////////////
void
SD_FadeOutMusic(void)
{
	/* Unused
    switch (MusicMode)
    {
        case smm_AdLib:
            // DEBUG - quick hack to turn the music off
            SD_MusicOff();
            break;
    }
	*/
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicPlaying() - returns true if music is currently playing, false if
//              not
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_MusicPlaying(void)
{
    boolean result;

    switch (MusicMode)
    {
        case smm_AdLib:
            result = sqActive;
            break;
        default:
            result = false;
            break;
    }

    return(result);
}

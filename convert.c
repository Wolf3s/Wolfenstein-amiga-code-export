#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
//#include <unistd.h>

#include "wl_def.h"

#define CONVERT_MUSIC

int     param_samplerate = 11025;
boolean  param_ignorenumchunks = false;
pictabletype	*pictable;
extern boolean			sqPlayedOnce;
extern  int                     sqHackLen;
extern  int                     sqHackSeqLen;

void SDL_Delay(uint32_t ms)
{
	//usleep(ms*1000);
}

void Quit (const char *errorStr, ...)
{
    char error[256];
    if(errorStr != NULL)
    {
        va_list vlist;
        va_start(vlist, errorStr);
        vsprintf(error, errorStr, vlist);
        va_end(vlist);
    }
    else error[0] = 0;

    //ShutdownId ();

    if (*error)
    {
        puts(error);
        exit(1);
    }

    exit(0);
}

void
CheckForEpisodes (void)
{
#ifdef SPEAR
    strcpy (extension, "sod");
#else
	// TODO other versions
	strcpy (extension, "wl6");
#endif
    strcpy (audioext, extension);
}

void ShutdownId (void)
{
	//printf("Conversion shutdown...\n");
	SD_Shutdown ();
	CA_Shutdown ();
	//CloseAudioFile();
}

void VL_MemToScreen (byte *source, int width, int height, int x, int y) {}

extern void CAL_SetupAudioFile(void);
extern void SDL_IMFMusicPlayer(void *udata, Uint8 *stream, int len);

#define SWAP32BE(x) (x)
#define NUM_OF_BYTES_FOR_SOUND_CALLBACK_WITH_DISABLED_SUBSYSTEM 1000

typedef struct
{
	uint32_t magic; /* magic number */
	uint32_t hdr_size; /* size of this header */ 
	uint32_t data_size; /* length of data (optional) */ 
	uint32_t encoding; /* data encoding format */
	uint32_t sample_rate; /* samples per second */
	uint32_t channels; /* number of interleaved channels */
} Audio_filehdr;

#define AUDIO_FILE_MAGIC ((uint32_t)0x2e736e64) /* Define the magic number */  
#define AUDIO_FILE_ENCODING_LINEAR_8 (2) /* 8-bit linear PCM */

uint8_t buff[NUM_OF_BYTES_FOR_SOUND_CALLBACK_WITH_DISABLED_SUBSYSTEM];

static void write_header(FILE *fd)
{
	Audio_filehdr header;
	uint32_t size;
	//size = ftell(fd);
	size = 0xffffffff;
	header.magic = SWAP32BE(AUDIO_FILE_MAGIC);
	header.hdr_size = SWAP32BE(sizeof(header));
	header.data_size = SWAP32BE(size);
	header.encoding = SWAP32BE(AUDIO_FILE_ENCODING_LINEAR_8);
	header.sample_rate = SWAP32BE(param_samplerate);
	header.channels = SWAP32BE((uint32_t)1);
	//fseek(fd, 0, SEEK_SET);
	fwrite(&header, sizeof(header), 1, fd);
}

int main(int argc, char *argv[])
{
	char filename[256];
	FILE *fd;

	if (argc >= 2)
	{
		param_samplerate = atoi(argv[1]);
	}
	printf("Conversion init, sample rate: %d Hz\n", param_samplerate);

	puts("Conversion startup...");
	CheckForEpisodes();
	SD_Startup();
	CAL_SetupAudioFile();
	atexit(ShutdownId);

	puts("Loading sounds...");
	SD_SetSoundMode(sdm_AdLib);
	CA_LoadAllSounds();

	mkdir("adlib", 0755); 

	// play them
	for (int i = 0; i < LASTSOUND; i++)
	{
		printf("Converting sound %d/%d", i, LASTSOUND-1);

		snprintf(filename, sizeof(filename), "adlib/sound%02d.au", i);
		fd = fopen(filename, "r");
		if (fd)
		{
			// already exists, skip this one
			fclose(fd);
			puts(" - already exists");
			continue;
		}

		SD_SetSoundMode(sdm_AdLib);
		SD_PlaySound(i);

		fd = fopen(filename, "w");
		write_header(fd);

		// render the sound data
		do
		{
			SDL_IMFMusicPlayer(NULL, buff, sizeof(buff));
			putchar('.'); fflush(stdout);
			fwrite(buff, sizeof(buff), 1, fd);
		}
		while (SD_SoundPlaying());
		// TODO silence detection
		//SDL_IMFMusicPlayer(NULL, buff, sizeof(buff));

		putchar('\n');

		fclose(fd);

		// reset the emulator
		SD_SetSoundMode(sdm_Off);
	}

#ifdef CONVERT_MUSIC
	const char indicators[] = {'-', '\\', '|', '/'};
	const char convtext[] = "Converting music";
	SD_SetMusicMode(smm_AdLib);
	for (int i = 0; i < LASTMUSIC; i++)
	{
		//printf("Converting music %d/%d", i, LASTMUSIC-1);
		printf("%s %d/%d", convtext, i, LASTMUSIC-1);

		snprintf(filename, sizeof(filename), "adlib/music%02d.au", i);
		fd = fopen(filename, "r");
		if (fd)
		{
			// already exists, skip this one
			fclose(fd);
			puts(" - already exists");
			continue;
		}

		SD_SetSoundMode(sdm_AdLib);
		//CA_CacheAudioChunk(STARTMUSIC+i);
		SD_StartMusic(STARTMUSIC+i);

		fd = fopen(filename, "w");
		write_header(fd);

		//SD_MusicOn();
		// render the sound data
		time_t tstart = time(NULL);
		short count = 0;
		do
		{
			int percent = (sqHackSeqLen - sqHackLen) * 100 / sqHackSeqLen;
			SDL_IMFMusicPlayer(NULL, buff, sizeof(buff));
			//printf("\nmusic playing %d playedonce %d", SD_MusicPlaying(), sqPlayedOnce);
			//putchar('.'); fflush(stdout);
			printf("\r%s %d/%d %3d%% [%c]", convtext, i, LASTMUSIC-1, percent, indicators[(count++) % sizeof(indicators)]);fflush(stdout);
			fwrite(buff, sizeof(buff), 1, fd);
		}
		while (/*SD_MusicPlaying() &&*/ !sqPlayedOnce);
		time_t tend = time(NULL);
		SD_MusicOff();
		UNCACHEAUDIOCHUNK(STARTMUSIC + i);

		//putchar('\n');
		printf(" %ld secs\n", tend-tstart);

		fclose(fd);

		// reset the emulator
		SD_SetSoundMode(sdm_Off);
	}
#endif

	printf("Conversion finished!\n");

	return 0;
}

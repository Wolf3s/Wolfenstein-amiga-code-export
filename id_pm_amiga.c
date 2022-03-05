#include "wl_def.h"
#undef PM_GetPage
#define GRAPHICS_DISPLAYINFO_H
#include <proto/exec.h>

static boolean PMStarted;

static FILE * PageFile = 0;
int ChunksInFile, PMSpriteStart, PMSoundStart;

typedef	struct {
	uint32_t offset;	/* Offset of chunk into file */
	word length;	/* Length of the chunk */
	//memptr addr;
} PageListStruct;


PageListStruct *PMInfos;
uint8_t **PMPages;

//bool PMSoundInfoPagePadded = false;

static void PML_ReadFromFile(byte *buf, long offset, word length)
{
	if (!buf)
		Quit("PML_ReadFromFile: Null pointer");
	if (!offset)
		Quit("PML_ReadFromFile: Zero offset");
	if (fseek(PageFile, offset, SEEK_SET) != 0)
		Quit("PML_ReadFromFile: Seek failed");
	if (fread(buf, 1, length, PageFile) != length)
		Quit("PML_ReadFromFile: Read failed");
}

static void PML_OpenPageFile()
{
	int i;
	PageListStruct *page;
	char fname[13] = "vswap.";

	strcat(fname, extension);

	PageFile = fopen(fname,"rb");
	if (PageFile == 0)
		Quit("PML_OpenPageFile: Unable to open page file");

	/* Read in header variables */
	ChunksInFile = 0;
	fread(&ChunksInFile, sizeof(word), 1, PageFile);
	PMSpriteStart = 0;
	fread(&PMSpriteStart, sizeof(word), 1, PageFile);
	PMSoundStart = 0;
	fread(&PMSoundStart, sizeof(word), 1, PageFile);
#ifdef __AMIGA__
	// this assumes sizeof(int) == 4
	ChunksInFile = SWAP32LE(ChunksInFile);
	PMSpriteStart = SWAP32LE(PMSpriteStart);
	PMSoundStart = SWAP32LE(PMSoundStart);
	//printf("%s %d %d %d\n", __FUNCTION__, ChunksInFile, PMSpriteStart, PMSoundStart);
#endif

    PMPages = (uint8_t **) calloc((ChunksInFile + 1) * sizeof(uint8_t *), 1);
    CHECKMALLOCRESULT(PMPages);

	/* Allocate and clear the page list */
	PMInfos = calloc(sizeof(PageListStruct) * ChunksInFile, 1);
	CHECKMALLOCRESULT(PMInfos);

	/* Read in the chunk offsets */
	for (i = 0, page = PMInfos; i < ChunksInFile; i++, page++) {
		//page->offset = ReadInt32(PageFile);
		fread(&page->offset, sizeof(uint32_t), 1, PageFile);
#ifdef __AMIGA__
		page->offset = SWAP32LE(page->offset);
		//printf("%s page %d offset %d\n", __FUNCTION__, i, page->offset);
#endif
	}

	/* Read in the chunk lengths */
	for (i = 0, page = PMInfos; i < ChunksInFile; i++, page++) {
		//page->length = ReadInt16(PageFile);
		page->length = 0;
		fread(&page->length, sizeof(word), 1, PageFile);
#ifdef __AMIGA__
		page->length = SWAP16LE(page->length);
		//printf("%s page %d length %d\n", __FUNCTION__, i, page->length);
#endif
	}
	
	// adjust the sizes of the sounds that span multiple pages
	word *soundInfoPage = (word *)PM_GetPage(ChunksInFile - 1);
	word NumDigi = (word) PM_GetPageSize(ChunksInFile - 1) / 4;
	for (i = 0; i < NumDigi; i++) {
		word startpage = soundInfoPage[i * 2] + PMSoundStart;
		word size = soundInfoPage[i * 2 + 1];
		//printf("%d digi %d size %u -> %u\n", i, startpage, PMInfos[startpage].length, size);
		PMInfos[startpage].length = size;
	}
	//Quit("That's all");
}

static void PML_ClosePageFile()
{
	if (PageFile) {
		fclose(PageFile);
		PageFile = NULL;
	}

	for (int i = 0; i < ChunksInFile; i++) {
		PM_FreePage(i);
	}

	if (PMPages) {
		free(PMPages);
		PMPages = NULL;
	}

	if (PMInfos) {
		free(PMInfos);
		PMInfos = NULL;
	}
}

uint32_t PM_GetPageSize(int page)
{
	if (page >= ChunksInFile)
		Quit("PM_GetPage: Invalid page request");

	return PMInfos[page].length;
}

uint8_t *PM_GetPage(int pagenum)
{
	PageListStruct *page;
	uint8_t *addr;

	if (pagenum >= ChunksInFile)
		Quit("PM_GetPage: Invalid page request");

	addr = PMPages[pagenum];

	if (addr == NULL) {
		page = &PMInfos[pagenum];
		boolean soundpage = (pagenum >= PMSoundStart && pagenum < ChunksInFile - 1);
		uint32_t offset = page->offset;
		word length = page->length;
		if (soundpage)
			addr = AllocVec(length, MEMF_CHIP);
		else
			addr = malloc(length);
		CHECKMALLOCRESULT(addr);
		PMPages[pagenum] = addr;

		PML_ReadFromFile(addr, offset, length);
#ifdef __AMIGA__
		if (pagenum < PMSpriteStart)
		{
			// walls
			VL_RemapBufferEHB(addr, addr, length);
		}
		else if (pagenum < PMSoundStart)
		{
			// sprites
			//printf("%s(%d) byteswapping sprite\n", __FUNCTION__, pagenum);
			t_compshape *shape = (t_compshape *)addr;
			shape->leftpix = SWAP16LE(shape->leftpix);
			shape->rightpix = SWAP16LE(shape->rightpix);
			uint16_t swidth = shape->rightpix - shape->leftpix;

			for (uint16_t texturecolumn = 0; texturecolumn <= swidth; texturecolumn++)
			{
				shape->dataofs[texturecolumn] = SWAP16LE(shape->dataofs[texturecolumn]);
				uint16_t *srcpost = (uint16_t *)&addr[shape->dataofs[texturecolumn]];
				*srcpost = SWAP16LE(*srcpost); // end
				uint16_t end = *srcpost/2;
				srcpost++;
				while (end != 0)
				{
					*srcpost = SWAP16LE(*srcpost); // source
					uint16_t source = *srcpost;
					srcpost++;
					*srcpost = SWAP16LE(*srcpost); // start
					uint16_t start = *srcpost / 2;
					source += start;
					srcpost++;
					uint16_t length = end - start;
					VL_RemapBufferEHB(addr+source, addr+source, length);
					*srcpost = SWAP16LE(*srcpost); // end
					end = *srcpost/2;
					srcpost++;
				}
			}
		}
		else if (pagenum >= PMSoundStart && pagenum < ChunksInFile - 1)
		{
			// sound samples
			//printf("%s(%d) fixing samples\n", __FUNCTION__, pagenum);
			for (int j = 0; j < length; j++)
			{
				addr[j] ^= 128;
			}
		}
		else if (pagenum == ChunksInFile - 1)
		{
			// digi info page
			word *soundInfoPage = (word *)addr;
			for (int j = 0; j < length/2; j++)
			{
				soundInfoPage[j] = SWAP16LE(soundInfoPage[j]);
			}
		}
#endif
	}
	return addr;
}

void PM_FreePage(int pagenum)
{
	uint8_t *addr;

	if (pagenum >= ChunksInFile)
		Quit("PM_FreePage: Invalid page request");

	addr = PMPages[pagenum];
	if (addr != NULL) {
		boolean soundpage = (pagenum >= PMSoundStart && pagenum < ChunksInFile - 1);
		if (soundpage)
			FreeVec(addr);
		else
			free(addr);
		PMPages[pagenum] = NULL;
	}
}

void PM_Startup()
{
	if (PMStarted)
		return;

	PML_OpenPageFile();

	PMStarted = true;
}

void PM_Shutdown()
{
	if (!PMStarted)
		return;

	PML_ClosePageFile();
}

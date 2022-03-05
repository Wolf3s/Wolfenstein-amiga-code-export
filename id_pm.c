#include "wl_def.h"
#ifdef __AMIGA__
#include "id_vl.h"
#endif

int ChunksInFile;
int PMSpriteStart;
int PMSoundStart;

bool PMSoundInfoPagePadded = false;

// holds the whole VSWAP
uint32_t *PMPageData;
size_t PMPageDataSize;

// ChunksInFile+1 pointers to page starts.
// The last pointer points one byte after the last page.
uint8_t **PMPages;

void PM_Startup()
{
    char fname[13] = "vswap.";
    strcat(fname,extension);

    FILE *file = fopen(fname,"rb");
    if(!file)
        CA_CannotOpen(fname);

    ChunksInFile = 0;
    fread(&ChunksInFile, sizeof(word), 1, file);
    PMSpriteStart = 0;
    fread(&PMSpriteStart, sizeof(word), 1, file);
    PMSoundStart = 0;
    fread(&PMSoundStart, sizeof(word), 1, file);
#ifdef __AMIGA__
	// this assumes sizeof(int) == 4
	ChunksInFile = SWAP32LE(ChunksInFile);
	PMSpriteStart = SWAP32LE(PMSpriteStart);
	PMSoundStart = SWAP32LE(PMSoundStart);
	printf("%s ChunksInFile %d PMSpriteStart %d PMSoundStart %d\n", __FUNCTION__, ChunksInFile, PMSpriteStart, PMSoundStart);
#endif

    uint32_t* pageOffsets = (uint32_t *) malloc((ChunksInFile + 1) * sizeof(int32_t));
    CHECKMALLOCRESULT(pageOffsets);
    fread(pageOffsets, sizeof(uint32_t), ChunksInFile, file);
#ifdef __AMIGA__
	for (int i = 0; i < ChunksInFile; i++)
		pageOffsets[i] = SWAP32LE(pageOffsets[i]);
#endif

    word *pageLengths = (word *) malloc(ChunksInFile * sizeof(word));
    CHECKMALLOCRESULT(pageLengths);
    fread(pageLengths, sizeof(word), ChunksInFile, file);
#ifdef __AMIGA__
	for (int i = 0; i < ChunksInFile; i++)
	{
		pageLengths[i] = SWAP16LE(pageLengths[i]);
	}
#endif

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    long pageDataSize = fileSize - pageOffsets[0];
    if(pageDataSize > (size_t) -1)
        Quit("The page file \"%s\" is too large!", fname);

    pageOffsets[ChunksInFile] = fileSize;

    uint32_t dataStart = pageOffsets[0];
    int i;

    // Check that all pageOffsets are valid
    for(i = 0; i < ChunksInFile; i++)
    {
        if(!pageOffsets[i]) continue;   // sparse page
        if(pageOffsets[i] < dataStart || pageOffsets[i] >= (size_t) fileSize)
            Quit("Illegal page offset for page %i: %u (filesize: %u)",
                    i, pageOffsets[i], fileSize);
    }

    // Calculate total amount of padding needed for sprites and sound info page
    int alignPadding = 0;
    for(i = PMSpriteStart; i < PMSoundStart; i++)
    {
        if(!pageOffsets[i]) continue;   // sparse page
        uint32_t offs = pageOffsets[i] - dataStart + alignPadding;
        if(offs & 1)
            alignPadding++;
    }

    if((pageOffsets[ChunksInFile - 1] - dataStart + alignPadding) & 1)
        alignPadding++;

    PMPageDataSize = (size_t) pageDataSize + alignPadding;
    PMPageData = (uint32_t *) malloc(PMPageDataSize);
    CHECKMALLOCRESULT(PMPageData);

    PMPages = (uint8_t **) malloc((ChunksInFile + 1) * sizeof(uint8_t *));
    CHECKMALLOCRESULT(PMPages);

    // Load pages and initialize PMPages pointers
    uint8_t *ptr = (uint8_t *) PMPageData;
    for(i = 0; i < ChunksInFile; i++)
    {
        if(i >= PMSpriteStart && i < PMSoundStart || i == ChunksInFile - 1)
        {
            size_t offs = ptr - (uint8_t *) PMPageData;

            // pad with zeros to make it 2-byte aligned
            if(offs & 1)
            {
                *ptr++ = 0;
                if(i == ChunksInFile - 1) PMSoundInfoPagePadded = true;
            }
        }

        PMPages[i] = ptr;

        if(!pageOffsets[i])
            continue;               // sparse page

        // Use specified page length, when next page is sparse page.
        // Otherwise, calculate size from the offset difference between this and the next page.
        uint32_t size;
        if(!pageOffsets[i + 1]) size = pageLengths[i];
        else size = pageOffsets[i + 1] - pageOffsets[i];

        fseek(file, pageOffsets[i], SEEK_SET);
        fread(ptr, 1, size, file);
#ifdef __AMIGA__
		if (i < PMSpriteStart)
		{
			// walls
			VL_RemapBufferEHB(ptr, ptr, size);
		}
		else if (/*i >= PMSpriteStart &&*/ i < PMSoundStart)
		{
			// sprites
			//printf("%s(%d) byteswapping sprite\n", __FUNCTION__, i);
			t_compshape *shape = (t_compshape *)ptr;
			shape->leftpix = SWAP16LE(shape->leftpix);
			shape->rightpix = SWAP16LE(shape->rightpix);
			uint16_t swidth = shape->rightpix - shape->leftpix;

			for (uint16_t texturecolumn = 0; texturecolumn <= swidth; texturecolumn++)
			{
				shape->dataofs[texturecolumn] = SWAP16LE(shape->dataofs[texturecolumn]);
				uint16_t *srcpost = (uint16_t *)&ptr[shape->dataofs[texturecolumn]];
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
					VL_RemapBufferEHB(ptr+source, ptr+source, length);
					*srcpost = SWAP16LE(*srcpost); // end
					end = *srcpost/2;
					srcpost++;
				}
			}
		}
		else if (i >= PMSoundStart && i < ChunksInFile - 1)
		{
			// sound samples
			//printf("%s(%d) fixing samples\n", __FUNCTION__, i);
			for (int j = 0; j < size; j++)
			{
				ptr[j] ^= 128;
			}
		}
		else if (i == ChunksInFile - 1)
		{
			// digi replacement info page
			word *soundInfoPage = (word *)ptr;
			for (int j = 0; j < size/2; j++)
			{
				soundInfoPage[j] = SWAP16LE(soundInfoPage[j]);
			}
		}
#endif
        ptr += size;
    }

    // last page points after page buffer
    PMPages[ChunksInFile] = ptr;

    free(pageLengths);
    free(pageOffsets);
    fclose(file);
}

void PM_Shutdown()
{
    free(PMPages);
    free(PMPageData);
}

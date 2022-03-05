// ID_VL.C

#include <string.h>
#ifdef __AMIGA__
//#include <cybergraphx/cybergraphics.h>
#include <proto/intuition.h>
//#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>
#include <graphics/videocontrol.h>
#define IPTR ULONG
#define Point W3DPoint
#undef SIGN
#endif
#include "wl_def.h"
/*#ifdef __AMIGA__
#include <cybergraphx/cybergraphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#endif*/
#pragma hdrstop

// Uncomment the following line, if you get destination out of bounds
// assertion errors and want to ignore them during debugging
//#define IGNORE_BAD_DEST

//#define USE_EHB
//#define WAITFORSAFE

#ifdef IGNORE_BAD_DEST
#undef assert
#define assert(x) if(!(x)) return
#define assert_ret(x) if(!(x)) return 0
#else
#define assert_ret(x) assert(x)
#endif

boolean fullscreen = true;
#if defined(_arch_dreamcast)
boolean usedoublebuffering = false;
unsigned screenWidth = 320;
unsigned screenHeight = 200;
unsigned screenBits = 8;
#elif defined(GP2X)
boolean usedoublebuffering = true;
unsigned screenWidth = 320;
unsigned screenHeight = 240;
#if defined(GP2X_940)
unsigned screenBits = 8;
#else
unsigned screenBits = 16;
#endif
#elif defined(__AMIGA__)
//boolean usedoublebuffering = true;
#else
boolean usedoublebuffering = true;
unsigned screenWidth = 640;
unsigned screenHeight = 400;
unsigned screenBits = -1;      // use "best" color depth according to libSDL
#endif

#ifdef __AMIGA__
/*static*/ struct Screen *screen = NULL;
/*static*/ struct Window *window = NULL;
static UWORD *pointermem = NULL;
static struct BitMap *tempbm = NULL;
//struct Library *CyberGfxBase = NULL;

/*static struct BitMap *screenBitMaps[2];
int currentBitMap = 0;
struct DBufInfo *dbuf = NULL;*/
struct ScreenBuffer *sbuf[2];
struct RastPort rastports[2];
unsigned	bufferofs;
unsigned	displayofs;
#ifdef WAITFORSAFE
struct MsgPort *dispport;
//struct MsgPort *safeport;
//static BOOL SafeToWrite;
static BOOL SafeToChange;
#endif
static BOOL bStandardBitMap;
/*static*/ BOOL bEHBScreen;

uint8_t *chunkyBuffer = NULL;

#ifdef USE_EHB
static uint8_t *pEHBBuf = NULL;
//static SDL_Color ehbpalette[32];
static uint8_t ehbxlat[256];
static uint8_t ehbunmap[64];
#endif

#else
SDL_Surface *screen = NULL;
//#endif
unsigned screenPitch;

SDL_Surface *screenBuffer = NULL;
unsigned bufferPitch;

SDL_Surface *curSurface = NULL;
unsigned curPitch;

//#ifndef __AMIGA__
unsigned scaleFactor;
#endif

boolean	 screenfaded;
unsigned bordercolor;

SDL_Color palette1[256], palette2[256];
SDL_Color curpal[256];

#define CASSERT(x) extern int ASSERT_COMPILE[((x) != 0) * 2 - 1];
#ifdef __AMIGA__
#define RGB(r, g, b) {(r)*255/63, (g)*255/63, (b)*255/63}
#else
#define RGB(r, g, b) {(r)*255/63, (g)*255/63, (b)*255/63, 0}
#endif

SDL_Color gamepal[]={
#ifdef SPEAR
    #include "sodpal.inc"
#else
    #include "wolfpal.inc"
#endif
};

CASSERT(lengthof(gamepal) == 256)

//===========================================================================

#ifdef __AMIGA__
void VL_ReAllocChunkyBuffer(void)
{
	chunkyBuffer = realloc(chunkyBuffer, viewwidth*viewheight);
	CHECKMALLOCRESULT(chunkyBuffer);
}

#ifdef KALMS_C2P
#include <SDI_compiler.h>
void ASM c2p1x1_8_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
void ASM c2p1x1_6_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
#ifdef USE_HALFWIDTH
void ASM c2p2x1_8_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
void ASM c2p2x1_6_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
#endif
#endif

#ifdef USE_EHB
byte VL_RemapColorEHB(byte color)
{
	return bEHBScreen ? ehbxlat[color] : color;
}

byte VL_UnmapColorEHB(byte color)
{
	return bEHBScreen ? ehbunmap[color] : color;
	/*
	// slow linear search
	for (int i = 0; i < 256; i++)
	{
		if (ehbxlat[i] == color)
		{
			return i;
		}
	}
	return 0;
	*/
}

void VL_RemapBufferEHB(byte *source, byte *dest, int size)
{
	if (!bEHBScreen)
		return;

	int16_t count = size;
	uint8_t *xlat = ehbxlat;
	uint8_t *from = source;
	uint8_t *to = dest;

	do
	{
		byte col = xlat[*from++];
		*to++ = col;
	} while (--count);
}

static inline int VL_ColorDistance(SDL_Color *e1, SDL_Color *e2)
{
	int cR=(int)e1->r-(int)e2->r;
	int cG=(int)e1->g-(int)e2->g;
	int cB=(int)e1->b-(int)e2->b;
	//int uR=(int)e1->r+(int)e2->r;
	//int distance=cR*cR*(2+uR/256) + cG*cG*4 + cB*cB*(2+(255-uR)/256);
	// ITU BT.601
	int distance = (cR*cR)*30 + (cG*cG)*59 + (cB*cB)*11;
	// ITU BT.709
	//int distance = (cR*cR)*21 + (cG*cG)*71 + (cB*cB)*7;
	// just sum the differences
	//int distance = (cR*cR) + (cG*cG) + (cB*cB);
	//printf("(%03u,%03u,%03u) -> (%03u,%03u,%03u) = %d\n", e1->r, e1->g, e1->b, e2->r, e2->g, e2->b, distance);
	return distance;
}

boolean VL_RemapBufferCustomEHB(byte *source, int size, SDL_Color *palette)
{
	//printf("%s(%p, %d, %p)\n", __FUNCTION__, source, size, palette);

	if (!bEHBScreen)
		return false;

	byte xlat[256];

	// find the closest color for each entry in the palette
	for (int i = 0; i < 256; i++)
	{
		int mindist = INT32_MAX;
		int minidx = 0;
		SDL_Color *e1 = &palette[i];
		//printf("remapping color %d (%03u,%03u,%03u)\n", i, e1->r, e1->g, e1->b);
		for (int j = 0; j < 64; j++)
		{
			SDL_Color *e2 = &gamepal[j];
			int dist = VL_ColorDistance(e1, e2);
			//printf("ehb %d dist %d\n", j, dist);
			if (dist < mindist)
			{
				//printf("new mindist %d ehb %d (%03u,%03u,%03u)\n", mindist, j,  e2->r, e2->g, e2->b);
				mindist = dist;
				minidx = j;
			}
		}
		//printf("found match %d\n", minidx);
		xlat[i] = minidx;
	}

	int16_t count = size;
	uint8_t *buf = source;
	do
	{
		byte col = xlat[*buf];
		*buf++ = col;
	} while (--count);
	// HACK! to prevent VL_MemToScreen from remapping it again
	source[0] = 255;

	return true;
}
#else
byte VL_RemapColorEHB(byte color) { return color; }
void VL_RemapBufferEHB(byte *source, byte *dest, int size) { return; }
boolean VL_RemapBufferCustomEHB(byte *source, int size, SDL_Color *palette) { return false; }
#endif

static void ChunkyToRastPort(byte *source, int width, int height, struct RastPort *rp, int x, int y)
{
#ifdef KALMS_C2P
	if ((width % 32) == 0 && (x % 8) == 0 && /*bStandardBitMap*/ GetBitMapAttr(rp->BitMap, BMA_FLAGS) & BMF_STANDARD)
	{
#ifdef USE_EHB
		if (bEHBScreen)
		{
			c2p1x1_6_c5_bm(width, height, x, y, source, rp->BitMap);
		}
		else
#endif
		{
			c2p1x1_8_c5_bm(width, height, x, y, source, rp->BitMap);
		}
	}
	else
#endif
	if (GfxBase->LibNode.lib_Version >= 40)
	{
		WriteChunkyPixels(rp, x, y, x + width - 1, y + height - 1, source, width);
	}
	/*
	else if (width % 16 == 0)
	{
		struct RastPort temprp;
		temprp = *rp;
		temprp.Layer = NULL;
		temprp.BitMap = tempbm;
		WritePixelArray8(rp, x, y, x + width - 1, y + height - 1, source, &temprp);
	}
	*/
	else
	{
		struct RastPort temprp;
		temprp = *rp;
		temprp.Layer = NULL;
		temprp.BitMap = tempbm;
		byte *p_read = source;
		for (int j = 0; j < height; j++)
		{
			//WritePixelLine8(rp, x, y + j, width, p_read, &temprp);
			// Kickstart 3.0 fix
			static byte wplbuf[320];
			memcpy(wplbuf, p_read, width);
			WritePixelLine8(rp, x, y + j, width, wplbuf, &temprp);
			p_read += width;
		}
	}
}

//#define AKIKO_C2P
#ifdef AKIKO_C2P
static void c2p1x1_8_akiko_bm(WORD chunkyx, WORD chunkyy, WORD offsx, WORD offsy, APTR chunkyscreen, struct BitMap *bitmap)
{
	volatile ULONG *c2p = GfxBase->ChunkyToPlanarPtr;
	ULONG *src = (ULONG *)chunkyscreen;
	UWORD bmBytesPerRow = bitmap->BytesPerRow;
	WORD planeofs = (offsx / 8) + offsy * bmBytesPerRow;
	PLANEPTR *bmPlanes = bitmap->Planes;
	ULONG *dst0 = (ULONG *)&bmPlanes[0][planeofs];
	ULONG *dst1 = (ULONG *)&bmPlanes[1][planeofs];
	ULONG *dst2 = (ULONG *)&bmPlanes[2][planeofs];
	ULONG *dst3 = (ULONG *)&bmPlanes[3][planeofs];
	ULONG *dst4 = (ULONG *)&bmPlanes[4][planeofs];
	ULONG *dst5 = (ULONG *)&bmPlanes[5][planeofs];
	ULONG *dst6 = (ULONG *)&bmPlanes[6][planeofs];
	ULONG *dst7 = (ULONG *)&bmPlanes[7][planeofs];
	UWORD stride = (bmBytesPerRow - (chunkyx / 8)) / sizeof(ULONG);
	UWORD rowbytes = chunkyx / (8 * sizeof(ULONG));

	OwnBlitter();
	for (WORD i = 0; i < chunkyy; i++)
	{
		for (WORD j = 0; j < rowbytes; j++)
		{
			*c2p = *src++;
			*c2p = *src++;
			*c2p = *src++;
			*c2p = *src++;
			*c2p = *src++;
			*c2p = *src++;
			*c2p = *src++;
			*c2p = *src++;
			*dst0++ = *c2p;
			*dst1++ = *c2p;
			*dst2++ = *c2p;
			*dst3++ = *c2p;
			*dst4++ = *c2p;
			*dst5++ = *c2p;
			*dst6++ = *c2p;
			*dst7++ = *c2p;
		}
		dst0 += stride;
		dst1 += stride;
		dst2 += stride;
		dst3 += stride;
		dst4 += stride;
		dst5 += stride;
		dst6 += stride;
		dst7 += stride;
	}
	DisownBlitter();
}

static ULONG interleave_bytes(UWORD input)
{
	ULONG word = input;
	word = (word ^ (word << 8)) & 0x00ff00ff;
	word = (word | (word << 8));
	return word;
}

static void c2p2x1_8_akiko_bm(WORD chunkyx, WORD chunkyy, WORD offsx, WORD offsy, APTR chunkyscreen, struct BitMap *bitmap)
{
	volatile ULONG *c2p = GfxBase->ChunkyToPlanarPtr;
	UWORD *src = (UWORD *)chunkyscreen;
	UWORD bmBytesPerRow = bitmap->BytesPerRow;
	WORD planeofs = (offsx / 8) + offsy * bmBytesPerRow;
	PLANEPTR *bmPlanes = bitmap->Planes;
	ULONG *dst0 = (ULONG *)&bmPlanes[0][planeofs];
	ULONG *dst1 = (ULONG *)&bmPlanes[1][planeofs];
	ULONG *dst2 = (ULONG *)&bmPlanes[2][planeofs];
	ULONG *dst3 = (ULONG *)&bmPlanes[3][planeofs];
	ULONG *dst4 = (ULONG *)&bmPlanes[4][planeofs];
	ULONG *dst5 = (ULONG *)&bmPlanes[5][planeofs];
	ULONG *dst6 = (ULONG *)&bmPlanes[6][planeofs];
	ULONG *dst7 = (ULONG *)&bmPlanes[7][planeofs];
	UWORD stride = (bmBytesPerRow - (chunkyx*2 / 8)) / sizeof(ULONG);
	UWORD rowbytes = chunkyx*2 / (8 * sizeof(ULONG));

	OwnBlitter();
	for (WORD i = 0; i < chunkyy; i++)
	{
		for (WORD j = 0; j < rowbytes; j++)
		{
#define DOUBLE_PIX(src) do { *c2p = interleave_bytes(*(src)++); } while(0)
			DOUBLE_PIX(src);
			DOUBLE_PIX(src);
			DOUBLE_PIX(src);
			DOUBLE_PIX(src);
			DOUBLE_PIX(src);
			DOUBLE_PIX(src);
			DOUBLE_PIX(src);
			DOUBLE_PIX(src);
#undef DOUBLE_PIX
			*dst0++ = *c2p;
			*dst1++ = *c2p;
			*dst2++ = *c2p;
			*dst3++ = *c2p;
			*dst4++ = *c2p;
			*dst5++ = *c2p;
			*dst6++ = *c2p;
			*dst7++ = *c2p;
		}
		dst0 += stride;
		dst1 += stride;
		dst2 += stride;
		dst3 += stride;
		dst4 += stride;
		dst5 += stride;
		dst6 += stride;
		dst7 += stride;
	}
	DisownBlitter();
}
#endif

void VL_DrawChunkyBuffer(void)
{
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif

#ifdef AKIKO_C2P
	if (bStandardBitMap && GfxBase->LibNode.lib_Version >= 40 && GfxBase->ChunkyToPlanarPtr)
	{
#ifdef USE_HALFWIDTH
		c2p2x1_8_akiko_bm(viewwidth, viewheight, viewscreenx, viewscreeny, chunkyBuffer, rp->BitMap);
#else
		c2p1x1_8_akiko_bm(viewwidth, viewheight, viewscreenx, viewscreeny, chunkyBuffer, rp->BitMap);
#endif
	}
	else
#endif
#ifdef KALMS_C2P
	if (bStandardBitMap)
	{
#ifdef USE_EHB
		if (bEHBScreen)
		{
#ifdef USE_HALFWIDTH
			//c2p2x1_6_c5_bm(viewwidth, viewheight, viewscreenx, viewscreeny, chunkyBuffer, rp->BitMap); TODO
#else
			c2p1x1_6_c5_bm(viewwidth, viewheight, viewscreenx, viewscreeny, chunkyBuffer, rp->BitMap);
#endif
		}
		else
#endif
		{
#ifdef USE_HALFWIDTH
			c2p2x1_8_c5_bm(viewwidth, viewheight, viewscreenx, viewscreeny, chunkyBuffer, rp->BitMap);
#else
			c2p1x1_8_c5_bm(viewwidth, viewheight, viewscreenx, viewscreeny, chunkyBuffer, rp->BitMap);
#endif
		}
	}
	else
#endif
	if (GfxBase->LibNode.lib_Version >= 40)
	{
		WriteChunkyPixels(rp, viewscreenx, viewscreeny, viewscreenx + viewwidth - 1, viewscreeny + viewheight - 1, chunkyBuffer, viewwidth);
	}
	else
	{
		WritePixelArray8(rp, viewscreenx, viewscreeny, viewscreenx + viewwidth - 1, viewscreeny + viewheight - 1, chunkyBuffer, NULL);
	}
}

void VL_ChangeScreenBuffer(void)
{
	struct ScreenBuffer* displaybuf = sbuf[displayofs];
	if (screen->ViewPort.RasInfo->BitMap == displaybuf->sb_BitMap)
	{
		// we are already using the display buffer
		return;
	}
#ifdef WAITFORSAFE
	if (!SafeToChange)
	{
		while (!GetMsg(dispport))
			WaitPort(dispport);
		SafeToChange = TRUE;
	}
	//WaitBlit();
	//displaybuf->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = safeport;
	displaybuf->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = dispport;
#endif
	while (!ChangeScreenBuffer(screen, displaybuf))
		Delay(1);
#ifdef WAITFORSAFE
	SafeToChange = FALSE;
	//SafeToWrite = FALSE;
#endif
}


void VL_WaitVBL(int vbls)
{
	while (vbls-- > 0)
		WaitTOF();
}
#endif


/*
=======================
=
= VL_Shutdown
=
=======================
*/

void	VL_Shutdown (void)
{
	if (sbuf[0])
	{
		FreeScreenBuffer(screen, sbuf[0]);
		sbuf[0] = NULL;
	}

	if (sbuf[1])
	{
		FreeScreenBuffer(screen,sbuf[1]);
		sbuf[1] = NULL;
	}

#ifdef WAITFORSAFE
	/*
	if (safeport)
	{
		if (!SafeToWrite)
		{
			while (!GetMsg(safeport))
				WaitPort(safeport);
			SafeToWrite = TRUE;
		}
		DeleteMsgPort(safeport);
		safeport = false;
	}
	*/

	if (dispport)
	{
		if (!SafeToChange)
		{
			while (!GetMsg(dispport))
				WaitPort(dispport);
			SafeToChange = TRUE;
		}
		DeleteMsgPort(dispport);
		dispport = false;
	}
#endif

	if (window)
	{
		CloseWindow(window);
		window = NULL;
	}

	if (screen)
	{
		CloseScreen(screen);
		screen = NULL;
	}

	if (tempbm)
	{
		FreeBitMap(tempbm);
		tempbm = NULL;
	}

	if (pointermem)
	{
		FreeVec(pointermem);
		pointermem = NULL;
	}

	if (chunkyBuffer)
	{
		free(chunkyBuffer);
		chunkyBuffer = NULL;
	}

#ifdef USE_EHB
	if (pEHBBuf)
	{
		free(pEHBBuf);
		pEHBBuf = NULL;
	}
#endif

	for (int i = 0; i < NUMLATCHPICS; i++)
	{
		FreeBitMap((struct BitMap *)latchpics[i]);
		latchpics[i] = NULL;
	}

	extern UWORD *fontTemplate[NUMFONT];
	for (int i = 0; i < NUMFONT; i++)
	{
		FreeVec(fontTemplate[i]);
		fontTemplate[i] = NULL;
	}

	//VL_SetTextMode ();
}


/*
=======================
=
= VL_SetVGAPlaneMode
=
=======================
*/

#ifdef __AROS__
// C99 hack
#undef OpenScreenTags
#define OpenScreenTags(arg1, ...) \
({ \
    IPTR __args[] = { __VA_ARGS__ }; \
    OpenScreenTagList((arg1), (struct TagItem *)__args); \
})
#undef OpenWindowTags
#define OpenWindowTags(arg1, ...) \
({ \
    IPTR __args[] = { __VA_ARGS__ }; \
    OpenWindowTagList((arg1), (struct TagItem *)__args); \
})
#endif

void	VL_SetVGAPlaneMode (void)
{
#ifdef __AMIGA__
	//memcpy(curpal, gamepal, sizeof(SDL_Color) * 256);

	ULONG modeid = INVALID_ID;
	ULONG depth = 8;

	/*if (CyberGfxBase)
	{
		modeid = BestCModeIDTags(
			CYBRBIDTG_NominalWidth, screenWidth,
			CYBRBIDTG_NominalHeight, screenHeight,
			CYBRBIDTG_Depth, depth,
			TAG_DONE);
		printf("%s:%d modeid %08x\n", __FUNCTION__, __LINE__, modeid);
	}*/

	if (modeid == (ULONG)INVALID_ID)
	{
		modeid = BestModeID(
			BIDTAG_NominalWidth, screenWidth,
			BIDTAG_NominalHeight, screenHeight,
			BIDTAG_Depth, depth,
			//BIDTAG_MonitorID, DEFAULT_MONITOR_ID,
			TAG_DONE);
		//printf("%s:%d modeid %08x\n", __FUNCTION__, __LINE__, modeid);
	}

#ifdef USE_EHB
	if (modeid == (ULONG)INVALID_ID)
	{
		depth = 6;
		modeid = BestModeID(
			BIDTAG_NominalWidth, screenWidth,
			BIDTAG_NominalHeight, screenHeight,
			BIDTAG_Depth, depth,
			BIDTAG_DIPFMustHave, DIPF_IS_EXTRAHALFBRITE,
			TAG_DONE);
		//printf("%s:%d modeid %08x\n", __FUNCTION__, __LINE__, modeid);
	}
#endif

	struct TagItem vctl[] =
	{
		{VTAG_BORDERBLANK_SET, TRUE},
		{VC_IntermediateCLUpdate, FALSE},
		{VTAG_END_CM, 0}
	};

	if ((screen = OpenScreenTags(NULL, 
		SA_DisplayID, modeid,
		SA_Width, screenWidth,
		SA_Height, screenHeight,
		SA_Depth, depth,
		SA_ShowTitle, FALSE,
		SA_Quiet, TRUE,
		SA_Draggable, FALSE,
		SA_Type, CUSTOMSCREEN,
		SA_VideoControl, (IPTR)vctl,
		SA_Interleaved, TRUE,
		TAG_DONE)))
	{
		if ((window = OpenWindowTags(NULL,
			WA_Flags, WFLG_BACKDROP | WFLG_REPORTMOUSE | WFLG_BORDERLESS | WFLG_ACTIVATE | WFLG_RMBTRAP,
			WA_InnerWidth, screenWidth,
			WA_InnerHeight, screenHeight,
			WA_CustomScreen, (IPTR)screen,
			TAG_DONE)))
		{
#ifdef USE_DOUBLEBUFFER
#ifdef WAITFORSAFE
			//printf("screen rp bm %p vp bm %p\n", screen->RastPort.BitMap, screen->ViewPort.RasInfo->BitMap);
			/*
			SafeToWrite = TRUE;
			safeport = CreateMsgPort();
			*/
			SafeToChange = TRUE;
			dispport = CreateMsgPort();
#endif
			sbuf[0] = AllocScreenBuffer(screen, 0, SB_SCREEN_BITMAP);
			sbuf[1] = AllocScreenBuffer(screen, 0, SB_COPY_BITMAP);
			for (int i = 0; i < 2; i++)
			{
				InitRastPort(&rastports[i]);
				rastports[i].BitMap = sbuf[i]->sb_BitMap;
				//printf("%d. buffer %p bitmap %p rp bm %p\n", i, sbuf[i], sbuf[i]->sb_BitMap, rastports[i].BitMap);
			}
			displayofs = bufferofs = 0;
			//printf("%s:%d bufferofs %d displayofs %d\n", __FUNCTION__, __LINE__, bufferofs, displayofs);
#endif

			pointermem = (UWORD *)AllocVec(2 * 6, MEMF_CLEAR | MEMF_CHIP);
			SetPointer(window, pointermem, 1, 1, 0, 0);
			bStandardBitMap = GetBitMapAttr(screen->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD;
			// always allocate as the screenshot and fizzle fade benefits from it
			//if (GfxBase->LibNode.lib_Version < 40)
			{
				tempbm = AllocBitMap(screenWidth, 1, screen->RastPort.BitMap->Depth, BMF_STANDARD, NULL);
			}
			bEHBScreen = screen->RastPort.BitMap->Depth == 6;
#ifdef USE_EHB
			if (bEHBScreen)
			{
				char fname[13] = "ehbpal0.";
				strcat(fname,extension);
				pEHBBuf = malloc(320*200);
				CHECKMALLOCRESULT(pEHBBuf);
				FILE *file = fopen(fname, "rb");
				if (file)
				{
					//fread(&ehbpalette, sizeof(ehbpalette), 1, file);
					memset(gamepal, 0, sizeof(gamepal));
					fread(&gamepal, 32*3, 1, file);
					// fill the halfbrite part for screenshots and remapping
					for (int i = 0; i < 32; i++)
					{
						gamepal[i+32].r = gamepal[i].r / 2;
						gamepal[i+32].g = gamepal[i].g / 2;
						gamepal[i+32].b = gamepal[i].b / 2;
					}
					fread(&ehbxlat, sizeof(ehbxlat), 1, file);
					fclose(file);
					// generate the unmap table
					for (int j = 0; j < 64; j++)
					{
						for (int i = 0; i < 256; i++)
						{
							if (ehbxlat[i] == j)
							{
								//printf("%s EHB %d -> %d\n", __FUNCTION__, j, i);
								ehbunmap[j] = i;
								break;
							}
						}
					}
				}
				//VL_SetPalette(ehbpalette, false);
				VL_SetPalette(gamepal, false);
				/*
				SDL_Color *col;
				col = &gamepal[0x6F]; printf("%d %d %d\n", col->r, col->g, col->b);
				col = &gamepal[0x19]; printf("%d %d %d\n", col->r, col->g, col->b);
				Quit("all done");
				*/
			}
			else
#endif
			{
				VL_SetPalette(gamepal, false);
			}
			return;
		}
	}
	//printf("%s:%d screen %p window %p\n", __FUNCTION__, __LINE__, screen, window);

	//VL_Shutdown();
	// painful death awaits

	Quit("Couldn't start up the graphics.");
#else
#ifdef SPEAR
    SDL_WM_SetCaption("Spear of Destiny", NULL);
#else
    SDL_WM_SetCaption("Wolfenstein 3D", NULL);
#endif

    if(screenBits == -1)
    {
        const SDL_VideoInfo *vidInfo = SDL_GetVideoInfo();
        screenBits = vidInfo->vfmt->BitsPerPixel;
    }

    screen = SDL_SetVideoMode(screenWidth, screenHeight, screenBits,
          (usedoublebuffering ? SDL_HWSURFACE | SDL_DOUBLEBUF : 0)
        | (screenBits == 8 ? SDL_HWPALETTE : 0)
        | (fullscreen ? SDL_FULLSCREEN : 0));
    if(!screen)
    {
        printf("Unable to set %ix%ix%i video mode: %s\n", screenWidth,
            screenHeight, screenBits, SDL_GetError());
        exit(1);
    }
    if((screen->flags & SDL_DOUBLEBUF) != SDL_DOUBLEBUF)
        usedoublebuffering = false;
    SDL_ShowCursor(SDL_DISABLE);

    SDL_SetColors(screen, gamepal, 0, 256);
    memcpy(curpal, gamepal, sizeof(SDL_Color) * 256);

    screenBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, screenWidth,
        screenHeight, 8, 0, 0, 0, 0);
    if(!screenBuffer)
    {
        printf("Unable to create screen buffer surface: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_SetColors(screenBuffer, gamepal, 0, 256);

    screenPitch = screen->pitch;
    bufferPitch = screenBuffer->pitch;

    curSurface = screenBuffer;
    curPitch = bufferPitch;

    scaleFactor = screenWidth/320;
    if(screenHeight/200 < scaleFactor) scaleFactor = screenHeight/200;

    pixelangle = (short *) malloc(screenWidth * sizeof(short));
    CHECKMALLOCRESULT(pixelangle);
    wallheight = (int *) malloc(screenWidth * sizeof(int));
    CHECKMALLOCRESULT(wallheight);
#endif
}

/*
=============================================================================

						PALETTE OPS

		To avoid snow, do a WaitVBL BEFORE calling these

=============================================================================
*/

/*
=================
=
= VL_ConvertPalette
=
=================
*/

void VL_ConvertPalette(byte *srcpal, SDL_Color *destpal, int numColors)
{
    for(int i=0; i<numColors; i++)
    {
        destpal[i].r = *srcpal++ * 255 / 63;
        destpal[i].g = *srcpal++ * 255 / 63;
        destpal[i].b = *srcpal++ * 255 / 63;
    }
}

/*
=================
=
= VL_FillPalette
=
=================
*/

void VL_FillPalette (int red, int green, int blue)
{
    int i;
#if 1
    for(i=0; i<256; i++)
    {
        curpal[i].r = red;
        curpal[i].g = green;
        curpal[i].b = blue;
    }

    VL_SetPalette(NULL, true);
#else
    SDL_Color pal[256];

    for(i=0; i<256; i++)
    {
        pal[i].r = red;
        pal[i].g = green;
        pal[i].b = blue;
    }

    VL_SetPalette(pal, true);
#endif
}

//===========================================================================

/*
=================
=
= VL_SetColor
=
=================
*/

#ifndef __AMIGA__ // UNUSED
void VL_SetColor	(int color, int red, int green, int blue)
{
    SDL_Color col = { red, green, blue };
    curpal[color] = col;

#ifdef __AMIGA__
	// TODO hack
	if (bEHBScreen)
		return;
	SetRGB32(&screen->ViewPort, color, red << 24, green << 24, blue << 24);
#else
    if(screenBits == 8)
        SDL_SetPalette(screen, SDL_PHYSPAL, &col, color, 1);
    else
    {
        SDL_SetPalette(curSurface, SDL_LOGPAL, &col, color, 1);
        SDL_BlitSurface(curSurface, NULL, screen, NULL);
        SDL_Flip(screen);
    }
#endif
}
#endif

//===========================================================================

/*
=================
=
= VL_GetColor
=
=================
*/

void VL_GetColor	(int color, int *red, int *green, int *blue)
{
    SDL_Color *col = &curpal[color];
    *red = col->r;
    *green = col->g;
    *blue = col->b;
}

//===========================================================================

/*
=================
=
= VL_SetPalette
=
=================
*/

void VL_SetPalette (SDL_Color *palette, bool forceupdate)
{
	if (palette)
		memcpy(curpal, palette, sizeof(SDL_Color) * 256);
	/*else
		memset(curpal, 0,  sizeof(SDL_Color) * 256);*/

#ifdef __AMIGA__
	int entry;
	static ULONG palette32[256 * 3 + 2];
	ULONG *sp = palette32;
	uint8_t *p = (uint8_t *)curpal;
	ULONG colors = bEHBScreen ? 32 : 256;

	*sp++ = colors << 16;
	for (entry = 0; entry < colors; ++entry)
	{
#if 1
		*sp++ = ((ULONG)*p++) << 24;
		*sp++ = ((ULONG)*p++) << 24;
		*sp++ = ((ULONG)*p++) << 24;
#else
		*sp++ = curpal[entry].r << 24;
		*sp++ = curpal[entry].g << 24;
		*sp++ = curpal[entry].b << 24;
#endif
	}
	*sp = 0;
	LoadRGB32(&screen->ViewPort, palette32);
#else
    if(screenBits == 8)
        SDL_SetPalette(screen, SDL_PHYSPAL, palette, 0, 256);
    else
    {
        SDL_SetPalette(curSurface, SDL_LOGPAL, palette, 0, 256);
        if(forceupdate)
        {
            SDL_BlitSurface(curSurface, NULL, screen, NULL);
            SDL_Flip(screen);
        }
    }
#endif
}


//===========================================================================

/*
=================
=
= VL_GetPalette
=
=================
*/

void VL_GetPalette (SDL_Color *palette)
{
    memcpy(palette, curpal, sizeof(SDL_Color) * 256);
}


//===========================================================================

/*
=================
=
= VL_FadeOut
=
= Fades the current palette to the given color in the given number of steps
=
=================
*/

void VL_FadeOut (int start, int end, int red, int green, int blue, int steps)
{
	int		    i,j,orig,delta;
	SDL_Color   *origptr, *newptr;

    red = red * 255 / 63;
    green = green * 255 / 63;
    blue = blue * 255 / 63;

	VL_WaitVBL(1);
	VL_GetPalette(palette1);
	memcpy(palette2, palette1, sizeof(SDL_Color) * 256);

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		origptr = &palette1[start];
		newptr = &palette2[start];
		for (j=start;j<=end;j++)
		{
			orig = origptr->r;
			delta = red-orig;
			newptr->r = orig + delta * i / steps;
			orig = origptr->g;
			delta = green-orig;
			newptr->g = orig + delta * i / steps;
			orig = origptr->b;
			delta = blue-orig;
			newptr->b = orig + delta * i / steps;
			origptr++;
			newptr++;
		}

		/*if(!usedoublebuffering || screenBits == 8)*/ VL_WaitVBL(1);
		VL_SetPalette (palette2, true);
	}

//
// final color
//
	VL_FillPalette (red,green,blue);

	screenfaded = true;
}


/*
=================
=
= VL_FadeIn
=
=================
*/

void VL_FadeIn (int start, int end, SDL_Color *palette, int steps)
{
	int i,j,delta;

	VL_WaitVBL(1);
	VL_GetPalette(palette1);
	memcpy(palette2, palette1, sizeof(SDL_Color) * 256);

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		for (j=start;j<=end;j++)
		{
			delta = palette[j].r-palette1[j].r;
			palette2[j].r = palette1[j].r + delta * i / steps;
			delta = palette[j].g-palette1[j].g;
			palette2[j].g = palette1[j].g + delta * i / steps;
			delta = palette[j].b-palette1[j].b;
			palette2[j].b = palette1[j].b + delta * i / steps;
		}

		/*if(!usedoublebuffering || screenBits == 8)*/ VL_WaitVBL(1);
		VL_SetPalette(palette2, true);
	}

//
// final color
//
	VL_SetPalette (palette, true);
	screenfaded = false;
}

/*
=============================================================================

							PIXEL OPS

=============================================================================
*/

/*byte *VL_LockSurface(SDL_Surface *surface)
{
    if(SDL_MUSTLOCK(surface))
    {
        if(SDL_LockSurface(surface) < 0)
            return NULL;
    }
    return (byte *) surface->pixels;
}

void VL_UnlockSurface(SDL_Surface *surface)
{
    if(SDL_MUSTLOCK(surface))
    {
        SDL_UnlockSurface(surface);
    }
}*/

/*
=================
=
= VL_Plot
=
=================
*/

void VL_Plot (int x, int y, int color)
{
#ifdef __AMIGA__
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
#ifdef USE_EHB
	if (bEHBScreen)
		color = ehbxlat[color];
#endif
	SetAPen(rp, color);
	WritePixel(rp, x, y);
#else
    assert(x >= 0 && (unsigned) x < screenWidth
            && y >= 0 && (unsigned) y < screenHeight
            && "VL_Plot: Pixel out of bounds!");

    VL_LockSurface(curSurface);
	((byte *) curSurface->pixels)[y * curPitch + x] = color;
    VL_UnlockSurface(curSurface);
#endif
}

/*
=================
=
= VL_GetPixel
=
=================
*/

byte VL_GetPixel (int x, int y)
{
#ifdef __AMIGA__
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
	byte col = (byte)ReadPixel(rp, x, y);
#ifdef USE_EHB
	if (bEHBScreen)
		col = VL_UnmapColorEHB(col);
#endif
#else
    assert_ret(x >= 0 && (unsigned) x < screenWidth
            && y >= 0 && (unsigned) y < screenHeight
            && "VL_GetPixel: Pixel out of bounds!");

    VL_LockSurface(curSurface);
	byte col = ((byte *) curSurface->pixels)[y * curPitch + x];
    VL_UnlockSurface(curSurface);
#endif

	return col;
}


/*
=================
=
= VL_Hlin
=
=================
*/

void VL_Hlin (unsigned x, unsigned y, unsigned width, int color)
{
#ifdef __AMIGA__
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
#ifdef USE_EHB
	if (bEHBScreen)
		color = ehbxlat[color];
#endif
	SetAPen(rp, color);
	RectFill(rp, x, y, x + width - 1, y);
#else
    assert(x >= 0 && x + width <= screenWidth
            && y >= 0 && y < screenHeight
            && "VL_Hlin: Destination rectangle out of bounds!");

    VL_LockSurface(curSurface);
    Uint8 *dest = ((byte *) curSurface->pixels) + y * curPitch + x;
    memset(dest, color, width);
    VL_UnlockSurface(curSurface);
#endif
}


/*
=================
=
= VL_Vlin
=
=================
*/

void VL_Vlin (int x, int y, int height, int color)
{
#ifdef __AMIGA__
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
#ifdef USE_EHB
	if (bEHBScreen)
		color = ehbxlat[color];
#endif
	SetAPen(rp, color);
	RectFill(rp, x, y, x, y + height - 1);
#else
	assert(x >= 0 && (unsigned) x < screenWidth
			&& y >= 0 && (unsigned) y + height <= screenHeight
			&& "VL_Vlin: Destination rectangle out of bounds!");

	VL_LockSurface(curSurface);
	Uint8 *dest = ((byte *) curSurface->pixels) + y * curPitch + x;

	while (height--)
	{
		*dest = color;
		dest += curPitch;
	}

	VL_UnlockSurface(curSurface);
#endif
}


/*
=================
=
= VL_Bar
=
=================
*/

void VL_Bar (int x, int y, int width, int height, int color)
{
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
#ifdef USE_EHB
	if (bEHBScreen)
		color = ehbxlat[color];
#endif
	SetAPen(rp, color);
	RectFill(rp, x, y, x + width - 1, y + height - 1);
}

/*void VL_BarScaledCoord (int scx, int scy, int scwidth, int scheight, int color)
{
	assert(scx >= 0 && (unsigned) scx + scwidth <= screenWidth
			&& scy >= 0 && (unsigned) scy + scheight <= screenHeight
			&& "VL_BarScaledCoord: Destination rectangle out of bounds!");

	VL_LockSurface(curSurface);
	Uint8 *dest = ((byte *) curSurface->pixels) + scy * curPitch + scx;

	while (scheight--)
	{
		memset(dest, color, scwidth);
		dest += curPitch;
	}

	VL_UnlockSurface(curSurface);
}
*/

/*
============================================================================

							MEMORY OPS

============================================================================
*/

/*
=================
=
= VL_MemToLatch
=
=================
*/

void VL_MemToLatch(byte *source, int width, int height,
    SDL_Surface *destSurface, int x, int y)
{
#ifdef __AMIGA__
	struct RastPort rp;
	InitRastPort(&rp);
	rp.BitMap = (struct BitMap *)destSurface;

#ifdef USE_EHB
	if (bEHBScreen)
	{
		VL_RemapBufferEHB(source, pEHBBuf, width*height);
		source = pEHBBuf;
	}
#endif
	ChunkyToRastPort(source, width, height, &rp, x, y);
	// debug
	//VL_LatchToScreen (destSurface, x, y, width, height, 30, 10);
#else
    assert(x >= 0 && (unsigned) x + width <= screenWidth
            && y >= 0 && (unsigned) y + height <= screenHeight
            && "VL_MemToLatch: Destination rectangle out of bounds!");

    VL_LockSurface(destSurface);
    int pitch = destSurface->pitch;
    byte *dest = (byte *) destSurface->pixels + y * pitch + x;
    for(int ysrc = 0; ysrc < height; ysrc++)
    {
        for(int xsrc = 0; xsrc < width; xsrc++)
        {
            dest[ysrc * pitch + xsrc] = source[(ysrc * (width >> 2) + (xsrc >> 2))
                + (xsrc & 3) * (width >> 2) * height];
        }
    }
    VL_UnlockSurface(destSurface);
#endif
}

//===========================================================================


/*
=================
=
= VL_MemToScreenScaledCoord
=
= Draws a block of data to the screen with scaling according to scaleFactor.
=
=================
*/

void VL_MemToScreen            (byte *source, int width, int height, int x, int y)
//void VL_MemToScreenScaledCoord (byte *source, int width, int height, int destx, int desty)
{//return;
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
#ifdef USE_EHB
	if (bEHBScreen && source[0] != 255)
	{
		VL_RemapBufferEHB(source, pEHBBuf, width*height);
		source = pEHBBuf;
	}
#endif
	ChunkyToRastPort(source, width, height, rp, x, y);
}

/*void VL_MemToScreenScaledCoord (byte *source, int width, int height, int destx, int desty)
{
    assert(destx >= 0 && destx + width * scaleFactor <= screenWidth
            && desty >= 0 && desty + height * scaleFactor <= screenHeight
            && "VL_MemToScreenScaledCoord: Destination rectangle out of bounds!");

    VL_LockSurface(curSurface);
    byte *vbuf = (byte *) curSurface->pixels;
    for(int j=0,scj=0; j<height; j++, scj+=scaleFactor)
    {
        for(int i=0,sci=0; i<width; i++, sci+=scaleFactor)
        {
            byte col = source[(j*(width>>2)+(i>>2))+(i&3)*(width>>2)*height];
            for(unsigned m=0; m<scaleFactor; m++)
            {
                for(unsigned n=0; n<scaleFactor; n++)
                {
                    vbuf[(scj+m+desty)*curPitch+sci+n+destx] = col;
                }
            }
        }
    }
    VL_UnlockSurface(curSurface);
}*/

/*
=================
=
= VL_MemToScreenScaledCoord
=
= Draws a part of a block of data to the screen.
= The block has the size origwidth*origheight.
= The part at (srcx, srcy) has the size width*height
= and will be painted to (destx, desty) with scaling according to scaleFactor.
=
=================
*/
/*
void VL_MemToScreenScaledCoord (byte *source, int origwidth, int origheight, int srcx, int srcy,
                                int destx, int desty, int width, int height)
{
    assert(destx >= 0 && destx + width * scaleFactor <= screenWidth
            && desty >= 0 && desty + height * scaleFactor <= screenHeight
            && "VL_MemToScreenScaledCoord: Destination rectangle out of bounds!");

    VL_LockSurface(curSurface);
    byte *vbuf = (byte *) curSurface->pixels;
    for(int j=0,scj=0; j<height; j++, scj+=scaleFactor)
    {
        for(int i=0,sci=0; i<width; i++, sci+=scaleFactor)
        {
            byte col = source[((j+srcy)*(origwidth>>2)+((i+srcx)>>2))+((i+srcx)&3)*(origwidth>>2)*origheight];
            for(unsigned m=0; m<scaleFactor; m++)
            {
                for(unsigned n=0; n<scaleFactor; n++)
                {
                    vbuf[(scj+m+desty)*curPitch+sci+n+destx] = col;
                }
            }
        }
    }
    VL_UnlockSurface(curSurface);
}
*/
#ifdef __AMIGA__
void VL_ScreenToMem (byte *dest, int width, int height, int x, int y)
{
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
	struct RastPort temprp;
	temprp = *rp;
	temprp.Layer = NULL;
	temprp.BitMap = tempbm;
	ReadPixelArray8(rp, x, y, x+width-1, y+height-1, dest, &temprp);
}
#endif
//==========================================================================

/*
=================
=
= VL_LatchToScreen
=
=================
*/

/*void VL_LatchToScreen (SDL_Surface *source, int x, int y)
{
#ifdef __AMIGA__
	//BltBitMap(screen->RastPort.BitMap, 0, 0, bitmap, 0, 0, bitmap->BytesPerRow * 8, bitmap->Rows, 0xc0, ~0, NULL);
	struct BitMap *bitmap = (struct BitMap *)source;
	BltBitMapRastPort(bitmap, 0, 0, &screen->RastPort, x, y, bitmap->BytesPerRow * 8, bitmap->Rows, 0xc0);
#else
	
#endif
}*/

void VL_LatchToScreen (SDL_Surface *source, int xsrc, int ysrc,
    int width, int height, int xdest, int ydest)
{
	struct BitMap *bitmap = (struct BitMap *)source;
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[bufferofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
	BltBitMapRastPort(bitmap, 0, 0, rp, xdest, ydest, width, height, 0xc0);
}


/*void VL_LatchToScreenScaledCoord(SDL_Surface *source, int xsrc, int ysrc,
    int width, int height, int scxdest, int scydest)
{
	assert(scxdest >= 0 && scxdest + width * scaleFactor <= screenWidth
			&& scydest >= 0 && scydest + height * scaleFactor <= screenHeight
			&& "VL_LatchToScreenScaledCoord: Destination rectangle out of bounds!");

	if(scaleFactor == 1)
    {
        // HACK: If screenBits is not 8 and the screen is faded out, the
        //       result will be black when using SDL_BlitSurface. The reason
        //       is that the logical palette needed for the transformation
        //       to the screen color depth is not equal to the logical
        //       palette of the latch (the latch is not faded). Therefore,
        //       SDL tries to map the colors...
        //       The result: All colors are mapped to black.
        //       So, we do the blit on our own...
        if(screenBits != 8)
        {
            VL_LockSurface(source);
            byte *src = (byte *) source->pixels;
            unsigned srcPitch = source->pitch;

            VL_LockSurface(curSurface);
            byte *vbuf = (byte *) curSurface->pixels;
            for(int j=0,scj=0; j<height; j++, scj++)
            {
                for(int i=0,sci=0; i<width; i++, sci++)
                {
                    byte col = src[(ysrc + j)*srcPitch + xsrc + i];
                    vbuf[(scydest+scj)*curPitch+scxdest+sci] = col;
                }
            }
            VL_UnlockSurface(curSurface);
            VL_UnlockSurface(source);
        }
        else
        {
            SDL_Rect srcrect = { xsrc, ysrc, width, height };
            SDL_Rect destrect = { scxdest, scydest, 0, 0 }; // width and height are ignored
            SDL_BlitSurface(source, &srcrect, curSurface, &destrect);
        }
    }
    else
    {
        VL_LockSurface(source);
        byte *src = (byte *) source->pixels;
        unsigned srcPitch = source->pitch;

        VL_LockSurface(curSurface);
        byte *vbuf = (byte *) curSurface->pixels;
        for(int j=0,scj=0; j<height; j++, scj+=scaleFactor)
        {
            for(int i=0,sci=0; i<width; i++, sci+=scaleFactor)
            {
                byte col = src[(ysrc + j)*srcPitch + xsrc + i];
                for(unsigned m=0; m<scaleFactor; m++)
                {
                    for(unsigned n=0; n<scaleFactor; n++)
                    {
                        vbuf[(scydest+scj+m)*curPitch+scxdest+sci+n] = col;
                    }
                }
            }
        }
        VL_UnlockSurface(curSurface);
        VL_UnlockSurface(source);
    }
}*/

//===========================================================================

/*
=================
=
= VL_ScreenToScreen
=
=================
*/

void VL_ScreenToScreen (SDL_Surface *source, SDL_Surface *dest)
{
    //SDL_BlitSurface(source, NULL, dest, NULL);
	//unused
#ifdef USE_DOUBLEBUFFER
	struct BitMap *bitmap = (struct BitMap *)source;
	struct RastPort rp;
	InitRastPort(&rp);
	rp.BitMap = (struct BitMap *)dest;
	BltBitMapRastPort(bitmap, 0, 0, &rp, 0, 0, screenWidth, screenHeight, 0xc0);
#else
	Quit("%s called!",__FUNCTION__);
#endif
}

SDL_Surface *VL_CreateSurface(int width, int height)
{
	return AllocBitMap(width, height, screen->RastPort.BitMap->Depth, BMF_INTERLEAVED, screen->RastPort.BitMap);
}

// structs by Yutaka Yasuda
typedef struct tagBITMAPFILEHEADER {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

#define BITMAPFILEHEADER_SIZE 14

typedef struct tagBITMAPINFOHEADER{
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPixPerMeter;
    int32_t  biYPixPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImporant;
} BITMAPINFOHEADER;

#define BITMAPINFOHEADER_SIZE 40

int SDL_SaveBMP(SDL_Surface* surface, const char* file)
{
#ifdef USE_DOUBLEBUFFER
	struct RastPort *rp = &rastports[displayofs];
#else
	struct RastPort *rp = &screen->RastPort;
#endif
	UBYTE *temp = calloc(screenWidth, 1);
	CHECKMALLOCRESULT(temp);
	FILE *fp = fopen(file, "wb");
	if (fp)
	{
		BITMAPFILEHEADER fileHeader;
		BITMAPINFOHEADER infoHeader;
		uint32_t colors = /*bEHBScreen ? 64 :*/ 256; // TODO fixme

		fileHeader.bfType = (0x42 << 8) | 0x4D; // "BM"
		fileHeader.bfReserved1 = 0;
		fileHeader.bfReserved2 = 0;
		fileHeader.bfOffBits = BITMAPFILEHEADER_SIZE + BITMAPINFOHEADER_SIZE + colors*4;
		fileHeader.bfSize = fileHeader.bfOffBits + screenWidth*screenHeight;
		fileHeader.bfSize = SWAP16LE(fileHeader.bfSize);
		fileHeader.bfOffBits = SWAP32LE(fileHeader.bfOffBits);
		fwrite(&fileHeader, BITMAPFILEHEADER_SIZE, 1, fp);
		
		infoHeader.biSize = SWAP32LE(BITMAPINFOHEADER_SIZE);
		infoHeader.biWidth = SWAP32LE(screenWidth);
		infoHeader.biHeight = SWAP32LE(screenHeight);
		infoHeader.biPlanes = SWAP16LE(1);
		infoHeader.biBitCount = SWAP16LE(8);
		infoHeader.biCompression = 0;
		infoHeader.biSizeImage = 0; // only for compressed?
		infoHeader.biXPixPerMeter = 0;
		infoHeader.biYPixPerMeter = 0;
		infoHeader.biClrUsed = colors;
		infoHeader.biClrImporant = 0;
		fwrite(&infoHeader, BITMAPINFOHEADER_SIZE, 1, fp);

		for (int i = 0; i < colors; i++)
		{
			fputc(curpal[i].b, fp);
			fputc(curpal[i].g, fp);
			fputc(curpal[i].r, fp);
			fputc(0, fp);
		}

		struct RastPort temprp;
		temprp = *rp;
		temprp.Layer = NULL;
		temprp.BitMap = tempbm;
		for (int y = 0; y < screenHeight; y++)
		{
			if (temprp.BitMap)
			{
				ReadPixelLine8(rp, 0, screenHeight-y-1, screenWidth, temp, &temprp);
			}
			else
			{
				for (int x = 0; x < screenWidth; x++)
					temp[x] = ReadPixel(rp, x, screenHeight-y-1);
			}
			fwrite(temp, screenWidth, 1, fp);
		}

		fclose(fp);
	}
	free(temp);

	return 0;
}

/*
#include <stdarg.h>
void BE_ST_DebugText(int x, int y, const char *fmt, ...)
{
	UBYTE buffer[256];
	va_list ap;
	struct RastPort *rp = window->RPort;

	va_start(ap, fmt); 
	vsnprintf((char *)buffer, sizeof(buffer), fmt, ap);
	va_end(ap);
	SetAPen(rp, 247);
	Move(rp, x + window->BorderLeft, y + rp->Font->tf_Baseline + window->BorderTop);
	Text(rp, buffer, strlen((char *)buffer));
}
*/

/*
extern struct Custom custom;

void BE_ST_DebugColor(unsigned color)
{
#if 0
	UWORD colors[16] = {0x0000,0x000A,0x00A0,0x00AA,0x0A00,0x0A0A,0x0A50,0x0AAA,
						0x0555,0x055F,0x05F5,0x05FF,0x0F55,0x0F5F,0x0FF5,0x0FFF};
	custom.color[0] = colors[color%16];
#endif
}
*/

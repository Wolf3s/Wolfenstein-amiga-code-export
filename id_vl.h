// ID_VL.H

// wolf compatability

void Quit (const char *error,...);

//===========================================================================

//#define SCREENWIDTH		80			// default screen width in bytes
#define SCREENWIDTH	320			// default screen width in pixels
#define MAXSCANLINES	200			// size of ylookup table

#define CHARWIDTH		2
#define TILEWIDTH		4

//===========================================================================


/*#ifdef __AMIGA__
extern SDL_Surface *screenBuffer, *curSurface;
#else
extern SDL_Surface *screen, *screenBuffer, *curSurface;
#endif

extern  boolean  fullscreen, usedoublebuffering;*/
#ifdef __AMIGA__
#define screenWidth 320
#define screenHeight 200
#define screenBits 8
#define scaleFactor 1
//extern  unsigned screenPitch, bufferPitch, curPitch;
SDL_Surface *VL_CreateSurface(int width, int height);
void VL_RemapBufferEHB(byte *source, byte *dest, int size);
boolean VL_RemapBufferCustomEHB(byte *source, int size, SDL_Color *palette);
byte VL_RemapColorEHB(byte color);
#else
extern  unsigned screenWidth, screenHeight, screenBits, screenPitch, bufferPitch, curPitch;
extern  unsigned scaleFactor;
#endif/**/

extern	unsigned	bufferofs;			// all drawing is reletive to this
extern	unsigned	displayofs,pelpan;	// last setscreen coordinates


//extern	unsigned	screenseg;			// set to 0xa000 for asm convenience

//extern	unsigned	linewidth;

extern uint8_t *chunkyBuffer; // allocate dynamically in NewViewSize/SetViewSize
// viewscreenx, viewscreeny, viewwidth, viewheight
//extern unsigned g_currentBitMap;

extern	unsigned	ylookup[MAXSCANLINES];

extern	boolean  screenfaded;
extern	unsigned bordercolor;

extern SDL_Color gamepal[256];
//extern byte *gamepal;

//===========================================================================

//
// VGA hardware routines
//

#ifdef __AMIGA__
void VL_ReAllocChunkyBuffer(void);
void VL_DrawChunkyBuffer(void);
void VL_ChangeScreenBuffer(void);
void VL_WaitVBL(int vbls);
#else
#define VL_WaitVBL(a) SDL_Delay((a)*8)
#endif

void VL_SetVGAPlaneMode (void);
void VL_SetTextMode (void);
void VL_Shutdown (void);

void VL_ConvertPalette(byte *srcpal, SDL_Color *destpal, int numColors);
void VL_FillPalette (int red, int green, int blue);
void VL_SetColor    (int color, int red, int green, int blue);
void VL_GetColor    (int color, int *red, int *green, int *blue);
void VL_SetPalette  (SDL_Color *palette, bool forceupdate);
void VL_GetPalette  (SDL_Color *palette);
/*void VL_SetPalette (byte *palette);
void VL_GetPalette (byte *palette);*/
void VL_FadeOut     (int start, int end, int red, int green, int blue, int steps);
void VL_FadeIn      (int start, int end, SDL_Color *palette, int steps);
//void VL_FadeIn (int start, int end, byte *palette, int steps);

/*#ifdef __AMIGA__
#define LOCK() vga_memory
#define UNLOCK()
#else
byte *VL_LockSurface(SDL_Surface *surface);
void VL_UnlockSurface(SDL_Surface *surface);

#define LOCK() VL_LockSurface(curSurface)
#define UNLOCK() VL_UnlockSurface(curSurface)
#endif*/

byte VL_GetPixel        (int x, int y);
void VL_Plot            (int x, int y, int color);
void VL_Hlin            (unsigned x, unsigned y, unsigned width, int color);
void VL_Vlin            (int x, int y, int height, int color);
void VL_Bar      (int x, int y, int width, int height, int color);
#define VL_BarScaledCoord VL_Bar
/*void VL_BarScaledCoord  (int scx, int scy, int scwidth, int scheight, int color);
#ifdef __AMIGA__
static
#endif
void inline VL_Bar      (int x, int y, int width, int height, int color)
{
    VL_BarScaledCoord(scaleFactor*x, scaleFactor*y,
        scaleFactor*width, scaleFactor*height, color);
}*/
/*#ifdef __AMIGA__
static
#endif
void inline VL_ClearScreen(int color)
{
    SDL_FillRect(curSurface, NULL, color);
}*/

void VL_MungePic                (byte *source, unsigned width, unsigned height);
void VL_DrawPicBare             (int x, int y, byte *pic, int width, int height);
void VL_MemToLatch              (byte *source, int width, int height,
                                    SDL_Surface *destSurface, int x, int y);
void VL_ScreenToScreen          (SDL_Surface *source, SDL_Surface *dest);
//void VL_MemToLatch (byte *source, int width, int height, unsigned dest);
//void VL_ScreenToScreen (unsigned source, unsigned dest,int width, int height);
void VL_MemToScreen (byte *source, int width, int height, int x, int y);
/*void VL_MemToScreenScaledCoord  (byte *source, int width, int height, int scx, int scy);
void VL_MemToScreenScaledCoord  (byte *source, int origwidth, int origheight, int srcx, int srcy,
                                    int destx, int desty, int width, int height);

#ifdef __AMIGA__
static
#endif
void inline VL_MemToScreen (byte *source, int width, int height, int x, int y)
{
#ifdef __AMIGA__
    // TODO fixme

#else
    VL_MemToScreenScaledCoord(source, width, height,
        scaleFactor*x, scaleFactor*y);
#endif
}*/
#ifdef __AMIGA__
void VL_ScreenToMem (byte *dest, int width, int height, int x, int y);
#endif

void VL_MaskedToScreen (byte *source, int width, int height, int x, int y);

//void VL_LatchToScreen (unsigned source, int width, int height, int x, int y);
//void VL_LatchToScreen (SDL_Surface *source, int x, int y);
void VL_LatchToScreen (SDL_Surface *source, int xsrc, int ysrc,
    int width, int height, int xdest, int ydest);

/*void VL_LatchToScreenScaledCoord (SDL_Surface *source, int xsrc, int ysrc,
    int width, int height, int scxdest, int scydest);

#ifdef __AMIGA__
static
#endif
void inline VL_LatchToScreen (SDL_Surface *source, int xsrc, int ysrc,
    int width, int height, int xdest, int ydest)
{
    VL_LatchToScreenScaledCoord(source,xsrc,ysrc,width,height,
        scaleFactor*xdest,scaleFactor*ydest);
}*/
/*
#ifdef __AMIGA__
static
#endif
void inline VL_LatchToScreenScaledCoord (SDL_Surface *source, int scx, int scy)
{
    VL_LatchToScreenScaledCoord(source,0,0,source->w,source->h,scx,scy);
}*/
/*
#ifdef __AMIGA__
static
#endif
void inline VL_LatchToScreen (SDL_Surface *source, int x, int y)
{
    VL_LatchToScreenScaledCoord(source,0,0,source->w,source->h,
        scaleFactor*x,scaleFactor*y);
}*/

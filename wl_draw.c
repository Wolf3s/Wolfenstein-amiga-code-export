// WL_DRAW.C

#include "wl_def.h"
#pragma hdrstop

//#define DEBUGWALLS

#include "wl_cloudsky.h"
#include "wl_atmos.h"
#include "wl_shade.h"

/*
=============================================================================

                               LOCAL CONSTANTS

=============================================================================
*/

// the door is the last picture before the sprites
#define DOORWALL        (PMSpriteStart-8)

#define ACTORSIZE       0x4000

//#define RAYCASTFAST // use 8.8 bit precision
//#define DOUBLEHORIZ // doubles the wall pixels horizontally

/*
=============================================================================

                              GLOBAL VARIABLES

=============================================================================
*/

#ifdef __AMIGA__
#define vbuf chunkyBuffer
#define vbufPitch viewwidth
#else
static byte *vbuf = NULL;
unsigned vbufPitch = 0;
#endif

int32_t    lasttimecount;
int32_t    frameon;
#ifdef __AMIGA__
#include <sys/time.h>
//#define FRAME_TIME
#endif
boolean fpscounter;
#ifdef USE_AUTOMAP
boolean automap;
#endif

int fps_frames=0, fps_time=0, fps=0;

#ifdef __AMIGA__
//int wallheight[MAXVIEWWIDTH];
word wallheight[MAXVIEWWIDTH];
#ifdef DOUBLEHORIZ
int fencepost; // in case the raycaster does something stupid in the double horizontal mode
#endif
#else
int *wallheight;
int min_wallheight;
#endif
unsigned int maxz = 0; // TODO remove


//
// math tables
//
#ifdef __AMIGA__
short pixelangle[MAXVIEWWIDTH];
#define far
#include "tables.h"
#else
short *pixelangle;
int32_t finetangent[FINEANGLES/4];
fixed sintable[ANGLES+ANGLES/4];
fixed *costable = sintable+(ANGLES/4);
#endif


//
// refresh variables
//
fixed   viewx,viewy;                    // the focal point
short   viewangle;
fixed   viewsin,viewcos;

void    TransformActor (objtype *ob);
void    BuildTables (void);
void    ClearScreen (void);
int     CalcRotate (objtype *ob);
void    DrawScaleds (void);
void    CalcTics (void);
void    ThreeDRefresh (void);



//
// wall optimization variables
//
int     lastside;               // true for vertical
int32_t    lastintercept;
int     lasttilehit;
int     lasttexture;

//
// ray tracing variables
//
short    focaltx,focalty,viewtx,viewty;
#ifdef RAYCASTFAST
word xpartialup,xpartialdown,ypartialup,ypartialdown;
#else
longword xpartialup,xpartialdown,ypartialup,ypartialdown;
#endif

#ifdef RAYCASTFAST
#define FASTSHIFT 8
#define FASTGLOBAL (1<<FASTSHIFT)
static inline int16_t FixedByFrac(int16_t a, int16_t b)
{
	return (int16_t)(((int32_t)a * (int32_t)b) >> FASTSHIFT);
}
#endif

short   midangle,angle;

word    tilehit;
int     pixx;

short   xtile,ytile;
short   xtilestep,ytilestep;
#ifdef RAYCASTFAST
int16_t    xintercept,yintercept;
#else
int32_t    xintercept,yintercept;
#endif
#ifndef __AMIGA__
word    xstep,ystep;
word    xspot,yspot;
int     texdelta;
#endif
#ifdef DOUBLEHORIZ
short lowdetail = 1;
#endif

word horizwall[MAXWALLTILES],vertwall[MAXWALLTILES];


/*
============================================================================

                           3 - D  DEFINITIONS

============================================================================
*/

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy               : globalx/globaly of point
=
= globals:
=   viewx,viewy         : point of view
=   viewcos,viewsin     : sin/cos of viewangle
=   scale               : conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/


//
// transform actor
//
void TransformActor (objtype *ob)
{
    fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
    gx = ob->x-viewx;
    gy = ob->y-viewy;

//
// calculate newx
//
    gxt = FixedMul(gx,viewcos);
    gyt = FixedMul(gy,viewsin);
    nx = gxt-gyt-ACTORSIZE;         // fudge the shape forward a bit, because
                                    // the midpoint could put parts of the shape
                                    // into an adjacent wall

//
// calculate newy
//
    gxt = FixedMul(gx,viewsin);
    gyt = FixedMul(gy,viewcos);
    ny = gyt+gxt;

//
// calculate perspective ratio
//
    ob->transx = nx;
    ob->transy = ny;

    if (nx<MINDIST)                 // too close, don't overflow the divide
    {
        ob->viewheight = 0;
        return;
    }

    ob->viewx = (word)(centerx + ny*scale/nx);

//
// calculate height (heightnumerator/(nx>>8))
//
#ifdef __AMIGA__
    ob->viewheight = heightnumdiv[nx>>8];
#else
    ob->viewheight = (word)(heightnumerator/(nx>>8));
	if ((nx >> 8) > maxz)
	{
		maxz = (nx >> 8);
		printf("%s maxz %u\n", __FUNCTION__, maxz);
	}
#endif
}

//==========================================================================

/*
========================
=
= TransformTile
=
= Takes paramaters:
=   tx,ty               : tile the object is centered in
=
= globals:
=   viewx,viewy         : point of view
=   viewcos,viewsin     : sin/cos of viewangle
=   scale               : conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
= Returns true if the tile is withing getting distance
=
========================
*/

boolean TransformTile (int tx, int ty, short *dispx, short *dispheight)
{
    fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
    gx = ((int32_t)tx<<TILESHIFT)+0x8000-viewx;
    gy = ((int32_t)ty<<TILESHIFT)+0x8000-viewy;

//
// calculate newx
//
    gxt = FixedMul(gx,viewcos);
    gyt = FixedMul(gy,viewsin);
    nx = gxt-gyt-0x2000;            // 0x2000 is size of object

//
// calculate newy
//
    gxt = FixedMul(gx,viewsin);
    gyt = FixedMul(gy,viewcos);
    ny = gyt+gxt;


//
// calculate height / perspective ratio
//
    if (nx<MINDIST)                 // too close, don't overflow the divide
        *dispheight = 0;
    else
    {
        *dispx = (short)(centerx + ny*scale/nx);
#ifdef __AMIGA__
        *dispheight = heightnumdiv[nx>>8];
#else
        *dispheight = (short)(heightnumerator/(nx>>8));
	if ((nx >> 8) > maxz)
	{
		maxz = (nx >> 8);
		printf("%s maxz %u\n", __FUNCTION__, maxz);
	}
#endif
    }

//
// see if it should be grabbed
//
    if (nx<TILEGLOBAL && ny>-TILEGLOBAL/2 && ny<TILEGLOBAL/2)
        return true;
    else
        return false;
}

//==========================================================================

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

#ifdef __AMIGA__
static inline
#endif
int CalcHeight()
{
#ifdef RAYCASTFAST
    // TODO viewx viewy
	// TODO viewcos viewsin
#if 1
	fixed z = FixedMul((xintercept<<(16-FASTSHIFT)) - viewx, viewcos)
        - FixedMul((yintercept<<(16-FASTSHIFT)) - viewy, viewsin);
    if(z < MINDIST) z = MINDIST;
    int height = heightnumerator / (z >> 8);
#else
    int16_t z = FixedByFrac(xintercept - (viewx >> 8), viewcos >> 8)
        - FixedByFrac(yintercept - (viewy >> 8), viewsin >> 8);
    if(z < MINDIST>>8) z = MINDIST>>8;
    int height = heightnumdiv[z];
#endif
#else
    fixed z = FixedMul(xintercept - viewx, viewcos)
        - FixedMul(yintercept - viewy, viewsin);
    if(z < MINDIST) z = MINDIST;
#ifdef __AMIGA__
    int height = heightnumdiv[z>>8];
#else
    int height = heightnumerator / (z >> 8);
	if ((z >> 8) > maxz)
	{
		maxz = (z >> 8);
		printf("%s maxz %u\n", __FUNCTION__, maxz);
	}
#endif
#endif
#ifndef __AMIGA__
    if(height < min_wallheight) min_wallheight = height;
#endif
    return height;
}

//==========================================================================

/*
===================
=
= ScalePost
=
===================
*/

byte *postsource;
#ifdef __AMIGA__
word postx;
word postwidth;
#else
int postx;
int postwidth;
#endif

#ifdef __AMIGA__
static word dc_iscale;
static word dc_frac;
static uint16_t dc_source;
//static uint8_t *dc_seg;
#define dc_seg postsource
//static int dc_length;
static word dc_length;
static int dc_dest;
//static word dc_dest;
//static int dc_width;
#define dc_width postwidth
word scalediv[MAXSCALEHEIGHT*2]; 

static void R_DrawColumn(void)
{
	word frac = dc_frac;
	word iscale = dc_iscale;
	uint8_t *source = dc_seg + dc_source;
	uint8_t *destPtr = &chunkyBuffer[dc_dest];
	int16_t count = dc_length;
	uint8_t pixel;
	uint16_t vga_width = viewwidth;

	switch (dc_width)
	{
		case 1:
#if 1
			do
			{
				*destPtr = source[frac >> 10];
				if (!(--count)) break;
				destPtr += vga_width;
				frac += iscale;
			} while (1);
#else
			do
			{
				*destPtr = source[frac >> 10];
				destPtr += vga_width;
				frac += iscale;
			} while (--count);
#endif
			break;

		case 2:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-1;
				frac += iscale;
			} while (--count);
			break;

		case 3:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-2;
				frac += iscale;
			} while (--count);
			break;

		case 4:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-3;
				frac += iscale;
			} while (--count);
			break;
#ifndef USE_HALFWIDTH
		case 5:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-4;
				frac += iscale;
			} while (--count);
			break;

		case 6:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-5;
				frac += iscale;
			} while (--count);
			break;

		case 7:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-6;
				frac += iscale;
			} while (--count);
			break;

		case 8:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-7;
				frac += iscale;
			} while (--count);
			break;

		case 9:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-8;
				frac += iscale;
			} while (--count);
			break;

		case 10:
			do
			{
				pixel = source[frac >> 10];
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr++ = pixel;
				*destPtr = pixel;
				destPtr += vga_width-9;
				frac += iscale;
			} while (--count);
			break;
#endif

		default:
			//Quit("%s dc_width %d\n", __FUNCTION__, dc_width);
			break;
	}
}

#if 0
#define SCALE_PIXEL \
	*destPtr = source[frac >> 10];
#define SCALE_PIXEL_ADVANCE \
	SCALE_PIXEL \
	destPtr += vga_width; \
	frac += iscale;
#define SCALE_PIXEL_PAIR \
	SCALE_PIXEL_ADVANCE \
	SCALE_PIXEL_ADVANCE
#define SCALE_PIXEL_PAIR_LAST \
	SCALE_PIXEL_ADVANCE \
	SCALE_PIXEL
#define SCALE_CASE(p) \
	case (p): \
	SCALE_PIXEL_PAIR
#define SCALE_CASE_LAST(p) \
	case (p): \
	SCALE_PIXEL_PAIR_LAST

static void R_DrawColumnWall(void)
{
	word frac = dc_frac;
	word iscale = dc_iscale;
	uint8_t *source = dc_seg + dc_source;
	uint8_t *destPtr = &chunkyBuffer[dc_dest];
	int16_t count = dc_length/2;
	//uint8_t pixel;
	uint16_t vga_width = viewwidth;

	//switch (count)
	{
		/*SCALE_CASE(32)
		SCALE_CASE(31)
		SCALE_CASE(30)
		SCALE_CASE(29)
		SCALE_CASE(28)
		SCALE_CASE(27)
		SCALE_CASE(26)
		SCALE_CASE(25)
		SCALE_CASE(24)
		SCALE_CASE(23)
		SCALE_CASE(22)
		SCALE_CASE(21)
		SCALE_CASE(20)
		SCALE_CASE(19)
		SCALE_CASE(18)
		SCALE_CASE(17)
		SCALE_CASE(16)
		SCALE_CASE(15)
		SCALE_CASE(14)
		SCALE_CASE(13)
		SCALE_CASE(12)
		SCALE_CASE(11)
		SCALE_CASE(10)
		SCALE_CASE(9)
		SCALE_CASE(8)
		SCALE_CASE(7)
		SCALE_CASE(6)
		SCALE_CASE(5)
		SCALE_CASE(4)
		SCALE_CASE(3)
		SCALE_CASE(2)
		SCALE_CASE_LAST(1)
		break;
		default:*/
			do
			{
				SCALE_PIXEL_PAIR
			} while (--count);
			//break;
	}
}
#endif

static uint16_t *linecmds;
static int16_t dc_sprtopoffset;
static word dc_screenstep;
static uint16_t dc_x;

static void generic_scale_masked_post(void)
{
	const uint16_t *srcpost = linecmds;

	word screenstep = dc_screenstep;
	int16_t sprtopoffset = dc_sprtopoffset;

	uint16_t end = (*srcpost) / 2;
	srcpost++;

	uint16_t buf = dc_x;
	uint16_t vga_width = viewwidth;

	while (end != 0)
	{
		dc_source = *srcpost;
		srcpost++;

		uint16_t start = *srcpost / 2;
		srcpost++;

		dc_source += start;

		int16_t length = end - start;
		int16_t topscreen = sprtopoffset + (screenstep * start);
		int16_t bottomscreen = topscreen + (screenstep * length);
		int16_t dc_yl = (topscreen + 0x80 - 1) >> 7;
		int16_t dc_yh = (bottomscreen - 1) >> 7;

		if (dc_yh >= viewheight)
			dc_yh = viewheight - 1;

		if (dc_yl < 0)
		{
			dc_frac = dc_iscale * (word)(-dc_yl);
			dc_yl = 0;
		}
		else
		{
			dc_frac = 0;
		}

		if (dc_yl <= dc_yh)
		{
			//dc_dest = buf + ylookup[dc_yl];
			dc_dest = buf + vga_width*dc_yl;
			dc_length = dc_yh - dc_yl + 1;
			R_DrawColumn();
		}

		end = *srcpost / 2;
		srcpost++;
	}
}
#endif

#ifdef __AMIGA__
static inline
#endif
void ScalePost()
{
#ifdef __AMIGA__
#ifdef COMPILED_SCALERS
	// try the compiler scalers
	int postheight = wallheight[postx] >> 2; // TODO
	if (postwidth == 1 && postheight < (uint16_t)maxscale)
	{
		t_compscale *compscaler = scaledirectory[postheight];
		uint8_t *src = postsource;
		uint8_t *dest = &chunkyBuffer[postx + (viewheight/2) * viewwidth];
		void (*scalefunc)(uint8_t *src __asm("a0"), uint8_t *dest __asm("a1")) = (void *)compscaler;
		scalefunc(src, dest);
		return;
	}
#endif

	dc_length = wallheight[postx] >> 2;
	dc_iscale = scalediv[dc_length];

	if (dc_length > viewheight)
	{
		// TODO use a lookup table for the fractional part?
		dc_frac = (dc_length-viewheight)/2*dc_iscale;
		dc_length = viewheight;
		dc_dest = postx;
	}
	else
	{
		int toppix = (viewheight-dc_length)/2;
		dc_frac = 0;
		dc_dest = (toppix*viewwidth)+postx;
	}

	//dc_seg = postsource;
	dc_source = 0;
	//dc_width = postwidth;
	/*if (postwidth == 1 && (dc_length & 1) == 0) R_DrawColumnWall();
	else*/
	R_DrawColumn();
#else
    int ywcount, yoffs, yw, yd, yendoffs;
    byte col;

#ifdef USE_SHADING
    byte *curshades = shadetable[GetShade(wallheight[postx])];
#endif

    ywcount = yd = wallheight[postx] >> 3;
    if(yd <= 0) yd = 100;

    yoffs = (viewheight / 2 - ywcount) * vbufPitch;
    if(yoffs < 0) yoffs = 0;
    yoffs += postx;

    yendoffs = viewheight / 2 + ywcount - 1;
    yw=TEXTURESIZE-1;

    while(yendoffs >= viewheight)
    {
        ywcount -= TEXTURESIZE/2;
        while(ywcount <= 0)
        {
            ywcount += yd;
            yw--;
        }
        yendoffs--;
    }
    if(yw < 0) return;

#ifdef USE_SHADING
    col = curshades[postsource[yw]];
#else
    col = postsource[yw];
#endif
    yendoffs = yendoffs * vbufPitch + postx;
    while(yoffs <= yendoffs)
    {
        vbuf[yendoffs] = col;
        ywcount -= TEXTURESIZE/2;
        if(ywcount <= 0)
        {
            do
            {
                ywcount += yd;
                yw--;
            }
            while(ywcount <= 0);
            if(yw < 0) break;
#ifdef USE_SHADING
            col = curshades[postsource[yw]];
#else
            col = postsource[yw];
#endif
        }
        yendoffs -= vbufPitch;
    }
#endif
}

#ifndef __AMIGA__
void GlobalScalePost(byte *vidbuf, unsigned pitch)
{
    vbuf = vidbuf;
    vbufPitch = pitch;
    ScalePost();
}
#endif

#if 0
#define HitVertWall()
#define HitHorizWall()
#define HitVertDoor()
#define HitHorizDoor()
#else
/*
====================
=
= HitVertWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

#ifdef __AMIGA__
static inline
#endif
void HitVertWall (void)
{
    int wallpic;
    int texture;

#ifdef RAYCASTFAST
    //texture = (yintercept<<TEXTUREFROMFIXEDSHIFT)&TEXTUREMASK;
    texture = (yintercept<<((TEXTURESHIFT*2)-FASTSHIFT))&TEXTUREMASK;
#else
    texture = ((yintercept/*+texdelta*/)>>TEXTUREFROMFIXEDSHIFT)&TEXTUREMASK;
#endif
    if (xtilestep == -1)
    {
        texture = TEXTUREMASK-texture;
#ifdef RAYCASTFAST
        xintercept += FASTGLOBAL;
#else
        xintercept += TILEGLOBAL;
#endif
    }

    if(lastside==1 && lastintercept==xtile && lasttilehit==tilehit /*&& !(lasttilehit & 0x40)*/)
    {
        if(/*(pixx&3) &&*/ texture == lasttexture)
        {
			/*
            ScalePost();
            postx = pixx;
			*/
            wallheight[pixx] = wallheight[pixx-1];
			postwidth++;
            return;
        }
        ScalePost();
        wallheight[pixx] = CalcHeight();
        postsource+=texture-lasttexture;
        postwidth=1;
        postx=pixx;
        lasttexture=texture;
        return;
    }

    if(lastside!=-1) ScalePost();

    lastside=1;
    lastintercept=xtile;
    lasttilehit=tilehit;
    lasttexture=texture;
    wallheight[pixx] = CalcHeight();
    postx = pixx;
    postwidth=1;

    if (tilehit & 0x40)
    {                                                               // check for adjacent doors
#ifdef RAYCASTFAST
        short ytile = yintercept>>FASTSHIFT;
#else
        short ytile = (short)(yintercept>>TILESHIFT);
#endif
        if ( tilemap[xtile-xtilestep][ytile]&0x80 )
            wallpic = DOORWALL+3;
        else
            wallpic = vertwall[tilehit & ~0x40];
    }
    else
        wallpic = vertwall[tilehit];

    postsource = PM_GetTexture(wallpic) + texture;
}


/*
====================
=
= HitHorizWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

#ifdef __AMIGA__
static inline
#endif
void HitHorizWall (void)
{
    int wallpic;
    int texture;

#ifdef RAYCASTFAST
    texture = (xintercept<<((TEXTURESHIFT*2)-FASTSHIFT))&TEXTUREMASK;
#else
    texture = ((xintercept/*+texdelta*/)>>TEXTUREFROMFIXEDSHIFT)&TEXTUREMASK;
#endif
    if (ytilestep == -1)
#ifdef RAYCASTFAST
        yintercept += FASTGLOBAL;
#else
        yintercept += TILEGLOBAL;
#endif
    else
        texture = TEXTUREMASK-texture;

    if(lastside==0 && lastintercept==ytile && lasttilehit==tilehit /*&& !(lasttilehit & 0x40)*/)
    {
        if(/*(pixx&3) &&*/ texture == lasttexture)
        {
			/*
            ScalePost();
            postx=pixx;
			*/
            wallheight[pixx] = wallheight[pixx-1];
			postwidth++;
            return;
        }
        ScalePost();
        wallheight[pixx] = CalcHeight();
        postsource+=texture-lasttexture;
        postwidth=1;
        postx=pixx;
        lasttexture=texture;
        return;
    }

    if(lastside!=-1) ScalePost();

    lastside=0;
    lastintercept=ytile;
    lasttilehit=tilehit;
    lasttexture=texture;
    wallheight[pixx] = CalcHeight();
    postx = pixx;
    postwidth=1;

    if (tilehit & 0x40)
    {                                                               // check for adjacent doors
#ifdef RAYCASTFAST
        short xtile = xintercept>>FASTSHIFT;
#else
        short xtile = (short)(xintercept>>TILESHIFT);
#endif
        if ( tilemap[xtile][ytile-ytilestep]&0x80)
            wallpic = DOORWALL+2;
        else
            wallpic = horizwall[tilehit & ~0x40];
    }
    else
        wallpic = horizwall[tilehit];

    postsource = PM_GetTexture(wallpic) + texture;
}

//==========================================================================

/*
====================
=
= HitHorizDoor
=
====================
*/

#ifdef __AMIGA__
static inline
#endif
void HitHorizDoor (void)
{
    int doorpage;
    int doornum;
    int texture;

    doornum = tilehit&0x7f;
#ifdef RAYCASTFAST
    texture = ((xintercept-(doorposition[doornum]>>(16-FASTSHIFT)))<<((TEXTURESHIFT*2)-FASTSHIFT))&TEXTUREMASK;
#else
    texture = ((xintercept-doorposition[doornum])>>TEXTUREFROMFIXEDSHIFT)&TEXTUREMASK;
#endif

    if(lasttilehit==tilehit)
    {
        if(/*(pixx&3) &&*/ texture == lasttexture)
        {
			/*
            ScalePost();
            postx=pixx;
			*/
            wallheight[pixx] = wallheight[pixx-1];
			postwidth++;
            return;
        }
        ScalePost();
        wallheight[pixx] = CalcHeight();
        postsource+=texture-lasttexture;
        postwidth=1;
        postx=pixx;
        lasttexture=texture;
        return;
    }

    if(lastside!=-1) ScalePost();

    lastside=2;
    lasttilehit=tilehit;
    lasttexture=texture;
    wallheight[pixx] = CalcHeight();
    postx = pixx;
    postwidth=1;

    switch(doorobjlist[doornum].lock)
    {
#ifdef __AMIGA__
        default: // shut up the compiler
#endif
        case dr_normal:
            doorpage = DOORWALL;
            break;
        case dr_lock1:
        case dr_lock2:
        case dr_lock3:
        case dr_lock4:
            doorpage = DOORWALL+6;
            break;
        case dr_elevator:
            doorpage = DOORWALL+4;
            break;
    }

    postsource = PM_GetTexture(doorpage) + texture;
}

//==========================================================================

/*
====================
=
= HitVertDoor
=
====================
*/

#ifdef __AMIGA__
static inline
#endif
void HitVertDoor (void)
{
    int doorpage;
    int doornum;
    int texture;

    doornum = tilehit&0x7f;
#ifdef RAYCASTFAST
    texture = ((yintercept-(doorposition[doornum]>>(16-FASTSHIFT)))<<((TEXTURESHIFT*2)-FASTSHIFT))&TEXTUREMASK;
#else
    texture = ((yintercept-doorposition[doornum])>>TEXTUREFROMFIXEDSHIFT)&TEXTUREMASK;
#endif

    if(lasttilehit==tilehit)
    {
        if(/*(pixx&3) &&*/ texture == lasttexture)
        {
			/*
            ScalePost();
            postx=pixx;
			*/
            wallheight[pixx] = wallheight[pixx-1];
			postwidth++;
            return;
        }
        ScalePost();
        wallheight[pixx] = CalcHeight();
        postsource+=texture-lasttexture;
        postwidth=1;
        postx=pixx;
        lasttexture=texture;
        return;
    }

    if(lastside!=-1) ScalePost();

    lastside=2;
    lasttilehit=tilehit;
    lasttexture=texture;
    wallheight[pixx] = CalcHeight();
    postx = pixx;
    postwidth=1;

    switch(doorobjlist[doornum].lock)
    {
#ifdef __AMIGA__
        default: // shut up the compiler
#endif
        case dr_normal:
            doorpage = DOORWALL+1;
            break;
        case dr_lock1:
        case dr_lock2:
        case dr_lock3:
        case dr_lock4:
            doorpage = DOORWALL+7;
            break;
        case dr_elevator:
            doorpage = DOORWALL+5;
            break;
    }

    postsource = PM_GetTexture(doorpage) + texture;
}
#endif

//==========================================================================

#define HitHorizBorder HitHorizWall
#define HitVertBorder HitVertWall

//==========================================================================

byte vgaCeiling[]=
{
#ifndef SPEAR
 0x1d,0x1d,0x1d,0x1d,0x1d,0x1d,0x1d,0x1d,0x1d,0xbf,
 0x4e,0x4e,0x4e,0x1d,0x8d,0x4e,0x1d,0x2d,0x1d,0x8d,
 0x1d,0x1d,0x1d,0x1d,0x1d,0x2d,0xdd,0x1d,0x1d,0x98,

 0x1d,0x9d,0x2d,0xdd,0xdd,0x9d,0x2d,0x4d,0x1d,0xdd,
 0x7d,0x1d,0x2d,0x2d,0xdd,0xd7,0x1d,0x1d,0x1d,0x2d,
 0x1d,0x1d,0x1d,0x1d,0xdd,0xdd,0x7d,0xdd,0xdd,0xdd
#else
 0x6f,0x4f,0x1d,0xde,0xdf,0x2e,0x7f,0x9e,0xae,0x7f,
 0x1d,0xde,0xdf,0xde,0xdf,0xde,0xe1,0xdc,0x2e,0x1d,0xdc
#endif
};

/*
=====================
=
= VGAClearScreen
=
=====================
*/

void VGAClearScreen (void)
{
#ifdef __AMIGA__
	uint8_t ccol = vgaCeiling[gamestate.episode*10+mapon];
	uint8_t fcol = 0x19;
	ccol = VL_RemapColorEHB(ccol);
	fcol = VL_RemapColorEHB(fcol);

	/*memset(ptr, ccol, viewwidth * (viewheight/2));
	ptr += viewwidth * (viewheight/2);
	memset(ptr, fcol, viewwidth * (viewheight/2));*/
	uint32_t *ptr = (uint32_t *)vbuf;
	uint32_t ceiling = ((uint32_t)ccol << 24) | ((uint32_t)ccol << 16) | ((uint32_t)ccol << 8) | (uint32_t)ccol;
	uint32_t floor = ((uint32_t)fcol << 24) | ((uint32_t)fcol << 16) | ((uint32_t)fcol << 8) | (uint32_t)fcol;
	int16_t count = viewwidth * (viewheight/2)/4;
	for (int16_t i = 0; i < count; i++)
	{
		*ptr++ = ceiling;
	}
	for (int16_t i = 0; i < count; i++)
	{
		*ptr++ = floor;
	}
#else
    byte ceiling=vgaCeiling[gamestate.episode*10+mapon];

    int y;
    byte *ptr = vbuf;
#ifdef USE_SHADING
    for(y = 0; y < viewheight / 2; y++, ptr += vbufPitch)
        memset(ptr, shadetable[GetShade((viewheight / 2 - y) << 3)][ceiling], viewwidth);
    for(; y < viewheight; y++, ptr += vbufPitch)
        memset(ptr, shadetable[GetShade((y - viewheight / 2) << 3)][0x19], viewwidth);
#else
    for(y = 0; y < viewheight / 2; y++, ptr += vbufPitch)
        memset(ptr, ceiling, viewwidth);
    for(; y < viewheight; y++, ptr += vbufPitch)
        memset(ptr, 0x19, viewwidth);
#endif
#endif
}

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

int CalcRotate (objtype *ob)
{
    int angle, viewangle;

    // this isn't exactly correct, as it should vary by a trig value,
    // but it is close enough with only eight rotations

    viewangle = player->angle + (centerx - ob->viewx)/8;

    if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
        angle = (viewangle-180) - ob->angle;
    else
        angle = (viewangle-180) - dirangle[ob->dir];

    angle+=ANGLES/16;
    while (angle>=ANGLES)
        angle-=ANGLES;
    while (angle<0)
        angle+=ANGLES;

    if (ob->state->rotate == 2)             // 2 rotation pain frame
        return 0;               // pain with shooting frame bugfix

    return angle/(ANGLES/8);
}

#define PRECISE_CLIPPING
void ScaleShape (int xcenter, int shapenum, unsigned height, uint32_t flags)
{
#ifdef __AMIGA__
	if ((height / 4) >= MAXSCALEHEIGHT*2 /*|| (height / 4) == 0*/)
		return;

	t_compshape *shape = (t_compshape *)PM_GetSprite(shapenum);

#ifdef PRECISE_CLIPPING
#ifdef USE_HALFWIDTH
	int32_t xscale = (int32_t)(height/2) << 12;
	int32_t xcent = ((int32_t)xcenter << 20) - ((int32_t)(height/2) << 17) + 0x80000;
#else
	int32_t xscale = (int32_t)height << 12;
	int32_t xcent = ((int32_t)xcenter << 20) - ((int32_t)height << 17) + 0x80000;
#endif
#else
	int16_t xscale = (int16_t)height >> 2;
	int16_t xcent = ((int16_t)xcenter << 6) - ((int16_t)height << 3) + 0x40;
#endif

	//
	// calculate edges of the shape
	//
#ifdef PRECISE_CLIPPING
	int16_t x1 = (int16_t)((int32_t)(xcent+((int32_t)shape->leftpix*xscale))>>20);
#else
	int16_t x1 = (int16_t)(xcent+((int16_t)shape->leftpix*xscale))>>6;
#endif

	if (x1 >= viewwidth)
		return; // off the right side

#ifdef PRECISE_CLIPPING
	int16_t x2 = (int16_t)((int32_t)(xcent+((int32_t)shape->rightpix*xscale))>>20);
#else
	int16_t x2 = (int16_t)(xcent+((int16_t)shape->rightpix*xscale))>>6;
#endif

	if (x2 < 0)
		return; // off the left side

	int quarterheight = height/4; // TODO remove this
#ifdef USE_HALFWIDTH
	word screenscale = scalediv[quarterheight/2];
#else
	word screenscale = scalediv[quarterheight];
#endif

	//
	// store information in a vissprite
	//
	word frac;

	if (x1 < 0)
	{
		frac = (-x1) * screenscale;
		x1 = 0;
	}
	else
	{
		frac = screenscale / 2;
	}

	if (x2 >= viewwidth)
		x2 = viewwidth - 1;

	uint16_t swidth = shape->rightpix - shape->leftpix;
	dc_seg = (uint8_t *)(shape);
	uint16_t *dataofs = shape->dataofs;
#ifdef USE_HALFWIDTH
	dc_iscale = scalediv[quarterheight];
#else
	dc_iscale = screenscale;
#endif
	dc_sprtopoffset = (viewheight << 6) - (quarterheight << 6);
	dc_screenstep = quarterheight << 1;

	if (height>256)
	{
		int16_t lastcolumn = -1;

		dc_width = 1;
		//dc_x = 0;

		for (; x1 <= x2; ++x1, frac += screenscale)
		{
			if (wallheight[x1] > height)
			{
				if (lastcolumn != -1)
				{
					linecmds = (uint16_t *)(&dc_seg[dataofs[lastcolumn]]);
					generic_scale_masked_post();
					//dc_width = 1;
					lastcolumn = -1;
				}
				continue;
			}

			int16_t texturecolumn = frac >> 10;

			if (texturecolumn > swidth)
				texturecolumn = swidth;

			if (texturecolumn == lastcolumn)
			{
				dc_width++;
				continue;
			}
			else
			{
				if (lastcolumn != -1)
				{
					linecmds = (uint16_t *)(&dc_seg[dataofs[lastcolumn]]);
					generic_scale_masked_post();
				}
				dc_x = x1;
				dc_width = 1;
				lastcolumn = texturecolumn;
			}
		}
		if (lastcolumn != -1)
		{
			linecmds = (uint16_t *)(&dc_seg[dataofs[lastcolumn]]);
			generic_scale_masked_post();
		}
	}
	else
	{
		dc_width = 1;

		for (; x1 <= x2; ++x1, frac += screenscale)
		{
			if (wallheight[x1] > height)
				continue;

			dc_x = x1;

			int16_t texturecolumn = frac >> 10;

			if (texturecolumn > swidth)
				texturecolumn = swidth;

			linecmds = (uint16_t *)(&dc_seg[dataofs[texturecolumn]]);

			generic_scale_masked_post();
		}
	}
#else
    t_compshape *shape;
    unsigned scale,pixheight;
    unsigned starty,endy;
    word *cmdptr;
    byte *cline;
    byte *line;
    byte *vmem;
    int actx,i,upperedge;
    short newstart;
    int scrstarty,screndy,lpix,rpix,pixcnt,ycnt;
    unsigned j;
    byte col;

#ifdef USE_SHADING
    byte *curshades;
    if(flags & FL_FULLBRIGHT)
        curshades = shadetable[0];
    else
        curshades = shadetable[GetShade(height)];
#endif

    shape = (t_compshape *) PM_GetSprite(shapenum);

    scale=height>>3;                 // low three bits are fractional
    if(!scale) return;   // too close or far away

    pixheight=scale*SPRITESCALEFACTOR;
    actx=xcenter-scale;
    upperedge=viewheight/2-scale;

    cmdptr=(word *) shape->dataofs;

#ifdef __AMIGA__
	word leftpix = SWAP16LE(shape->leftpix);
	word rightpix = SWAP16LE(shape->rightpix);
    for(i=leftpix,pixcnt=i*pixheight,rpix=(pixcnt>>6)+actx;i<=rightpix;i++,cmdptr++)
#else
    for(i=shape->leftpix,pixcnt=i*pixheight,rpix=(pixcnt>>6)+actx;i<=shape->rightpix;i++,cmdptr++)
#endif
    {
        lpix=rpix;
        if(lpix>=viewwidth) break;
        pixcnt+=pixheight;
        rpix=(pixcnt>>6)+actx;
        if(lpix!=rpix && rpix>0)
        {
            if(lpix<0) lpix=0;
#ifdef __AMIGA__
            if(rpix>viewwidth) rpix=viewwidth,i=rightpix+1;
            cline=(byte *)shape + SWAP16LE(*cmdptr);
#else
            if(rpix>viewwidth) rpix=viewwidth,i=shape->rightpix+1;
            cline=(byte *)shape + *cmdptr;
#endif
            while(lpix<rpix)
            {
                if(wallheight[lpix]<=(int)height)
                {
                    line=cline;
                    while((endy = READWORD(line)) != 0)
                    {
                        endy >>= 1;
                        newstart = READWORD(line);
                        starty = READWORD(line) >> 1;
                        j=starty;
                        ycnt=j*pixheight;
                        screndy=(ycnt>>6)+upperedge;
                        if(screndy<0) vmem=vbuf+lpix;
                        else vmem=vbuf+screndy*vbufPitch+lpix;
                        for(;j<endy;j++)
                        {
                            scrstarty=screndy;
                            ycnt+=pixheight;
                            screndy=(ycnt>>6)+upperedge;
                            if(scrstarty!=screndy && screndy>0)
                            {
#ifdef USE_SHADING
                                col=curshades[((byte *)shape)[newstart+j]];
#else
                                col=((byte *)shape)[newstart+j];
#endif
                                if(scrstarty<0) scrstarty=0;
                                if(screndy>viewheight) screndy=viewheight,j=endy;

                                while(scrstarty<screndy)
                                {
                                    *vmem=col;
                                    vmem+=vbufPitch;
                                    scrstarty++;
                                }
                            }
                        }
                    }
                }
                lpix++;
            }
        }
    }
#endif
}

void SimpleScaleShape (int xcenter, int shapenum, unsigned height)
{
#ifdef __AMIGA__
	t_compshape *shape = (t_compshape *)PM_GetSprite(shapenum);

#ifdef USE_HALFWIDTH
	int16_t xscale = (int16_t)height/2;
	int16_t xcent = ((int16_t)xcenter << 6) - ((int16_t)height/2 << 5) + 0x40;
#else
	int16_t xscale = (int16_t)height;
	int16_t xcent = ((int16_t)xcenter << 6) - ((int16_t)height << 5) + 0x40;
#endif

	//
	// calculate edges of the shape
	//
	int16_t x1 = (int16_t)(xcent+((int16_t)shape->leftpix*xscale))>>6;

	if (x1 >= viewwidth)
		return; // off the right side

	//int16_t x2 = (int16_t)(xcent+((int16_t)shape->rightpix+*xscale))>>6;
	int16_t x2 = (int16_t)(xcent+((int16_t)(shape->rightpix+1)*xscale))>>6;

	if (x2 < 0)
		return; // off the left side

#ifdef USE_HALFWIDTH
	word screenscale = scalediv[height/2];
#else
	word screenscale = scalediv[height];
#endif

	//
	// store information in a vissprite
	//
	word frac;

	if (x1 < 0)
	{
		frac = screenscale * (-x1);
		x1 = 0;
	}
	else
	{
		frac = screenscale / 2;
	}


	if (x2 >= viewwidth)
		x2 = viewwidth - 1;

#ifdef USE_HALFWIDTH
	dc_iscale = scalediv[height];
#else
	dc_iscale = screenscale;
#endif
	dc_seg = (uint8_t *)(shape);
	uint16_t *dataofs = shape->dataofs;
	dc_sprtopoffset = (viewheight << 6) - (height << 6);
	dc_screenstep = height << 1;
	uint16_t swidth = shape->rightpix - shape->leftpix;

	if (height>64)
	{
		int16_t lastcolumn = -1;

		//dc_x = 0;
		dc_width = 1;

		for (; x1 < x2; ++x1, frac += screenscale)
		{
			int16_t texturecolumn = frac >> 10;

			if (texturecolumn > swidth)
				texturecolumn = swidth;

			if (texturecolumn == lastcolumn)
			{
				dc_width++;
				continue;
			}
			else
			{
				if (lastcolumn != -1)
				{
					linecmds = (uint16_t *)(&dc_seg[dataofs[lastcolumn]]);
					generic_scale_masked_post();
				}
				dc_width = 1;
				dc_x = x1;
				lastcolumn = texturecolumn;
			}
		}
		if (lastcolumn != -1)
		{
			linecmds = (uint16_t *)(&dc_seg[dataofs[lastcolumn]]);
			generic_scale_masked_post();
		}
	}
	else
	{
		dc_width = 1;

		//for (; x1 <= x2; ++x1, frac += screenscale)
		for (; x1 < x2; ++x1, frac += screenscale)
		{
			dc_x = x1;

			int16_t texturecolumn = frac >> 10;

			if (texturecolumn > swidth)
				texturecolumn = swidth;

			linecmds = (uint16_t *)(&dc_seg[dataofs[texturecolumn]]);

			generic_scale_masked_post();
		}
	}
#else
    t_compshape   *shape;
    unsigned scale,pixheight;
    unsigned starty,endy;
    word *cmdptr;
    byte *cline;
    byte *line;
    int actx,i,upperedge;
    short newstart;
    int scrstarty,screndy,lpix,rpix,pixcnt,ycnt;
    unsigned j;
    byte col;
    byte *vmem;

    shape = (t_compshape *) PM_GetSprite(shapenum);

    scale=height>>1;
    pixheight=scale*SPRITESCALEFACTOR;
    actx=xcenter-scale;
    upperedge=viewheight/2-scale;

    cmdptr=shape->dataofs;

#ifdef __AMIGA__
	word leftpix = SWAP16LE(shape->leftpix);
	word rightpix = SWAP16LE(shape->rightpix);

	for(i=leftpix,pixcnt=i*pixheight,rpix=(pixcnt>>6)+actx;i<=rightpix;i++,cmdptr++)
#else
    for(i=shape->leftpix,pixcnt=i*pixheight,rpix=(pixcnt>>6)+actx;i<=shape->rightpix;i++,cmdptr++)
#endif
    {
        lpix=rpix;
        if(lpix>=viewwidth) break;
        pixcnt+=pixheight;
        rpix=(pixcnt>>6)+actx;
        if(lpix!=rpix && rpix>0)
        {
            if(lpix<0) lpix=0;
#ifdef __AMIGA__
            if(rpix>viewwidth) rpix=viewwidth,i=rightpix+1;
            cline = (byte *)shape + SWAP16LE(*cmdptr);
#else
            if(rpix>viewwidth) rpix=viewwidth,i=shape->rightpix+1;
            cline = (byte *)shape + *cmdptr;
#endif
            while(lpix<rpix)
            {
                line=cline;
                while((endy = READWORD(line)) != 0)
                {
                    endy >>= 1;
                    newstart = READWORD(line);
                    starty = READWORD(line) >> 1;
                    j=starty;
                    ycnt=j*pixheight;
                    screndy=(ycnt>>6)+upperedge;
                    if(screndy<0) vmem=vbuf+lpix;
                    else vmem=vbuf+screndy*vbufPitch+lpix;
                    for(;j<endy;j++)
                    {
                        scrstarty=screndy;
                        ycnt+=pixheight;
                        screndy=(ycnt>>6)+upperedge;
                        if(scrstarty!=screndy && screndy>0)
                        {
                            col=((byte *)shape)[newstart+j];
                            if(scrstarty<0) scrstarty=0;
                            if(screndy>viewheight) screndy=viewheight,j=endy;

                            while(scrstarty<screndy)
                            {
                                *vmem=col;
                                vmem+=vbufPitch;
                                scrstarty++;
                            }
                        }
                    }
                }
                lpix++;
            }
        }
    }
#endif
}

#ifdef USE_AUTOMAP
void SimpleScaleWallTile(int x, int y, int wallpic, int height)
{
#ifdef USE_HALFWIDTH
	int16_t width = (int16_t)height/2;
	x /= 2;
#else
	int16_t width = (int16_t)height;
#endif

	if (x < 0 || y < 0 || x+width > viewwidth || y+height > viewheight)
		return;

	dc_length = height;
	dc_iscale = scalediv[height];
	dc_frac = 0;
	postsource = PM_GetTexture(wallpic);
	postwidth = 1;

	word screenscale = scalediv[width];
	word frac = 0;
	for (int16_t x1 = x; x1 < x+width; x1++)
	{
		dc_dest = (y*viewwidth)+x1;
		frac += screenscale;
		dc_source = (frac >> 10) * 64;
		R_DrawColumn();
	}
}
#endif

/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#define MAXVISABLE 250

typedef struct
{
    short      viewx,
               viewheight,
               shapenum;
    short      flags;          // this must be changed to uint32_t, when you
                               // you need more than 16-flags for drawing
#ifdef USE_DIR3DSPR
    statobj_t *transsprite;
#endif
} visobj_t;

visobj_t vislist[MAXVISABLE];
visobj_t *visptr,*visstep,*farthest;

void DrawScaleds (void)
{
    int      i,least,numvisable,height;
    byte     *tilespot,*visspot;
    unsigned spotloc;

    statobj_t *statptr;
    objtype   *obj;

    visptr = &vislist[0];

//
// place static objects
//
    for (statptr = &statobjlist[0] ; statptr !=laststatobj ; statptr++)
    {
        if ((visptr->shapenum = statptr->shapenum) == -1)
            continue;                                               // object has been deleted

        if (!*statptr->visspot)
            continue;                                               // not visable

        if (TransformTile (statptr->tilex,statptr->tiley,
            &visptr->viewx,&visptr->viewheight) && statptr->flags & FL_BONUS)
        {
            GetBonus (statptr);
            if(statptr->shapenum == -1)
                continue;                                           // object has been taken
        }

        if (!visptr->viewheight)
            continue;                                               // to close to the object

#ifdef USE_DIR3DSPR
        if(statptr->flags & FL_DIR_MASK)
            visptr->transsprite=statptr;
        else
            visptr->transsprite=NULL;
#endif

        if (visptr < &vislist[MAXVISABLE-1])    // don't let it overflow
        {
            visptr->flags = (short) statptr->flags;
            visptr++;
        }
    }

//
// place active objects
//
    for (obj = player->next;obj;obj=obj->next)
    {
        if ((visptr->shapenum = obj->state->shapenum)==0)
            continue;                                               // no shape

        spotloc = (obj->tilex<<mapshift)+obj->tiley;   // optimize: keep in struct?
        visspot = &spotvis[0][0]+spotloc;
        tilespot = &tilemap[0][0]+spotloc;

        //
        // could be in any of the nine surrounding tiles
        //
        if (*visspot
            || ( *(visspot-1) && !*(tilespot-1) )
            || ( *(visspot+1) && !*(tilespot+1) )
            || ( *(visspot-65) && !*(tilespot-65) )
            || ( *(visspot-64) && !*(tilespot-64) )
            || ( *(visspot-63) && !*(tilespot-63) )
            || ( *(visspot+65) && !*(tilespot+65) )
            || ( *(visspot+64) && !*(tilespot+64) )
            || ( *(visspot+63) && !*(tilespot+63) ) )
        {
            obj->active = ac_yes;
            TransformActor (obj);
            if (!obj->viewheight)
                continue;                                               // too close or far away

            visptr->viewx = obj->viewx;
            visptr->viewheight = obj->viewheight;
            if (visptr->shapenum == -1)
                visptr->shapenum = obj->temp1;  // special shape

            if (obj->state->rotate)
                visptr->shapenum += CalcRotate (obj);

            if (visptr < &vislist[MAXVISABLE-1])    // don't let it overflow
            {
                visptr->flags = (short) obj->flags;
#ifdef USE_DIR3DSPR
                visptr->transsprite = NULL;
#endif
                visptr++;
            }
            obj->flags |= FL_VISABLE;
        }
        else
            obj->flags &= ~FL_VISABLE;
    }

//
// draw from back to front
//
    numvisable = (int) (visptr-&vislist[0]);

    if (!numvisable)
        return;                                                                 // no visable objects

    for (i = 0; i<numvisable; i++)
    {
        least = 32000;
        for (visstep=&vislist[0] ; visstep<visptr ; visstep++)
        {
            height = visstep->viewheight;
            if (height < least)
            {
                least = height;
                farthest = visstep;
            }
        }
        //
        // draw farthest
        //
#ifdef USE_DIR3DSPR
        if(farthest->transsprite)
            Scale3DShape(vbuf, vbufPitch, farthest->transsprite);
        else
#endif
            ScaleShape(farthest->viewx, farthest->shapenum, farthest->viewheight, farthest->flags);

        farthest->viewheight = 32000;
    }
}

//==========================================================================

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

int weaponscale[NUMWEAPONS] = {SPR_KNIFEREADY, SPR_PISTOLREADY,
    SPR_MACHINEGUNREADY, SPR_CHAINREADY};

void DrawPlayerWeapon (void)
{
    int shapenum;

#ifndef SPEAR
    if (gamestate.victoryflag)
    {
#ifndef APOGEE_1_0
        if (player->state == &s_deathcam && (GetTimeCount()&32) )
            SimpleScaleShape(viewwidth/2,SPR_DEATHCAM,viewheight+1);
#endif
        return;
    }
#endif

    if (gamestate.weapon != -1)
    {
        shapenum = weaponscale[gamestate.weapon]+gamestate.weaponframe;
        SimpleScaleShape(viewwidth/2,shapenum,viewheight+1);
    }

    if (demorecord || demoplayback)
        SimpleScaleShape(viewwidth/2,SPR_DEMO,viewheight+1);
}


//==========================================================================


/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics (void)
{
//
// calculate tics since last refresh for adaptive timing
//
#ifdef __AMIGA__
	long	newtime;

	if (lasttimecount > TimeCount)
		TimeCount = lasttimecount;		// if the game was paused a LONG time

	newtime = TimeCount;
	tics = newtime-lasttimecount;
	while (!tics)
	{
		SDL_Delay(1);
		newtime = TimeCount;
		tics = newtime-lasttimecount;
	}

	lasttimecount = newtime;

	if (tics>MAXTICS)
	{
		TimeCount -= (tics-MAXTICS);
		tics = MAXTICS;
	}
#else
    if (lasttimecount > (int32_t) GetTimeCount())
        lasttimecount = GetTimeCount();    // if the game was paused a LONG time

	uint32_t curtime = SDL_GetTicks();
    tics = (curtime * 7) / 100 - lasttimecount;
    if(!tics)
    {
        // wait until end of current tic
        SDL_Delay(((lasttimecount + 1) * 100) / 7 - curtime);
        tics = 1;
    }

    lasttimecount += tics;

    if (tics>MAXTICS)
        tics = MAXTICS;
#endif
}


//==========================================================================

#ifdef RAYCASTFAST
// 8.8 bit fixed point version
void AsmRefresh()
{
    int16_t xstep,ystep;
    int16_t xpartial,ypartial;
    word xspot,yspot;

    for(pixx=0;pixx<viewwidth;pixx++)
    {
        short angl=midangle+pixelangle[pixx];
        if(angl<0) angl+=FINEANGLES;
        if(angl>=FINEANGLES) angl-=FINEANGLES;
        if(angl<ANG90)
        {
            xtilestep=1;
            ytilestep=-1;
            xstep=(int16_t)(finetangent[ANG90-1-angl] >> (16-FASTSHIFT));
            ystep=(int16_t)(-finetangent[angl] >> (16-FASTSHIFT));
            xpartial=xpartialup;
            ypartial=ypartialdown;
        }
        else if(angl<ANG180)
        {
            xtilestep=-1;
            ytilestep=-1;
            xstep=(int16_t)(-finetangent[angl-ANG90] >> (16-FASTSHIFT));
            ystep=(int16_t)(-finetangent[ANG180-1-angl] >> (16-FASTSHIFT));
            xpartial=xpartialdown;
            ypartial=ypartialdown;
        }
        else if(angl<ANG270)
        {
            xtilestep=-1;
            ytilestep=1;
            xstep=(int16_t)(-finetangent[ANG270-1-angl] >> (16-FASTSHIFT));
            ystep=(int16_t)(finetangent[angl-ANG180] >> (16-FASTSHIFT));
            xpartial=xpartialdown;
            ypartial=ypartialup;
        }
        else if(angl<FINEANGLES)
        {
            xtilestep=1;
            ytilestep=1;
            xstep=(int16_t)(finetangent[angl-ANG270] >> (16-FASTSHIFT));
            ystep=(int16_t)(finetangent[FINEANGLES-1-angl] >> (16-FASTSHIFT));
            xpartial=xpartialup;
            ypartial=ypartialup;
        }

        // TODO viewx viewy
        yintercept=FixedByFrac(ystep,xpartial)+(viewy>>(16-FASTSHIFT));
        xtile=focaltx+xtilestep;
        xspot=(word)((xtile<<mapshift)+((uint16_t)yintercept>>FASTSHIFT));
        xintercept=FixedByFrac(xstep,ypartial)+(viewx>>(16-FASTSHIFT));
        ytile=focalty+ytilestep;
        yspot=(word)((((uint16_t)xintercept>>FASTSHIFT)<<mapshift)+ytile);

        do
        {
            if(ytilestep==-1 && (yintercept>>FASTSHIFT)<=ytile) goto horizentry;
            if(ytilestep==1 && (yintercept>>FASTSHIFT)>=ytile) goto horizentry;
vertentry:
            tilehit=((byte *)tilemap)[xspot];
            if(tilehit)
            {
                if(tilehit&0x80)
                {
                    int16_t yintbuf=yintercept+(ystep>>1);
                    if((yintbuf>>FASTSHIFT)!=(yintercept>>FASTSHIFT))
                        goto passvert;
                    if((byte)yintbuf<doorposition[tilehit&0x7f]>>(16-FASTSHIFT))
                        goto passvert;
                    yintercept=yintbuf;
                    xintercept=(xtile<<FASTSHIFT)|(FASTGLOBAL>>1);
                    ytile = (short) (yintercept >> FASTSHIFT);
                    HitVertDoor();
                }
                else
                {
                    if(tilehit==64)
                    {
                        int16_t yintbuf=yintercept+((ystep*pwallpos)>>6);
                        if((yintbuf>>FASTSHIFT)!=(yintercept>>FASTSHIFT))
                            goto passvert;

                        xintercept=(xtile<<FASTSHIFT) + (pwallpos<<(FASTSHIFT-6))*xtilestep;
                        yintercept=yintbuf;
                        tilehit=pwalltile;
                    }
                    else
                    {
                        xintercept=xtile<<FASTSHIFT;
                    }
                    ytile = (short) (yintercept >> FASTSHIFT);
                    HitVertWall();
                }
                break;
            }
passvert:
            *((byte *)spotvis+xspot)=1;
            xtile+=xtilestep;
            yintercept+=ystep;
            xspot=(word)((xtile<<mapshift)+((uint16_t)yintercept>>FASTSHIFT));
        }
        while(1);
        continue;

        do
        {
            if(xtilestep==-1 && (xintercept>>FASTSHIFT)<=xtile) goto vertentry;
            if(xtilestep==1 && (xintercept>>FASTSHIFT)>=xtile) goto vertentry;
horizentry:
            tilehit=((byte *)tilemap)[yspot];
            if(tilehit)
            {
                if(tilehit&0x80)
                {
                    int16_t xintbuf=xintercept+(xstep>>1);
                    if((xintbuf>>FASTSHIFT)!=(xintercept>>FASTSHIFT))
                        goto passhoriz;
                    if((byte)xintbuf<doorposition[tilehit&0x7f]>>(16-FASTSHIFT))
                        goto passhoriz;
                    xintercept=xintbuf;
                    yintercept=(ytile<<FASTSHIFT)|(FASTGLOBAL>>1);
                    xtile = (short) (xintercept >> FASTSHIFT);
                    HitHorizDoor();
                }
                else
                {
                    if(tilehit==64)
                    {
                        int16_t xintbuf=xintercept+((xstep*pwallpos)>>6);
                        if((xintbuf>>FASTSHIFT)!=(xintercept>>FASTSHIFT))
                            goto passhoriz;

                        yintercept=(ytile<<FASTSHIFT) + (pwallpos<<(FASTSHIFT-6))*ytilestep;
                        xintercept=xintbuf;
                        tilehit=pwalltile;
                    }
                    else
                    {
                        yintercept=ytile<<FASTSHIFT;
                    }
                    xtile = (short) (xintercept >> FASTSHIFT);
                    HitHorizWall();
                }
                break;
            }
passhoriz:
            *((byte *)spotvis+yspot)=1;
            ytile+=ytilestep;
            xintercept+=xstep;
            yspot=(word)((((uint16_t)xintercept>>FASTSHIFT)<<mapshift)+ytile);
        }
        while(1);
    }
}
#else

void AsmRefresh()
{
    int32_t xstep,ystep;
    longword xpartial,ypartial;
#ifndef __AMIGA__
    boolean playerInPushwallBackTile = tilemap[focaltx][focalty] == 64;
#endif
#ifdef __AMIGA__
    word xspot,yspot;
#endif

    for(pixx=0;pixx<viewwidth;pixx++)
    {
        short angl=midangle+pixelangle[pixx];
        if(angl<0) angl+=FINEANGLES;
        if(angl>=FINEANGLES) angl-=FINEANGLES;
        if(angl<ANG90)
        {
            xtilestep=1;
            ytilestep=-1;
            xstep=finetangent[ANG90-1-angl];
            ystep=-finetangent[angl];
            xpartial=xpartialup;
            ypartial=ypartialdown;
        }
        else if(angl<ANG180)
        {
            xtilestep=-1;
            ytilestep=-1;
            xstep=-finetangent[angl-ANG90];
            ystep=-finetangent[ANG180-1-angl];
            xpartial=xpartialdown;
            ypartial=ypartialdown;
        }
        else if(angl<ANG270)
        {
            xtilestep=-1;
            ytilestep=1;
            xstep=-finetangent[ANG270-1-angl];
            ystep=finetangent[angl-ANG180];
            xpartial=xpartialdown;
            ypartial=ypartialup;
        }
        else if(angl<FINEANGLES)
        {
            xtilestep=1;
            ytilestep=1;
            xstep=finetangent[angl-ANG270];
            ystep=finetangent[FINEANGLES-1-angl];
            xpartial=xpartialup;
            ypartial=ypartialup;
        }
        yintercept=FixedMul(ystep,xpartial)+viewy;
        xtile=focaltx+xtilestep;
        xspot=(word)((xtile<<mapshift)+((uint32_t)yintercept>>16));
        xintercept=FixedMul(xstep,ypartial)+viewx;
        ytile=focalty+ytilestep;
        yspot=(word)((((uint32_t)xintercept>>16)<<mapshift)+ytile);
#ifndef __AMIGA__
        texdelta=0;
        // Special treatment when player is in back tile of pushwall
        if(playerInPushwallBackTile)
        {
            if(    pwalldir == di_east && xtilestep ==  1
                || pwalldir == di_west && xtilestep == -1)
            {
                int32_t yintbuf = yintercept - ((ystep * (64 - pwallpos)) >> 6);
                if((yintbuf >> 16) == focalty)   // ray hits pushwall back?
                {
                    if(pwalldir == di_east)
                        xintercept = (focaltx << TILESHIFT) + (pwallpos << 10);
                    else
                        xintercept = (focaltx << TILESHIFT) - TILEGLOBAL + ((64 - pwallpos) << 10);
                    yintercept = yintbuf;
                    ytile = (short) (yintercept >> TILESHIFT);
                    tilehit = pwalltile;
                    HitVertWall();
                    continue;
                }
            }
            else if(pwalldir == di_south && ytilestep ==  1
                ||  pwalldir == di_north && ytilestep == -1)
            {
                int32_t xintbuf = xintercept - ((xstep * (64 - pwallpos)) >> 6);
                if((xintbuf >> 16) == focaltx)   // ray hits pushwall back?
                {
                    xintercept = xintbuf;
                    if(pwalldir == di_south)
                        yintercept = (focalty << TILESHIFT) + (pwallpos << 10);
                    else
                        yintercept = (focalty << TILESHIFT) - TILEGLOBAL + ((64 - pwallpos) << 10);
                    xtile = (short) (xintercept >> TILESHIFT);
                    tilehit = pwalltile;
                    HitHorizWall();
                    continue;
                }
            }
        }
#endif

        do
        {
            if(ytilestep==-1 && (yintercept>>16)<=ytile) goto horizentry;
            if(ytilestep==1 && (yintercept>>16)>=ytile) goto horizentry;
vertentry:
#ifndef __AMIGA__
            if((uint32_t)yintercept>mapheight*65536-1 || (word)xtile>=mapwidth)
            {
                if(xtile<0) xintercept=0, xtile=0;
                else if(xtile>=mapwidth) xintercept=mapwidth<<TILESHIFT, xtile=mapwidth-1;
                else xtile=(short) (xintercept >> TILESHIFT);
                if(yintercept<0) yintercept=0, ytile=0;
                else if(yintercept>=(mapheight<<TILESHIFT)) yintercept=mapheight<<TILESHIFT, ytile=mapheight-1;
                yspot=0xffff;
                tilehit=0;
                HitHorizBorder();
                break;
            }
            if(xspot>=maparea) break;
#endif
            tilehit=((byte *)tilemap)[xspot];
            if(tilehit)
            {
#ifdef USE_AUTOMAP
				*((byte *)mapvis+xspot)=1;
#endif
                if(tilehit&0x80)
                {
                    int32_t yintbuf=yintercept+(ystep>>1);
                    if((yintbuf>>16)!=(yintercept>>16))
                        goto passvert;
                    if((word)yintbuf<doorposition[tilehit&0x7f])
                        goto passvert;
                    yintercept=yintbuf;
                    xintercept=(xtile<<TILESHIFT)|0x8000;
                    ytile = (short) (yintercept >> TILESHIFT);
                    HitVertDoor();
#ifdef DOUBLEHORIZ
					if (lowdetail)
					{
						wallheight[pixx+1] = wallheight[pixx];
						postwidth++;
						pixx++;
					}
#endif
                }
                else
                {
                    if(tilehit==64)
                    {
#ifdef __AMIGA__
                        int32_t yintbuf=yintercept+((ystep*pwallpos)>>6);
                        if((yintbuf>>16)!=(yintercept>>16))
                            goto passvert;

                        xintercept=(xtile<<TILESHIFT) + (pwallpos<<10)*xtilestep;
                        yintercept=yintbuf;
                        ytile = (short) (yintercept >> TILESHIFT);
                        tilehit=pwalltile;
                        HitVertWall();
#ifdef DOUBLEHORIZ
						if (lowdetail)
						{
							wallheight[pixx+1] = wallheight[pixx];
							postwidth++;
							pixx++;
						}
#endif
#else
                        if(pwalldir==di_west || pwalldir==di_east)
                        {
                            int32_t yintbuf;
                            int pwallposnorm;
                            int pwallposinv;
                            if(pwalldir==di_west)
                            {
                                pwallposnorm = 64-pwallpos;
                                pwallposinv = pwallpos;
                            }
                            else
                            {
                                pwallposnorm = pwallpos;
                                pwallposinv = 64-pwallpos;
                            }
                            if(pwalldir == di_east && xtile==pwallx && ((uint32_t)yintercept>>16)==pwally
                                || pwalldir == di_west && !(xtile==pwallx && ((uint32_t)yintercept>>16)==pwally))
                            {
                                yintbuf=yintercept+((ystep*pwallposnorm)>>6);
                                if((yintbuf>>16)!=(yintercept>>16))
                                    goto passvert;

                                xintercept=(xtile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
                                yintercept=yintbuf;
                                ytile = (short) (yintercept >> TILESHIFT);
                                tilehit=pwalltile;
                                HitVertWall();
                            }
                            else
                            {
                                yintbuf=yintercept+((ystep*pwallposinv)>>6);
                                if((yintbuf>>16)!=(yintercept>>16))
                                    goto passvert;

                                xintercept=(xtile<<TILESHIFT)-(pwallposinv<<10);
                                yintercept=yintbuf;
                                ytile = (short) (yintercept >> TILESHIFT);
                                tilehit=pwalltile;
                                HitVertWall();
                            }
                        }
                        else
                        {
                            int pwallposi = pwallpos;
                            if(pwalldir==di_north) pwallposi = 64-pwallpos;
                            if(pwalldir==di_south && (word)yintercept<(pwallposi<<10)
                                || pwalldir==di_north && (word)yintercept>(pwallposi<<10))
                            {
                                if(((uint32_t)yintercept>>16)==pwally && xtile==pwallx)
                                {
                                    if(pwalldir==di_south && (int32_t)((word)yintercept)+ystep<(pwallposi<<10)
                                            || pwalldir==di_north && (int32_t)((word)yintercept)+ystep>(pwallposi<<10))
                                        goto passvert;

                                    if(pwalldir==di_south)
                                        yintercept=(yintercept&0xffff0000)+(pwallposi<<10);
                                    else
                                        yintercept=(yintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
                                    xintercept=xintercept-((xstep*(64-pwallpos))>>6);
                                    xtile = (short) (xintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitHorizWall();
                                }
                                else
                                {
                                    texdelta = -(pwallposi<<10);
                                    xintercept=xtile<<TILESHIFT;
                                    ytile = (short) (yintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitVertWall();
                                }
                            }
                            else
                            {
                                if(((uint32_t)yintercept>>16)==pwally && xtile==pwallx)
                                {
                                    texdelta = -(pwallposi<<10);
                                    xintercept=xtile<<TILESHIFT;
                                    ytile = (short) (yintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitVertWall();
                                }
                                else
                                {
                                    if(pwalldir==di_south && (int32_t)((word)yintercept)+ystep>(pwallposi<<10)
                                            || pwalldir==di_north && (int32_t)((word)yintercept)+ystep<(pwallposi<<10))
                                        goto passvert;

                                    if(pwalldir==di_south)
                                        yintercept=(yintercept&0xffff0000)-((64-pwallpos)<<10);
                                    else
                                        yintercept=(yintercept&0xffff0000)+((64-pwallpos)<<10);
                                    xintercept=xintercept-((xstep*pwallpos)>>6);
                                    xtile = (short) (xintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitHorizWall();
                                }
                            }
                        }
#endif
                    }
                    else
                    {
                        xintercept=xtile<<TILESHIFT;
                        ytile = (short) (yintercept >> TILESHIFT);
                        HitVertWall();
#ifdef DOUBLEHORIZ
						if (lowdetail)
						{
							wallheight[pixx+1] = wallheight[pixx];
							postwidth++;
							pixx++;
						}
#endif
                    }
                }
                break;
            }
passvert:
            *((byte *)spotvis+xspot)=1;
            xtile+=xtilestep;
            yintercept+=ystep;
            xspot=(word)((xtile<<mapshift)+((uint32_t)yintercept>>16));
        }
        while(1);
        continue;

        do
        {
            if(xtilestep==-1 && (xintercept>>16)<=xtile) goto vertentry;
            if(xtilestep==1 && (xintercept>>16)>=xtile) goto vertentry;
horizentry:
#ifndef __AMIGA__
            if((uint32_t)xintercept>mapwidth*65536-1 || (word)ytile>=mapheight)
            {
                if(ytile<0) yintercept=0, ytile=0;
                else if(ytile>=mapheight) yintercept=mapheight<<TILESHIFT, ytile=mapheight-1;
                else ytile=(short) (yintercept >> TILESHIFT);
                if(xintercept<0) xintercept=0, xtile=0;
                else if(xintercept>=(mapwidth<<TILESHIFT)) xintercept=mapwidth<<TILESHIFT, xtile=mapwidth-1;
                xspot=0xffff;
                tilehit=0;
                HitVertBorder();
                break;
            }
            if(yspot>=maparea) break;
#endif
            tilehit=((byte *)tilemap)[yspot];
            if(tilehit)
            {
#ifdef USE_AUTOMAP
				*((byte *)mapvis+yspot)=1;
#endif
                if(tilehit&0x80)
                {
                    int32_t xintbuf=xintercept+(xstep>>1);
                    if((xintbuf>>16)!=(xintercept>>16))
                        goto passhoriz;
                    if((word)xintbuf<doorposition[tilehit&0x7f])
                        goto passhoriz;
                    xintercept=xintbuf;
                    yintercept=(ytile<<TILESHIFT)+0x8000;
                    xtile = (short) (xintercept >> TILESHIFT);
                    HitHorizDoor();
#ifdef DOUBLEHORIZ
					if (lowdetail)
					{
						wallheight[pixx+1] = wallheight[pixx];
						postwidth++;
						pixx++;
					}
#endif
                }
                else
                {
                    if(tilehit==64)
                    {
#ifdef __AMIGA__
                        int32_t xintbuf=xintercept+((xstep*pwallpos)>>6);
                        if((xintbuf>>16)!=(xintercept>>16))
                            goto passhoriz;

                        yintercept=(ytile<<TILESHIFT) + (pwallpos<<10)*ytilestep;
                        xintercept=xintbuf;
                        xtile = (short) (xintercept >> TILESHIFT);
                        tilehit=pwalltile;
                        HitHorizWall();
#ifdef DOUBLEHORIZ
						if (lowdetail)
						{
							wallheight[pixx+1] = wallheight[pixx];
							postwidth++;
							pixx++;
						}
#endif
#else
                        if(pwalldir==di_north || pwalldir==di_south)
                        {
                            int32_t xintbuf;
                            int pwallposnorm;
                            int pwallposinv;
                            if(pwalldir==di_north)
                            {
                                pwallposnorm = 64-pwallpos;
                                pwallposinv = pwallpos;
                            }
                            else
                            {
                                pwallposnorm = pwallpos;
                                pwallposinv = 64-pwallpos;
                            }
                            if(pwalldir == di_south && ytile==pwally && ((uint32_t)xintercept>>16)==pwallx
                                || pwalldir == di_north && !(ytile==pwally && ((uint32_t)xintercept>>16)==pwallx))
                            {
                                xintbuf=xintercept+((xstep*pwallposnorm)>>6);
                                if((xintbuf>>16)!=(xintercept>>16))
                                    goto passhoriz;

                                yintercept=(ytile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
                                xintercept=xintbuf;
                                xtile = (short) (xintercept >> TILESHIFT);
                                tilehit=pwalltile;
                                HitHorizWall();
                            }
                            else
                            {
                                xintbuf=xintercept+((xstep*pwallposinv)>>6);
                                if((xintbuf>>16)!=(xintercept>>16))
                                    goto passhoriz;

                                yintercept=(ytile<<TILESHIFT)-(pwallposinv<<10);
                                xintercept=xintbuf;
                                xtile = (short) (xintercept >> TILESHIFT);
                                tilehit=pwalltile;
                                HitHorizWall();
                            }
                        }
                        else
                        {
                            int pwallposi = pwallpos;
                            if(pwalldir==di_west) pwallposi = 64-pwallpos;
                            if(pwalldir==di_east && (word)xintercept<(pwallposi<<10)
                                    || pwalldir==di_west && (word)xintercept>(pwallposi<<10))
                            {
                                if(((uint32_t)xintercept>>16)==pwallx && ytile==pwally)
                                {
                                    if(pwalldir==di_east && (int32_t)((word)xintercept)+xstep<(pwallposi<<10)
                                            || pwalldir==di_west && (int32_t)((word)xintercept)+xstep>(pwallposi<<10))
                                        goto passhoriz;

                                    if(pwalldir==di_east)
                                        xintercept=(xintercept&0xffff0000)+(pwallposi<<10);
                                    else
                                        xintercept=(xintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
                                    yintercept=yintercept-((ystep*(64-pwallpos))>>6);
                                    ytile = (short) (yintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitVertWall();
                                }
                                else
                                {
                                    texdelta = -(pwallposi<<10);
                                    yintercept=ytile<<TILESHIFT;
                                    xtile = (short) (xintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitHorizWall();
                                }
                            }
                            else
                            {
                                if(((uint32_t)xintercept>>16)==pwallx && ytile==pwally)
                                {
                                    texdelta = -(pwallposi<<10);
                                    yintercept=ytile<<TILESHIFT;
                                    xtile = (short) (xintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitHorizWall();
                                }
                                else
                                {
                                    if(pwalldir==di_east && (int32_t)((word)xintercept)+xstep>(pwallposi<<10)
                                            || pwalldir==di_west && (int32_t)((word)xintercept)+xstep<(pwallposi<<10))
                                        goto passhoriz;

                                    if(pwalldir==di_east)
                                        xintercept=(xintercept&0xffff0000)-((64-pwallpos)<<10);
                                    else
                                        xintercept=(xintercept&0xffff0000)+((64-pwallpos)<<10);
                                    yintercept=yintercept-((ystep*pwallpos)>>6);
                                    ytile = (short) (yintercept >> TILESHIFT);
                                    tilehit=pwalltile;
                                    HitVertWall();
                                }
                            }
                        }
#endif
                    }
                    else
                    {
                        yintercept=ytile<<TILESHIFT;
                        xtile = (short) (xintercept >> TILESHIFT);
                        HitHorizWall();
#ifdef DOUBLEHORIZ
						if (lowdetail)
						{
							wallheight[pixx+1] = wallheight[pixx];
							postwidth++;
							pixx++;
						}
#endif
                    }
                }
                break;
            }
passhoriz:
            *((byte *)spotvis+yspot)=1;
            ytile+=ytilestep;
            xintercept+=xstep;
            yspot=(word)((((uint32_t)xintercept>>16)<<mapshift)+ytile);
        }
        while(1);
    }
}
#endif


/*
====================
=
= WallRefresh
=
====================
*/

void WallRefresh (void)
{
#ifdef RAYCASTFAST
    xpartialdown = (viewx>>(16-FASTSHIFT))&(FASTGLOBAL-1);
    xpartialup = FASTGLOBAL-xpartialdown;
    ypartialdown = (viewy>>(16-FASTSHIFT))&(FASTGLOBAL-1);
    ypartialup = FASTGLOBAL-ypartialdown;
#else
    xpartialdown = viewx&(TILEGLOBAL-1);
    xpartialup = TILEGLOBAL-xpartialdown;
    ypartialdown = viewy&(TILEGLOBAL-1);
    ypartialup = TILEGLOBAL-ypartialdown;
#endif

#ifndef __AMIGA__
    min_wallheight = viewheight;
#endif
    lastside = -1;                  // the first pixel is on a new wall
    AsmRefresh ();
    ScalePost ();                   // no more optimization on last post
}

void CalcViewVariables()
{
    viewangle = player->angle;
    midangle = viewangle*(FINEANGLES/ANGLES);
    viewsin = sintable[viewangle];
    viewcos = costable[viewangle];
    viewx = player->x - FixedMul(focallength,viewcos);
    viewy = player->y + FixedMul(focallength,viewsin);
//bug("viewangle %03d midangle %04d px %d vx %d py %d vy %d\n", viewangle, midangle, player->x, viewx, player->y, viewy);
    focaltx = (short)(viewx>>TILESHIFT);
    focalty = (short)(viewy>>TILESHIFT);

#ifndef __AMIGA__
    viewtx = (short)(player->x >> TILESHIFT);
    viewty = (short)(player->y >> TILESHIFT);
#endif
}

//==========================================================================

/*
========================
=
= ThreeDRefresh
=
========================
*/

void    ThreeDRefresh (void)
{
//
// clear out the traced array
//
    memset(spotvis,0,maparea);
    spotvis[player->tilex][player->tiley] = 1;       // Detect all sprites over player fix

    /*vbuf = VL_LockSurface(screenBuffer);
    vbuf+=screenofs;
    vbufPitch = bufferPitch;*/

    CalcViewVariables();


//
// follow the walls from there to the right, drawing as we go
//
#ifdef USE_AUTOMAP
	if (automap)
		memset(vbuf, 0x19, viewwidth*viewheight);
	else
#endif
    VGAClearScreen ();
#if defined(USE_FEATUREFLAGS) && defined(USE_STARSKY)
    if(GetFeatureFlags() & FF_STARSKY)
        DrawStarSky(vbuf, vbufPitch);
#endif

#ifdef USE_AUTOMAP
	if (!automap)
#endif
    WallRefresh ();

#if defined(USE_FEATUREFLAGS) && defined(USE_PARALLAX)
    if(GetFeatureFlags() & FF_PARALLAXSKY)
        DrawParallax(vbuf, vbufPitch);
#endif
#if defined(USE_FEATUREFLAGS) && defined(USE_CLOUDSKY)
    if(GetFeatureFlags() & FF_CLOUDSKY)
        DrawClouds(vbuf, vbufPitch, min_wallheight);
#endif
#ifdef USE_FLOORCEILINGTEX
    DrawFloorAndCeiling(vbuf, vbufPitch, min_wallheight);
#endif


//
// draw all the scaled images
//
#ifdef USE_AUTOMAP
	if (!automap)
#endif
    DrawScaleds();                  // draw scaled stuff

#if defined(USE_FEATUREFLAGS) && defined(USE_RAIN)
    if(GetFeatureFlags() & FF_RAIN)
        DrawRain(vbuf, vbufPitch);
#endif
#if defined(USE_FEATUREFLAGS) && defined(USE_SNOW)
    if(GetFeatureFlags() & FF_SNOW)
        DrawSnow(vbuf, vbufPitch);
#endif

#ifdef USE_AUTOMAP
	if (!automap)
#endif
    DrawPlayerWeapon ();    // draw player's hands

#ifdef USE_AUTOMAP
	if (automap)
	{
#ifdef USE_HALFWIDTH
		word mwidth = viewwidth*2 / 8;
#else
		word mwidth = viewwidth / 8;
#endif
		word mheight = viewheight / 8;
		word ofsx = player->tilex;
		word ofsy = player->tiley;
		for(word i=0;i<mheight;i++)
		{
			for(word j=0;j<mwidth;j++)
			{
				int16_t mx = j + ofsx - mwidth/2;
				int16_t my = i + ofsy - mheight/2;
				if (mx < 0 || mx >= MAPSIZE || my < 0 || my >= MAPSIZE) continue;
				if (!mapvis[mx][my]) continue;
				word tile = tilemap[mx][my];
				if (!tile) continue;
				word wallpic = 0;
				if (tile & 0x80)
				{
					word doornum = tile & 0x7F;
					switch(doorobjlist[doornum].lock)
					{
						case dr_normal:
							wallpic = DOORWALL;
							break;
						case dr_lock1:
						case dr_lock2:
						case dr_lock3:
						case dr_lock4:
							wallpic = DOORWALL+6;
							break;
						case dr_elevator:
							wallpic = DOORWALL+4;
							break;
					}
				}
				else
				{
					tile &= 0x3F;
					// sky and elevator tiles need special treatment as they have different textures for the vertical and horizontal side
					if ((tile == 16 || tile == 21 || tile == 22) && (my == 0 || tilemap[mx][my-1]) && (my == MAPSIZE-1 || tilemap[mx][my+1]))
						wallpic = vertwall[tile];
					else
						wallpic = horizwall[tile];
				}

				SimpleScaleWallTile(j*8, i*8, wallpic, 8);
			}
		}
	}
#endif

#ifdef __AMIGA__
	VL_DrawChunkyBuffer();
	if (automap)
	{
#ifdef USE_HALFWIDTH
		word mwidth = viewwidth*2 / 8;
#else
		word mwidth = viewwidth / 8;
#endif
		word mheight = viewheight / 8;
		// simple player marker
		short angle = player->angle;
		if (angle < 0) angle += ANGLES;
		angle = (angle + ANGLES/8);
		if (angle >= ANGLES) angle -= ANGLES;
		angle = angle * 4 / ANGLES;
		// TODO add arrow symbols with 8 rotations
		const char *anglechars[] = {">","^","<", "v"};
		fontcolor = 0;
		fontnumber = 0;
		px = viewscreenx + (mwidth)/2*8;
		py = viewscreeny + (mheight)/2*8;
		CA_CacheGrChunk (STARTFONT + fontnumber);
		VWB_DrawPropString(anglechars[angle]);
	}
#endif

    if(Keyboard[sc_Tab] && viewsize == 21 && gamestate.weapon != -1)
        ShowActStatus();

    /*VL_UnlockSurface(screenBuffer);
    vbuf = NULL;*/

//
// show screen and time last cycle
//

    if (fizzlein)
    {
        FizzleFade(screenBuffer, 0, 0, screenWidth, screenHeight, 20, false);
        fizzlein = false;

        lasttimecount = GetTimeCount();          // don't make a big tic count
    }
    else
    {
#ifndef REMDEBUG
        if (fpscounter)
        {
            fontnumber = 0;
            SETFONTCOLOR(7,127);

            PrintX=4; PrintY=1;
            VWB_Bar(0,0,50,10,bordercol);

            US_PrintSigned(fps);
#ifdef FRAME_TIME
            US_Print(" ms");
#else
            US_Print(" fps");
#endif
        }
#endif
#ifdef __AMIGA__
		//VH_UpdateScreen();
#else
        SDL_BlitSurface(screenBuffer, NULL, screen, NULL);
        SDL_Flip(screen);
#endif
    }
#ifdef USE_DOUBLEBUFFER
	displayofs = bufferofs;
	VL_ChangeScreenBuffer();
	bufferofs ^= 1;
	//printf("%s:%d bufferofs %d displayofs %d\n", __FUNCTION__, __LINE__, bufferofs, displayofs);
#endif

#ifndef REMDEBUG
    if (fpscounter)
    {
#ifdef __AMIGA__
		// this is more accurate, especially at low fps
		int current = SDL_GetTicks();
		if (fps_time == 0 || fps_time >= current)
		{
#ifdef FRAME_TIME
			fps_time = current-1;
#else
			fps_time = 1000;
#endif
		}
#ifdef FRAME_TIME
		fps = (current-fps_time); // frame time
#else
		fps = 1000/(current-fps_time);
#endif
		fps_time = current;
#else
        fps_frames++;
        fps_time+=tics;

        if(fps_time>35)
        {
            fps_time-=35;
            fps=fps_frames<<1;
            fps_frames=0;
        }
#endif
    }
#endif
}

#ifdef __AMIGA__
static int16_t AngleFromSlope(fixed y, fixed x)
{
	return tantoangle[(y*(TANTABLESIZE-1))/x];
}

static int16_t PointToAngle(fixed x, fixed y)
{
	if (!x && !y)
		return 0;

	if (x >= 0)
	{
		if (y >= 0)
		{
			if (x > y)
				return AngleFromSlope(y, x);	/* octant 0*/
			else
				return ANG90 - 1 - AngleFromSlope(x, y);	/* octant 1 */
		}
		else
		{
			y = -y;
			if (x > y)
				return FINEANGLES - 1 - AngleFromSlope(y, x);	/* octant 8 */
			else
				return ANG270 + AngleFromSlope(x, y);	/* octant 7 */
		}
	}
	else
	{
		x = -x;
		if (y >= 0)
		{
			if (x > y)
				return ANG180 - 1 - AngleFromSlope(y, x);	/* octant 3 */
			else
				return ANG90 + AngleFromSlope(x, y);		/* octant 2 */
		}
		else
		{
			y = -y;
			if (x > y)
				return ANG180 + AngleFromSlope(y, x);	/* octant 4 */
			else
				return ANG270 - 1 - AngleFromSlope(x, y);	/* octant 5 */
		}
	}

	return 0;
}

int16_t ATan2(fixed y, fixed x)
{
	int16_t ang = PointToAngle(x, y);
	// convert from fine to angles
	return (ang + 5) / 10;
}
#endif

//
//	ID Engine
//	ID_IN.c - Input Manager
//	v1.0d1
//	By Jason Blochowiak
//

//
//	This module handles dealing with the various input devices
//
//	Depends on: Memory Mgr (for demo recording), Sound Mgr (for timing stuff),
//				User Mgr (for command line parms)
//
//	Globals:
//		LastScan - The keyboard scan code of the last key pressed
//		LastASCII - The ASCII value of the last key pressed
//	DEBUG - there are more globals
//

#ifdef __AMIGA__
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/lowlevel.h>
#include <proto/keymap.h>
#include <devices/input.h>
#include <SDI_interrupt.h>
#endif
#define Point W3DPoint
#undef SIGN
#include "wl_def.h"


/*
=============================================================================

					GLOBAL VARIABLES

=============================================================================
*/


//
// configuration variables
//
boolean MousePresent;
boolean forcegrabmouse;


// 	Global variables
#ifdef __AMIGA__
volatile boolean    Keyboard[128];
#else
volatile boolean    Keyboard[SDLK_LAST];
#endif
volatile boolean	Paused;
volatile char		LastASCII;
volatile ScanCode	LastScan;

//KeyboardDef	KbdDefs = {0x1d,0x38,0x47,0x48,0x49,0x4b,0x4d,0x4f,0x50,0x51};
static KeyboardDef KbdDefs = {
    sc_Control,             // button0
    sc_Alt,                 // button1
    sc_Home,                // upleft
    sc_UpArrow,             // up
    sc_PgUp,                // upright
    sc_LeftArrow,           // left
    sc_RightArrow,          // right
    sc_End,                 // downleft
    sc_DownArrow,           // down
    sc_PgDn                 // downright
};

#ifndef __AMIGA__
static SDL_Joystick *Joystick;
#endif
int JoyNumButtons;
static int JoyNumHats;

static bool GrabInput = false;

// from id_vl.c
extern struct Window *window;

/*
=============================================================================

					LOCAL VARIABLES

=============================================================================
*/
#ifdef __AMIGA__
static ULONG joyPort = 1;
static uint16_t g_mouseButtonState;
static int16_t g_mx = 0;
static int16_t g_my = 0;
static struct MsgPort *g_inputPort = NULL;
static struct IOStdReq *g_inputReq = NULL;
byte ASCIINames[128]; // Unshifted ASCII for scan codes
byte ShiftNames[128]; // Shifted ASCII for scan codes
#else
byte        ASCIINames[] =		// Unshifted ASCII for scan codes       // TODO: keypad
{
//	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,8  ,9  ,0  ,0  ,0  ,13 ,0  ,0  ,	// 0
    0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,27 ,0  ,0  ,0  ,	// 1
	' ',0  ,0  ,0  ,0  ,0  ,0  ,39 ,0  ,0  ,'*','+',',','-','.','/',	// 2
	'0','1','2','3','4','5','6','7','8','9',0  ,';',0  ,'=',0  ,0  ,	// 3
	'`','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',	// 4
	'p','q','r','s','t','u','v','w','x','y','z','[',92 ,']',0  ,0  ,	// 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0		// 7
};
byte ShiftNames[] =		// Shifted ASCII for scan codes
{
//	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,8  ,9  ,0  ,0  ,0  ,13 ,0  ,0  ,	// 0
    0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,27 ,0  ,0  ,0  ,	// 1
	' ',0  ,0  ,0  ,0  ,0  ,0  ,34 ,0  ,0  ,'*','+','<','_','>','?',	// 2
	')','!','@','#','$','%','^','&','*','(',0  ,':',0  ,'+',0  ,0  ,	// 3
	'~','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',	// 4
	'P','Q','R','S','T','U','V','W','X','Y','Z','{','|','}',0  ,0  ,	// 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0		// 7
};
byte SpecialNames[] =	// ASCII for 0xe0 prefixed codes
{
//	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 0
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,13 ,0  ,0  ,0  ,	// 1
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 2
	0  ,0  ,0  ,0  ,0  ,'/',0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 3
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 4
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0   	// 7
};
#endif


static	boolean		IN_Started;

static	Direction	DirTable[] =		// Quick lookup for total direction
{
    dir_NorthWest,	dir_North,	dir_NorthEast,
    dir_West,		dir_None,	dir_East,
    dir_SouthWest,	dir_South,	dir_SouthEast
};


///////////////////////////////////////////////////////////////////////////
//
//	INL_GetMouseButtons() - Gets the status of the mouse buttons from the
//		mouse driver
//
///////////////////////////////////////////////////////////////////////////
static int
INL_GetMouseButtons(void)
{
#ifdef __AMIGA__
	return g_mouseButtonState;
#else
    int buttons = SDL_GetMouseState(NULL, NULL);
    int middlePressed = buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE);
    int rightPressed = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
    buttons &= ~(SDL_BUTTON(SDL_BUTTON_MIDDLE) | SDL_BUTTON(SDL_BUTTON_RIGHT));
    if(middlePressed) buttons |= 1 << 2;
    if(rightPressed) buttons |= 1 << 1;

    return buttons;
#endif
}

#ifdef __AMIGA__
void IN_GetMouseDelta(int *dx,int *dy)
{
    *dx = g_mx;
    *dy = g_my;
}
#endif

///////////////////////////////////////////////////////////////////////////
//
//	IN_GetJoyDelta() - Returns the relative movement of the specified
//		joystick (from +/-127)
//
///////////////////////////////////////////////////////////////////////////
void IN_GetJoyDelta(int *dx,int *dy)
{
#ifdef __AMIGA__
	ULONG portState;
	int x = 0;
	int y = 0;

	portState = ReadJoyPort(joyPort);

	//if (((portState & JP_TYPE_MASK) == JP_TYPE_GAMECTLR) || ((portState & JP_TYPE_MASK) == JP_TYPE_JOYSTK))
	{
		if (portState & JPF_JOY_UP)
			y = -128;
		else if (portState & JPF_JOY_DOWN)
			y = 127;

		if (portState & JPF_JOY_LEFT)
			x = -128;
		else if (portState & JPF_JOY_RIGHT)
			x = 127;
	}
#else
    if(!Joystick)
    {
        *dx = *dy = 0;
        return;
    }

    SDL_JoystickUpdate();
#ifdef _arch_dreamcast
    int x = 0;
    int y = 0;
#else
    int x = SDL_JoystickGetAxis(Joystick, 0) >> 8;
    int y = SDL_JoystickGetAxis(Joystick, 1) >> 8;
#endif

    if(param_joystickhat != -1)
    {
        uint8_t hatState = SDL_JoystickGetHat(Joystick, param_joystickhat);
        if(hatState & SDL_HAT_RIGHT)
            x += 127;
        else if(hatState & SDL_HAT_LEFT)
            x -= 127;
        if(hatState & SDL_HAT_DOWN)
            y += 127;
        else if(hatState & SDL_HAT_UP)
            y -= 127;

        if(x < -128) x = -128;
        else if(x > 127) x = 127;

        if(y < -128) y = -128;
        else if(y > 127) y = 127;
    }
#endif

    *dx = x;
    *dy = y;
}

///////////////////////////////////////////////////////////////////////////
//
//	IN_GetJoyFineDelta() - Returns the relative movement of the specified
//		joystick without dividing the results by 256 (from +/-127)
//
///////////////////////////////////////////////////////////////////////////
#ifdef _arch_dreamcast
void IN_GetJoyFineDelta(int *dx, int *dy) // dreamcast
{
    if(!Joystick)
    {
        *dx = 0;
        *dy = 0;
        return;
    }

    SDL_JoystickUpdate();
    int x = SDL_JoystickGetAxis(Joystick, 0);
    int y = SDL_JoystickGetAxis(Joystick, 1);

    if(x < -128) x = -128;
    else if(x > 127) x = 127;

    if(y < -128) y = -128;
    else if(y > 127) y = 127;

    *dx = x;
    *dy = y;
}
#endif

/*
===================
=
= IN_JoyButtons
=
===================
*/

int IN_JoyButtons()
{
#ifdef __AMIGA__
	int res = 0;
	ULONG portState;

	portState = ReadJoyPort(joyPort);

	//if (((portState & JP_TYPE_MASK) == JP_TYPE_GAMECTLR) || ((portState & JP_TYPE_MASK) == JP_TYPE_JOYSTK))
	{
		if (portState & JPF_BUTTON_RED) res |= 1 << 0;
		if (portState & JPF_BUTTON_BLUE) res |= 1 << 1;
		if (portState & JPF_BUTTON_YELLOW) res |= 1 << 2;
		if (portState & JPF_BUTTON_GREEN) res |= 1 << 3;
		if (portState & JPF_BUTTON_REVERSE) res |= 1 << 4;
		if (portState & JPF_BUTTON_FORWARD) res |= 1 << 5;
		if (portState & JPF_BUTTON_PLAY) res |= 1 << 6;
	}
#else
    if(!Joystick) return 0;

    SDL_JoystickUpdate();

    int res = 0;
    for(int i = 0; i < JoyNumButtons && i < 32; i++)
        res |= SDL_JoystickGetButton(Joystick, i) << i;
#endif
    return res;
}

boolean IN_JoyPresent()
{
#ifdef __AMIGA__
	return true;
#else
    return Joystick != NULL;
#endif
}

#ifdef __AMIGA__
HANDLERPROTO(BEL_ST_InputHandler, struct InputEvent *, struct InputEvent *moo, APTR id)
{
	struct InputEvent *coin;
	int scanCode, isRelease;
	//extern struct Window *window;

	//if (!window || !(window->Flags & WFLG_WINDOWACTIVE) || (window->WScreen && window->WScreen != IntuitionBase->FirstScreen))
	//	return moo;

	for (coin = moo; coin; coin = coin->ie_NextEvent)
	{
		isRelease = coin->ie_Code & IECODE_UP_PREFIX;
		scanCode = coin->ie_Code & ~IECODE_UP_PREFIX;

		// keyboard
		if (coin->ie_Class == IECLASS_RAWKEY)
		{
			if (isRelease)
			{
				Keyboard[scanCode] = 0;
			}
			else
			{
				UWORD sym = scanCode;
				UWORD mod = coin->ie_Qualifier;

				if(mod & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
				{
					if(sym < lengthof(ShiftNames) && ShiftNames[sym])
						LastASCII = ShiftNames[sym];
				}
				else
				{
					if(sym < lengthof(ASCIINames) && ASCIINames[sym])
						LastASCII = ASCIINames[sym];
				}

				LastScan = sym;
				Keyboard[sym] = 1;
				if (LastScan == RAWKEY_HELP)
					Paused = 1;
			}
		}

		// mouse
		else if ((coin->ie_Class == IECLASS_RAWMOUSE))
		{
			switch (scanCode)
			{
				case IECODE_LBUTTON:
					if (!isRelease)
						g_mouseButtonState |= 1;
					else
						g_mouseButtonState &= ~1;
					break;
				case IECODE_RBUTTON:
					if (!isRelease)
						g_mouseButtonState |= 2;
					else
						g_mouseButtonState &= ~2;
					break;
				case IECODE_MBUTTON:
					if (!isRelease)
						g_mouseButtonState |= 4;
					else
						g_mouseButtonState &= ~4;
					break;
			}
			g_mx += coin->ie_position.ie_xy.ie_x;
			g_my += coin->ie_position.ie_xy.ie_y;
		}
	}

	return moo;
}
MakeInterruptPri(g_inputHandler, BEL_ST_InputHandler, "Wolfenstein 3D", NULL, 100);
#else
static void processEvent(SDL_Event *event)
{
    switch (event->type)
    {
        // exit if the window is closed
        case SDL_QUIT:
            Quit(NULL);

        // check for keypresses
        case SDL_KEYDOWN:
        {
            if(event->key.keysym.sym==SDLK_SCROLLOCK || event->key.keysym.sym==SDLK_F12)
            {
                GrabInput = !GrabInput;
                SDL_WM_GrabInput(GrabInput ? SDL_GRAB_ON : SDL_GRAB_OFF);
                return;
            }

            LastScan = event->key.keysym.sym;
            SDLMod mod = SDL_GetModState();
            if(Keyboard[sc_Alt])
            {
                if(LastScan==SDLK_F4)
                    Quit(NULL);
            }

            if(LastScan == SDLK_KP_ENTER) LastScan = SDLK_RETURN;
            else if(LastScan == SDLK_RSHIFT) LastScan = SDLK_LSHIFT;
            else if(LastScan == SDLK_RALT) LastScan = SDLK_LALT;
            else if(LastScan == SDLK_RCTRL) LastScan = SDLK_LCTRL;
            else
            {
                if((mod & KMOD_NUM) == 0)
                {
                    switch(LastScan)
                    {
                        case SDLK_KP2: LastScan = SDLK_DOWN; break;
                        case SDLK_KP4: LastScan = SDLK_LEFT; break;
                        case SDLK_KP6: LastScan = SDLK_RIGHT; break;
                        case SDLK_KP8: LastScan = SDLK_UP; break;
                    }
                }
            }

            int sym = LastScan;
            if(sym >= 'a' && sym <= 'z')
                sym -= 32;  // convert to uppercase

            if(mod & (KMOD_SHIFT | KMOD_CAPS))
            {
                if(sym < lengthof(ShiftNames) && ShiftNames[sym])
                    LastASCII = ShiftNames[sym];
            }
            else
            {
                if(sym < lengthof(ASCIINames) && ASCIINames[sym])
                    LastASCII = ASCIINames[sym];
            }
            if(LastScan<SDLK_LAST)
                Keyboard[LastScan] = 1;
            if(LastScan == SDLK_PAUSE)
                Paused = true;
            break;
		}

        case SDL_KEYUP:
        {
            int key = event->key.keysym.sym;
            if(key == SDLK_KP_ENTER) key = SDLK_RETURN;
            else if(key == SDLK_RSHIFT) key = SDLK_LSHIFT;
            else if(key == SDLK_RALT) key = SDLK_LALT;
            else if(key == SDLK_RCTRL) key = SDLK_LCTRL;
            else
            {
                if((SDL_GetModState() & KMOD_NUM) == 0)
                {
                    switch(key)
                    {
                        case SDLK_KP2: key = SDLK_DOWN; break;
                        case SDLK_KP4: key = SDLK_LEFT; break;
                        case SDLK_KP6: key = SDLK_RIGHT; break;
                        case SDLK_KP8: key = SDLK_UP; break;
                    }
                }
            }

            if(key<SDLK_LAST)
                Keyboard[key] = 0;
            break;
        }

#if defined(GP2X)
        case SDL_JOYBUTTONDOWN:
            GP2X_ButtonDown(event->jbutton.button);
            break;

        case SDL_JOYBUTTONUP:
            GP2X_ButtonUp(event->jbutton.button);
            break;
#endif
    }
}
#endif

void IN_WaitAndProcessEvents()
{
#ifdef __AMIGA__
	SDL_Delay(5);
#else
    SDL_Event event;
    if(!SDL_WaitEvent(&event)) return;
    do
    {
        processEvent(&event);
    }
    while(SDL_PollEvent(&event));
#endif
}

void IN_ProcessEvents()
{
#ifdef __AMIGA__
	// nothing to do here
#else
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        processEvent(&event);
    }
#endif
}


///////////////////////////////////////////////////////////////////////////
//
//	IN_Startup() - Starts up the Input Mgr
//
///////////////////////////////////////////////////////////////////////////
void
IN_Startup(void)
{
	if (IN_Started)
		return;

    IN_ClearKeysDown();

#ifdef __AMIGA__
	JoyNumButtons = 7;
	JoyNumHats = 0;

	//ModifyIDCMP(window, IDCMP_RAWKEY/*|IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS|IDCMP_DELTAMOVE*/);
	if ((g_inputPort = CreateMsgPort()))
	{
		if ((g_inputReq = (struct IOStdReq *)CreateIORequest(g_inputPort, sizeof(*g_inputReq))))
		{
			if (!OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)g_inputReq, 0))
			{
				g_inputReq->io_Data = (void *)&g_inputHandler;
				g_inputReq->io_Command = IND_ADDHANDLER;
				if (!DoIO((struct IORequest *)g_inputReq))
				{
					//return true;
				}
			}
		}
	}

	for (int i = 0; i < 128; i++)
	{
		struct InputEvent ie;
		UBYTE bufascii;

		ie.ie_Class = IECLASS_RAWKEY;
		ie.ie_SubClass = 0;
		ie.ie_Code = i;
		ie.ie_Qualifier = 0;
		ie.ie_EventAddress = NULL;

		if (MapRawKey(&ie, (STRPTR)&bufascii, sizeof(bufascii), NULL) > 0)
		{
			ASCIINames[i] = bufascii;
			//printf("%s rawkey %x name %c\n", __FUNCTION__, i, bufascii);
		}
		else
		{
			ASCIINames[i] = 0;
		}

		ie.ie_Qualifier = IEQUALIFIER_LSHIFT;
		if (MapRawKey(&ie, (STRPTR)&bufascii, sizeof(bufascii), NULL) > 0)
		{
			ShiftNames[i] = bufascii;
			//printf("%s rawkey %x shifted name %c\n", __FUNCTION__, i, bufascii);
		}
		else
		{
			ShiftNames[i] = 0;
		}
	}

	GrabInput = true;

    if(param_joystickindex >= 0 && param_joystickindex < 2)
    {
        joyPort = param_joystickindex;
    }
#else
    if(param_joystickindex >= 0 && param_joystickindex < SDL_NumJoysticks())
    {
        Joystick = SDL_JoystickOpen(param_joystickindex);
        if(Joystick)
        {
            JoyNumButtons = SDL_JoystickNumButtons(Joystick);
            if(JoyNumButtons > 32) JoyNumButtons = 32;      // only up to 32 buttons are supported
            JoyNumHats = SDL_JoystickNumHats(Joystick);
            if(param_joystickhat < -1 || param_joystickhat >= JoyNumHats)
                Quit("The joystickhat param must be between 0 and %i!", JoyNumHats - 1);
        }
    }

    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

    /*if(fullscreen || forcegrabmouse)
    {
        GrabInput = true;
        SDL_WM_GrabInput(SDL_GRAB_ON);
    }*/
#endif

    // I didn't find a way to ask libSDL whether a mouse is present, yet...
#if defined(GP2X)
    MousePresent = false;
#elif defined(_arch_dreamcast)
    MousePresent = DC_MousePresent();
#else
    MousePresent = true;
#endif

    IN_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//	IN_Shutdown() - Shuts down the Input Mgr
//
///////////////////////////////////////////////////////////////////////////
void
IN_Shutdown(void)
{
	if (!IN_Started)
		return;

#ifdef __AMIGA__
	if (g_inputReq)
	{
		g_inputReq->io_Data = (void *)&g_inputHandler;
		g_inputReq->io_Command = IND_REMHANDLER;
		DoIO((struct IORequest *)g_inputReq);
		CloseDevice((struct IORequest *)g_inputReq);
		DeleteIORequest((struct IORequest *)g_inputReq);
		g_inputReq = NULL;
	}

	if (g_inputPort)
	{
		DeleteMsgPort(g_inputPort);
		g_inputPort = NULL;
	}
#else
    if(Joystick)
        SDL_JoystickClose(Joystick);
#endif

	IN_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//	IN_ClearKeysDown() - Clears the keyboard array
//
///////////////////////////////////////////////////////////////////////////
void
IN_ClearKeysDown(void)
{
	LastScan = sc_None;
	LastASCII = key_None;
	memset ((void *) Keyboard,0,sizeof(Keyboard));
}


///////////////////////////////////////////////////////////////////////////
//
//	IN_ReadControl() - Reads the device associated with the specified
//		player and fills in the control info struct
//
///////////////////////////////////////////////////////////////////////////
void
IN_ReadControl(int player,ControlInfo *info)
{
	word		buttons;
	int			dx,dy;
	Motion		mx,my;

	dx = dy = 0;
	mx = my = motion_None;
	buttons = 0;

	IN_ProcessEvents();

    if (Keyboard[KbdDefs.upleft])
        mx = motion_Left,my = motion_Up;
    else if (Keyboard[KbdDefs.upright])
        mx = motion_Right,my = motion_Up;
    else if (Keyboard[KbdDefs.downleft])
        mx = motion_Left,my = motion_Down;
    else if (Keyboard[KbdDefs.downright])
        mx = motion_Right,my = motion_Down;

    if (Keyboard[KbdDefs.up])
        my = motion_Up;
    else if (Keyboard[KbdDefs.down])
        my = motion_Down;

    if (Keyboard[KbdDefs.left])
        mx = motion_Left;
    else if (Keyboard[KbdDefs.right])
        mx = motion_Right;

    if (Keyboard[KbdDefs.button0])
        buttons += 1 << 0;
    if (Keyboard[KbdDefs.button1])
        buttons += 1 << 1;

	dx = mx * 127;
	dy = my * 127;

	info->x = dx;
	info->xaxis = mx;
	info->y = dy;
	info->yaxis = my;
	info->button0 = (buttons & (1 << 0)) != 0;
	info->button1 = (buttons & (1 << 1)) != 0;
	info->button2 = (buttons & (1 << 2)) != 0;
	info->button3 = (buttons & (1 << 3)) != 0;
	info->dir = DirTable[((my + 1) * 3) + (mx + 1)];
}

///////////////////////////////////////////////////////////////////////////
//
//	IN_WaitForKey() - Waits for a scan code, then clears LastScan and
//		returns the scan code
//
///////////////////////////////////////////////////////////////////////////
ScanCode
IN_WaitForKey(void)
{
	ScanCode	result;

	while ((result = LastScan)==0)
		IN_WaitAndProcessEvents();
	LastScan = 0;
	return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//	IN_WaitForASCII() - Waits for an ASCII char, then clears LastASCII and
//		returns the ASCII value
//
///////////////////////////////////////////////////////////////////////////
char
IN_WaitForASCII(void)
{
	char		result;

	while ((result = LastASCII)==0)
		IN_WaitAndProcessEvents();
	LastASCII = '\0';
	return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//	IN_Ack() - waits for a button or key press.  If a button is down, upon
// calling, it must be released for it to be recognized
//
///////////////////////////////////////////////////////////////////////////

boolean	btnstate[NUMBUTTONS];

void IN_StartAck(void)
{
    IN_ProcessEvents();
//
// get initial state of everything
//
	IN_ClearKeysDown();
	memset(btnstate, 0, sizeof(btnstate));

	int buttons = IN_JoyButtons() << 4;

	if(MousePresent)
		buttons |= IN_MouseButtons();

	for(int i = 0; i < NUMBUTTONS; i++, buttons >>= 1)
		if(buttons & 1)
			btnstate[i] = true;
}


boolean IN_CheckAck (void)
{
    IN_ProcessEvents();
//
// see if something has been pressed
//
	if(LastScan)
		return true;

	int buttons = IN_JoyButtons() << 4;

	if(MousePresent)
		buttons |= IN_MouseButtons();

	for(int i = 0; i < NUMBUTTONS; i++, buttons >>= 1)
	{
		if(buttons & 1)
		{
			if(!btnstate[i])
            {
                // Wait until button has been released
                do
                {
                    IN_WaitAndProcessEvents();
                    buttons = IN_JoyButtons() << 4;

                    if(MousePresent)
                        buttons |= IN_MouseButtons();
                }
                while(buttons & (1 << i));

				return true;
            }
		}
		else
			btnstate[i] = false;
	}

	return false;
}


void IN_Ack (void)
{
	IN_StartAck ();

    do
    {
        IN_WaitAndProcessEvents();
    }
	while(!IN_CheckAck ());
}


///////////////////////////////////////////////////////////////////////////
//
//	IN_UserInput() - Waits for the specified delay time (in ticks) or the
//		user pressing a key or a mouse button. If the clear flag is set, it
//		then either clears the key or waits for the user to let the mouse
//		button up.
//
///////////////////////////////////////////////////////////////////////////
boolean IN_UserInput(longword delay)
{
	longword	lasttime;

	lasttime = GetTimeCount();
	IN_StartAck ();
	do
	{
        IN_ProcessEvents();
		if (IN_CheckAck())
			return true;
        SDL_Delay(5);
	} while (GetTimeCount() - lasttime < delay);
	return(false);
}

//===========================================================================

/*
===================
=
= IN_MouseButtons
=
===================
*/
int IN_MouseButtons (void)
{
	if (MousePresent)
		return INL_GetMouseButtons();
	else
		return 0;
}

bool IN_IsInputGrabbed()
{
    return GrabInput;
}

void IN_CenterMouse()
{
#ifdef __AMIGA__
	g_mx = g_my = 0;
#else
    SDL_WarpMouse(screenWidth / 2, screenHeight / 2);
#endif
}

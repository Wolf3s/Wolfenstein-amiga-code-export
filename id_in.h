//
//	ID Engine
//	ID_IN.h - Header file for Input Manager
//	v1.0d1
//	By Jason Blochowiak
//

#ifndef	__ID_IN__
#define	__ID_IN__

#ifdef	__DEBUG__
#define	__DEBUG_InputMgr__
#endif

typedef	int		ScanCode;
#define	sc_None			0
#define	sc_Bad			0xff
#ifdef __AMIGA__
#include "rawkeycodes.h"
// RAWKEY_TILDE clases with sc_None and key_None

#define	sc_Return		RAWKEY_RETURN
#define	sc_Enter		sc_Return
#define	sc_Escape		RAWKEY_ESCAPE
#define	sc_Space		RAWKEY_SPACE
#define	sc_BackSpace	RAWKEY_BACKSPACE
#define	sc_Tab			RAWKEY_TAB
#define	sc_Alt			RAWKEY_LALT
#define	sc_Control		RAWKEY_LCONTROL
#define	sc_CapsLock		RAWKEY_CAPSLOCK
#define	sc_LShift		RAWKEY_LSHIFT
#define	sc_RShift		RAWKEY_RSHIFT
#define	sc_UpArrow		RAWKEY_UP
#define	sc_DownArrow	RAWKEY_DOWN
#define	sc_LeftArrow	RAWKEY_LEFT
#define	sc_RightArrow	RAWKEY_RIGHT
#define	sc_Insert		RAWKEY_INSERT
#define	sc_Delete		RAWKEY_DELETE
#define	sc_Home			RAWKEY_HOME
#define	sc_End			RAWKEY_END
#define	sc_PgUp			RAWKEY_PAGEUP
#define	sc_PgDn			RAWKEY_PAGEDOWN
#define	sc_F1			RAWKEY_F1
#define	sc_F2			RAWKEY_F2
#define	sc_F3			RAWKEY_F3
#define	sc_F4			RAWKEY_F4
#define	sc_F5			RAWKEY_F5
#define	sc_F6			RAWKEY_F6
#define	sc_F7			RAWKEY_F7
#define	sc_F8			RAWKEY_F8
#define	sc_F9			RAWKEY_F9
#define	sc_F10			RAWKEY_F10
#define	sc_F11			RAWKEY_F11
#define	sc_F12			RAWKEY_F12

#define sc_ScrollLock		RAWKEY_SCROLLOCK
#define sc_PrintScreen		RAWKEY_PRINT

#define	sc_1			RAWKEY_1
#define	sc_2			RAWKEY_2
#define	sc_3			RAWKEY_3
#define	sc_4			RAWKEY_4
#define	sc_5			RAWKEY_5
#define	sc_6			RAWKEY_6
#define	sc_7			RAWKEY_7
#define	sc_8			RAWKEY_8
#define	sc_9			RAWKEY_9
#define	sc_0			RAWKEY_0

#define	sc_A			RAWKEY_A
#define	sc_B			RAWKEY_B
#define	sc_C			RAWKEY_C
#define	sc_D			RAWKEY_D
#define	sc_E			RAWKEY_E
#define	sc_F			RAWKEY_F
#define	sc_G			RAWKEY_G
#define	sc_H			RAWKEY_H
#define	sc_I			RAWKEY_I
#define	sc_J			RAWKEY_J
#define	sc_K			RAWKEY_K
#define	sc_L			RAWKEY_L
#define	sc_M			RAWKEY_M
#define	sc_N			RAWKEY_N
#define	sc_O			RAWKEY_O
#define	sc_P			RAWKEY_P
#define	sc_Q			RAWKEY_Q
#define	sc_R			RAWKEY_R
#define	sc_S			RAWKEY_S
#define	sc_T			RAWKEY_T
#define	sc_U			RAWKEY_U
#define	sc_V			RAWKEY_V
#define	sc_W			RAWKEY_W
#define	sc_X			RAWKEY_X
#define	sc_Y			RAWKEY_Y
#define	sc_Z			RAWKEY_Z

#else
#define	sc_Return		SDLK_RETURN
#define	sc_Enter		sc_Return
#define	sc_Escape		SDLK_ESCAPE
#define	sc_Space		SDLK_SPACE
#define	sc_BackSpace	SDLK_BACKSPACE
#define	sc_Tab			SDLK_TAB
#define	sc_Alt			SDLK_LALT
#define	sc_Control		SDLK_LCTRL
#define	sc_CapsLock		SDLK_CAPSLOCK
#define	sc_LShift		SDLK_LSHIFT
#define	sc_RShift		SDLK_RSHIFT
#define	sc_UpArrow		SDLK_UP
#define	sc_DownArrow	SDLK_DOWN
#define	sc_LeftArrow	SDLK_LEFT
#define	sc_RightArrow	SDLK_RIGHT
#define	sc_Insert		SDLK_INSERT
#define	sc_Delete		SDLK_DELETE
#define	sc_Home			SDLK_HOME
#define	sc_End			SDLK_END
#define	sc_PgUp			SDLK_PAGEUP
#define	sc_PgDn			SDLK_PAGEDOWN
#define	sc_F1			SDLK_F1
#define	sc_F2			SDLK_F2
#define	sc_F3			SDLK_F3
#define	sc_F4			SDLK_F4
#define	sc_F5			SDLK_F5
#define	sc_F6			SDLK_F6
#define	sc_F7			SDLK_F7
#define	sc_F8			SDLK_F8
#define	sc_F9			SDLK_F9
#define	sc_F10			SDLK_F10
#define	sc_F11			SDLK_F11
#define	sc_F12			SDLK_F12

#define sc_ScrollLock		SDLK_SCROLLOCK
#define sc_PrintScreen		SDLK_PRINT

#define	sc_1			SDLK_1
#define	sc_2			SDLK_2
#define	sc_3			SDLK_3
#define	sc_4			SDLK_4
#define	sc_5			SDLK_5
#define	sc_6			SDLK_6
#define	sc_7			SDLK_7
#define	sc_8			SDLK_8
#define	sc_9			SDLK_9
#define	sc_0			SDLK_0

#define	sc_A			SDLK_a
#define	sc_B			SDLK_b
#define	sc_C			SDLK_c
#define	sc_D			SDLK_d
#define	sc_E			SDLK_e
#define	sc_F			SDLK_f
#define	sc_G			SDLK_g
#define	sc_H			SDLK_h
#define	sc_I			SDLK_i
#define	sc_J			SDLK_j
#define	sc_K			SDLK_k
#define	sc_L			SDLK_l
#define	sc_M			SDLK_m
#define	sc_N			SDLK_n
#define	sc_O			SDLK_o
#define	sc_P			SDLK_p
#define	sc_Q			SDLK_q
#define	sc_R			SDLK_r
#define	sc_S			SDLK_s
#define	sc_T			SDLK_t
#define	sc_U			SDLK_u
#define	sc_V			SDLK_v
#define	sc_W			SDLK_w
#define	sc_X			SDLK_x
#define	sc_Y			SDLK_y
#define	sc_Z			SDLK_z
#endif

#define	key_None		0

typedef	enum		{
						demo_Off,demo_Record,demo_Playback,demo_PlayDone
					} Demo;
typedef	enum		{
						ctrl_Keyboard,
						ctrl_Keyboard1 = ctrl_Keyboard,ctrl_Keyboard2,
						ctrl_Joystick,
						ctrl_Joystick1 = ctrl_Joystick,ctrl_Joystick2,
						ctrl_Mouse
					} ControlType;
typedef	enum		{
						motion_Left = -1,motion_Up = -1,
						motion_None = 0,
						motion_Right = 1,motion_Down = 1
					} Motion;
typedef	enum		{
						dir_North,dir_NorthEast,
						dir_East,dir_SouthEast,
						dir_South,dir_SouthWest,
						dir_West,dir_NorthWest,
						dir_None
					} Direction;
typedef	struct		{
						boolean		button0,button1,button2,button3;
						short		x,y;
						Motion		xaxis,yaxis;
						Direction	dir;
					} CursorInfo;
typedef	CursorInfo	ControlInfo;
typedef	struct		{
						ScanCode	button0,button1,
									upleft,		up,		upright,
									left,				right,
									downleft,	down,	downright;
					} KeyboardDef;
typedef	struct		{
						word		joyMinX,joyMinY,
									threshMinX,threshMinY,
									threshMaxX,threshMaxY,
									joyMaxX,joyMaxY,
									joyMultXL,joyMultYL,
									joyMultXH,joyMultYH;
					} JoystickDef;
// Global variables
extern  volatile boolean    Keyboard[];
extern           boolean    MousePresent;
extern  volatile boolean    Paused;
extern  volatile char       LastASCII;
extern  volatile ScanCode   LastScan;
extern           int        JoyNumButtons;
extern           boolean    forcegrabmouse;


// Function prototypes
#define	IN_KeyDown(code)	(Keyboard[(code)])
#define	IN_ClearKey(code)	{Keyboard[code] = false;\
							if (code == LastScan) LastScan = sc_None;}

// DEBUG - put names in prototypes
extern	void		IN_Startup(void),IN_Shutdown(void);
extern	void		IN_ClearKeysDown(void);
extern	void		IN_ReadControl(int,ControlInfo *);
extern	void		IN_GetJoyAbs(word joy,word *xp,word *yp);
extern	void		IN_SetupJoy(word joy,word minx,word maxx,
								word miny,word maxy);
extern	void		IN_StopDemo(void),IN_FreeDemoBuffer(void),
					IN_Ack(void);
extern	boolean		IN_UserInput(longword delay);
extern	char		IN_WaitForASCII(void);
extern	ScanCode	IN_WaitForKey(void);
extern	word		IN_GetJoyButtonsDB(word joy);
extern	const char *IN_GetScanName(ScanCode);

void    IN_WaitAndProcessEvents();
void    IN_ProcessEvents();

int     IN_MouseButtons (void);
#ifdef __AMIGA__
void    IN_GetMouseDelta(int *dx,int *dy);
#endif
boolean IN_JoyPresent();
void    IN_SetJoyCurrent(int joyIndex);
int     IN_JoyButtons (void);
void    IN_GetJoyDelta(int *dx,int *dy);
void    IN_GetJoyFineDelta(int *dx, int *dy);

void    IN_StartAck(void);
boolean IN_CheckAck (void);
bool    IN_IsInputGrabbed();
void    IN_CenterMouse();

#endif

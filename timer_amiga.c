#include <proto/timer.h>
#include <proto/exec.h>
#include <proto/lowlevel.h>
#include <proto/dos.h>
#include <time.h> 
#define Point W3DPoint
//#include "wl_def.h"
//#include "id_sd.h"

static struct timerequest	*timerio;
static struct MsgPort	*timerport;
struct Device	*TimerBase;
static struct timeval basetime;
#ifdef __AROS__
struct Library *LowLevelBase;
#endif


// timer interrupt
static APTR timerIntHandle = NULL;

#define	TickBase	70
uint32_t	TimeCount = 0;

static void LL_TimerInterrupt(void)
{
	TimeCount++;
}

static int timer_init(void)
{
	if ((timerport = CreateMsgPort()))
	{
		if ((timerio = (struct timerequest *)CreateIORequest(timerport, sizeof(struct timerequest))))
		{
			if (OpenDevice((STRPTR) TIMERNAME, UNIT_MICROHZ, (struct IORequest *) timerio, 0) == 0)
			{
				TimerBase = timerio->tr_node.io_Device;
				GetSysTime(&basetime);

				return 0;
			}
			DeleteIORequest((struct IORequest *)timerio);
		}
		DeleteMsgPort(timerport);
	}

	return -1;
}

static void timer_shutdown(void)
{
	if (TimerBase)
	{
		if (!CheckIO((struct IORequest *)timerio))
		{
			AbortIO((struct IORequest *)timerio);
			WaitIO((struct IORequest *)timerio);
		}
		CloseDevice((struct IORequest *)timerio);
		TimerBase = NULL;
	}
	if (timerio)
	{
		DeleteIORequest((struct IORequest *)timerio);
		timerio = NULL;
	}
	if (timerport)
	{
		DeleteMsgPort(timerport);
		timerport = NULL;
	}
}

int SDL_Init(uint32_t flags)
{
#ifdef __AROS__
	LowLevelBase = OpenLibrary("lowlevel.library", 0);
#endif

	timerIntHandle = AddTimerInt((APTR)LL_TimerInterrupt, NULL);
	if (timerIntHandle)
	{
		StartTimerInt(timerIntHandle, (1000 * 1000) / TickBase, TRUE); // 70 Hz timer interrupt
		//return 0;
		return timer_init();
	}


	return -1;
}

void SDL_Quit(void)
{
	if (timerIntHandle)
	{
		StopTimerInt(timerIntHandle);
		RemTimerInt(timerIntHandle);
		timerIntHandle = NULL;
	}
#ifdef __AROS__
	CloseLibrary(LowLevelBase);
#endif
	timer_shutdown();
}

void SDL_Delay(uint32_t ms)
{
	if (!TimerBase || ms == 0)
		return;

	timerio->tr_node.io_Command = TR_ADDREQUEST;
	timerio->tr_time.tv_secs  = ms / 1000;
	timerio->tr_time.tv_micro = (ms % 1000) * 1000;

	DoIO((struct IORequest *) timerio);
	//Delay(1);
}

uint32_t SDL_GetTicks(void)
{
	struct timeval tv;
	uint32_t ticks;

	if (!TimerBase)
		return 0;

	GetSysTime(&tv);

	if (basetime.tv_micro > tv.tv_micro)
	{
		tv.tv_secs--;
		tv.tv_micro += 1000000;
	}

	ticks = ((tv.tv_secs - basetime.tv_secs) * 1000) + ((tv.tv_micro - basetime.tv_micro)/1000);

	return ticks;
}

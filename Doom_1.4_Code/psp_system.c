#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <psppower.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "doomdef.h"
#include "m_misc.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "m_argv.h"

#define printf pspDebugScreenPrintf

int pspDveMgrSetVideoOut(int, int, int, int, int, int, int);

extern int psp_use_tv;

typedef unsigned char      uint8_t;
typedef signed   char      sint8_t;
typedef unsigned short     uint16_t;
typedef signed   short     sint16_t;
typedef signed   int       sint32_t;

extern byte *vid_mem;

int quit_requested = 0;

int psp_cheat_select = 0;


#define MIN_ZONESIZE  (2*1024*1024)
#define MAX_ZONESIZE  (6*1024*1024)

void psp_getevents (void);

static int stick_disabled = 0;
static int stick_cx = 128;
static int stick_cy = 128;
static int stick_minx = 0;
static int stick_miny = 0;
static int stick_maxx = 255;
static int stick_maxy = 255;
static int ctrl_cheat1= 0;
static int ctrl_cheat2= 0;
static int ctrl_cheat3= 0;

/**********************************************************************/
// Called by DoomMain.
void I_Init (void)
{
	int p;

	I_InitSound ();
	I_InitMusic ();
	//I_InitGraphics ();

	// Init PSP controller stuff
	p = M_CheckParm ("-noanalog");
	if (p)
		stick_disabled = 1;
	else
	{
		p = M_CheckParm ("-analogcx");
		if (p && p < myargc - 1)
			stick_cx = atoi (myargv[p+1]);
		p = M_CheckParm ("-analogcy");
		if (p && p < myargc - 1)
			stick_cy = atoi (myargv[p+1]);
		p = M_CheckParm ("-analogminx");
		if (p && p < myargc - 1)
			stick_minx = atoi (myargv[p+1]);
		p = M_CheckParm ("-analogminy");
		if (p && p < myargc - 1)
			stick_miny = atoi (myargv[p+1]);
		p = M_CheckParm ("-analogmaxx");
		if (p && p < myargc - 1)
			stick_maxx = atoi (myargv[p+1]);
		p = M_CheckParm ("-analogmaxy");
		if (p && p < myargc - 1)
			stick_maxy = atoi (myargv[p+1]);
	}

	p = M_CheckParm ("-cheat1");
	if (p && p < myargc - 1)
		ctrl_cheat1 = atoi (myargv[p+1]);
	p = M_CheckParm ("-cheat2");
	if (p && p < myargc - 1)
		ctrl_cheat2 = atoi (myargv[p+1]);
	p = M_CheckParm ("-cheat3");
	if (p && p < myargc - 1)
		ctrl_cheat3 = atoi (myargv[p+1]);

}

/**********************************************************************/
// Called by startup code
// to get the ammount of memory to malloc
// for the zone management.
byte*	I_ZoneBase (int *size)
{
	byte *zone;
	int p;

	p = M_CheckParm ("-heapsize");
	if (p && p < myargc - 1)
		*size = 1024 * atoi (myargv[p+1]);
	else
		*size = MAX_ZONESIZE;

	if ((zone = (byte *)malloc(*size)) == NULL)
	{
		printf("Couldn't allocate %d bytes for zone\n", *size);
		*size = MIN_ZONESIZE;
		if ((zone = (byte *)malloc(*size)) == NULL)
			I_Error ("malloc() %d bytes for zone management failed", *size);
	}

	printf ("I_ZoneBase(): Allocated %d bytes for zone management\n", *size);

	return zone;
}

/**********************************************************************/
// Called by D_DoomLoop,
// returns current time in tics.

extern volatile int snd_ticks; // advanced by sound thread

int I_GetTime (void)
{
  return snd_ticks;
}

/**********************************************************************/
//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//
void I_StartFrame (void)
{
	if (quit_requested)
		I_Error ("User forced quit via Home button\n");
    psp_getevents ();
}

/**********************************************************************/
//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic (void)
{
	if (quit_requested)
		I_Error ("User forced quit via Home button\n");
}

/**********************************************************************/
// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.

// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.
ticcmd_t	emptycmd;
ticcmd_t* I_BaseTiccmd (void)
{
  return &emptycmd;
}

/**********************************************************************/
// Called by M_Responder when quit is selected.
// Clean exit, displays sell blurb.
void I_Quit (void)
{
	D_QuitNetGame ();
	I_ShutdownSound();
	I_ShutdownMusic();
	M_SaveDefaults ();
	I_ShutdownGraphics();

	sceKernelDelayThread(5*1000*1000);
	sceKernelExitGame();
}

/**********************************************************************/
// Allocates from low memory under dos,
// just mallocs under unix
byte* I_AllocLow (int length)
{
  byte*	mem;

  if ((mem = (byte *)malloc (length)) == NULL)
    I_Error ("Out of memory allocating %d bytes", length);
  memset (mem,0,length);
  return mem;
}

/**********************************************************************/
void I_Tactile (int on, int off, int total)
{
  // UNUSED.
  on = off = total = 0;
}

/**********************************************************************/
void *I_malloc (size_t size)
{
  void *b;

  if ((b = malloc (size)) == NULL)
    I_Error ("Out of memory allocating %d bytes", size);
  return b;
}

/**********************************************************************/
void *I_calloc (size_t nelt, size_t esize)
{
  void *b;

  if ((b = calloc (nelt, esize)) == NULL)
    I_Error ("Out of memory allocating %d bytes", nelt * esize);
  return b;
}

/**********************************************************************/
void I_Error (char *error, ...)
{
	va_list	argptr;
	char msg[256];

	if (psp_use_tv)
		pspDveMgrSetVideoOut(0, 0, 480, 272, 1, 15, 0); // LCD

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0x000000);
	pspDebugScreenSetTextColor(0xffffff);

	// Message first.
	va_start (argptr, error);
	vsprintf (msg, error, argptr);
	va_end (argptr);
	printf("Error: %s\n", msg);

	// Shutdown. Here might be other errors.
	if (demorecording)
		G_CheckDemoStatus ();

	D_QuitNetGame ();
	I_ShutdownGraphics ();

	sceKernelDelayThread(5*1000*1000);
	sceKernelExitGame();
}

/**********************************************************************/
// misc extra functions needed by DOOM

int access(const char *path, int mode)
{
	FILE *lock;

	if ((lock = fopen(path, "rb"))) {
		fclose(lock);
		return 0;
	}

	return -1;
}

sint32_t _atoi(uint8_t *s)
{
#define ISNUM(c) ((c) >= '0' && (c) <= '9')

  register uint32_t i = 0;
  register uint32_t sign = 0;
  register uint8_t *p = s;

  /* Conversion starts at the first numeric character or sign. */
  while(*p && !ISNUM(*p) && *p != '-') p++;

  /*
     If we got a sign, set a flag.
     This will negate the value before return.
   */
  if(*p == '-')
    {
      sign++;
      p++;
    }

  /* Don't care when 'u' overflows (Bug?) */
  while(ISNUM(*p))
    {
      i *= 10;
      i += *p++ - '0';
    }

  /* Return according to sign */
  if(sign)
    return - i;
  else
    return i;

#undef ISNUM
}

int islower(int c)
{
	if (c < 'a')
		return 0;

	if (c > 'z')
		return 0;

	// passed both criteria, so it
	// is a lower case alpha char
	return 1;
}

int _toupper(int c)
{
	if ( islower( c ) ){
		c -= 32;
	}
	return c;
}

/**********************************************************************/

void psp_do_cheat(int cheat)
{
    event_t event;
    char *str;
    int i;

	switch (cheat)
	{
		case 1: // God Mode
		str = "iddqd";
		break;
		case 2: // Fucking Arsenal
		str = "idfa";
		break;
		case 3: // Key Full Ammo
		str = "idkfa";
		break;
		case 4: // No Clipping
		str = "idclip";
		break;
		case 5: // Toggle Map
		str = "iddt";
		break;
		case 6: // Invincible with Chainsaw
		str = "idchoppers";
		break;
		case 7: // Berserker Strength Power-up
		str = "idbeholds";
		break;
		case 8: // Invincibility Power-up
		str = "idbeholdv";
		break;
		case 9: // Invisibility Power-Up
		str = "idbeholdi";
		break;
		case 10: // Automap Power-up
		str = "idbeholda";
		break;
		case 11: // Anti-Radiation Suit Power-up
		str = "idbeholdr";
		break;
		case 12: // Light-Amplification Visor Power-up
		str = "idbeholdl";
		break;
		default:
		return;
	}

	for (i=0; i<strlen(str); i++)
	{
        event.type = ev_keydown;
        event.data1 = str[i];
        D_PostEvent (&event);
        event.type = ev_keyup;
        event.data1 = str[i];
        D_PostEvent (&event);
	}
}

extern int get_text_osk(char *input, unsigned short *intext, unsigned short *desc);
extern void video_set_vmode(void);

void psp_send_string(void)
{
    event_t event;
	int ok, i;
	char str[64];
	unsigned short intext[128]  = { 0 }; // text already in the edit box on start
	unsigned short desc[128]	= { 'E', 'n', 't', 'e', 'r', ' ', 'T', 'e', 'x', 't', 0 }; // description

	ok = get_text_osk(str, intext, desc);

	video_set_vmode();

	if (ok)
	{
		strcat(str, " ");
		for (i=0; i<strlen(str); i++)
		{
	   	    event.type = ev_keydown;
	        event.data1 = str[i];
	        D_PostEvent (&event);
	        event.type = ev_keyup;
	        event.data1 = str[i];
	        D_PostEvent (&event);
		}
	}
}

void psp_getevents (void)
{
	SceCtrlData pad;
    event_t event;
    event_t joyevent;
    event_t mouseevent;
    short mousex, mousey;
    char weapons[8] = { '1', '2', '3', '3', '4', '5', '6', '7' };
    static u32 previous = -1;
	static int pad_weapon = 2;
	static int rx, ry;

	sceCtrlReadBufferPositive(&pad, 1);
	if (previous == -1)
	{
		previous = pad.Buttons;
		rx = ry = abs(stick_cx - 128) > 16 || abs(stick_cy - 128) > 16 ? 32 : 24;
	}

	if (!stick_disabled)
	{
		// we don't use the min/max yet, and center just affects the comparison
		mousex = mousey = 0;
		if (abs(pad.Lx - 128) > rx)
			mousex = (pad.Lx - 128);
		if (abs(128 - pad.Ly) > ry)
			mousey = (128 - pad.Ly);

		if (mousex || mousey)
		{
   	    	mouseevent.type = ev_mouse;
   	    	mouseevent.data1 = 0; // no mouse buttons
   	    	//mouseevent.data1 = pad.Buttons & PSP_CTRL_SELECT ? 1 : 0;
   	    	//mouseevent.data1 |= pad.Buttons & PSP_CTRL_START ? 2 : 0;
   	    	mouseevent.data2 = (mousex << 1);
       		mouseevent.data3 = (mousey << 1);
        	D_PostEvent (&mouseevent);
    	}
	}

	if (pad.Buttons == previous)
		return;

    joyevent.type = ev_joystick;
    joyevent.data1 = joyevent.data2 = joyevent.data3 = 0;

    // X = Ctrl (Fire)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_CROSS)
        joyevent.data1 |= 1;

    // [] = SHIFT (Run)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_SQUARE)
        joyevent.data1 |= 4;

    // O = SPACE (Open/Operate)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_CIRCLE)
        joyevent.data1 |= 8;

    // directionals
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_LEFT)
        joyevent.data2 = -1;
    else if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_RIGHT)
        joyevent.data2 = 1;
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_UP)
        joyevent.data3 = -1;
    else if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_DOWN)
        joyevent.data3 = 1;

    // LTRIGGER = ',' (Strafe Left)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_LTRIGGER && !(previous & PSP_CTRL_LTRIGGER))
    {
        event.type = ev_keydown;
        event.data1 = ',';
        D_PostEvent (&event);
    }
    else if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && !(pad.Buttons & PSP_CTRL_LTRIGGER) && previous & PSP_CTRL_LTRIGGER)
    {
        event.type = ev_keyup;
        event.data1 = ',';
        D_PostEvent (&event);
    }
    // RTRIGGER = '.' (Strafe Right)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_RTRIGGER && !(previous & PSP_CTRL_RTRIGGER))
    {
        event.type = ev_keydown;
        event.data1 = '.';
        D_PostEvent (&event);
    }
    else if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && !(pad.Buttons & PSP_CTRL_RTRIGGER) && previous & PSP_CTRL_RTRIGGER)
    {
        event.type = ev_keyup;
        event.data1 = '.';
        D_PostEvent (&event);
    }

    // LTRIGGER + TRIANGLE =  (Prev Weapon)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_LTRIGGER && !(previous & PSP_CTRL_LTRIGGER))
    {
        pad_weapon -= 1;
        if (pad_weapon<0) pad_weapon = 0;
        event.type = ev_keydown;
        event.data1 = weapons[pad_weapon];
        D_PostEvent (&event);
    }
    else if (pad.Buttons & PSP_CTRL_TRIANGLE && !(pad.Buttons & PSP_CTRL_LTRIGGER) && previous & PSP_CTRL_LTRIGGER)
    {
        event.type = ev_keyup;
        event.data1 = weapons[pad_weapon];
        D_PostEvent (&event);
    }
    // RTRIGGER + TRIANGLE =  (Next Weapon)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_RTRIGGER && !(previous & PSP_CTRL_RTRIGGER))
    {
        pad_weapon += 1;
        if (pad_weapon>7) pad_weapon = 7;
        event.type = ev_keydown;
        event.data1 = weapons[pad_weapon];
        D_PostEvent (&event);
    }
    else if (pad.Buttons & PSP_CTRL_TRIANGLE && !(pad.Buttons & PSP_CTRL_RTRIGGER) && previous & PSP_CTRL_RTRIGGER)
    {
        event.type = ev_keyup;
        event.data1 = weapons[pad_weapon];
        D_PostEvent (&event);
    }

    // Start = (Pause)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_START && !(previous & PSP_CTRL_START))
    {
        event.type = ev_keydown;
        event.data1 = KEY_PAUSE;
        D_PostEvent (&event);
    }
    else if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && !(pad.Buttons & PSP_CTRL_START) && previous & PSP_CTRL_START)
    {
        event.type = ev_keyup;
        event.data1 = KEY_PAUSE;
        D_PostEvent (&event);
    }

    // Start + TRIANGLE = (get text string)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_START && !(previous & PSP_CTRL_START))
		psp_send_string();

    // Select = ESC (Menu)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && pad.Buttons & PSP_CTRL_SELECT && !(previous & PSP_CTRL_SELECT))
    {
      event.type = ev_keydown;
      event.data1 = 9;
      D_PostEvent (&event);
    }
    else if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && !(pad.Buttons & PSP_CTRL_SELECT) && previous & PSP_CTRL_SELECT)
    {
      event.type = ev_keyup;
      event.data1 = 9;
      D_PostEvent (&event);
	}
    // Select + TRIANGLE = TAB (Show Map)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_SELECT && !(previous & PSP_CTRL_SELECT))
    {
        event.type = ev_keydown;
        event.data1 = KEY_ESCAPE;
        D_PostEvent (&event);
    }
    else if (pad.Buttons & PSP_CTRL_TRIANGLE && !(pad.Buttons & PSP_CTRL_SELECT) && previous & PSP_CTRL_SELECT)
    {
        event.type = ev_keyup;
        event.data1 = KEY_ESCAPE;
        D_PostEvent (&event);
    }

    // UP + TRIANGLE =  F11 (Gamma)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_UP && !(previous & PSP_CTRL_UP))
    {
        pad_weapon -= 1;
        if (pad_weapon<1) pad_weapon = 1;
        event.type = ev_keydown;
        event.data1 = KEY_F11;
        D_PostEvent (&event);
    }
    else if (pad.Buttons & PSP_CTRL_TRIANGLE && !(pad.Buttons & PSP_CTRL_UP) && previous & PSP_CTRL_UP)
    {
        event.type = ev_keyup;
        event.data1 = KEY_F11;
        D_PostEvent (&event);
    }

    // DOWN + TRIANGLE =  F5 (Detail)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_DOWN && !(previous & PSP_CTRL_DOWN))
    {
        pad_weapon -= 1;
        if (pad_weapon<1) pad_weapon = 1;
        event.type = ev_keydown;
        event.data1 = KEY_F5;
        D_PostEvent (&event);
    }
    else if (pad.Buttons & PSP_CTRL_TRIANGLE && !(pad.Buttons & PSP_CTRL_DOWN) && previous & PSP_CTRL_DOWN)
    {
        event.type = ev_keyup;
        event.data1 = KEY_F5;
        D_PostEvent (&event);
    }

    // CIRCLE + TRIANGLE =  (Cheat1)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_CIRCLE && !(previous & PSP_CTRL_CIRCLE))
		psp_do_cheat(ctrl_cheat1);

    // CROSS + TRIANGLE =  (Cheat2)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_CROSS && !(previous & PSP_CTRL_CROSS))
		psp_do_cheat(ctrl_cheat2);

    // SQUARE + TRIANGLE =  (Cheat1)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_SQUARE && !(previous & PSP_CTRL_SQUARE))
		psp_do_cheat(ctrl_cheat3);

	// LEFT + TRIANGLE = (Prev Cheat)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_LEFT && !(previous & PSP_CTRL_LEFT))
    	psp_cheat_select = psp_cheat_select > 0 ? psp_cheat_select - 1 : 12;

	// RIGHT + TRIANGLE = (Next Cheat)
    if (pad.Buttons & PSP_CTRL_TRIANGLE && pad.Buttons & PSP_CTRL_RIGHT && !(previous & PSP_CTRL_RIGHT))
    	psp_cheat_select = psp_cheat_select < 12 ? psp_cheat_select + 1 : 0;

    // TRIANGLE release = y / BACKSPACE (Exit, Backspace, or do cheat when done selecting)
    if (!(pad.Buttons & PSP_CTRL_TRIANGLE) && previous & PSP_CTRL_TRIANGLE)
    {
		if (psp_cheat_select)
		{
			psp_do_cheat(psp_cheat_select);
			psp_cheat_select = 0;
		}

        event.type = ev_keydown;
        event.data1 = 'y';
        D_PostEvent (&event);
        event.type = ev_keyup;
        event.data1 = 'y';
        D_PostEvent (&event);

        event.type = ev_keydown;
        event.data1 = KEY_BACKSPACE;
        D_PostEvent (&event);
        event.type = ev_keyup;
        event.data1 = KEY_BACKSPACE;
        D_PostEvent (&event);

        event.type = ev_keydown;
        event.data1 = KEY_BACKSPACE;
        D_PostEvent (&event);
        event.type = ev_keyup;
        event.data1 = KEY_BACKSPACE;
        D_PostEvent (&event);
    }

    D_PostEvent (&joyevent);

	previous = pad.Buttons;
}

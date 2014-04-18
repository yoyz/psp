// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspmoduleinfo.h>
#include <psputility.h>
#include <psputility_osk.h>
#include <pspaudio.h>
#include <pspaudiolib.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <kubridge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pspsdk.h>
#include <psputility_netmodules.h>
#include <psputility_netparam.h>
#include <pspwlan.h>
#include <pspnet.h>
#include <pspnet_apctl.h>

#include "intraFont.h"

#define printf pspDebugScreenPrintf

#define VERS	 1
#define REVS	 4


#include "doomdef.h"

#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "m_fixed.h"


int pspDveMgrCheckVideoOut();
int pspDveMgrSetVideoOut(int, int, int, int, int, int, int);


int VERSION = 110;

char psp_home[256];

int psp_net_available = 0;
char *psp_net_ipaddr = 0;
char szMyIPAddr[32];

int psp_use_intrafont = 0;
intraFont *ltn8 = 0;

extern int quit_requested;

PSP_MODULE_INFO("DOOM", 0, VERS, REVS);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_MAX();
//PSP_HEAP_SIZE_KB(12000);

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common) {
	quit_requested = 1;
	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp) {
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void) {
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
	if(thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}

/*
 * OSK support code
 *
 */

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4) /* change this if you change to another screenmode */
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
#define ZBUF_SIZE (BUF_WIDTH SCR_HEIGHT * 2) /* zbuffer seems to be 16-bit? */

static unsigned int __attribute__((aligned(16))) list[262144];

static void SetupGu(void)
{
	sceGuInit();
	sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,(void*)0x88000,BUF_WIDTH);
	sceGuDepthBuffer((void*)0x110000,BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
	sceGuDepthRange(0xc350,0x2710);
	sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuFinish();
	sceGuSync(0,0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}

static void ResetGu(void)
{
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);
	sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH/2), 2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	sceGuDepthRange(65535, 0);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuFinish();
	sceGuSync(0,0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}

int get_text_osk(char *input, unsigned short *intext, unsigned short *desc)
{
	int done=0;
	unsigned short outtext[128] = { 0 }; // text after input

	SetupGu();

	SceUtilityOskData data;
	memset(&data, 0, sizeof(data));
	data.language = 2;			// key glyphs: 0-1=hiragana, 2+=western/whatever the other field says
	data.lines = 1;				// just one line
	data.unk_24 = 1;			// set to 1
	data.desc = desc;
	data.intext = intext;
	data.outtextlength = 128;	// sizeof(outtext) / sizeof(unsigned short)
	data.outtextlimit = 50;		// just allow 50 chars
	data.outtext = (unsigned short*)outtext;

	SceUtilityOskParams osk;
	memset(&osk, 0, sizeof(osk));
	osk.base.size = sizeof(osk);
	// dialog language: 0=Japanese, 1=English, 2=French, 3=Spanish, 4=German,
	// 5=Italian, 6=Dutch, 7=Portuguese, 8=Russian, 9=Korean, 10-11=Chinese, 12+=default
	osk.base.language = 1;
	osk.base.buttonSwap = 1;		// X button: 1
	osk.base.graphicsThread = 17;	// gfx thread pri
	osk.base.unknown = 19;			// unknown thread pri (?)
	osk.base.fontThread = 18;
	osk.base.soundThread = 16;
	osk.unk_48 = 1;
	osk.data = &data;

	int rc = sceUtilityOskInitStart(&osk);
	if (rc) return 0;

	while(!done) {
		int i,j=0;

		sceGuStart(GU_DIRECT,list);

		// clear screen
		sceGuClearColor(0xff554433);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

		sceGuFinish();
		sceGuSync(0,0);

		switch(sceUtilityOskGetStatus())
		{
			case PSP_UTILITY_DIALOG_INIT :
			break;
			case PSP_UTILITY_DIALOG_VISIBLE :
			sceUtilityOskUpdate(2); // 2 is taken from ps2dev.org recommendation
			break;
			case PSP_UTILITY_DIALOG_QUIT :
			sceUtilityOskShutdownStart();
			break;
			case PSP_UTILITY_DIALOG_FINISHED :
			done = 1;
			break;
			case PSP_UTILITY_DIALOG_NONE :
			default :
			break;
		}

		for(i = 0; data.outtext[i]; i++)
			if (data.outtext[i]!='\0' && data.outtext[i]!='\n' && data.outtext[i]!='\r')
			{
				input[j] = data.outtext[i];
				j++;
			}
		input[j] = 0;

		// wait TWO vblanks because one makes the input "twitchy"
		sceDisplayWaitVblankStart();
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}

	ResetGu();

	return 1;
}

/*
 * intraFont support code
 *
 */

void psp_font_init(void)
{
	ResetGu();

	intraFontInit();

	// Load font
    ltn8 = intraFontLoad("flash0:/font/ltn8.pgf"); // small latin sans-serif regular

	// Make sure the fonts are loaded
	psp_use_intrafont = ltn8 ? 1 : 0;
}


/*
 * GUI support code
 *
 */

extern char *RequestFile (char *initialPath);
void psp_net_connect(void *arg);
void psp_net_disconnect(void *arg);

void gui_PrePrint(void)
{
	if (psp_use_intrafont)
	{
		sceGuStart(GU_DIRECT, list);

		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumPerspective( 75.0f, 16.0f/9.0f, 0.5f, 1000.0f);

        sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();

		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();

		sceGuClearColor(0xFF7F7F7F);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
	}
}

void gui_PostPrint(void)
{
	if (psp_use_intrafont)
	{
        // End drawing
		sceGuFinish();
		sceGuSync(0,0);

		// Swap buffers (waiting for vsync)
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
}

int gui_PrintWidth(char *text)
{
	if (psp_use_intrafont)
		return intraFontMeasureText(ltn8, text);
	else
		return strlen(text)*7;
}

void gui_Print(char *text, u32 fc, u32 bc, int x, int y)
{
	if (psp_use_intrafont)
	{
		intraFontSetStyle(ltn8, 1.0f, fc, bc,0); // scale = 1.0
		intraFontPrint(ltn8, x, y, text);
	}
	else
	{
		static int lasty = -1;

		pspDebugScreenSetTextColor(0xFFFFFFFF);
		pspDebugScreenSetBackColor(0xFF000000);
		if (y != lasty)
		{
			// erase line if not the same as last print
			pspDebugScreenSetXY(0, y/8);
			pspDebugScreenPrintf("                                                                    ");
			lasty = y;
		}
		pspDebugScreenSetTextColor(fc);
		pspDebugScreenSetBackColor(bc);
		pspDebugScreenSetXY(x/7, y/8);
		pspDebugScreenPrintf("%s", text);
	}
}

char *RequestString (char *initialStr)
{
	int ok, i;
	static char str[64];
	unsigned short intext[128]  = { 0 }; // text already in the edit box on start
	unsigned short desc[128]	= { 'E', 'n', 't', 'e', 'r', ' ', 'S', 't', 'r', 'i', 'n', 'g', 0 }; // description

	if (initialStr[0] != 0)
		for (i=0; i<=strlen(initialStr); i++)
			intext[i] = (unsigned short)initialStr[i];

	ok = get_text_osk(str, intext, desc);

	if (!psp_use_intrafont)
	{
		pspDebugScreenInit();
		pspDebugScreenSetBackColor(0xFF000000);
		pspDebugScreenSetTextColor(0xFFFFFFFF);
		pspDebugScreenClear();
	}

	if (ok)
		return str;

	return 0;
}


struct gui_menu {
	char *text;
	unsigned int flags;
	void *field1;
	void *field2;
	unsigned int enable;
};

struct gui_list {
	char *text;
	int index;
};

// gui_menu.flags
#define GUI_DIVIDER   0
#define GUI_TEXT      1
#define GUI_MENU      2
#define GUI_FUNCTION  3
#define GUI_SELECT    4
#define GUI_TOGGLE    5
#define GUI_INTEGER   6
#define GUI_FILE      7
#define GUI_STRING    8

#define GUI_CENTER 0x10000000
#define GUI_LEFT   0x20000000
#define GUI_RIGHT  0x30000000

// gui_menu.enable
#define GUI_DISABLED 0
#define GUI_ENABLED  1
#define GUI_SET_ME   0xFFFFFFFF

// misc
#define GUI_END_OF_MENU 0xFFFFFFFF
#define GUI_END_OF_LIST 0xFFFFFFFF

int psp_cpu_speed = 0;

int psp_use_tv = 0;

int psp_lcd_res = 0;
int psp_lcd_sync = 0;
int psp_lcd_aspect = 0; // default to FOV = 90 degrees until fix fov issues
int psp_lcd_detail = 0;

int psp_tv_cable = -1;
int psp_tv_res = 0;
int psp_tv_sync = 0;
int psp_tv_laced = 1;
int psp_tv_aspect = 0;
int psp_tv_detail = 0;
int psp_tv_cx = 0;
int psp_tv_cy = 0;

int psp_snd_upd = 0;
int psp_sfx_enabled = 1;
int psp_music_enabled = 1;

int psp_stick_disabled = 0;
int psp_stick_cx = 128;
int psp_stick_cy = 128;
int psp_stick_minx = 0;
int psp_stick_miny = 0;
int psp_stick_maxx = 255;
int psp_stick_maxy = 255;
int psp_ctrl_cheat1= 0;
int psp_ctrl_cheat2= 0;
int psp_ctrl_cheat3= 0;

char *psp_iwad_file = 0;
char *psp_pwad_file1 = 0;
char *psp_pwad_file2 = 0;
char *psp_pwad_file3 = 0;
char *psp_pwad_file4 = 0;
char *psp_deh_file1 = 0;
char *psp_deh_file2 = 0;
char *psp_deh_file3 = 0;
char *psp_deh_file4 = 0;

int psp_game_nomonsters = 0;
int psp_game_respawn = 0;
int psp_game_fast = 0;
int psp_game_turbo = 0;
int psp_game_maponhu = 0;
int psp_game_rotatemap = 0;
int psp_game_deathmatch = 0;
int psp_game_altdeath = 0;
int psp_game_record = 0;
int psp_game_playdemo = 0;
int psp_game_forcedemo = 0;
int psp_game_timedemo = 0;
int psp_game_timer = 0;
int psp_game_avg = 0;
int psp_game_statcopy = 0;
int psp_game_version = 110;
int psp_game_pcchksum = 1;
int psp_game_skill = 2; // Hurt Me Plenty (medium)
int psp_game_level = 1;

int psp_net_enabled = 0;
int psp_net_type = 0;
int psp_net_accesspoint = 1;
int psp_net_port = 5029; // 5000 + 0x1d
int psp_net_ticdup = 1;
int psp_net_extratic = 0;
int psp_net_player1 = 1;
char *psp_net_player2 = 0;
char *psp_net_player3 = 0;
char *psp_net_player4 = 0;
int psp_net_error = 0;

int net_access_range[2] = { 1, 1 };
char net_ap_str[1024] = "Net Access Points : ";

int connected_accesspoint = 0;


int gui_menu_len(struct gui_menu *menu)
{
	int c = 0;
	while (menu[c].flags != GUI_END_OF_MENU)
		c++;
	return c;
}

int gui_list_len(struct gui_list *list)
{
	int c = 0;
	while (list[c].index != GUI_END_OF_LIST)
		c++;
	return c;
}

void do_gui(struct gui_menu *menu, void *menufn, char *exit_msg)
{
	SceCtrlData pad;
	int msy, mlen;
	int i, j, k, min, max;
	int csel = 0;
	u32 fc, bc;
	int tx, ty;
	char line[69];

	mlen = gui_menu_len(menu);
	msy = 136 - mlen*8;

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	sceCtrlReadBufferPositive(&pad, 1);
	while (!(pad.Buttons & PSP_CTRL_CIRCLE))
	{
		void (*fnptr)(struct gui_menu *);
		fnptr = menufn;
		if (fnptr)
			(*fnptr)(menu);
		if (pad.Buttons & PSP_CTRL_CROSS)
		{
			if (menu[csel].enable == GUI_ENABLED)
				switch (menu[csel].flags & 0x0FFFFFFF)
				{
					void (*fnptr)(void *);
					char temp[256];
					char *req;

					case GUI_MENU:
					while (pad.Buttons & PSP_CTRL_CROSS)
						sceCtrlReadBufferPositive(&pad, 1);
					do_gui((struct gui_menu *)menu[csel].field1, menu[csel].field2, "Return to Prev Menu");
					break;
					case GUI_FUNCTION:
					while (pad.Buttons & PSP_CTRL_CROSS)
						sceCtrlReadBufferPositive(&pad, 1);
					fnptr = menu[csel].field1;
					(*fnptr)(menu[csel].field2);
					break;
					case GUI_TOGGLE:
					while (pad.Buttons & PSP_CTRL_CROSS)
						sceCtrlReadBufferPositive(&pad, 1);
					*(int *)menu[csel].field1 ^= 1;
					break;
					case GUI_SELECT:
					if (pad.Buttons & (PSP_CTRL_RIGHT | PSP_CTRL_DOWN))
					{
						// next selection
						while (pad.Buttons & (PSP_CTRL_RIGHT | PSP_CTRL_DOWN))
							sceCtrlReadBufferPositive(&pad, 1);
						j = gui_list_len((struct gui_list *)menu[csel].field1);
						k = *(int *)menu[csel].field2;
						k = (k == (j - 1)) ? 0 : k + 1;
						*(int *)menu[csel].field2 = k;
					}
					if (pad.Buttons & (PSP_CTRL_LEFT | PSP_CTRL_UP))
					{
						// previous selection
						while (pad.Buttons & (PSP_CTRL_LEFT | PSP_CTRL_UP))
							sceCtrlReadBufferPositive(&pad, 1);
						j = gui_list_len((struct gui_list *)menu[csel].field1);
						k = *(int *)menu[csel].field2;
						k = (k == 0) ? j - 1 : k - 1;
						*(int *)menu[csel].field2 = k;
					}
					break;
					case GUI_FILE:
					while (pad.Buttons & PSP_CTRL_CROSS)
						sceCtrlReadBufferPositive(&pad, 1);
					if (*(int *)menu[csel].field1)
						free(*(char **)menu[csel].field1);
					strcpy(temp, psp_home);
					strcat(temp, (char *)menu[csel].field2);
					strcat(temp, "/");
					req = RequestFile(temp);
					*(char **)menu[csel].field1 = req ? strdup(req) : 0;
					break;
					case GUI_STRING:
					while (pad.Buttons & PSP_CTRL_CROSS)
						sceCtrlReadBufferPositive(&pad, 1);
					temp[0] = 0;
					if (*(int *)menu[csel].field1)
					{
						strcpy(temp, *(char **)menu[csel].field1);
						free(*(char **)menu[csel].field1);
					}
					req = RequestString(temp);
					*(char **)menu[csel].field1 = req ? strdup(req) : 0;
					break;
					case GUI_INTEGER:
					if (menu[csel].field2)
					{
						int *rng = (int *)menu[csel].field2;
						min = rng[0];
						max = rng[1];
					}
					else
					{
						min = (int)0x80000000;
						max = 0x7FFFFFFF;
					}
					if (pad.Buttons & PSP_CTRL_RIGHT)
					{
						// +1
						//while (pad.Buttons & PSP_CTRL_RIGHT)
						//	sceCtrlReadBufferPositive(&pad, 1);
						sceKernelDelayThread(200*1000);
						*(int *)menu[csel].field1 += 1;
						if (*(int *)menu[csel].field1 > max)
							*(int *)menu[csel].field1 = max;
					}
					if (pad.Buttons & PSP_CTRL_LEFT)
					{
						// -1
						//while (pad.Buttons & PSP_CTRL_LEFT)
						//	sceCtrlReadBufferPositive(&pad, 1);
						sceKernelDelayThread(200*1000);
						*(int *)menu[csel].field1 -= 1;
						if (*(int *)menu[csel].field1 < min)
							*(int *)menu[csel].field1 = min;
					}
					if (pad.Buttons & PSP_CTRL_DOWN)
					{
						// +10
						//while (pad.Buttons & PSP_CTRL_DOWN)
						//	sceCtrlReadBufferPositive(&pad, 1);
						sceKernelDelayThread(200*1000);
						*(int *)menu[csel].field1 += 10;
						if (*(int *)menu[csel].field1 > max)
							*(int *)menu[csel].field1 = max;
					}
					if (pad.Buttons & PSP_CTRL_UP)
					{
						// -10
						//while (pad.Buttons & PSP_CTRL_UP)
						//	sceCtrlReadBufferPositive(&pad, 1);
						sceKernelDelayThread(200*1000);
						*(int *)menu[csel].field1 -= 10;
						if (*(int *)menu[csel].field1 < min)
							*(int *)menu[csel].field1 = min;
					}
					break;
				}
		}
		else
		{
			if (pad.Buttons & PSP_CTRL_UP)
			{
				while (pad.Buttons & PSP_CTRL_UP)
					sceCtrlReadBufferPositive(&pad, 1);
				csel = (csel == 0) ? mlen - 1 : csel - 1;
			}
			if (pad.Buttons & PSP_CTRL_DOWN)
			{
				while (pad.Buttons & PSP_CTRL_DOWN)
					sceCtrlReadBufferPositive(&pad, 1);
				csel = (csel == (mlen - 1)) ? 0 : csel + 1;
			}
		}

		sceDisplayWaitVblankStart();
		gui_PrePrint();
		fc = 0xFFFFFFFF;
		bc = 0xFF000000;

		switch (menu[csel].flags & 0x0FFFFFFF)
		{
			case GUI_MENU:
			gui_Print("X = Enter Sub-Menu", fc, bc, 14, 264);
			break;
			case GUI_FUNCTION:
			gui_Print("X = Do Function", fc, bc, 14, 264);
			break;
			case GUI_SELECT:
			gui_Print("X = Hold to change Selection with D-Pad", fc, bc, 14, 264);
			break;
			case GUI_TOGGLE:
			gui_Print("X = Toggle Flag", fc, bc, 14, 264);
			break;
			case GUI_INTEGER:
			gui_Print("X = Hold to change Integer with D-Pad", fc, bc, 14, 264);
			break;
			case GUI_FILE:
			gui_Print("X = Select File", fc, bc, 14, 264);
			break;
			case GUI_STRING:
			gui_Print("X = Input String", fc, bc, 14, 264);
			break;
		}
		sprintf(line, "O = %s", exit_msg);
		gui_Print(line, fc, bc, 466 - gui_PrintWidth(line), 264);

		for (i=0; i<mlen; i++)
		{
			char temp[10];

			bc = 0xFF000000;
			if ((menu[i].flags & 0x0FFFFFFF) == GUI_DIVIDER)
				continue;
			if (menu[i].enable == GUI_ENABLED)
				fc = (i==csel) ? 0xFFFFFFFF : 0xFFCCCCCC;
			else
				fc = 0xFFAAAAAA;

			strcpy(line, menu[i].text);
			switch (menu[i].flags & 0x0FFFFFFF)
			{
				case GUI_SELECT:
				strcat(line, " : ");
				strcat(line, ((struct gui_list *)menu[i].field1)[*(int *)menu[i].field2].text);
				break;
				case GUI_TOGGLE:
				strcat(line, " : ");
				strcat(line, *(int *)menu[i].field1 ? "on" : "off");
				break;
				case GUI_INTEGER:
				strcat(line, " : ");
				sprintf(temp, "%d", *(int *)menu[i].field1);
				strcat(line, temp);
				break;
				case GUI_FILE:
				strcat(line, " : ");
				strcat(line, *(int *)menu[i].field1 ? *(char **)menu[i].field1 : "");
				break;
				case GUI_STRING:
				strcat(line, " : ");
				strcat(line, *(int *)menu[i].field1 ? *(char **)menu[i].field1 : "");
				break;
				case GUI_TEXT:
				if ((int)menu[i].field1)
					fc = (u32)menu[i].field1 | 0xFF000000;
				if ((int)menu[i].field2)
					bc = (u32)menu[i].field2 | 0xFF000000;
				break;
			}
			switch (menu[i].flags & 0xF0000000)
			{
				case GUI_LEFT:
				tx = 7;
				ty = msy + i * 16;
				break;
				case GUI_RIGHT:
				tx = 473 - gui_PrintWidth(line);
				ty = msy + i * 16;
				break;
				case GUI_CENTER:
				default:
				tx = 240 - gui_PrintWidth(line) / 2;
				ty = msy + i * 16;
			}
			gui_Print(line, fc, bc, tx, ty);
			bc = 0xFF000000;
		}
		gui_PostPrint();

		sceKernelDelayThread(20*1000);
		sceCtrlReadBufferPositive(&pad, 1);
		if (quit_requested)
		{
			sceKernelDelayThread(5*1000*1000);
			sceKernelExitGame();
		}
	}
	while (pad.Buttons & PSP_CTRL_CIRCLE)
		sceCtrlReadBufferPositive(&pad, 1);

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();
}

void set_myargv(void);
void get_myargv(void);

void psp_load_defaults()
{
	int i;
	FILE *handle;
	char temp[256];

	strcpy(temp, psp_home);
	strcat(temp, "doom.set");
	handle = fopen (temp, "r");
	if (handle == NULL)
		return;

	for (i = 0 ; i < MAXARGVS; i++)
	{
		temp[0] = 0;
		fgets(temp, 255, handle);
		printf(" %d : %s", i, temp);
		if (temp[0] == 0)
			break;
		temp[strlen(temp) - 1] = 0;
		myargv[i] = strdup(temp);
	}
	myargc = i;

	fclose (handle);

	get_myargv();
}

void psp_load_settings(void *arg)
{
	int i;
	char *req;

	req = RequestFile(psp_home);

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	// free old argv entries
	if (myargc)
		for (i = 0 ; i < myargc; i++)
		{
			free(myargv[i]);
			myargv[i] = 0;
		}
	myargc = 0;

	if (req)
	{
		FILE *handle;
		char temp[256];

		printf("Attempting to load settings from %s\n\n", req);

		handle = fopen (req, "r");
		if (handle == NULL)
		{
			printf("Error! Couldn't open file %s\n\n", req);
			sceKernelDelayThread(2*1000*1000);
			return;
		}

		for (i = 0 ; i < MAXARGVS; i++)
		{
			temp[0] = 0;
			fgets(temp, 255, handle);
			printf(" %d : %s", i, temp);
			if (temp[0] == 0)
				break;
			temp[strlen(temp) - 1] = 0;
			myargv[i] = strdup(temp);
		}
		myargc = i;

		fclose (handle);

		get_myargv();

		printf("\nSettings loaded\n\n");
		sceKernelDelayThread(3*1000*1000);
	}
	else
	{
		printf("You need to enter a filename to load!\n\n");
		sceKernelDelayThread(2*1000*1000);
	}
	pspDebugScreenClear();
}

void psp_save_settings(void *arg)
{
	int ok, i;
	char filename[64];
	unsigned short intext[128]  = { 'd', 'o', 'o', 'm', '.', 's', 'e', 't', 0 }; // text already in the edit box on start
	unsigned short desc[128]	= { 'E', 'n', 't', 'e', 'r', ' ', 'F', 'i', 'l', 'e', ' ', 'N', 'a', 'm', 'e', 0 }; // description

	ok = get_text_osk(filename, intext, desc);

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	if (ok)
	{
		FILE *handle;
		char temp[256];

		printf("Attempting to save settings to %s%s\n\n", psp_home, filename);

		strcpy(temp, psp_home);
		strcat(temp, filename);
		handle = fopen (temp, "wb");
		if (handle == NULL)
		{
			printf("Error! Couldn't open file %s\n\n", temp);
			sceKernelDelayThread(2*1000*1000);
			return;
		}

		set_myargv();
		for (i = 0 ; i < myargc; i++)
		{
			fwrite (myargv[i], 1, strlen(myargv[i]), handle);
			fwrite ("\n", 1, 1, handle);
		}

		fclose (handle);

		printf("Settings saved to %s\n\n", temp);
		sceKernelDelayThread(2*1000*1000);
	}
	else
	{
		printf("You need to enter a filename to save!\n\n");
		sceKernelDelayThread(2*1000*1000);
	}
	pspDebugScreenClear();
}

static void drawLine(int inX0, int inY0, int inX1, int inY1, u32 inColor, u32* inDestination, int inWidth)
{
     int tempDY = inY1 - inY0;
     int tempDX = inX1 - inX0;
     int tempStepX, tempStepY;
     int inZ;

     if(tempDY < 0) {
          tempDY = -tempDY;  tempStepY = -inWidth;
     } else {
          tempStepY = inWidth;
     }

     if(tempDX < 0) {
          tempDX = -tempDX;  tempStepX = -1;
     } else {
          tempStepX = 1;
     }

     tempDY <<= 1;
     tempDX <<= 1;

     inY0 *= inWidth;
     inY1 *= inWidth;
     inDestination[inX0 + inY0] = inColor;
     if(tempDX > tempDY) {
          int tempFraction = tempDY - (tempDX >> 1);
          while(inX0 != inX1) {
               if(tempFraction >= 0) {
                    inY0 += tempStepY;
                    tempFraction -= tempDX;
               }
               inX0 += tempStepX;
               tempFraction += tempDY;
               if (!psp_tv_laced)
                    inDestination[inX0 + inY0] = inColor;
               else {
                    inZ = inY0 / inWidth;
                    if (inZ & 1)
                         inDestination[inX0 + (inZ>>1) * inWidth] = inColor;
                    else
                         inDestination[inX0 + ((inZ>>1) + 262) * inWidth] = inColor;
               }
          }
     } else {
          int tempFraction = tempDX - (tempDY >> 1);
          while(inY0 != inY1) {
               if(tempFraction >= 0) {
                    inX0 += tempStepX;
                    tempFraction -= tempDY;
               }
               inY0 += tempStepY;
               tempFraction += tempDX;
               if (!psp_tv_laced)
                    inDestination[inX0 + inY0] = inColor;
               else {
                    inZ = inY0 / inWidth;
                    if (inZ & 1)
                         inDestination[inX0 + (inZ>>1) * inWidth] = inColor;
                    else
                         inDestination[inX0 + ((inZ>>1) + 262) * inWidth] = inColor;
               }
          }
     }
}

void psp_tv_center(void *arg)
{
	SceCtrlData pad;
	int cx, cy, mx, my, h, w;

	if (psp_tv_cable > 0)
	{
		if (psp_tv_cable == 1)
			pspDveMgrSetVideoOut(2, 0x1d1, 720, 503, 1, 15, 0); // composite
		else
			if (psp_tv_laced)
				pspDveMgrSetVideoOut(0, 0x1d1, 720, 503, 1, 15, 0); // component interlaced
			else
				pspDveMgrSetVideoOut(0, 0x1d2, 720, 480, 1, 15, 0); // component progressive

		sceDisplaySetFrameBuf((void *)0x44000000, 768, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);

		switch (psp_tv_res)
		{
			case 1:
			w = 704;
			h = 448;
			mx = 720 - w;
			my = 480 - h;
			break;
			case 2:
			w = 640;
			h = 400;
			mx = 720 - w;
			my = 480 - h;
			break;
			case 0:
			default:
			w = 720;
			h = 480;
			mx = 720 - w;
			my = 480 - h;
			break;
		}
		cx = psp_tv_cx > mx ? mx : psp_tv_cx;
		cy = psp_tv_cy > my ? my : psp_tv_cy;


		sceCtrlReadBufferPositive(&pad, 1);
		while (!(pad.Buttons & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS)))
		{
			if (pad.Buttons & PSP_CTRL_UP)
				cy = cy > 0 ? cy-1 : 0;
			if (pad.Buttons & PSP_CTRL_DOWN)
				cy = cy < my ? cy+1 : my;
			if (pad.Buttons & PSP_CTRL_LEFT)
				cx = cx > 0 ? cx-1 : 0;
			if (pad.Buttons & PSP_CTRL_RIGHT)
				cx = cx < mx ? cx+1 : mx;

			sceDisplayWaitVblankStart();
			memset((void *)0x44000000, 0, 503*768*4); // clear screen

			drawLine(cx, cy, cx+w-1, cy, 0xFFFFFF, (u32 *)0x44000000, 768);
			drawLine(cx+w-1, cy, cx+w-1, cy+h-1, 0xFFFFFF, (u32 *)0x44000000, 768);
			drawLine(cx+w-1, cy+h-1, cx, cy+h-1, 0xFFFFFF, (u32 *)0x44000000, 768);
			drawLine(cx, cy+h-1, cx, cy, 0xFFFFFF, (u32 *)0x44000000, 768);
			drawLine(cx, cy, cx+w-1, cy+h-1, 0xFFFFFF, (u32 *)0x44000000, 768);
			drawLine(cx, cy+h-1, cx+w-1, cy, 0xFFFFFF, (u32 *)0x44000000, 768);

			sceKernelDelayThread(100*1000);
			sceCtrlReadBufferPositive(&pad, 1);
		}
		while (pad.Buttons & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS))
			sceCtrlReadBufferPositive(&pad, 1);

		psp_tv_cx = cx;
		psp_tv_cy = cy;

		pspDveMgrSetVideoOut(0, 0, 480, 272, 1, 15, 0); // LCD
		pspDebugScreenInit();
		pspDebugScreenSetBackColor(0xFF000000);
		pspDebugScreenSetTextColor(0xFFFFFFFF);
		pspDebugScreenClear();
	}
}

void psp_stick_calibrate(void *arg)
{
	SceCtrlData pad;
	int cx, cy, mx, my, Mx, My;
	u32 prev;
	u32 *addr;

	cx = cy = mx = my = Mx = My = 128;

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();
	printf("    Move the stick to the corners, release it, then press X or O\n");

	drawLine(112, 8, 112+255, 8, 0xFFFFFF, (u32 *)0x44000000, 512);
	drawLine(112+255, 8, 112+255, 8+255, 0xFFFFFF, (u32 *)0x44000000, 512);
	drawLine(112+255, 8+255, 112, 8+255, 0xFFFFFF, (u32 *)0x44000000, 512);
	drawLine(112, 8+255, 112, 8, 0xFFFFFF, (u32 *)0x44000000, 512);

	addr = (u32 *)(0x44000000 + (112+cx)*4 +(8+cy)*512*4);
	prev = *addr;

	sceCtrlReadBufferPositive(&pad, 1);
	while (!(pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)))
	{
		sceDisplayWaitVblankStart();
		*addr = prev;

		sceCtrlReadBufferPositive(&pad, 1);
		cx = pad.Lx;
		cy = pad.Ly;
		if (cx<mx) mx = cx;
		if (cx>Mx) Mx = cx;
		if (cy<my) my = cy;
		if (cy>My) My = cy;

		addr = (u32 *)(0x44000000 + (112+cx)*4 +(8+cy)*512*4);
		prev = *addr;
		*addr = (u32)0xFFFFFF;

		pspDebugScreenSetXY(0, 4);
		printf("Current");
		pspDebugScreenSetXY(0, 5);
		printf(" %02X %02X", cx, cy);
		pspDebugScreenSetXY(0, 7);
		printf("Minimum");
		pspDebugScreenSetXY(0, 8);
		printf(" %02X %02X", mx, my);
		pspDebugScreenSetXY(0, 10);
		printf("Maximum");
		pspDebugScreenSetXY(0, 11);
		printf(" %02X %02X", Mx, My);
	}
	while (pad.Buttons & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS))
		sceCtrlReadBufferPositive(&pad, 1);

	pspDebugScreenClear();

	psp_stick_cx = cx;
	psp_stick_cy = cy;
	psp_stick_minx = mx;
	psp_stick_miny = my;
	psp_stick_maxx = Mx;
	psp_stick_maxy = My;
}

void InfraFunc(struct gui_menu *menu)
{

}

void psp_gui(void)
{
	struct gui_menu AboutLevel[] = {
		{ "Doom for the PSP", GUI_CENTER | GUI_TEXT, (void *)0xFFFFFF, (void *)0x1020FF, GUI_DISABLED },
		{ "by Chilly Willy", GUI_CENTER | GUI_TEXT, (void *)0xFFFFFF, (void *)0x1020FF, GUI_DISABLED },
		{ "based on ADoomPPC v1.7 by Jarmo Laakkonen", GUI_CENTER | GUI_TEXT, (void *)0x88FF88, 0, GUI_DISABLED },
		{ "based on ADoomPPC v1.3 by Joseph Fenton", GUI_CENTER | GUI_TEXT, (void *)0x88FF88, 0, GUI_DISABLED },
		{ "based on ADoom v1.2 by Peter McGavin", GUI_CENTER | GUI_TEXT, (void *)0x88FF88, 0, GUI_DISABLED },
		{ "", GUI_CENTER | GUI_DIVIDER, 0, 0, GUI_DISABLED },
		{ "Alternate MIDI Instruments by Zer0-X/o'Moses", GUI_CENTER | GUI_TEXT, (void *)0x8888FF, 0, GUI_DISABLED },
		{ "based on samples from Christian Buchner's GMPlay V1.3", GUI_CENTER | GUI_TEXT, (void *)0x8888FF, 0, GUI_DISABLED },
		{ "and the original MIDI Instruments by Joseph Fenton", GUI_CENTER | GUI_TEXT, (void *)0x8888FF, 0, GUI_DISABLED },
		{ "", GUI_CENTER | GUI_DIVIDER, 0, 0, GUI_DISABLED },
		{ "Uses intraFont by BenHur", GUI_CENTER | GUI_TEXT, (void *)0xFF8888, 0, GUI_DISABLED },
		{ "TV-out support lib thanks to Dark_AleX", GUI_CENTER | GUI_TEXT, (void *)0x010101, (void *)0xFFFFFF, GUI_DISABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_list net_type_list[] = {
		{ "TCP/IP Infrastructure", 0 },
		{ "TCP/IP Ad-Hoc", 1 },
		{ 0, GUI_END_OF_LIST }
	};

	int net_ticdup_range[2] = { 1, 9 };
	int net_player1_range[2] = { 1, 4 };
	int net_port_range[2] = { 5000, 65535 };

	struct gui_menu InfraLevel[] = {
		{ "Network Access Point", GUI_CENTER | GUI_INTEGER, &psp_net_accesspoint, &net_access_range, GUI_ENABLED },
		{ "Connect to Access Point", GUI_CENTER | GUI_FUNCTION, &psp_net_connect, 0, GUI_ENABLED },
		{ "Disconnect from Access Point", GUI_CENTER | GUI_FUNCTION, &psp_net_disconnect, 0, GUI_ENABLED },
		{ "Network Port", GUI_CENTER | GUI_INTEGER, &psp_net_port, &net_port_range, GUI_ENABLED },
		{ "", GUI_CENTER | GUI_DIVIDER, 0, 0, GUI_DISABLED },
		{ "Network Player #1 Addr", GUI_LEFT | GUI_STRING, &psp_net_player2, 0, GUI_ENABLED },
		{ "Network Player #2 Addr", GUI_LEFT | GUI_STRING, &psp_net_player3, 0, GUI_ENABLED },
		{ "Network Player #3 Addr", GUI_LEFT | GUI_STRING, &psp_net_player4, 0, GUI_ENABLED },
		{ "Local Network Address ", GUI_LEFT | GUI_STRING, &psp_net_ipaddr, 0, GUI_DISABLED },
		{ net_ap_str, GUI_LEFT | GUI_TEXT, 0, 0, GUI_DISABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	int game_level_range[2] = { 1, 36 };

	struct gui_list game_skill_list[] = {
		{ "I'm Too Young To Die", 0 },
		{ "Hey, Not Too Rough", 1 },
		{ "Hurt Me Plenty", 2 },
		{ "Ultra-Violence", 3 },
		{ "Nightmare!", 4 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_menu NetLevel[] = {
		{ "Play Network Game", GUI_CENTER | GUI_TOGGLE, &psp_net_enabled, 0, GUI_ENABLED },
		{ "Player Number",  GUI_CENTER | GUI_INTEGER, &psp_net_player1, &net_player1_range, GUI_ENABLED },
		{ "Deathmatch", GUI_CENTER | GUI_TOGGLE, &psp_game_deathmatch, 0, GUI_ENABLED },
		{ "Alt Deathmatch", GUI_CENTER | GUI_TOGGLE, &psp_game_altdeath, 0, GUI_ENABLED },
		{ "Starting Skill Level", GUI_CENTER | GUI_SELECT, &game_skill_list, &psp_game_skill, GUI_ENABLED },
		{ "Starting Map Level", GUI_CENTER | GUI_INTEGER, &psp_game_level, &game_level_range, GUI_ENABLED },
		{ "Timed Game", GUI_CENTER | GUI_INTEGER, &psp_game_timer, 0, GUI_ENABLED },
		{ "A.V.G.", GUI_CENTER | GUI_TOGGLE, &psp_game_avg, 0, GUI_ENABLED },
		{ "Copy Stats", GUI_CENTER | GUI_TOGGLE, &psp_game_statcopy, 0, GUI_ENABLED },
		{ "Force Version", GUI_CENTER | GUI_INTEGER, &psp_game_version, 0, GUI_ENABLED },
		{ "Use PC Checksum", GUI_CENTER | GUI_TOGGLE, &psp_game_pcchksum, 0, GUI_ENABLED },
		{ "Network Type", GUI_CENTER | GUI_SELECT, &net_type_list, &psp_net_type, GUI_DISABLED },
		{ "Network Type Settings", GUI_CENTER | GUI_MENU, &InfraLevel, &InfraFunc, GUI_ENABLED },
		{ "Network Extra Tic", GUI_CENTER | GUI_TOGGLE, &psp_net_extratic, 0, GUI_ENABLED },
		{ "Network Tic Dup", GUI_CENTER | GUI_INTEGER, &psp_net_ticdup, &net_ticdup_range, GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_menu GameLevel[] = {
		{ "No Monsters", GUI_CENTER | GUI_TOGGLE, &psp_game_nomonsters, 0, GUI_ENABLED },
		{ "Respawn", GUI_CENTER | GUI_TOGGLE, &psp_game_respawn, 0, GUI_ENABLED },
		{ "Fast", GUI_CENTER | GUI_TOGGLE, &psp_game_fast, 0, GUI_ENABLED },
		{ "Turbo", GUI_CENTER | GUI_TOGGLE, &psp_game_turbo, 0, GUI_ENABLED },
		{ "Map on HU", GUI_CENTER | GUI_TOGGLE, &psp_game_maponhu, 0, GUI_ENABLED },
		{ "Rotate Map", GUI_CENTER | GUI_TOGGLE, &psp_game_rotatemap, 0, GUI_ENABLED },
		{ "Force Demo", GUI_CENTER | GUI_TOGGLE, &psp_game_forcedemo, 0, GUI_ENABLED },
		{ "Play Demo", GUI_CENTER | GUI_INTEGER, &psp_game_playdemo, 0, GUI_ENABLED },
		{ "Time Demo", GUI_CENTER | GUI_INTEGER, &psp_game_timedemo, 0, GUI_ENABLED },
		{ "Record Demo", GUI_CENTER | GUI_INTEGER, &psp_game_record, 0, GUI_ENABLED },
		//{ "Warp to Level", GUI_CENTER | GUI_SELECT, &game_warp_list, &psp_game_warp, GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_menu FileLevel[] = {
		{ "Main WAD", GUI_LEFT | GUI_FILE, &psp_iwad_file, "iwad", GUI_ENABLED },
		{ "Patch WAD", GUI_LEFT | GUI_FILE, &psp_pwad_file1, "pwad", GUI_ENABLED },
		{ "Patch WAD", GUI_LEFT | GUI_FILE, &psp_pwad_file2, "pwad", GUI_ENABLED },
		{ "Patch WAD", GUI_LEFT | GUI_FILE, &psp_pwad_file3, "pwad", GUI_ENABLED },
		{ "Patch WAD", GUI_LEFT | GUI_FILE, &psp_pwad_file4, "pwad", GUI_ENABLED },
		{ "DEH File", GUI_LEFT | GUI_FILE, &psp_deh_file1, "deh", GUI_ENABLED },
		{ "DEH File", GUI_LEFT | GUI_FILE, &psp_deh_file2, "deh", GUI_ENABLED },
		{ "DEH File", GUI_LEFT | GUI_FILE, &psp_deh_file3, "deh", GUI_ENABLED },
		{ "DEH File", GUI_LEFT | GUI_FILE, &psp_deh_file4, "deh", GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_list ctrl_cheat_list[] = {
		{ "none", 0 },
		{ "God Mode", 1 },
		{ "Fucking Arsenal", 2 },
		{ "Key Full Ammo", 3 },
		{ "No Clipping", 4 },
		{ "Toggle Map", 5 },
		{ "Invincible with Chainsaw", 6 },
		{ "Berserker Strength Power-up", 7 },
		{ "Invincibility Power-up", 8 },
		{ "Invisibility Power-Up", 9 },
		{ "Automap Power-up", 10 },
		{ "Anti-Radiation Suit Power-up", 11 },
		{ "Light-Amplification Visor Power-up", 12 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_menu ControlLevel[] = {
		{ "Disable Analog Stick", GUI_CENTER | GUI_TOGGLE, &psp_stick_disabled, 0, GUI_ENABLED },
		{ "Calibrate Analog Stick", GUI_CENTER | GUI_FUNCTION, &psp_stick_calibrate, 0, GUI_ENABLED },
		{ "TRIANGLE + CIRCLE Cheat", GUI_CENTER | GUI_SELECT, &ctrl_cheat_list, &psp_ctrl_cheat1, GUI_ENABLED },
		{ "TRIANGLE + CROSS Cheat", GUI_CENTER | GUI_SELECT, &ctrl_cheat_list, &psp_ctrl_cheat2, GUI_ENABLED },
		{ "TRIANGLE + SQUARE Cheat", GUI_CENTER | GUI_SELECT, &ctrl_cheat_list, &psp_ctrl_cheat3, GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_list snd_upd_list[] = {
		{ "140 Hz", 0 },
		{ "70 Hz", 1 },
		{ "35 Hz", 2 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_menu SoundLevel[] = {
		{ "Update Frequency", GUI_CENTER | GUI_SELECT, &snd_upd_list, &psp_snd_upd, GUI_ENABLED },
		{ "Sound Effects", GUI_CENTER | GUI_TOGGLE, &psp_sfx_enabled, 0, GUI_ENABLED },
		{ "Music", GUI_CENTER | GUI_TOGGLE, &psp_music_enabled, 0, GUI_SET_ME },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_list tv_res_list[] = {
		{ "720x480", 0 },
		{ "704x448", 1 },
		{ "640x400", 2 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_list tv_cable_list[] = {
		{ "None found", 0 },
		{ "Composite", 1 },
		{ "Component", 2 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_list psp_detail_list[] = {
		{ "High", 0 },
		{ "Low", 1 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_list psp_aspect_list[] = {
		{ "4:3", 0 },
		{ "16:9", 1 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_menu TvLevel[] = {
		{ "Cable", GUI_CENTER | GUI_SELECT, &tv_cable_list, &psp_tv_cable, GUI_DISABLED },
		{ "Resolution", GUI_CENTER | GUI_SELECT, &tv_res_list, &psp_tv_res, GUI_ENABLED },
		{ "Sync to VBlank", GUI_CENTER | GUI_TOGGLE, &psp_tv_sync, 0, GUI_ENABLED },
		{ "Detail", GUI_CENTER | GUI_SELECT, &psp_detail_list, &psp_tv_detail, GUI_ENABLED },
		{ "Interlaced", GUI_CENTER | GUI_TOGGLE, &psp_tv_laced, 0, GUI_SET_ME },
		{ "Aspect Ratio", GUI_CENTER | GUI_SELECT, &psp_aspect_list, &psp_tv_aspect, GUI_ENABLED },
		{ "Center Screen", GUI_CENTER | GUI_FUNCTION, &psp_tv_center, 0, GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_list lcd_res_list[] = {
		{ "480x272", 0 },
		{ "368x272", 1 },
		{ "320x240", 2 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_menu LcdLevel[] = {
		{ "Resolution", GUI_CENTER | GUI_SELECT, &lcd_res_list, &psp_lcd_res, GUI_ENABLED },
		{ "Sync to VBlank", GUI_CENTER | GUI_TOGGLE, &psp_lcd_sync, 0, GUI_ENABLED },
		{ "Detail", GUI_CENTER | GUI_SELECT, &psp_detail_list, &psp_lcd_detail, GUI_ENABLED },
		{ "Aspect Ratio", GUI_CENTER | GUI_SELECT, &psp_aspect_list, &psp_lcd_aspect, GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_list psp_display_list[] = {
		{ "Use LCD", 0 },
		{ "Use TV", 1 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_menu VideoLevel[] = {
		{ "LCD Settings", GUI_CENTER | GUI_MENU, LcdLevel, 0, GUI_ENABLED },
		{ "TV Settings", GUI_CENTER | GUI_MENU, TvLevel, 0, GUI_SET_ME },
		{ "Display", GUI_CENTER | GUI_SELECT, &psp_display_list, &psp_use_tv, GUI_SET_ME },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_list cpu_speed_list[] = {
		{ "Default", 0 },
		{ "133/66 MHz", 1 },
		{ "222/111 MHz", 2 },
		{ "266/133 MHz", 3 },
		{ "300/150 MHz", 4 },
		{ "333/166 MHz", 5 },
		{ 0, GUI_END_OF_LIST }
	};

	struct gui_menu CpuLevel[] = {
		{ "CPU Clock Frequency", GUI_CENTER | GUI_SELECT, &cpu_speed_list, &psp_cpu_speed, GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	struct gui_menu TopLevel[] = {
		{ "Load Settings", GUI_CENTER | GUI_FUNCTION, &psp_load_settings, 0, GUI_ENABLED },
		{ "Save Settings", GUI_CENTER | GUI_FUNCTION, &psp_save_settings, 0, GUI_ENABLED },
		{ "CPU Settings", GUI_CENTER |GUI_MENU, CpuLevel, 0, GUI_ENABLED },
		{ "Video Settings", GUI_CENTER | GUI_MENU, VideoLevel, 0, GUI_ENABLED },
		{ "Sound Settings", GUI_CENTER | GUI_MENU, SoundLevel, 0, GUI_ENABLED },
		{ "Controller Settings", GUI_CENTER | GUI_MENU, ControlLevel, 0, GUI_ENABLED },
		{ "File Settings", GUI_CENTER | GUI_MENU, FileLevel, 0, GUI_ENABLED },
		{ "Game Settings", GUI_CENTER | GUI_MENU, GameLevel, 0, GUI_ENABLED },
		{ "Network Settings", GUI_CENTER |GUI_MENU, NetLevel, 0, GUI_ENABLED },
		{ "About", GUI_CENTER | GUI_MENU, AboutLevel, 0, GUI_ENABLED },
		{ 0, GUI_END_OF_MENU, 0, 0, 0 } // end of menu
	};

	FILE *temp;
	char str[256];

	// start by setting some of the enables that depend on certain variables
	VideoLevel[1].enable = (psp_tv_cable > 0) ? GUI_ENABLED : GUI_DISABLED;
	VideoLevel[2].enable = (psp_tv_cable > 0) ? GUI_ENABLED : GUI_DISABLED;
	TvLevel[4].enable = (psp_tv_cable == 2) ? GUI_ENABLED : GUI_DISABLED;
	psp_tv_laced = (psp_tv_cable == 1) ? 1 : 0; // force laced if composite cable
	sprintf(str,"%s%s",psp_home,"MIDI_Instruments");
	temp = fopen(str, "rb");
	psp_music_enabled = temp ? 1 : 0;
	SoundLevel[2].enable = temp ? GUI_ENABLED : GUI_DISABLED;
	if (temp)
		fclose(temp);

	do_gui(TopLevel, (void *)0, "Start DOOM");

	// now set the arg list
	set_myargv();
}

// Network support code

int connect_to_apctl(int config) {
  int err, i;
  int stateLast = -1;

  if (sceWlanGetSwitchState() != 1)
    printf("Please enable WLAN or press a button to procede without networking.\n");
  for (i=0; i<10; i++)
    {
      SceCtrlData pad;
      if (sceWlanGetSwitchState() == 1) break;
      sceCtrlReadBufferPositive(&pad, 1);
      if (pad.Buttons) return 0;
	  printf("%d... ", 10-i);
      sceKernelDelayThread(1000 * 1000);
    }
  printf("\n");
  if (i == 10) return 0;

  err = sceNetApctlConnect(config);
  if (err != 0) {
    printf("sceNetApctlConnect returns %08X\n", err);
    return 0;
  }

  printf("Connecting...\n");
  while (1) {
    int state;
    err = sceNetApctlGetState(&state);
    if (err != 0) {
      printf("sceNetApctlGetState returns $%x\n", err);
      break;
    }
    if (state != stateLast) {
      printf("  Connection state %d of 4.\n", state);
      stateLast = state;
    }
    if (state == 4) {
      break;
    }
    sceKernelDelayThread(50 * 1000);
  }
  connected_accesspoint = config;
  printf("Connected!\n");
  sceKernelDelayThread(3000 * 1000);

  if (err != 0) {
    return 0;
  }

  return 1;
}

char *getconfname(int confnum) {
  static char confname[128];

  if (sceUtilityCheckNetParam(confnum) != 0)
  	return 0;

  sceUtilityGetNetParam(confnum, PSP_NETPARAM_NAME, (netData *)confname);
  return confname;
}

int net_thread(SceSize args, void *argp)
{
  int selComponent = psp_net_accesspoint; // access point selector

  printf("Using connection %d (%s) to connect...\n", selComponent, getconfname(selComponent));

  if (connect_to_apctl(selComponent))
  {
    if (sceNetApctlGetInfo(8, szMyIPAddr) != 0)
      strcpy(szMyIPAddr, "unknown IP address");
    printf("IP: %s\n", szMyIPAddr);
    psp_net_available = 1;
  }
  else
    psp_net_available = -1;

  return 0;
}

int InitialiseNetwork(void)
{
  int err;

  printf("load network modules...");
  err = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
  if (err != 0)
  {
    printf("Error, could not load PSP_NET_MODULE_COMMON %08X\n", err);
    return 1;
  }
  err = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
  if (err != 0)
  {
    printf("Error, could not load PSP_NET_MODULE_INET %08X\n", err);
    return 1;
  }
  printf("done\n");

  err = pspSdkInetInit();
  if (err != 0)
  {
    printf("Error, could not initialise the network %08X\n", err);
    return 1;
  }
  return 0;
}

void psp_net_init (void)
{
	int i;

	if (InitialiseNetwork() != 0)
	{
		psp_net_error = 1;
		printf("Networking not available. Will be disabled for game.\n");
		sceKernelDelayThread(4*1000*1000);
		return;
	}

	for (i=1; i<100; i++)
	{
		char *cfg;

		cfg = getconfname(i);
		if (cfg)
		{
			printf("Found connection %d (%s)\n", i, cfg);
			strcat(net_ap_str, cfg);
			strcat(net_ap_str, " ");
		}
		else
			break;
	}
	if (i>1)
		net_access_range[1] = i-1;

	sceKernelDelayThread(2*1000*1000);
}

void psp_net_connect (void * arg)
{
	SceUID thid;
	int i;

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	// check if connected
	if (connected_accesspoint)
		if (connected_accesspoint == psp_net_accesspoint)
		{
			printf("Already connected to access point.\n");
			sceKernelDelayThread(4*1000*1000);
			return;
		}

	thid = sceKernelCreateThread("net_thread", net_thread, 0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if (thid < 0) {
		printf("Could not create network thread. Networking disabled for game.\n");
		sceKernelDelayThread(4*1000*1000);
		return;
	}
	sceKernelStartThread(thid, 0, NULL);
	for (i=0; i<30; i++)
	{
		if (psp_net_available) break;
		sceKernelDelayThread(1000*1000);
	}
	if (psp_net_available != 1)
	{
		printf("Networking failed to connect. Will be disabled for game.\n");
		sceKernelDelayThread(4*1000*1000);
		pspDebugScreenClear();
		return;
	}

	psp_net_ipaddr = strdup(szMyIPAddr);

	printf("Networking successfully started. Networking enabled for game.\n");
	sceKernelDelayThread(4*1000*1000);

	pspDebugScreenClear();
}

void psp_net_disconnect(void *arg)
{
	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	// check if connected
	if (connected_accesspoint)
	{
		// disconnect access point
		sceNetApctlDisconnect();
		psp_net_available = 0;
		connected_accesspoint = 0;
		printf("Access point disconnected.\n");
		sceKernelDelayThread(2*1000*1000);
	}
	else
	{
		printf("Already disconnected.\n");
		sceKernelDelayThread(4*1000*1000);
	}

	pspDebugScreenClear();
}

void psp_net_reconnect(void)
{
	int i, p;

	p = M_CheckParm ("-cpuMHz");

	// check if need to disconnect
	if ((psp_net_enabled && (psp_net_accesspoint != connected_accesspoint))
	  || p
	  || (!psp_net_enabled && connected_accesspoint))
	{
		// disconnect access point
		sceNetApctlDisconnect();
		psp_net_available = 0;
		connected_accesspoint = 0;
		sceKernelDelayThread(2*1000*1000);
	}

	// check if need to change CPU speed
	if (p && p < myargc - 1)
	{
		i = atoi (myargv[p+1]);
		scePowerSetClockFrequency(i, i, i>>1);
	}

	// check if need to reconnect
	if (psp_net_enabled && (psp_net_accesspoint != connected_accesspoint))
	{
		SceUID thid;
		int i;

		// reconnect to selected access point
		thid = sceKernelCreateThread("net_thread", net_thread, 0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
		if (thid < 0) {
			printf("Could not create network thread. Networking disabled for game.\n");
			psp_net_enabled = 0;
		}
		sceKernelStartThread(thid, 0, NULL);
		for (i=0; i<30; i++)
		{
			if (psp_net_available) break;
			sceKernelDelayThread(1000*1000);
		}
		if (psp_net_available != 1)
		{
			printf("Couldn't connect to access point. Networking disabled for game.\n");
			psp_net_enabled = 0;
		}
		else
			printf("Connected to selected access point.\n");
		sceKernelDelayThread(3*1000*1000);
	}
}

int main (int argc, char **argv)
{
    int i, p;

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	SetupCallbacks();

	// get launch directory name for our home
	strncpy(psp_home,argv[0],sizeof(psp_home)-1);
	psp_home[sizeof(psp_home)-1] = 0;
	char *str_ptr=strrchr(psp_home,'/');
	if (str_ptr){
		str_ptr++;
		*str_ptr = 0;
	}

	if (sceKernelDevkitVersion() >= 0x03070110)
		if (kuKernelGetModel() == PSP_MODEL_SLIM_AND_LITE)
		{
			char str[256];
			sprintf(str,"%s%s",psp_home,"dvemgr.prx");
			if (pspSdkLoadStartModule(str, PSP_MEMORY_PARTITION_KERNEL) >= 0)
				psp_tv_cable = pspDveMgrCheckVideoOut();
		}

	psp_net_init();
	psp_font_init();

	if ((myargv = malloc(sizeof(char *) * MAXARGVS)) == NULL)
	{
		printf("malloc(%d) failed", sizeof(char *) * MAXARGVS);
		sceKernelDelayThread(5*1000*1000);
		sceKernelExitGame();
	}
	memset (myargv, 0, sizeof(char *)*MAXARGVS);
	myargc = 0;

	psp_load_defaults();

	if (!psp_iwad_file)
	{
		char temp[256];
		FILE *hnd;

		strcpy(temp, psp_home);
		strcat(temp, "iwad/doom1.wad");
		hnd = fopen(temp, "rb");
		if (hnd)
		{
			fclose(hnd);
			psp_iwad_file = strdup(temp);
		}
		else
		{
			strcpy(temp, psp_home);
			strcat(temp, "iwad/DOOM1.WAD");
			hnd = fopen(temp, "rb");
			if (hnd)
			{
				fclose(hnd);
				psp_iwad_file = strdup(temp);
			}
		}
	}

	psp_gui();

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xFF000000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	printf ("DOOM v%d.%d for the PSP\n\n", VERS, REVS);
	printf ("Args passed to D_DoomMain() are:\n");
	for (i = 1 ; i < myargc; i++)
		printf (" %s", myargv[i]);
	printf ("\n\n");

/* The original fixed point code is faster on GCC */
#ifdef USE_FLOAT_FIXED
	SetFPMode ();  /* set FPU rounding mode to "trunc towards -infinity" */
#endif

	psp_net_reconnect(); // disconnect and reconnect if changed connection or CPU speed

	i = scePowerGetCpuClockFrequency();
	printf("The current CPU speed is %d MHz\n\n", i);
	sceKernelDelayThread(1*1000*1000);

	p = M_CheckParm ("-forceversion");
	if (p && p < myargc - 1)
		VERSION = atoi (myargv[p+1]);

	D_DoomMain ();

	sceKernelDelayThread(5*1000*1000);
	sceKernelExitGame();
	return 0;
}


#ifndef __SASC
void main_cleanup(void)
{
}
#endif


int isIWADDoom2 (void)
{
    int         p;
    char wadname[256];

	if (!psp_iwad_file)
		return 1;

    for (p=strlen(psp_iwad_file)-1; p>0; p--)
        if (psp_iwad_file[p] == '/') break;
    strcpy(wadname, &psp_iwad_file[p+1]);

	if (!strcasecmp(wadname, "doom2f.wad"))
        return 1;

	if (!strcasecmp(wadname, "doom2.wad"))
        return 1;

	if (!strcasecmp(wadname, "plutonia.wad"))
        return 1;

	if (!strcasecmp(wadname, "tnt.wad"))
        return 1;

	return 0;
}

void set_myargv(void)
{
	char temp[256];
	int i;

	// free old argv entries
	if (myargc)
		for (i = 0 ; i < myargc; i++)
		{
			free(myargv[i]);
			myargv[i] = 0;
		}

	myargv[0] = strdup("Doom");
	myargc = 1;

	if (psp_use_tv)
	{
		myargv[myargc] = strdup("-tv");
		myargc++;

		switch (psp_tv_res)
		{
			case 0:
			myargv[myargc] = strdup("-width");
			myargc++;
			myargv[myargc] = strdup("720");
			myargc++;
			myargv[myargc] = strdup("-height");
			myargc++;
			myargv[myargc] = strdup("480");
			myargc++;
			break;
			case 1:
			myargv[myargc] = strdup("-width");
			myargc++;
			myargv[myargc] = strdup("704");
			myargc++;
			myargv[myargc] = strdup("-height");
			myargc++;
			myargv[myargc] = strdup("448");
			myargc++;
			break;
			case 2:
			myargv[myargc] = strdup("-width");
			myargc++;
			myargv[myargc] = strdup("640");
			myargc++;
			myargv[myargc] = strdup("-height");
			myargc++;
			myargv[myargc] = strdup("400");
			myargc++;
			break;
		}

		if (psp_tv_sync)
		{
			myargv[myargc] = strdup("-vsync");
			myargc++;
		}

		if (psp_tv_laced)
		{
			myargv[myargc] = strdup("-laced");
			myargc++;
		}

		if (psp_tv_aspect)
		{
			myargv[myargc] = strdup("-16:9");
			myargc++;
		}

		if (psp_tv_detail)
		{
			myargv[myargc] = strdup("-lowdetail");
			myargc++;
		}

		myargv[myargc] = strdup("-tvcx");
		myargc++;
		sprintf(temp, "%d", psp_tv_cx);
		myargv[myargc] = strdup(temp);
		myargc++;
		myargv[myargc] = strdup("-tvcy");
		myargc++;
		sprintf(temp, "%d", psp_tv_cy);
		myargv[myargc] = strdup(temp);
		myargc++;
	}
	else
	{
		switch (psp_lcd_res)
		{
			case 0:
			myargv[myargc] = strdup("-width");
			myargc++;
			myargv[myargc] = strdup("480");
			myargc++;
			myargv[myargc] = strdup("-height");
			myargc++;
			myargv[myargc] = strdup("272");
			myargc++;
			break;
			case 1:
			myargv[myargc] = strdup("-width");
			myargc++;
			myargv[myargc] = strdup("368");
			myargc++;
			myargv[myargc] = strdup("-height");
			myargc++;
			myargv[myargc] = strdup("272");
			myargc++;
			break;
			case 2:
			myargv[myargc] = strdup("-width");
			myargc++;
			myargv[myargc] = strdup("320");
			myargc++;
			myargv[myargc] = strdup("-height");
			myargc++;
			myargv[myargc] = strdup("240");
			myargc++;
			break;
		}

		if (psp_lcd_sync)
		{
			myargv[myargc] = strdup("-vsync");
			myargc++;
		}

		if (psp_lcd_aspect)
		{
			myargv[myargc] = strdup("-16:9");
			myargc++;
		}

		if (psp_lcd_detail)
		{
			myargv[myargc] = strdup("-lowdetail");
			myargc++;
		}
	}

	if (!psp_sfx_enabled)
	{
		myargv[myargc] = strdup("-nosfx");
		myargc++;
	}

	if (psp_music_enabled)
	{
		myargv[myargc] = strdup("-music");
		myargc++;
	}

	switch (psp_snd_upd)
	{
		case 0:
		myargv[myargc] = strdup("-140Hz");
		myargc++;
		break;
		case 1:
		myargv[myargc] = strdup("-70Hz");
		myargc++;
		break;
	}

	if (psp_iwad_file)
	{
		myargv[myargc] = strdup("-iwad");
		myargc++;
		myargv[myargc] = strdup(psp_iwad_file);
		myargc++;
	}

	if (psp_pwad_file1 || psp_pwad_file2 || psp_pwad_file3 || psp_pwad_file4)
	{
		myargv[myargc] = strdup("-file");
		myargc++;
		if (psp_pwad_file1)
		{
			myargv[myargc] = strdup(psp_pwad_file1);
			myargc++;
		}
		if (psp_pwad_file2)
		{
			myargv[myargc] = strdup(psp_pwad_file2);
			myargc++;
		}
		if (psp_pwad_file3)
		{
			myargv[myargc] = strdup(psp_pwad_file3);
			myargc++;
		}
		if (psp_pwad_file4)
		{
			myargv[myargc] = strdup(psp_pwad_file4);
			myargc++;
		}
	}

	if (psp_deh_file1 || psp_deh_file2 || psp_deh_file3 || psp_deh_file4)
	{
		myargv[myargc] = strdup("-deh");
		myargc++;
		if (psp_deh_file1)
		{
			myargv[myargc] = strdup(psp_deh_file1);
			myargc++;
		}
		if (psp_deh_file2)
		{
			myargv[myargc] = strdup(psp_deh_file2);
			myargc++;
		}
		if (psp_deh_file3)
		{
			myargv[myargc] = strdup(psp_deh_file3);
			myargc++;
		}
		if (psp_deh_file4)
		{
			myargv[myargc] = strdup(psp_deh_file4);
			myargc++;
		}
	}

	if (psp_game_nomonsters)
	{
		myargv[myargc] = strdup("-nomonsters");
		myargc++;
	}

	if (psp_game_respawn)
	{
		myargv[myargc] = strdup("-respawn");
		myargc++;
	}

	if (psp_game_fast)
	{
		myargv[myargc] = strdup("-fast");
		myargc++;
	}

	if (psp_game_turbo)
	{
		myargv[myargc] = strdup("-turbo");
		myargc++;
	}

	if (psp_game_maponhu)
	{
		myargv[myargc] = strdup("-maponhu");
		myargc++;
	}

	if (psp_game_rotatemap)
	{
		myargv[myargc] = strdup("-rotatemap");
		myargc++;
	}

	if (psp_game_deathmatch)
	{
		myargv[myargc] = strdup("-deathmatch");
		myargc++;
	}

	if (psp_game_altdeath)
	{
		myargv[myargc] = strdup("-altdeath");
		myargc++;
	}

	if (psp_game_record)
	{
		myargv[myargc] = strdup("-record");
		myargc++;
		sprintf(temp, "demo%1d", psp_game_record);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_game_playdemo)
	{
		myargv[myargc] = strdup("-playdemo");
		myargc++;
		sprintf(temp, "demo%1d", psp_game_playdemo);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_game_forcedemo)
	{
		myargv[myargc] = strdup("-forcedemo");
		myargc++;
	}

	if (psp_game_timedemo)
	{
		myargv[myargc] = strdup("-timedemo");
		myargc++;
		sprintf(temp, "demo%1d", psp_game_timedemo);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_game_timer)
	{
		myargv[myargc] = strdup("-timer");
		myargc++;
		sprintf(temp, "%2d", psp_game_timer);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_game_avg)
	{
		myargv[myargc] = strdup("-avg");
		myargc++;
	}

	if (psp_game_statcopy)
	{
		myargv[myargc] = strdup("-statcopy");
		myargc++;
	}

	if (psp_game_version != 110)
	{
		myargv[myargc] = strdup("-forceversion");
		myargc++;
		sprintf(temp, "%3d", psp_game_version);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_cpu_speed != 0)
	{
		myargv[myargc] = strdup("-cpuMHz");
		myargc++;
		switch (psp_cpu_speed)
		{
			case 1:
			myargv[myargc] = strdup("133");
			myargc++;
			break;
			case 2:
			myargv[myargc] = strdup("222");
			myargc++;
			break;
			case 3:
			myargv[myargc] = strdup("266");
			myargc++;
			break;
			case 4:
			myargv[myargc] = strdup("300");
			myargc++;
			break;
			case 5:
			myargv[myargc] = strdup("333");
			myargc++;
			break;
		}
	}

	if (psp_stick_disabled)
	{
		myargv[myargc] = strdup("-noanalog");
		myargc++;
	}
	else
	{
		myargv[myargc] = strdup("-analogcx");
		myargc++;
		sprintf(temp, "%d", psp_stick_cx);
		myargv[myargc] = strdup(temp);
		myargc++;
		myargv[myargc] = strdup("-analogcy");
		myargc++;
		sprintf(temp, "%d", psp_stick_cy);
		myargv[myargc] = strdup(temp);
		myargc++;
		myargv[myargc] = strdup("-analogminx");
		myargc++;
		sprintf(temp, "%d", psp_stick_minx);
		myargv[myargc] = strdup(temp);
		myargc++;
		myargv[myargc] = strdup("-analogminy");
		myargc++;
		sprintf(temp, "%d", psp_stick_miny);
		myargv[myargc] = strdup(temp);
		myargc++;
		myargv[myargc] = strdup("-analogmaxx");
		myargc++;
		sprintf(temp, "%d", psp_stick_maxx);
		myargv[myargc] = strdup(temp);
		myargc++;
		myargv[myargc] = strdup("-analogmaxy");
		myargc++;
		sprintf(temp, "%d", psp_stick_maxy);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_ctrl_cheat1)
	{
		myargv[myargc] = strdup("-cheat1");
		myargc++;
		sprintf(temp, "%d", psp_ctrl_cheat1);
		myargv[myargc] = strdup(temp);
		myargc++;
	}
	if (psp_ctrl_cheat2)
	{
		myargv[myargc] = strdup("-cheat2");
		myargc++;
		sprintf(temp, "%d", psp_ctrl_cheat2);
		myargv[myargc] = strdup(temp);
		myargc++;
	}
	if (psp_ctrl_cheat3)
	{
		myargv[myargc] = strdup("-cheat3");
		myargc++;
		sprintf(temp, "%d", psp_ctrl_cheat3);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_net_enabled && !psp_net_error)
	{
		myargv[myargc] = strdup("-typenet");
		myargc++;
		sprintf(temp, "%d", psp_net_type);
		myargv[myargc] = strdup(temp);
		myargc++;

		myargv[myargc] = strdup("-accesspoint");
		myargc++;
		sprintf(temp, "%d", psp_net_accesspoint);
		myargv[myargc] = strdup(temp);
		myargc++;

		if (psp_net_port != 5029)
		{
			myargv[myargc] = strdup("-port");
			myargc++;
			sprintf(temp, "%d", psp_net_port);
			myargv[myargc] = strdup(temp);
			myargc++;
		}

		if (psp_net_ticdup > 1)
		{
			myargv[myargc] = strdup("-dup");
			myargc++;
			sprintf(temp, "%d", psp_net_ticdup);
			myargv[myargc] = strdup(temp);
			myargc++;
		}

		if (psp_net_extratic)
		{
			myargv[myargc] = strdup("-extratic");
			myargc++;
		}

		if (psp_game_pcchksum)
		{
			myargv[myargc] = strdup("-pcchecksum");
			myargc++;
		}

		myargv[myargc] = strdup("-net");
		myargc++;
		sprintf(temp, "%d", psp_net_player1);
		myargv[myargc] = strdup(temp);
		myargc++;

		if (psp_net_player2)
		{
			if (psp_net_player2[0] >= '1' && psp_net_player2[0] <= '9')
			{
				strcpy(temp, ".");
				strcat(temp, psp_net_player2);
			}
			else
				strcpy(temp, psp_net_player2);
			myargv[myargc] = strdup(temp);
			myargc++;
		}

		if (psp_net_player3)
		{
			if (psp_net_player3[0] >= '1' && psp_net_player3[0] <= '9')
			{
				strcpy(temp, ".");
				strcat(temp, psp_net_player3);
			}
			else
				strcpy(temp, psp_net_player3);
			myargv[myargc] = strdup(temp);
			myargc++;
		}

		if (psp_net_player4)
		{
			if (psp_net_player4[0] >= '1' && psp_net_player4[0] <= '9')
			{
				strcpy(temp, ".");
				strcat(temp, psp_net_player4);
			}
			else
				strcpy(temp, psp_net_player4);
			myargv[myargc] = strdup(temp);
			myargc++;
		}
	}

	if (psp_game_skill != 2)
	{
		myargv[myargc] = strdup("-skill");
		myargc++;
		sprintf(temp, "%d", psp_game_skill+1);
		myargv[myargc] = strdup(temp);
		myargc++;
	}

	if (psp_game_level != 1)
	{
		myargv[myargc] = strdup("-warp");
		myargc++;
		if (isIWADDoom2())
		{
			sprintf(temp, "%d", psp_game_level);
			myargv[myargc] = strdup(temp);
			myargc++;
		}
		else
		{
			sprintf(temp, "%d", (psp_game_level-1) / 9 + 1);
			myargv[myargc] = strdup(temp);
			myargc++;
			sprintf(temp, "%d", (psp_game_level-1) % 9 + 1);
			myargv[myargc] = strdup(temp);
			myargc++;
		}
	}
}

int first_argv(char *check)
{
    int		i;

    for (i=1; i<myargc; i++)
		if ( !strcasecmp(check, myargv[i]) )
		    return i;

    return 0;
}

void get_myargv(void)
{
	int i, j;

	psp_use_tv = 0;
	if (first_argv("-tv"))
	{
		psp_use_tv = 1;

		psp_tv_res = 0;
		i = first_argv("-width");
		if (i)
		{
			sscanf(myargv[i+1], "%d", &j);
			switch (j)
			{
				case 720:
				psp_tv_res = 0;
				break;
				case 704:
				psp_tv_res = 1;
				break;
				case 640:
				psp_tv_res = 2;
				break;
			}
		}

		psp_tv_sync = 0;
		if (first_argv("-vsync"))
			psp_tv_sync = 1;

		psp_tv_laced = 0;
		if (first_argv("-laced"))
			psp_tv_laced = 1;

		psp_tv_aspect = 0;
		if (first_argv("-16:9"))
			psp_tv_aspect = 1;

		psp_tv_detail = 0;
		if (first_argv("-lowdetail"))
			psp_tv_detail = 1;

		psp_tv_cx = 0;
		i = first_argv("-tvcx");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_tv_cx);

		psp_tv_cy = 0;
		i = first_argv("-tvcy");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_tv_cy);
	}
	else
	{
		psp_lcd_res = 0;
		i = first_argv("-width");
		if (i)
		{
			sscanf(myargv[i+1], "%d", &j);
			switch (j)
			{
				case 480:
				psp_lcd_res = 0;
				break;
				case 368:
				psp_lcd_res = 1;
				break;
				case 320:
				psp_lcd_res = 2;
				break;
			}
		}

		psp_lcd_sync = 0;
		if (first_argv("-vsync"))
			psp_lcd_sync = 1;

		psp_lcd_aspect = 0;
		if (first_argv("-16:9"))
			psp_lcd_aspect = 1;

		psp_lcd_detail = 0;
		if (first_argv("-lowdetail"))
			psp_lcd_detail = 1;
	}

	psp_sfx_enabled = 1;
	if (first_argv("-nosfx"))
		psp_sfx_enabled = 0;

	psp_music_enabled = 0;
	if (first_argv("-music"))
		psp_music_enabled = 1;

	psp_snd_upd = 2;
	if (first_argv("-140Hz"))
		psp_snd_upd = 0;
	if (first_argv("-70Hz"))
		psp_snd_upd = 1;

	i = first_argv("-iwad");
	if (i)
		psp_iwad_file = strdup(myargv[i+1]);

	i = first_argv("-file");
	if (i)
	{
		psp_pwad_file1 = strdup(myargv[i+1]);
		if ((i+2 < myargc) && myargv[i+2][0] != '-')
		{
			psp_pwad_file2 = strdup(myargv[i+2]);
			if ((i+3 < myargc) && myargv[i+3][0] != '-')
			{
				psp_pwad_file3 = strdup(myargv[i+3]);
				if ((i+4 < myargc) && myargv[i+4][0] != '-')
					psp_pwad_file4 = strdup(myargv[i+4]);
			}
		}
	}

	i = first_argv("-deh");
	if (i)
	{
		psp_deh_file1 = strdup(myargv[i+1]);
		if ((i+2 < myargc) && myargv[i+2][0] != '-')
		{
			psp_deh_file2 = strdup(myargv[i+2]);
			if ((i+3 < myargc) && myargv[i+3][0] != '-')
			{
				psp_deh_file3 = strdup(myargv[i+3]);
				if ((i+4 < myargc) && myargv[i+4][0] != '-')
					psp_deh_file4 = strdup(myargv[i+1]);
			}
		}
	}

	psp_game_nomonsters = 0;
	if (first_argv("-nomonsters"))
		psp_game_nomonsters = 1;

	psp_game_respawn = 0;
	if (first_argv("-respawn"))
		psp_game_respawn = 1;

	psp_game_fast = 0;
	if (first_argv("-fast"))
		psp_game_fast = 1;

	psp_game_turbo = 0;
	if (first_argv("-turbo"))
		psp_game_turbo = 1;

	psp_game_maponhu = 0;
	if (first_argv("-maponhu"))
		psp_game_maponhu = 1;

	psp_game_rotatemap = 0;
	if (first_argv("-rotatemap"))
		psp_game_rotatemap = 1;

	psp_game_deathmatch = 0;
	if (first_argv("-deathmatch"))
		psp_game_deathmatch = 1;

	psp_game_altdeath = 0;
	if (first_argv("-altdeath"))
		psp_game_altdeath = 1;

	psp_game_record = 0;
	i = first_argv("-record");
	if (i)
		psp_game_record = (int)(myargv[i+1][4]) - 0x30;

	psp_game_playdemo = 0;
	i = first_argv("-playdemo");
	if (i)
		psp_game_playdemo = (int)(myargv[i+1][4]) - 0x30;

	psp_game_forcedemo = 0;
	if (first_argv("-forcedemo"))
		psp_game_forcedemo = 1;

	psp_game_timedemo = 0;
	i = first_argv("-timedemo");
	if (i)
		psp_game_timedemo = (int)(myargv[i+1][4]) - 0x30;

	psp_game_timer = 0;
	i = first_argv("-timer");
	if (i)
		sscanf(myargv[i+1], "%d", &psp_game_timer);

	psp_game_avg = 0;
	if (first_argv("-avg"))
		psp_game_avg = 1;

	psp_game_statcopy = 0;
	if (first_argv("-statcopy"))
		psp_game_statcopy = 1;

	psp_game_version = 110;
	i = first_argv("-forceversion");
	if (i)
		sscanf(myargv[i+1], "%d", &psp_game_version);

	psp_cpu_speed = 0;
	i = first_argv("-cpuMHz");
	if (i)
	{
		sscanf(myargv[i+1], "%d", &psp_cpu_speed);
		switch (psp_cpu_speed)
		{
			case 133:
			psp_cpu_speed = 1;
			break;
			case 222:
			psp_cpu_speed = 2;
			break;
			case 266:
			psp_cpu_speed = 3;
			break;
			case 300:
			psp_cpu_speed = 4;
			break;
			case 333:
			psp_cpu_speed = 5;
			break;
			default:
			psp_cpu_speed = 0;
		}
	}

	psp_stick_disabled = 0;
	i = first_argv("-noanalog");
	if (i)
		psp_stick_disabled = 1;
	else
	{
		psp_stick_cx = 128;
		i = first_argv("-analogcx");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_stick_cx);
		psp_stick_cy = 128;
		i = first_argv("-analogcy");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_stick_cy);
		psp_stick_minx = 0;
		i = first_argv("-analogminx");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_stick_minx);
		psp_stick_miny = 0;
		i = first_argv("-analogminy");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_stick_miny);
		psp_stick_maxx = 255;
		i = first_argv("-analogmaxx");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_stick_maxx);
		psp_stick_maxy = 255;
		i = first_argv("-analogmaxy");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_stick_maxy);
	}

	psp_ctrl_cheat1 = 0;
	i = first_argv("-cheat1");
	if (i)
		sscanf(myargv[i+1], "%d", &psp_ctrl_cheat1);
	psp_ctrl_cheat2 = 0;
	i = first_argv("-cheat2");
	if (i)
		sscanf(myargv[i+1], "%d", &psp_ctrl_cheat2);
	psp_ctrl_cheat3 = 0;
	i = first_argv("-cheat3");
	if (i)
		sscanf(myargv[i+1], "%d", &psp_ctrl_cheat3);

	psp_net_enabled = 0;
	i = first_argv("-net");
	if (i && !psp_net_error)
	{
		psp_net_enabled = 1;
		sscanf(myargv[i+1], "%d", &psp_net_player1);
		if ((i+2 < myargc) && myargv[i+2][0] != '-')
		{
			if (myargv[i+2][0] == '.')
				psp_net_player2 = strdup(&myargv[i+2][1]);
			else
				psp_net_player2 = strdup(myargv[i+2]);
			if ((i+3 < myargc) && myargv[i+3][0] != '-')
			{
				if (myargv[i+3][0] == '.')
					psp_net_player3 = strdup(&myargv[i+3][1]);
				else
					psp_net_player3 = strdup(myargv[i+3]);
				if ((i+4 < myargc) && myargv[i+4][0] != '-')
				{
					if (myargv[i+4][0] == '.')
						psp_net_player4 = strdup(&myargv[i+4][1]);
					else
						psp_net_player4 = strdup(myargv[i+4]);
				}
			}
		}

		psp_net_type = 0;
		i = first_argv("-typenet");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_net_type);

		psp_net_accesspoint = 1;
		i = first_argv("-accesspoint");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_net_accesspoint);
		if (psp_net_accesspoint > net_access_range[1])
			psp_net_accesspoint = 1;

		psp_net_port = 5029; // 5000 + 0x1d
		i = first_argv("-port");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_net_port);

		psp_net_ticdup = 0;
		i = first_argv("-dup");
		if (i)
			sscanf(myargv[i+1], "%d", &psp_net_ticdup);

		psp_net_extratic = 0;
		if (first_argv("-extratic"))
			psp_net_extratic = 1;

		psp_game_pcchksum = 0;
		if (first_argv("-pcchecksum"))
			psp_game_pcchksum = 1;
	}

	psp_game_skill = 2;
	i = first_argv("-skill");
	if (i)
		sscanf(myargv[i+1], "%d", &psp_game_skill);

	psp_game_level = 1;
	i = first_argv("-warp");
	if (i && isIWADDoom2())
		sscanf(myargv[i+1], "%d", &psp_game_level);
	else if (i)
	{
		int t1, t2;
		sscanf(myargv[i+1], "%d", &t1);
		sscanf(myargv[i+2], "%d", &t2);
		psp_game_level = (t1-1)*9 + t2;
	}

}

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspgu.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define printf pspDebugScreenPrintf

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "d_main.h"

#include "r_draw.h"
#include "w_wad.h"
#include "z_zone.h"

/**********************************************************************/
void video_cleanup (void);

int pspDveMgrCheckVideoOut();
int pspDveMgrSetVideoOut(int, int, int, int, int, int, int);


int SCREENWIDTH;
int SCREENHEIGHT;
int weirdaspect;

#define NUMPALETTES 14

static u32 video_colourtable[NUMPALETTES][256];
static int video_palette_index = 0;
static int video_palette_changed = 0;

static int video_doing_fps = 0;
static int video_is_laced = 0;
static int video_cable = 0;
static int video_is_tv = 0;
static int video_vsync = 0;

static int lineBytes, lineWidth;

static u32 *vram1;
static u32 *vram2;
static int vramflip = 0;
static int vramoffset;

static unsigned int total_frames = 0;

#ifdef PROFILE
unsigned int profile[32][4];
#endif

/**********************************************************************/
static void video_do_fps (u32 *base, int yoffset)
{
}

/**********************************************************************/
void video_set_vmode(void)
{
	if (video_is_tv)
	{
		if (video_cable == 1)
			pspDveMgrSetVideoOut(2, 0x1d1, 720, 503, 1, 15, 0); // composite
		else
			if (video_is_laced)
				pspDveMgrSetVideoOut(0, 0x1d1, 720, 503, 1, 15, 0); // component interlaced
			else
				pspDveMgrSetVideoOut(0, 0x1d2, 720, 480, 1, 15, 0); // component progressive
	}
	else
		sceDisplaySetMode(0,480,272);
}

/**********************************************************************/
// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics (void)
{
	int p;

	atexit(video_cleanup);

	video_cable = pspDveMgrCheckVideoOut();

	video_doing_fps = M_CheckParm ("-fps");
	video_is_laced = M_CheckParm ("-laced");
	video_is_tv = M_CheckParm ("-tv");
	video_vsync = M_CheckParm ("-vsync");

	lineWidth = (SCREENWIDTH > 480) ? 768 : 512;
	lineBytes = lineWidth * 4;

	if (video_is_tv) {
		int i = 0;
		int j = 0;

		p = M_CheckParm ("-tvcx");
		if (p)
			sscanf(myargv[p+1], "%d", &i);
		p = M_CheckParm ("-tvcy");
		if (p)
			sscanf(myargv[p+1], "%d", &j);

		vramoffset = i*4 + lineBytes * (video_is_laced ? j/2 : j);
	} else
		vramoffset = SCREENWIDTH == 480 ? 0 : SCREENWIDTH == 368 ? (480-368)*2 : (480-320)*2 + (272-240)*lineBytes/2;

	if (video_is_tv)
	{
		// put it in the extra 32MB since we KNOW we're on a slim
		vram1 = (u32 *)0x4A000000;
		vram2 = (u32 *)(0x4A000000 + lineBytes * (video_is_laced ? 503 : 480));
	}
	else
	{
		// put it in EDRAM in case we're on a phat PSP
		vram1 = (u32 *)0x44000000;
		vram2 = (u32 *)(0x44000000 + lineBytes * 272);
	}

	I_RecalcPalettes ();

	video_set_vmode();
}

/**********************************************************************/
void I_ShutdownGraphics (void)
{
}

/**********************************************************************/
// recalculate colourtable[][] after changing usegamma
void I_RecalcPalettes (void)
{
  int p, i;
  byte *palette;
  static int lu_palette;

  lu_palette = W_GetNumForName ("PLAYPAL");
  for (p = 0; p < NUMPALETTES; p++) {
    palette = (byte *) W_CacheLumpNum (lu_palette, PU_STATIC)+p*768;
    for (i=0; i<256; i++) {
        // Better to define c locally here instead of for the whole function:
        u32 r = gammatable[usegamma][palette[i*3]];
        u32 g = gammatable[usegamma][palette[i*3+1]];
        u32 b = gammatable[usegamma][palette[i*3+2]];
        video_colourtable[p][i] = b<<16 | g<<8 | r;
    }
  }
}

/**********************************************************************/
// Takes full 8 bit values.
void I_SetPalette (byte *palette, int palette_index)
{
  video_palette_changed = 1;
  video_palette_index = palette_index;
}

/**********************************************************************/
// Called by anything that renders to screens[0] (except 3D view)
void I_MarkRect (int left, int top, int width, int height)
{
  M_AddToBox (dirtybox, left, top);
  M_AddToBox (dirtybox, left + width - 1, top + height - 1);
}

/**********************************************************************/
void I_StartUpdate (void)
{
}

/**********************************************************************/
void I_UpdateNoBlit (void)
{
}

/**********************************************************************/
void I_FinishUpdate (void)
{
	int top, left, width, height;
	int i, j;
	u32 *base_address;
	static u32 *palette = video_colourtable[0];

	if (total_frames == 0)
	{
		memset((void *)vram1, 0, lineBytes * (!video_is_tv ? 272 : video_is_laced ? 503 : 480));
		memset((void *)vram2, 0, lineBytes * (!video_is_tv ? 272 : video_is_laced ? 503 : 480));
	}
	total_frames++;

    if (video_palette_changed != 0) {
      palette = video_colourtable[video_palette_index];
      video_palette_changed = 0;
    }

#if 0
	/* update only the viewwindow and dirtybox when gamestate == GS_LEVEL */
	if (gamestate == GS_LEVEL) {
    	if (dirtybox[BOXLEFT] < viewwindowx)
    		left = dirtybox[BOXLEFT];
    	else
    		left = viewwindowx;
    	if (dirtybox[BOXRIGHT] + 1 > viewwindowx + scaledviewwidth)
    		width = dirtybox[BOXRIGHT] + 1 - left;
    	else
    		width = viewwindowx + scaledviewwidth - left;
    	if (dirtybox[BOXBOTTOM] < viewwindowy) /* BOXBOTTOM is really the top! */
    		top = dirtybox[BOXBOTTOM];
    	else
    		top = viewwindowy;
    	if (dirtybox[BOXTOP] + 1 > viewwindowy + viewheight)
    		height = dirtybox[BOXTOP] + 1 - top;
    	else
    		height = viewwindowy + viewheight - top;
    	M_ClearBox (dirtybox);
#ifdef RANGECHECK
    	if (left < 0 || left + width > SCREENWIDTH || top < 0 || top + height > SCREENHEIGHT)
    		I_Error ("I_FinishUpdate: Box out of range: %d %d %d %d", left, top, width, height);
#endif
	} else {
    	left = 0;
    	top = 0;
    	width = SCREENWIDTH;
    	height = SCREENHEIGHT;
	}
#else
   	left = 0;
   	top = 0;
   	width = SCREENWIDTH;
   	height = SCREENHEIGHT;
#endif

	base_address = vramflip ? vram1 : vram2;
	base_address = (u32 *)((u32)base_address + vramoffset);

    //start_timer ();
#if 0
	for (j=top; j<height; j++)
		for (i=left; i<width; i++)
		{
			if (video_is_laced)
				if (j & 1)
					base_address[i + (j>>1)*lineWidth] = palette[screens[0][i + j*SCREENWIDTH]];
				else
					base_address[i + (j>>1)*lineWidth + 262*lineWidth] = palette[screens[0][i + j*SCREENWIDTH]];
			else
				base_address[i + j*lineWidth] = palette[screens[0][i + j*SCREENWIDTH]];
		}
#else
	for (j=top; j<height; j++)
		for (i=left; i<width; i+=4)
		{
			u32 fp = *(u32 *)&screens[0][i + j*SCREENWIDTH];
			if (video_is_laced)
			{
				if (j & 1)
				{
					base_address[i + (j>>1)*lineWidth] = palette[fp&0xff];
					base_address[i + 1 + (j>>1)*lineWidth] = palette[(fp>>8)&0xff];
					base_address[i + 2 + (j>>1)*lineWidth] = palette[(fp>>16)&0xff];
					base_address[i + 3 + (j>>1)*lineWidth] = palette[fp>>24];
				}
				else
				{
					base_address[i + (j>>1)*lineWidth + 262*lineWidth] = palette[fp&0xff];
					base_address[i + 1 + (j>>1)*lineWidth + 262*lineWidth] = palette[(fp>>8)&0xff];
					base_address[i + 2 + (j>>1)*lineWidth + 262*lineWidth] = palette[(fp>>16)&0xff];
					base_address[i + 3 + (j>>1)*lineWidth + 262*lineWidth] = palette[fp>>24];
				}
			}
			else
			{
				base_address[i + j*lineWidth] = palette[fp&0xff];
				base_address[i + 1 + j*lineWidth] = palette[(fp>>8)&0xff];
				base_address[i + 2 + j*lineWidth] = palette[(fp>>16)&0xff];
				base_address[i + 3 + j*lineWidth] = palette[fp>>24];
			}
		}
#endif
    //lock_time += end_timer ();

    if (video_doing_fps)
      video_do_fps (base_address, 0);

	if (video_vsync)
		sceDisplayWaitVblankStart();

	sceDisplaySetFrameBuf((void *) vramflip ? vram1 : vram2, lineWidth, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);

	vramflip ^= 1;
}

/**********************************************************************/
// Wait for vertical retrace or pause a bit.  Use when quit game.
void I_WaitVBL(int count)
{
  for ( ; count > 0; count--)
    sceDisplayWaitVblankStart();
}

/**********************************************************************/
void I_ReadScreen (byte* scr)
{
  memcpy (scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

/**********************************************************************/
void I_BeginRead (void)
{
}

/**********************************************************************/
void I_EndRead (void)
{
}

/**********************************************************************/
static void calc_time (u32 time, char *msg)
{
#ifdef PSP //__VBCC__
	printf ("Total %s = %ld us  (%ld us/frame)\n", msg, time, time / total_frames);
#else
  printf ("Total %s = %ldu us  (%ldu us/frame)\n", msg, time, time / total_frames);
#endif
}

/**********************************************************************/
void video_cleanup (void)
{
  I_ShutdownGraphics ();

  if (total_frames > 0) {
    printf ("Total number of frames = %u\n", total_frames);
    //calc_time (wpa8_time, "WritePixelArray8 time ");
    //calc_time (lock_time, "LockBitMap time       ");
    //calc_time (c2p_time, "Chunky2Planar time    ");
#ifdef PROFILE
    {
      int i;

      for (i=0; i<32; i++)
        calc_time (profile[n][2], "Profile Time ");
    }
#endif
  }
}

/**********************************************************************/

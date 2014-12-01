#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <SDL/SDL.h>
#include "../common/callback.h"
#define VERS    1 //Talk about this
#define REVS    0
 
PSP_MODULE_INFO("Hello World2", PSP_MODULE_USER, VERS, REVS);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER); 
PSP_HEAP_SIZE_MAX();

enum {
  TRIANGLE, CIRCLE, CROSS, SQUARE, 
  LTRIGGER, RTRIGGER, 
  DOWN,     LEFT,   UP,    RIGHT, 
  SELECT,   START
};

 
#define printf pspDebugScreenPrintf

int main()
{       

  SDL_Surface *screen = NULL;
  SDL_Rect     r;
  SDL_Event event;  
  SceCtrlData buttonInput;

  int running = isRunning();

  running=1;
  int size_x=128;
  int size_y=128;
  int x=0;
  int y=0;

  int vertical=0;
  int horizontal=0;

  int col=0;

  int up_x=0;
  int up_y=0;
  //int size=0;
  pspDebugScreenInit();
  setupExitCallback();
  //SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK);
  //SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_TIMER);
  //SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_TIMER);
  //SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK);
  screen = SDL_SetVideoMode( 480, 272, 16, SDL_SWSURFACE );
  
  SDL_Joystick *joystick = NULL; 
  SDL_JoystickEventState(SDL_ENABLE); 
  joystick = SDL_JoystickOpen(0); 

  while(running>0)
    {
      col++;
      while(SDL_PollEvent(&event))
	{
	  switch(event.type)
	    {            
	    case SDL_JOYBUTTONDOWN:
	      {
		if (event.jbutton.button == CIRCLE)
		  x=1;
		if (event.jbutton.button == TRIANGLE)
		  x=-1;
		if (event.jbutton.button == CROSS)
		  y=1;
		if (event.jbutton.button == SQUARE)
		  y=-1;

		if (event.jbutton.button == UP)
		  vertical=-1;
		if (event.jbutton.button == DOWN)
		  vertical=1;
		if (event.jbutton.button == LEFT)
		  horizontal=-1;
		if (event.jbutton.button == RIGHT)
		  horizontal=1;

		if (event.jbutton.button == START)
		  running=0;


		break;
	      }

		 
	    case SDL_JOYBUTTONUP:
	      {
		if (event.jbutton.button == CIRCLE)
		  x=0;
		if (event.jbutton.button == TRIANGLE)
		  x=0;
		if (event.jbutton.button == CROSS)
		  y=0;
		if (event.jbutton.button == SQUARE)
		  y=0;

		if (event.jbutton.button == UP)
		  vertical=0;
		if (event.jbutton.button == DOWN)
		  vertical=0;
		if (event.jbutton.button == LEFT)
		  horizontal=0;
		if (event.jbutton.button == RIGHT)
		  horizontal=0;


		break;
	      }
		/*
	      if (event.jbutton.button == 9)
		x = 0;
	      break;
	      */

	      //*/
	    }
	  //x=x++;               
	}

      size_x=size_x+x;
      if (size_x<1) size_x=1;

      size_y=size_y+y;
      if (size_y<1) size_y=1;

      up_y=up_y+vertical;
      up_x=up_x+horizontal;
      //x=x++;               
      r.x = up_x;
      r.y = up_y;
      r.w = size_y;
      r.h = size_x;
      SDL_FillRect(screen, &r, 0xBBBBBB+col);
      SDL_Flip(screen);
      
      //pspDebugScreenSetXY(0, 0);
      //printf("Hello World:%d",x);


      //running++;
      SDL_Delay(10);
      SDL_FillRect(screen,NULL, 0x000000);

    }
  
  SDL_Quit();
  sceKernelExitGame();	
  return 0;
}

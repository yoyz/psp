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
 
#define printf pspDebugScreenPrintf

int main()
{       

  SDL_Surface *screen = NULL;
  SDL_Rect     r;
  SDL_Event event;  
  SceCtrlData buttonInput;

  int running = isRunning();

  running=1;
  int x=127;
  int y=0;
  int col=0;
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
	      if (event.jbutton.button == 1)
		x=x++;
	      break;
	      if (event.jbutton.button == 5)
		x=x++;
	      break;
	      if (event.jbutton.button == 9)
		x=x++;
	      break;

	    case SDL_KEYDOWN:
	      x=x++;               
	      break;
	      /*
	    case SDL_JOYBUTTONUP:
	      if (event.jbutton.button == 1)
		0;
	      if (event.jbutton.button == 5)
		boutonCinq = 0;
	      if (event.jbutton.button == 9)
		boutonNeuf = 0;
	      break;
	      */
	    }
	  x=x++;               
	}
      //x=x++;               
      r.x = 0;
      r.y = 0;
      r.w = 128;
      r.h = x;
      SDL_FillRect(screen, &r, 0xBBBBBB+col);
      SDL_Flip(screen);
      
      //pspDebugScreenSetXY(0, 0);
      //printf("Hello World:%d",x);


      //running++;
      SDL_Delay(50);
      SDL_FillRect(screen,NULL, 0x000000);

    }
  
  SDL_Quit();
  sceKernelExitGame();	
  return 0;
}

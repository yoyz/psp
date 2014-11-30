#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <SDL/SDL.h>
#include "../common/callback.h"
#define VERS    1 //Talk about this
#define REVS    0
 
PSP_MODULE_INFO("Hello World", PSP_MODULE_USER, VERS, REVS);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER); 
PSP_HEAP_SIZE_MAX();
 
#define printf pspDebugScreenPrintf

int main()
{       

  SDL_Surface *screen = NULL;
  SDL_Rect     r;

  SceCtrlData buttonInput;

  int running = isRunning();

  running=0;

  pspDebugScreenInit();
  setupExitCallback();
  SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
  screen = SDL_SetVideoMode( 480, 272, 16, SDL_SWSURFACE );
  
  

  while(running<240)
    {

      r.x = 0;
      r.y = 0;
      r.w = 128;
      r.h = running;
      SDL_FillRect(screen, &r, 0xBBBBBB);
      SDL_Flip(screen);


      running++;
      SDL_Delay(50);
    }
  
  SDL_Quit();
  sceKernelExitGame();	
  return 0;
}

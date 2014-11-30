TARGET		= hello_sdl
OBJS		= main.o ../common/callback.o

INCDIR		=
CFLAGS		= -G0 -Wall -O2  -I${PSPDEV}/psp/include/ -L${PSPDEV}/psp/lib
CXXFLAGS	= $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS	= $(CFLAGS)

LIBDIR		=
LDFLAGS	=
#LIBS		= -lm -lSDL -lSDLmain
#LIBS		= -lm -lSDL -lSDLmain -lGL -lGLU -lglut -lpspvfpu -lm -lpspdebug -lpspgu -lpspctrl -lpspge -lpspdisplay -lpsphprm -lpspsdk -lpsprtc -lpspaudio -lc -lpspuser -lpsputility -lpspkernel -lpspnet_inet -ljpeg -lpspirkeyb -lpsppower

#LIBS		= -lm  -lc -lSDL_image -lpng -lz -lstdc++ -lSDL -lSDLmain -lGL -lGLU -lglut -lpspvfpu -lm -lpspdebug -lpspgu -lpspctrl -lpspge -lpspdisplay -lpsphprm -lpspsdk -lpsprtc -lpspaudio -lc -lpspuser -lpsputility -lpspkernel -lpspnet_inet -ljpeg -lpspirkeyb -lpsppower

#LIBS            = -lSDL -lSDLmain -lGL -lGLU -lglut -lpspvfpu -lm -lpspdebug -lpspgu -lpspctrl -lpspge -lpspdisplay -lpsphprm -lpspsdk -lpsprtc -lpspaudio -lc -lpspuser -lpsputility -lpspkernel -lpspnet_inet -ljpeg -lpspirkeyb -lpsppower  -lm  -lc -lSDL_image -lpng -lz -lstdc++  

LIBS             = -lGLU -lGL  -ljpeg -lstdc++ -lm -lc -lpspvfpu  
LIBS            += ${shell ${PSPDEV}/psp/bin/sdl-config --libs }

BUILD_PRX = 1 

EXTRA_TARGETS	= EBOOT.PBP
PSP_EBOOT_TITLE= hello_sdl

PSPSDK	= $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

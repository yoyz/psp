TARGET		= hello_world
EXTRA_TARGETS	= EBOOT.PBP
OBJS		= psp_callback.o main.o
PSP_EBOOT_TITLE = Hello_World



BUILD_PRX=1

PSP_FW_VERSION = 500


INCDIR		=
CFLAGS		= -G0 -Wall -O2
CXXFLAGS	= $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS	= $(CFLAGS)

LIBDIR		=
LDFLAGS	=
LIBS		= -lGLU -lGL  -ljpeg -lstdc++ -lm -lc -lpspvfpu
LIBS            += ${shell ${PSPDEV}/psp/bin/sdl-config --libs } -lSDL_ttf -lfreetype -lm

#BUILD_PRX = 1 

PSPSDK	= $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

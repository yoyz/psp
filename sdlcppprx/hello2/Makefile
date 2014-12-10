TARGET = mymodule
OBJS = main.o  ../common/callback.o

# Define to build this as a prx (instead of a static elf)
BUILD_PRX=1
# Define the name of our custom exports (minus the .exp extension)
#PRX_EXPORTS=exports.exp

#USE_PSPSDK_LIBC = 1

INCDIR = 
CFLAGS = -O2 -G0 -Wall -I${PSPDEV}/psp/include/ -L${PSPDEV}/psp/lib

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS   =  -lGLU -lGL  -ljpeg -lstdc++ -lm -lc -lpspvfpu -lSDL -lSDLmain  ${shell ${PSPDEV}/psp/bin/sdl-config --libs } 

LIBDIR =  

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

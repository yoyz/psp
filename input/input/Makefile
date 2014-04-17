TARGET		= ButtonInput
OBJS		= main.o ../common/callback.o
 
INCDIR		=
CFLAGS		= -O2 -G0 -Wall
CXXFLAGS	= $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS	= $(CFLAGS)
 
BUILD_PRX	= 1
 
LIBDIR		= ./
LIBS		= -lm
LDFLAGS	=
 
EXTRA_TARGETS		= EBOOT.PBP
PSP_EBOOT_TITLE	= ButtonInput
 
PSPSDK	= $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

TARGET = Doom
OBJS1= \
	doomdef.o \
	doomstat.o \
	dstrings.o \
	psp_system.o \
	psp_sound.o \
	psp_video.o \
	psp_net.o \
	dehacked.o \
	tables.o \
	f_finale.o \
	f_wipe.o \
	d_main.o \
	d_net.o \
	d_items.o \
	g_game.o

OBJS2= \
	m_menu.o \
	m_misc.o \
	m_argv.o \
	m_bbox.o \
	m_cheat.o \
	m_random.o \
	am_map.o \
	p_ceilng.o \
	p_doors.o \
	p_enemy.o \
	p_floor.o \
	p_inter.o \
	p_lights.o \
	p_map.o \
	p_maputl.o \
	p_plats.o \
	p_pspr.o \
	p_setup.o \
	p_sight.o \
	p_spec.o \
	p_switch.o \
	p_mobj.o \
	p_telept.o \
	p_tick.o \
	p_saveg.o \
	p_user.o

OBJS3 = \
	r_bsp.o \
	r_data.o \
	r_draw.o \
	r_main.o \
	r_plane.o \
	r_segs.o \
	r_sky.o \
	r_things.o \
	w_wad.o \
	wi_stuff.o \
	v_video.o \
	st_lib.o \
	st_stuff.o \
	hu_stuff.o \
	hu_lib.o \
	s_sound.o \
	z_zone.o \
	info.o \
	sounds.o \
	psp_main.o \
	pspDveManager.o \
	reqfile.o \
	intraFont.o \
	m_fixed.o \
	m_swap.o

OBJS = $(OBJS1) $(OBJS2) $(OBJS3)

INCDIR = ./include
CFLAGS = -O2 -G0 -Wall -fno-exceptions -ffast-math -DNORMALUNIX -DUNROLL -Diabs=abs -fsigned-char -DPSP
#CFLAGS += -DUSE_FLOAT_FIXED
CXXFLAGS = $(CFLAGS) -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PSP_FW_VERSION = 371

LIBDIR = ./lib
LIBS = -lpspwlan -lpspgum -lpspgu -lpsputility -lpspaudiolib -lpspaudio -lpsppower -lm -lpspkubridge
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = DOOM
PSP_EBOOT_ICON="icon0.png"
PSP_EBOOT_UNKPNG="pic0.png"
PSP_EBOOT_PIC1="pic1.png"
#PSP_EBOOT_SND0="snd0.at3"

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

#
# usim CADR simulator
# $Id$
#

#---------- figure out what we are runnign on ---------

OS_NAME = $(shell uname)
MACH_NAME = $(shell uname -m)

ifeq ($(OS_NAME), Darwin)
OS = OSX
endif

ifeq ($(OS_NAME), Linux)
OS = LINUX
endif

ifeq ($(OS_NAME), NetBSD)
OS = NETBSD
endif

#--------- options ---------

DISPLAY = SDL
#DISPLAY = X11

#KEYBOARD = OLD
KEYBOARD = NEW

ifeq ($(OS_NAME), Darwin)
DISPLAY = X11
KEYBOARD = OLD
endif

ifeq ($(OS_NAME), Linux)
DISPLAY = X11
KEYBOARD = OLD
endif

#----------- code ------------

USIM_SRC = main.c decode.c ucode.c disk.c iob.c chaos.c ether.c uart.c syms.c config.c 
USIM_HDR = ucode.h config.h

ifeq ($(DISPLAY), SDL)
DISPLAY_SRC = sdl.c
USIM_LIBS = -lSDL -lpthread
DEFINES = -DDISPLAY_SDL
endif

ifeq ($(DISPLAY), X11)
DISPLAY_SRC = x11.c
USIM_LIBS = -L/usr/X11R6/lib -lX11 -lpthread
DEFINES = -DDISPLAY_X11
endif

ifeq ($(KEYBOARD), OLD)
KEYBOARD_SRC = kbd_old.c
endif

ifeq ($(KEYBOARD), NEW)
KEYBOARD_SRC = kbd_new.c
endif

# Mac OSX
ifeq ($(OS), OSX)
LFLAGS = -m32 -framework Cocoa
USIM_SRC += Files.c glob.c
USIM_HDR += Files.h glob.h
USIM_LIBS = -lSDLmain -lSDL -lpthread -lobjc
CFLAGS = -O -m32 -I/usr/X11/include -DDISPLAY_X11 -DMAP_SITE_TREE_DIRECTORY $(DEFINES)
# good for G5
#CFLAGS = -fast $(DEFINES)
endif

ifeq ($(DISPLAY), X11)
LFLAGS = -m32
ifeq ($(OS), LINUX)
USIM_LIBS = -L/usr/lib/x86_64-linux-gnu -lX11 -lpthread
else
USIM_LIBS = -L/usr/X11R6/lib -lX11 -lpthread
endif
endif

# Linux
ifeq ($(OS), LINUX)
#CFLAGS = -g
#CFLAGS = -O -pg -g -fprofile-arcs
#CFLAGS= -O3 -march=pentium3 -mfpmath=sse -mmmx -msse $(DEFINES) -Walle
#CFLAGS = -O3 -fomit-frame-pointer -mcpu=i686 -g $(DEFINES)
#CFLAGS= -O3 -mfpmath=sse -mmmx -msse $(DEFINES) -Walle
CFLAGS = -mfpmath=sse -mmmx -msse -DMAP_SITE_TREE_DIRECTORY $(DEFINES) -g
LFLAGS = -ldl -L/usr/lib
USIM_SRC += Files.c glob.c
USIM_HDR += Files.h glob.h
USIM_LIBS += -lrt
endif

# NetBSD
ifeq ($(OS), NETBSD)
CFLAGS = -O2 -g $(DEFINES) -I/usr/pkg/include
LFLAGS = -R/usr/pkg/lib -L/usr/pkg/lib
USIM_SRC += Files.c glob.c
USIM_HDR += Files.h glob.h
endif

# override above if 64 bit
ifeq ($(MACH_NAME), x86_64)
M32 = -m32

ifeq ($(DISPLAY), SDL)
USIM_LIBS = /usr/lib/libSDL-1.2.so.0.7.0 -lpthread
endif

endif

#DEFINES=-DLASHUP
DEFINES += -DCADR2

USIM_OBJ = $(USIM_SRC:.c=.o) $(DISPLAY_SRC:.c=.o) $(KEYBOARD_SRC:.c=.o)

SRC = $(USIM_SRC) $(DISPLAY_SRC) $(KEYBOARD_SRC)

all: usim readmcr diskmaker lod lmfs disk.img

usim: $(USIM_OBJ)
	$(CC) -o usim $(LFLAGS) $(USIM_OBJ) $(USIM_LIBS)

run:
	./usim >xx

readmcr: readmcr.c
	$(CC) $(CFLAGS) -o $@ $<

diskmaker: diskmaker.c
	$(CC) $(CFLAGS) -o $@ $<

lmfs: lmfs.c
	$(CC) $(CFLAGS) -o $@ $<

lod: lod.c macro.c
	$(CC) $(CFLAGS) -o $@ $<

disk.img: diskmaker
	./diskmaker -c -f disk.img -t template.disk9

clean:
	rm -f *.o usim lod readmcr diskmaker lmfs xx
	rm -f *~



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

#--------- options ---------

DISPLAY = SDL
#DISPLAY = X11

#KEYBOARD = OLD
KEYBOARD = NEW

#----------- code ------------

USIM_SRC = main.c decode.c ucode.c disk.c iob.c chaos.c syms.c config.c
USIM_HDR = ucode.h config.h

ifeq ($(DISPLAY), SDL)
DISPLAY_SRC = sdl.c
USIM_LIBS = -lSDL -lpthread
DEFINES = -DDISPLAY_SDL
endif

ifeq ($(DISPLAY), X11)
DISPLAY_SRC = x11.c
USIM_LIBS = -L/usr/X11R6/lib -lX11
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
LFLAGS = -framework Cocoa
USIM_LIBS = -lSDLmain -lSDL -lpthread -lobjc
CFLAGS = -O $(DEFINES)
endif

ifeq ($(DISPLAY), X11)
LFLAGS =
USIM_LIBS = -L/usr/X11R6/lib -lX11
endif

# Linux
ifeq ($(OS), LINUX)
#CFLAGS = -g
#CFLAGS = -O -pg -g -fprofile-arcs
#CFLAGS= -O3 -march=pentium3 -mfpmath=sse -mmmx -msse $(DEFINES) -Walle
#CFLAGS = -O3 -fomit-frame-pointer -mcpu=i686 -g $(DEFINES)
#CFLAGS= -O3 -mfpmath=sse -mmmx -msse $(DEFINES) -Walle
CFLAGS = -O3 -mfpmath=sse -mmmx -msse $(DEFINES) -Walle $(M32) -g
LFLAGS = $(M32) -L/usr/lib
endif

# override above if 64 bit
ifeq ($(MACH_NAME), x86_64)
M32 = -m32
USIM_LIBS = /usr/lib/libSDL-1.2.so.0.7.0 -lpthread
endif

#DEFINES=-DLASHUP

USIM_OBJ = $(USIM_SRC:.c=.o) $(DISPLAY_SRC:.c=.o) $(KEYBOARD_SRC:.c=.o)

SRC = $(USIM_SRC) $(DISPLAY_SRC) $(KEYBOARD_SRC)

all: usim readmcr diskmaker lod lmfs

usim: $(USIM_OBJ)
	echo mmm $(MACH_NAME)
	$(CC) -o usim $(LFLAGS) $(USIM_OBJ) $(USIM_LIBS)

#usim: $(SRC) $(USIM_HDR)
#	$(CC) -o usim $(CFLAGS) $(SRC) $(USIM_LIBS)

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

clean:
	rm -f *.o usim lod readmcr diskmaker xx
	rm -f *~



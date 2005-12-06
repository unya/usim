#
# usim CADR simulator
# $Id$
#

OS = LINUX
#OS = OSX

#DISPLAY = SDL
DISPLAY = X11

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

# Mac OSX
ifeq ($(OS), OSX)
USIM_LIBS = -framework SDL -lpthread
#USIM_LIBS = -lSDLmain -lSDL -lpthread -lobjc
CFLAGS = -O3 -fomit-frame-pointer -framework Cocoa $(DEFINES)
endif

# Linux
ifeq ($(OS), LINUX)
#CFLAGS = -g
#CFLAGS = -O -pg -g -fprofile-arcs
CFLAGS = -O3 -fomit-frame-pointer -mcpu=i686 -g $(DEFINES)
endif


#DEFINES=-DLASHUP

USIM_OBJ = $(USIM_SRC:.o=.c) $(DISPLAY_SRC:.o=.c)

all: usim readmcr diskmaker lod

usim: $(USIM_SRC) $(USIM_HDR)
	$(CC) -o usim $(CFLAGS) $(USIM_SRC) $(DISPLAY_SRC) $(USIM_LIBS)
#	./usim >xx

run:
	./usim >xx

readmcr: readmcr.c
	$(CC) $(CFLAGS) -o $@ $<

diskmaker: diskmaker.c
	$(CC) $(CFLAGS) -o $@ $<

lod: lod.c macro.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f *.o usim lod readmcr diskmaker xx
	rm -f *~



#
# usim CADR simulator
# $Id$
#

DISPLAY = SDL
KEYBOARD = NEW

USIM_SRC = usim.c decode.c ucode.c x11.c kbd.c disk.c iob.c chaos.c syms.c config.c 
USIM_HDR = ucode.h config.h

ifeq ($(DISPLAY), SDL)
DISPLAY_SRC = sdl.c
DISPLAY_LIBS = $(shell sdl-config --libs)
DISPLAY_DEFINES =  $(shell sdl-config --cflags) -DDISPLAY_SDL
endif

ifeq ($(DISPLAY), X11)
DISPLAY_SRC = x11.c
DISPLAY_LIBS = -lX11 -lpthread
DISPLAY_DEFINES = -DDISPLAY_X11
endif

ifeq ($(KEYBOARD), OLD)
KEYBOARD_SRC = kbd_old.c
endif

ifeq ($(KEYBOARD), NEW)
KEYBOARD_SRC = kbd_new.c
endif

# Linux / BSD / OS X
LFLAGS = -L/usr/lib -lpthread
OPTFLAGS = -mtune=native -O2
DBGFLAGS = -g3
#DBGFLAGS += -DKBD_DEBUG=1
#DBGFLAGS += -DCHAOS_DEBUG=1
CFLAGS = -std=c99 -D_XOPEN_SOURCE=600
CFLAGS += $(OPTFLAGS) $(DBGFLAGS) $(DEFINES)

# built-in CHAOS FILE: server at 0404
DEFINES += -DMAP_SITE_TREE_DIRECTORY
USIM_SRC += Files.c glob.c
USIM_HDR += Files.h glob.h

USIM_OBJ = $(USIM_SRC:.c=.o) $(DISPLAY_SRC:.c=.o) $(KEYBOARD_SRC:.c=.o)

all: usim readmcr diskmaker lod lmfs disk.img

usim: $(USIM_OBJ)
	$(CC) $(CFLAGS) -o $@ $(USIM_OBJ) $(LFLAGS) $(DISPLAY_LIBS)

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

disk.img:
	./diskmaker -c -f disk.img -t disk.cfg

clean:
	rm -f *.o usim lod readmcr diskmaker lmfs xx
	rm -f *~

x11.c: DEFINES+=$(DISPLAY_DEFINES)
sdl.o: DEFINES+=$(DISPLAY_DEFINES)
iob.o: DEFINES+=$(DISPLAY_DEFINES)
kbd_new.o: DEFINES+=$(DISPLAY_DEFINES)
kbd_old.o: DEFINES+=$(DISPLAY_DEFINES)
main.o: DEFINES+=$(DISPLAY_DEFINES)

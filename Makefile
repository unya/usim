#
# usim CADR simulator
# $Id$
#

USIM_SRC = main.c decode.c ucode.c disk.c iob.c syms.c config.c sdl.c
USIM_HDR = ucode.h config.h

USIM_LIBS = -lSDL -lpthread

#CFLAGS = -O -pg -g -fprofile-arcs
#CFLAGS = -g
CFLAGS = -O3 -fomit-frame-pointer -mcpu=i686

all: usim readmcr diskmaker

usim: $(USIM_SRC) $(USIM_HDR)
	cc -o usim $(CFLAGS) $(USIM_SRC) $(USIM_LIBS)
#	./usim >xx

run:
	./usim >xx

readmcr: readmcr.c
	cc -o $@ $<

diskmaker: diskmaker.c
	cc -o $@ $<

clean:
	rm -f *.o usim readmcr diskmaker disk.img xx
	rm -f *~



#
# usim CADR simulator
# $Id$
#

USIM_SRC = main.c decode.c ucode.c disk.c iob.c syms.c sdl.c
USIM_LIBS = -lSDL -lpthread

all: usim readmcr diskmaker

usim: $(USIM_SRC)
	cc -o usim -g $(USIM_SRC) $(USIM_LIBS)
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



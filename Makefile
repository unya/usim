#
# usim CADR simulator
# $Id$
#

USIM_SRC = main.c decode.c ucode.c disk.c syms.c

all: usim readmcr diskmaker

usim: $(USIM_SRC)
	cc -o usim -g $(USIM_SRC)
	./usim >xx

readmcr: readmcr.c
	cc -o $@ $<

diskmaker: diskmaker.c
	cc -o $@ $<

clean:
	rm -f *.o usim readmcr diskmaker disk.img xx
	rm -f *~



#
# usim CADR simulator
# $Id$
#

SRC = main.c decode.c ucode.c disk.c

all: usim readmcr diskmaker

usim: $(SRC)
	cc -o usim -g $(SRC)
	./usim >xx

readmcr: readmcr.c
	cc -o $@ $<

diskmaker: diskmaker.c
	cc -o $@ $<

clean:
	rm -f *.o usim readmcr diskmaker disk.img xx
	rm -f *~



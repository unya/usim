#
# usim CADR simulator
# $Id$
#

SRC = main.c decode.c ucode.c

all: usim readmcr

usim: $(SRC)
	cc -o usim -g $(SRC)
	./usim >xx

readmcr: readmcr.c
	cc -o readmcr readmcr.c

clean:
	rm -f *.o usim readmcr



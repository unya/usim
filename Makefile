VERSION = 0.9-ams

CC = gcc -std=gnu99
CC += -Wall -Wextra -fsanitize=undefined

CFLAGS = -g3 -O3

all: TAGS usim readmcr diskmaker lod lmfs cc

usim: usim.o ucode.o mem.o iob.o mouse.o kbd.o tv.o x11.o chaos.o Files.o disk.o lashup.o uart.o decode.o syms.o glob.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lX11

readmcr: readmcr.o
	$(CC) $(CFLAGS) -o $@ $^

diskmaker: diskmaker.o
	$(CC) $(CFLAGS) -o $@ $^

lmfs: lmfs.o
	$(CC) $(CFLAGS) -o $@ $^

lod: lod.o
	$(CC) $(CFLAGS) -o $@ $^

cc: cc.o decode.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o
	rm -f *~
	rm -f xx
	rm -f usim lod readmcr diskmaker lmfs cc

TAGS:
	find . -type f -iname "*.[ch]" | etags -

.PHONY: format-source
format-source:
	find -name "*.[ch]" -exec ./format-source {} \;

dist:
	rm -rf usim-$(VERSION)
	svn export . usim-$(VERSION)
	tar zcf usim-$(VERSION).tar.gz usim-$(VERSION)


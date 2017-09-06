VERSION = 0.9-ams

CC = gcc
CFLAGS = -g3 -O3 -std=gnu99

USIM_LDFLAGS = -lpthread -lX11

all: TAGS usim readmcr diskmaker lod lmfs cc

usim: usim.o decode.o ucode.o x11.o kbd.o disk.o iob.o chaos.o uart.o syms.o config.o Files.o glob.o
	$(CC) $(CFLAGS) -o $@ $^ $(USIM_LDFLAGS)

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

disk.img: diskmaker
	./diskmaker -c

clean:
	rm -f *.o
	rm -f *~
	rm -f xx
	rm -f usim lod readmcr diskmaker lmfs cc

TAGS:
	find . -type f -iname "*.[ch]" | etags -

dist:
	rm -rf usim-$(VERSION)
	svn export . usim-$(VERSION)
	tar zcf usim-$(VERSION).tar.gz usim-$(VERSION)


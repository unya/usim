/*
 * read cadr .mcr file on a little endian linux box
 *
 * just a simple hack to see if we can decode the binary file
 * before we try to turn it into a microcode band.
 *
 * $Id$
 */
#include <stdio.h>
#include <fcntl.h>

#include "ucode.h"

int debug;
int needswap;
ucw_t ucode[16*1024];

unsigned int
read16(int fd)
{
	unsigned char b[2];
	int ret;

	ret = read(fd, b, 2);
	if (ret < 2) {
		printf("eof!\n");
		exit(1);
	}

	if (needswap)
		return (b[1] << 8) | b[0];

	return (b[0] << 8) | b[1];
}

unsigned int
read32(int fd)
{
	unsigned char b[4];
	int ret;

	ret = read(fd, b, 4);
	if (ret < 4) {
		printf("eof!\n");
		exit(1);
	}

	if (needswap)
		return (b[1] << 24) | (b[0] << 16) | (b[3] << 8) | b[2];

	return (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
//	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

int
read_i_mem(int fd, int start, int size)
{
	int i, loc;

	loc = start;
	for (i = 0; i < size; i++) {
		unsigned int w1, w2, w3, w4;
		unsigned long long ll;

		w1 = read16(fd);
		w2 = read16(fd);
		w3 = read16(fd);
		w4 = read16(fd);

		ll =
			((unsigned long long)w1 << 48) |
			((unsigned long long)w2 << 32) |
			((unsigned long long)w3 << 16) |
			((unsigned long long)w4 << 0);

		if (0) {
			printf("%03o %016Lo\n",
			       loc, ll);
		}

		ucode[loc] = ll;
		loc++;
	}
}

int
read_d_mem(int fd, int start, int size)
{
	int i;
	unsigned int v1, v2;

	for (i = 0; i < size; i++) {
		v1 = read16(fd);
		v2 = read16(fd);
	}
}

int
read_a_mem(int fd, int start, int size)
{
	int i;
	unsigned int v;

	printf("a-memory; start %o, size %o\n", start, size);
	for (i = 0; i < size; i++) {
		v = read32(fd);
		if ((i >= 0600 && i <= 0610) |
		    (i < 010))
		{
			printf("%o <- %o\n", i, v);
		}
	}
}

int
read_main_mem(int fd, int start, int size)
{
	int i, j;
	unsigned int v1, v2;
	off_t o;
	unsigned int b[256];

	v1 = read32(fd);
	printf("start %d, size %d\n", start, size);

	o = lseek(fd, 0, SEEK_CUR);
//	printf("offset %d\n", o);

#if 0
	o = (o + 256*4-1) & ~01777;
	lseek(fd, o, SEEK_SET);

	for (i = 0; i < size; i++) {
		read(fd, b, 256*4);
	}
#endif

#if 0
	for (i = 0; i < start; i++) {
		for (j = 0; j < 256; j++) {
			read32(fd);
		}
	}
#endif
}

void
usage(void)
{
	fprintf(stderr, "readmcr <mcr-filename>\n");
	exit(1);
}

extern char *optarg;
extern int optind;

main(int argc, char *argv[])
{
	int c, fd, done, skip;

	needswap = 1;
	skip = 0;

	while ((c = getopt(argc, argv, "bds:")) != -1) {
		switch (c) {
		case 'b':
			needswap = 0;
			break;
		case 'd':
			debug++;
			break;
		case 's':
			skip = atoi(optarg);
			break;
		}
	}

	if (optind >= argc)
		usage();

	//"ucadr.mcr.979"

	fd = open(argv[optind], O_RDONLY);
	if (fd) {
		int code, start, size;
		int i, loc;

		if (skip) {
			while (skip--)
				read32(fd);
		}

		done = 0;
		while (!done) {
			code = read32(fd);
			start = read32(fd);
			size = read32(fd);

			printf("code %d, start %o, size %o\n",
			       code, start, size);

			switch (code) {
			case 1:
				printf("i-memory\n");
				read_i_mem(fd, start, size);
				break;
			case 2:
				printf("d-memory\n");
				read_d_mem(fd, start, size);
				break;
			case 3:
				printf("main-memory\n");
				read_main_mem(fd, start, size);
				break;
			case 4:
				printf("a-memory\n");
				read_a_mem(fd, start, size);
				done = 1;
				break;
			}
		}
	}
}


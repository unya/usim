/*
 * read cadr .mcr file on a little endian linux box
 */
#include <stdio.h>
#include <fcntl.h>

#include "ucode.h"

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
//	return (b[0] << 8) | b[1];
	return (b[1] << 8) | b[0];
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
//	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
	return (b[1] << 24) | (b[0] << 16) | (b[3] << 8) | b[2];
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

	for (i = 0; i < size; i++) {
		v = read32(fd);
	}
}

int
read_main_mem(int fd, int start, int size)
{
	int i, j;
	unsigned int v1, v2;

	v1 = read32(fd);
	printf("start %d\n", start);
#if 0
	for (i = 0; i < start; i++) {
		for (j = 0; j < 256; j++) {
			read32(fd);
		}
	}
#endif
}


main()
{
	int fd;

	fd = open("ucadr.mcr.979", O_RDONLY);
	if (fd) {
		int code, start, size;
		int i, loc;

		while (1) {
			code = read32(fd);
			start = read32(fd);
			size = read32(fd);

			printf("code %d, start %o, size %o\n",
			       code, start, size);

			switch (code) {
			case 1:
				read_i_mem(fd, start, size);
				break;
			case 2:
				read_d_mem(fd, start, size);
				break;
			case 3:
				read_main_mem(fd, start, size);
				break;
			case 4:
				read_a_mem(fd, start, size);
				break;
			}
		}
	}
}

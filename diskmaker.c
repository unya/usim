/*
 * this is a total hack.
 * make a disk image for the CADR simulator.
 *
 * Note: this is designed on run on little-endian; the NEED_SWAP might well
 * be a crock.
 *
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

char *img_filename;
char *mcr_filename;
char *lod1_filename;
char *lod2_filename;
unsigned int buffer[256];

int cyls, heads, blocks_per_track;
int use_lod2;

void
swapbytes(unsigned int *buf)
{
	int i;
#if 0
	unsigned char *p = (unsigned char *)buf;

	for (i = 0; i < 256*4; i += 2) {
		unsigned char t;
		t = p[i];
		p[i] = p[i+1];
		p[i+1] = t;
	}
#endif
#if 0
	int i;

	for (i = 0; i < 256; i++) {
		buf[i] = htonl(buf[i]);
	}
#endif
#if 1
	unsigned short *p = (unsigned short *)buf;

	for (i = 0; i < 256*2; i += 2) {
		unsigned short t;
		t = p[i];
		p[i] = p[i+1];
		p[i+1] = t;
	}
#endif
}

struct {
	char *name;
	int start;
	int size;
} parts[] = {
	{ "MCR1", 021,     0224 },
	{ "MCR2", 0245,    0224 },
#if 0
	{ "PAGE", 0524,    020464 },
	{ "LOD1", 021210,  020464 },
	{ "LOD2", 041674,  020464 },
	{ "LOD3", 062360,  020464 },
	{ "LOD4", 0103044, 020464 },
	{ "LOD5", 0123530, 020464 },
	{ "LOD6", 0144214, 020464 },
	{ "LOD7", 0164700, 020464 },
#endif
#if 0
	{ "PAGE", 0524,    045600 },
	{ "LOD1", 046324,  045600 },
	{ "LOD2", 0114124,  045600 },
#endif
#if 0
	{ "PAGE",    0524,  061400 },
	{ "LOD1",  062124,  061400 },
	{ "LOD2", 0143524,  061400 },
#endif
#if 1
	{ "PAGE",    0524,  0100000 },
	{ "LOD1", 0100524,  061400 },
	{ "LOD2", 0162124,  061400 },
//	{ "FILE", 0243524,  070000 },
#endif
	{ (char *)0, 0, 0 }
};

unsigned long
str4(char *s)
{
#ifdef NEED_SWAP
	return (s[0]<<24) | (s[1]<<16) | (s[2]<<8) | s[3];
#else
	return (s[3]<<24) | (s[2]<<16) | (s[1]<<8) | s[0];
#endif
}

int
part_offset(char *name)
{
	int i;
	for (i = 0; parts[i].name; i++) {
		if (strcmp(name, parts[i].name) == 0) {
			return parts[i].start;
		}
	}

	return -1;
}

int
make_labl(int fd)
{
	printf("making LABL...\n");

	memset((char *)buffer, 0, sizeof(buffer));

	/*
	 * try to look like a Trident T-300
	 */
	cyls = 815;
	heads = 19;
	blocks_per_track = 17;

	buffer[0] = str4("LABL");	/* label LABL */
	buffer[1] = 1;			/* version = 1 */
	buffer[2] = cyls;		/* # cyls */
	buffer[3] = heads;		/* # heads */
	buffer[4] = blocks_per_track;	/* # blocks */
	buffer[5] = heads*blocks_per_track; /* heads*blocks */
	buffer[6] = str4("MCR1");	/* name of micr part */
	buffer[7] = str4("LOD1");	/* name of load part */

	if (use_lod2) {
		buffer[7] = str4("LOD2");	/* name of load part */
	}

	{
		int i, count;
		int p = 0200;
		
		count = 0;
		for (i = 0; parts[i].name; i++)
			count++;

		printf("%d partitions\n", i);

		buffer[p++] = count; /* # of partitions */
		buffer[p++] = 7; /* words / partition */

		for (i = 0; i < count; i++) {
			unsigned long n;
			char *pn = parts[i].name;

			printf("%s, start %o, size %o\n",
			       pn, parts[i].start, parts[i].size);

			n = str4(pn);

			buffer[p++] = n;
			buffer[p++] = parts[i].start;
			buffer[p++] = parts[i].size;
			buffer[p++] = str4("    ");
			buffer[p++] = str4("    ");
			buffer[p++] = str4("    ");
			buffer[p++] = str4("    ");

		}
	}

	/* pack brand text - offset 010, 32 bytes */
	memset((char *)&buffer[010], '\200', 32);

	/* pack text label - offset 020, 32 bytes */
	memset((char *)&buffer[020], ' ', 32);
	memcpy((char *)&buffer[020], "CADR diskmaker image", 21);

	/* comment - offset 030, 32 bytes */
	memset((char *)&buffer[030], '\200', 32);

	strcpy((char *)&buffer[030], mcr_filename);
	printf("comment: '%s'\n", mcr_filename);

#ifdef NEED_SWAP
	swapbytes(buffer);
#endif

	write(fd, buffer, 256*4);
}

int
write_block(int fd, int block_no, unsigned char *buf)
{
	off_t offset, ret;
	int size;

	offset = block_no * (256*4);

	ret = lseek(fd, offset, SEEK_SET);
	if (ret != offset) {
		perror("lseek");
		return -1;
	}

	size = 256*4;
	ret = write(fd, buf, size);
	if (ret != size) {
		printf("disk write error; ret %d, size %d\n", ret, size);
		perror("write");
		return -1;
	}

	return 0;
}

int
make_mcr1(int fd)
{
	int ret, count, i, fd1, offset;
	unsigned char b[256*4];

	printf("making MCR1...\n");

	fd1 = open(mcr_filename, O_RDONLY);

	count = 0;
	offset = part_offset("MCR1");
	printf("offset %o\n", offset);

	while (1) {
		ret = read(fd1, b, 256*4);
		if (ret <= 0)
			break;

#ifndef NEED_SWAP
		swapbytes((unsigned int *)b);
#endif
		/* MCR1 start XXX */

		write_block(fd, offset/*021*/+count, b);

		count++;

		if (ret < 256*4)
			break;

	}

	printf("%d blocks\n", count);
	return 0;
}

int
make_lod1(int fd)
{
	int ret, count, i, fd1, offset;
	unsigned char b[256*4];

	printf("making LOD1...\n");

	fd1 = open(lod1_filename, O_RDONLY);

	count = 0;
	offset = part_offset("LOD1");
	printf("offset %o\n", offset);

	while (1) {
		ret = read(fd1, b, 256*4);
		if (ret <= 0)
			break;

		/* LOD1 start XXX */
		write_block(fd, offset+/*021210*/+count, b);

		count++;

		if (ret < 256*4)
			break;
	}

	printf("%d blocks\n", count);
	return 0;
}

int
make_lod2(int fd)
{
	int ret, count, i, fd1, offset;
	unsigned char b[256*4];

	printf("making LOD2...\n");

	fd1 = open(lod2_filename, O_RDONLY);

	count = 0;
	offset = part_offset("LOD2");
	printf("offset %o\n", offset);

	while (1) {
		ret = read(fd1, b, 256*4);
		if (ret <= 0)
			break;

		/* LOD2 start XXX */
		write_block(fd, offset/*041674*/+count, b);

		count++;

		if (ret < 256*4)
			break;
	}

	printf("%d blocks\n", count);
	return 0;
}

int
make_file(int fd)
{
	int ret, count, i, fd1, offset;
	unsigned char b[256*4];

	count = 0;
	offset = part_offset("FILE");

	if (offset == 0)
		return -1;

	printf("making FILE...\n");
	printf("offset %o\n", offset);

	while (1) {
		memset(b, 0, sizeof(b));

		write_block(fd, offset+count, b);
		count++;

		if (ret < 256*4)
			break;
	}

	printf("%d blocks\n", count);
	return 0;
}


main(int argc, char *argv[])
{
	int fd;

	use_lod2 = 0;

	img_filename = strdup("disk.img");
	mcr_filename = strdup("ucadr.mcr.841");
	lod1_filename = strdup("partition-78.48.lod1");
	lod2_filename = strdup("partition-sys210.lod2");

#if 0
	mcr_filename = strdup("ucadr.mcr.979");
	lod1_filename = strdup("partition-sys210.lod2");
#endif

#if 0
	mcr_filename = strdup("ucadr.mcr.896");
	lod1_filename = strdup("partition-sys210.lod2");
#endif

	fd = open(img_filename, O_RDWR | O_CREAT, 0666);
	if (fd < 0) {
		perror(img_filename);
		exit(1);
	}

	make_labl(fd);
	make_mcr1(fd);
	make_lod1(fd);
	if (use_lod2) {
		make_lod2(fd);
	}
	make_file(fd);

	exit(0);
}

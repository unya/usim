/*
 * this is a total hack.
 * make a disk image for the CADR simulator.
 *
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

char *filename;
unsigned int buffer[256];

int cyls, heads, blocks_per_track;

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

	buffer[0] = 011420440514; /* label LABL */
	buffer[1] = 1; /* version = 1 */
	buffer[2] = cyls; /* # cyls */
	buffer[3] = heads; /* # heads */
	buffer[4] = blocks_per_track; /* # blocks */
	buffer[5] = heads*blocks_per_track; /* heads*blocks */
	buffer[6] = 'MCR1'; /* name of micr part */
	buffer[7] = 'LOD1'; /* name of micr part */

	buffer[0200] = 3; /* # of partitions */
	buffer[0201] = 4; /* words / partition */

	buffer[0202] = 'MCR1'; /* start of partition info */
	buffer[0203] = 17/*18*/; /* micr address */
	buffer[0204] = 148;   /* # blocks */
	buffer[0205] = 0;

	buffer[0206] = 'PAGE'; /* start of partition info */
buffer[0206] = 010521640520;
	buffer[0207] = 01000;
	buffer[0210] = 10;
	buffer[0211] = 0;

	buffer[0212] = 'LOD1'; /* start of partition info */
	buffer[0213] = 0;
	buffer[0214] = 0;
	buffer[0215] = 0;

/* pack text label - offset 020, 32 bytes */

	swapbytes(buffer);

	write(fd, buffer, 256*4);
}

int
write_block(int fd, int block_no, unsigned char *buf)
{
	off_t offset, ret;
	int size;

	offset = block_no * (256*4);

//	if (block_no == 18) printf("write_block() block %d, offset %d\n",
//				   block_no, offset);

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
	int ret, count, i, fd1;
	unsigned char b[256*4];

	printf("making MCR1...\n");

	fd1 = open("ucadr.mcr.979", O_RDONLY);

	count = 0;
	while (1) {
		ret = read(fd1, b, 256*4);
		if (ret <= 0)
			break;

#if 0
		for (i = 0; i < 1024; i += 2) {
			unsigned char t;
			t = b[i];
			b[i] = b[i+1];
			b[i+1] = t;
		}
#endif

		write_block(fd, 17/*18*/+count, b);

		count++;

		if (ret < 256*4)
			break;

	}

	printf("%d blocks\n", count);
}

main(int argc, char *argv[])
{
	int fd;

	filename = strdup("disk.img");

	fd = open(filename, O_RDWR | O_CREAT, 0666);
	if (fd < 0) {
		perror(filename);
		exit(1);
	}

	make_labl(fd);
	make_mcr1(fd);

	exit(0);
}

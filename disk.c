/*
 * disk.c
 * simple CADR disk emulation
 * attempts to emulate the disk controller on a CADR
 *
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "ucode.h"

/*
	disk controller registers:
	  0 read status
	  1 read ma
	  2 read da
	  3 read ecc
	  4 load cmd
	  5 load clp
	  6 load da
	  7 start

	Commands (cmd reg)
	  00 read
	  10 read compare
	  11 write
	  02 read all
	  13 write all
	  04 seek
	  05 at ease
	  1005 recalibreate
	  405 fault clear
	  06 offset clear
	  16 stop,reset

	Status bits (status reg)
	  0 active-
	  1 any attention
	  2 sel unit attention
	  3 intr
	  4 multiple select
	  5 no select
	  6 sel unit fault
	  7 sel unit read only
	  8 on cyl sync-
	  9 sel unit on line-
	  10 sel unit seek error
	  11 timeout error
	  12 start block error
	  13 stopped by error
	  14 overrun
	  15 ecc.soft

	  16 ecc.hard
	  17 header ecc err
	  18 header compare err
	  19 mem parity err
	  20 nmx error
	  21 ccw cyc
	  22 read comp diff
	  23 internal parity err
	  
	  24-31 block.ctr

	Disk address (da reg)
	  31 n/c
	  30 unit2
	  29 unit1
	  28 unit0

	  27 cyl11
	  ...
	  16 cyl0	  
	  
	  15 head7
	  ...
	  8  head0

	  7  block7
	  ...
	  0  block0
*/

int disk_fd;

int disk_status = 1;
int disk_cmd;
int disk_clp;
int disk_da;

int
disk_get_status(void)
{
	return disk_status;
}

int
disk_set_da(int v)
{
	disk_da = v;
}

int
disk_set_clp(int v)
{
	disk_clp = v;
}

int
disk_set_cmd(int v)
{
	disk_cmd = v;
}

int cyls, heads, blocks_per_track;

void
_swaplongbytes(unsigned int *buf)
{
	int i;
#if 0
	unsigned char *p = (unsigned char *)buf;

	for (i = 0; i < 256*4; i += 4) {
		unsigned char t;
		t = p[i];
		p[i] = p[i+1];
		p[i+1] = t;
	}
#endif
#if 0
	for (i = 0; i < 256; i++) {
		buf[i] = ntohl(buf[i]);
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
_disk_read(int block_no, unsigned int *buffer)
{
	off_t offset, ret;
	int size;

	offset = block_no * (256*4);

	if (1) printf("disk: file image block %d, offset %ld\n", block_no, offset);

	ret = lseek(disk_fd, offset, SEEK_SET);
	if (ret != offset) {
		perror("lseek");
		return -1;
	}

	size = 256*4;

	ret = read(disk_fd, buffer, size);
	if (ret != size) {
		printf("disk read error; ret %d, size %d\n", ret, size);
		perror("read");
		return -1;
	}

	/* byte order fixups? */
	_swaplongbytes((unsigned int *)buffer);

	return 0;
}

int
disk_read_block(int unit, int cyl, int head, int block)
{
	int vma, block_no, i;
	unsigned int buffer[256];

	vma = 0;
	block_no =
		(cyl * blocks_per_track * heads) +
		(head * blocks_per_track) + block;

	if (disk_fd) {
		_disk_read(block_no, buffer);
		for (i = 0; i < 256; i++) {
			write_mem(vma + i, buffer[i]);
		}
		return 0;
	}

	/* hack to fake a disk label when no image is present */
	if (unit == 0 && cyl == 0 && head == 0 && block == 0) {
		write_mem(vma + 0, 011420440514); /* label LABL */
		write_mem(vma + 1, 000000000001); /* version = 1 */
		write_mem(vma + 2, 000000001000); /* # cyls */
		write_mem(vma + 3, 000000000004); /* # heads */
		write_mem(vma + 4, 000000000100); /* # blocks */
		write_mem(vma + 5, 000000000400); /* heads*blocks */
		write_mem(vma + 6, 000000001234); /* name of micr part */
		write_mem(vma + 0200, 1); /* # of partitions */
		write_mem(vma + 0201, 1); /* words / partition */

		write_mem(vma + 0202, 01234); /* start of partition info */
		write_mem(vma + 0203, 01000); /* micr address */
		write_mem(vma + 0204, 010);   /* # blocks */
		/* pack text label - offset 020, 32 bytes */
		return 0;
	}
}

int
disk_start(void)
{
	int unit, cyl, head, block;

	unit = (disk_da >> 28) & 07;
	cyl = (disk_da >> 16) & 07777;
	head = (disk_da >> 8) & 0377;
	block = disk_da & 0377;

	printf("disk: unit %d, CHB %o/%o/%o\n",
	       unit, cyl, head, block);

	printf("disk: start, cmd ");

	switch (disk_cmd & 07777) {
	case 0:
		printf("read\n");
		disk_read_block(unit, cyl, head, block);
		break;
	case 010:
		printf("read compare\n");
		break;
	case 011:
		printf("write\n");
		break;
	case 01005:
		printf("recalibrate\n");
		break;
	case 0405:
		printf("fault clear\n");
		break;
	default:
		printf("unknown\n");
	}

}

int
disk_init(char *filename)
{
	unsigned int label[256];

	disk_fd = open(filename, O_RDWR);
	if (disk_fd < 0) {
		disk_fd = 0;
		perror(filename);
		return -1;
	}

	_disk_read(0, label);
	if (label[0] != 011420440514) {
		printf("disk: invalid pack label - disk image ignored");
		close(disk_fd);
		disk_fd = 0;
	}

	cyls = label[2];
	heads = label[3];
	blocks_per_track = label[4];

	printf("disk: image CHB %o/%o/%o\n", cyls, heads, blocks_per_track);

	return 0;
}

/*
 * disk.c
 *
 * simple CADR disk emulation
 * attempts to emulate the disk controller on a CADR
 *
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "ucode.h"

#define DELAY_DISK_INTERRUPT
#define ALLOW_DISK_WRITE

/*
	disk controller registers:
	  0 read status
	  1 read ma
	  2 read da
	  3 read ecc
	  4 load cmd
	  5 load clp (command list pointer)
	  6 load da (disk address)
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

	Command bits
	  0
	  1 cmd
	  2
	  3 cmd to memory
	  4 servo offset plus
	  5 servo offset
	  6 data strobe early
	  7 data strobe late
	  8 fault clear
	  9 recalibrate
	  10 attn intr enb
	  11 done intr enb

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

	  ---

	  CLP (command list pointer) points to list of CCW's
	  Each CCW is phy address to write block

	  clp register (22 bits)
	  [21:16][15:0]
	  fixed  counts up

	  clp address is used to read in new ccw
	  ccw's are read (up to 65535)

	  ccw is used to produce dma address
	  dma address comes from ccw + 8 bit counter

	  ccw
	  [21:1][1]
          physr  |
	  addr   0 = last ccw, 1 = more ccw's

	  ccw   counter
	  [21:8][7:0]

	  ---

	  read ma register
	   t0  t1 CLP
	  [23][22][21:0]
            |   |
            |   type 1 (show how controller is strapped; i.e. what type of
            type 0      disk drive)

	    (trident is type 0)


*/

int disk_fd;

int disk_status = 1;
int disk_cmd;
int disk_clp;
int disk_ma;
int disk_ecc;
int disk_da;

int disk_byteswap;

int
disk_set_byteswap(int on)
{
	disk_byteswap = on;
}

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

	if ((disk_cmd & 06000) == 0)
		deassert_xbus_interrupt();
}

int cyls, heads, blocks_per_track;
int cur_unit, cur_cyl, cur_head, cur_block;

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

	tracedio("disk: file image block %d(10), offset %ld(10)\n",
		 block_no, offset);

	ret = lseek(disk_fd, offset, SEEK_SET);
	if (ret != offset) {
		printf("disk: image file seek error\n");
		perror("lseek");
		return -1;
	}

	size = 256*4;

	ret = read(disk_fd, buffer, size);
	if (ret != size) {
		printf("disk read error; ret %d, offset %lu, size %d\n",
		       ret, offset, size);
		perror("read");

		memset((char *)buffer, 0, size);
		return -1;
	}

#if 0
	/* byte order fixups? */
	if (disk_byteswap) {
		_swaplongbytes((unsigned int *)buffer);
	}
#endif

	return 0;
}

int
_disk_write(int block_no, unsigned int *buffer)
{
	off_t offset, ret;
	int size;

	offset = block_no * (256*4);

	tracedio("disk: file image block %d, offset %ld\n", block_no, offset);

	ret = lseek(disk_fd, offset, SEEK_SET);
	if (ret != offset) {
		printf("disk: image file seek error\n");
		perror("lseek");
		return -1;
	}

	size = 256*4;

#if 0
	/* byte order fixups? */
	if (disk_byteswap) {
		_swaplongbytes((unsigned int *)buffer);
	}
#endif

	ret = write(disk_fd, buffer, size);
	if (ret != size) {
		printf("disk write error; ret %d, size %d\n", ret, size);
		perror("write");
		return -1;
	}

	return 0;
}

int
disk_read_block(unsigned int vma, int unit, int cyl, int head, int block)
{
	int block_no, i;
	unsigned int buffer[256];

	block_no =
		(cyl * blocks_per_track * heads) +
		(head * blocks_per_track) + block;

	if (disk_fd) {
		_disk_read(block_no, buffer);
#if 0
		if (block_no == 10312)
		for (i = 0; i < 32; i++) {
			tracedio("read; vma %011o <- %011o\n",
				 vma + i, buffer[i]);
		}
#endif
		for (i = 0; i < 256; i++) {
			write_phy_mem(vma + i, buffer[i]);
		}
		return 0;
	}

	/* hack to fake a disk label when no image is present */
	if (unit == 0 && cyl == 0 && head == 0 && block == 0) {
		write_phy_mem(vma + 0, 011420440514); /* label LABL */
		write_phy_mem(vma + 1, 000000000001); /* version = 1 */
		write_phy_mem(vma + 2, 000000001000); /* # cyls */
		write_phy_mem(vma + 3, 000000000004); /* # heads */
		write_phy_mem(vma + 4, 000000000100); /* # blocks */
		write_phy_mem(vma + 5, 000000000400); /* heads*blocks */
		write_phy_mem(vma + 6, 000000001234); /* name of micr part */
		write_phy_mem(vma + 0200, 1); /* # of partitions */
		write_phy_mem(vma + 0201, 1); /* words / partition */

		write_phy_mem(vma + 0202, 01234); /* start of partition info */
		write_phy_mem(vma + 0203, 01000); /* micr address */
		write_phy_mem(vma + 0204, 010);   /* # blocks */
		/* pack text label - offset 020, 32 bytes */
		return 0;
	}
}

int
disk_write_block(unsigned int vma, int unit, int cyl, int head, int block)
{
	int block_no, i;
	unsigned int buffer[256];

	block_no =
		(cyl * blocks_per_track * heads) +
		(head * blocks_per_track) + block;

	if (disk_fd) {
		for (i = 0; i < 256; i++) {
			read_phy_mem(vma + i, &buffer[i]);
		}
#if 0
		if (block_no == 1812)
		for (i = 0; i < 32; i++) {
			tracedio("write; vma %011o <- %011o\n",
				 vma + i, buffer[i]);
		}
#endif
		_disk_write(block_no, buffer);
		return 0;
	}

	return 0;
}

void
disk_throw_interrupt(void)
{
	tracedio("disk: throw interrupt\n");
	disk_status |= 1<<3;
	assert_xbus_interrupt();
}

#ifdef DELAY_DISK_INTERRUPT
static int disk_interrupt_delay;

void
disk_future_interrupt()
{
	disk_interrupt_delay = 100;
}

void
disk_poll()
{
	if (disk_interrupt_delay) {
		if (--disk_interrupt_delay == 0) {
			disk_throw_interrupt();
		}
	}
}
#else
void disk_poll() {}
#endif

void
disk_show_cur_addr(void)
{
	tracedio("disk: unit %d, CHB %o/%o/%o\n",
	       cur_unit, cur_cyl, cur_head, cur_block);
}

void
disk_decode_addr(void)
{
	cur_unit = (disk_da >> 28) & 07;
	cur_cyl = (disk_da >> 16) & 07777;
	cur_head = (disk_da >> 8) & 0377;
	cur_block = disk_da & 0377;
}

void
disk_undecode_addr(void)
{
	disk_da =
		((cur_unit & 07) << 28) |
		((cur_cyl & 07777) << 16) |
		((cur_head & 0377) << 8) |
		((cur_block & 0377));
}

void
disk_incr_block(void)
{
	cur_block++;
	if (cur_block >= blocks_per_track) {
		cur_block = 0;
		cur_head++;
		if (cur_head >= heads) {
			cur_head = 0;
			cur_cyl++;
		}
	}
}

void
disk_start_read(void)
{
	unsigned int ccw;
	unsigned int vma;
	int i;

	disk_decode_addr();

	/* process ccw's */
	for (i = 0; i < 65535; i++) {
		int f;

		f = read_phy_mem(disk_clp, &ccw);
		if (f) {
			printf("disk: mem[clp=%o] yielded fault (no page)\n",
			       disk_clp);

			/* huh.  what to do now? */
			return;
		}

		tracedio("disk: mem[clp=%o] -> ccw %08o\n", disk_clp, ccw);

		vma = ccw & ~0377;
		disk_ma = vma;

		disk_show_cur_addr();

		disk_read_block(vma, cur_unit, cur_cyl, cur_head, cur_block);

//		disk_incr_block();
			
		if ((ccw & 1) == 0) {
			tracedio("disk: last ccw\n");
			break;
		}

disk_incr_block();

		disk_clp++;
	}

	disk_undecode_addr();

	if (disk_cmd & 04000) {
#if 0
		disk_throw_interrupt();
#else
		disk_future_interrupt();
#endif
	}
}

void
disk_start_read_compare(void)
{
	disk_decode_addr();
	disk_show_cur_addr();
}

void
disk_start_write(void)
{
#ifndef ALLOW_DISK_WRITE
	disk_decode_addr();
	disk_show_cur_addr();
#else
	unsigned int ccw;
	unsigned int vma;
	int i;

	disk_decode_addr();

	/* process ccw's */
	for (i = 0; i < 65535; i++) {
		int f;

		f = read_phy_mem(disk_clp, &ccw);
		if (f) {
			printf("disk: mem[clp=%o] yielded fault (no page)\n",
			       disk_clp);

			/* huh.  what to do now? */
			return;
		}

		tracedio("disk: mem[clp=%o] -> ccw %08o\n", disk_clp, ccw);

		vma = ccw & ~0377;
		disk_ma = vma;

		disk_show_cur_addr();

		disk_write_block(vma, cur_unit, cur_cyl, cur_head, cur_block);

//		disk_incr_block();
			
		if ((ccw & 1) == 0) {
			tracedio("disk: last ccw\n");
			break;
		}

disk_incr_block();

		disk_clp++;
	}

	disk_undecode_addr();

	if (disk_cmd & 04000) {
#ifdef DELAY_DISK_INTERRUPT
		disk_future_interrupt();
#else
		disk_throw_interrupt();
#endif
	}
#endif
}

int
disk_start(void)
{
	tracedio("disk: start, cmd (%o) ", disk_cmd);

	switch (disk_cmd & 01777) {
	case 0:
		tracedio("read\n");
		disk_start_read();
		break;
	case 010:
		tracedio("read compare\n");
		disk_start_read_compare();
		break;
	case 011:
		tracedio("write\n");
		disk_start_write();
		break;
	case 01005:
		tracedio("recalibrate\n");
		break;
	case 0405:
		tracedio("fault clear\n");
		break;
	default:
		tracedio("unknown\n");
	}
}

int
disk_xbus_write(int offset, unsigned int v)
{
	tracef("disk register write, offset %o <- %o\n", offset, v);

	switch (offset) {
	case 0370:
		tracedio/*tracef*/("disk: load status %o\n", v);
		break;
	case 0374:
		disk_set_cmd(v);
		tracedio/*tracef*/("disk: load cmd %o\n", v);
		break;
	case 0375:
		tracedio("disk: load clp %o (phys page %o)\n", v, v << 8);
		disk_set_clp(v);
		break;
	case 0376:
		disk_set_da(v);
		tracef("disk: load da %o\n", v);
		break;
	case 0377:
		disk_start();
		break;
	default:
		tracedio("disk: unknown reg write %o\n", offset);
		break;
	}

	return 0;
}

int
disk_xbus_read(int offset, unsigned int *pv)
{
	tracef("disk register read, offset %o\n", offset);

	switch (offset) {
	case 0370:
		tracef("disk: read status\n");
		*pv = disk_get_status();
		break;
	case 0371:
		tracef("disk: read ma\n");
		*pv = disk_ma;
		break;
	case 0372:
		tracef("disk: read da\n");
		*pv = disk_da;
		break;
	case 0373:
		tracef("disk: read ecc\n");
		*pv = disk_ecc;
		break;
	case 0374:
		tracef("disk: status read\n");
		/* disk ready */
		*pv = disk_get_status();
		break;
	case 0375:
		*pv = disk_clp;
		break;
	case 0376:
		*pv = disk_da;
		break;
	case 0377:
		*pv = 0;
		break;
	default:
		tracedio("disk: unknown reg read %o\n", offset);
		if (offset != 0)
		{
			extern int trace_mcr_labels_flag;
			extern int u_pc;
			trace_mcr_labels_flag = 1;
			printf("u_pc %011o\n", u_pc);
		}
		break;
	}

	return 0;
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
		printf("disk: invalid pack label - disk image ignored\n");
		printf("label %o\n", label[0]);
		close(disk_fd);
		disk_fd = 0;
	}

	cyls = label[2];
	heads = label[3];
	blocks_per_track = label[4];

	printf("disk: image CHB %o/%o/%o\n", cyls, heads, blocks_per_track);

	/* hack to find mcr symbol file from disk pack label */
	if (label[030] != 0 && label[030] != 0200200200200) {
		char fn[1024], *s;
		strcpy(fn, (char *)&label[030]);
		printf("disk: pack label comment '%s'\n", fn);
		s = strstr(fn, ".mcr.");
		if (s)
			memcpy(s, ".sym.", 5);
		config_set_mcrsym_filename(fn);
	}

	return 0;
}

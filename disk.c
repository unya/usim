// disk.c -- emulate a Trident disk
//
// Each disk block contains one Lisp Machine page worth of data,
// i.e. 256. words or 1024. bytes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "usim.h"
#include "ucode.h"
#include "mem.h"

int disk_fd;
uint8_t *disk_mm;

int disk_status = 1;
int disk_cmd;
int disk_clp;
int disk_ma;
int disk_ecc;
int disk_da;

int disk_byteswap;

int cyls;
int heads;
int blocks_per_track;
int cur_unit;
int cur_cyl;
int cur_head;
int cur_block;

// Words in the disk image are written as little endian, this routine
// will swap bytes around so that on disk word values will be in
// little endian order match if we are reading the file on a big
// endian machine.
void
_swaplongbytes(unsigned int *buf, int word_count)
{
#define SWAP_LONG(x) ( (((x) & 0xff000000) >> 24) |	\
		       (((x) & 0x00ff0000) >> 8) |	\
		       (((x) & 0x0000ff00) << 8) |	\
		       (((x) & 0x000000ff) << 24) )

	for (int i = 0; i < word_count; i++) {
		buf[i] = SWAP_LONG(buf[i]);
	}
}

int
_disk_read(int block_no, unsigned int *buffer)
{
	off_t offset;
	int size;

	offset = block_no * (256 * 4);

	tracedio("disk: file image block %d(10), offset %ld(10)\n", block_no, (long) offset);

	size = 256 * 4;
	memcpy(buffer, disk_mm + offset, size);

	if (disk_byteswap) {
		_swaplongbytes((unsigned int *) buffer, 256);
	}

	return 0;
}

int
_disk_write(int block_no, unsigned int *buffer)
{
	off_t offset;
	int size;

	offset = block_no * (256 * 4);

	tracedio("disk: file image block %d, offset %ld\n", block_no, (long) offset);

	size = 256 * 4;
	memcpy(disk_mm + offset, buffer, size);

	if (disk_byteswap) {
		_swaplongbytes((unsigned int *) buffer, 256);
	}

	return 0;
}

int
disk_read_block(unsigned int vma, int unit, int cyl, int head, int block)
{
	int block_no;
	int i;
	unsigned int buffer[256];

	block_no = (cyl * blocks_per_track * heads) + (head * blocks_per_track) + block;
	if (disk_fd) {
		if (_disk_read(block_no, buffer) < 0) {
			printf("disk_read_block: error reading block_no %d\n", block_no);
			return -1;
		}
		for (i = 0; i < 256; i++) {
			write_phy_mem(vma + i, buffer[i]);
		}
		return 0;
	}

	// Hack to fake a disk label when no disk image is present.
#define LABEL_LABL 011420440514ULL
#define LABEL_BLANK 020020020020ULL
	if (unit == 0 && cyl == 0 && head == 0 && block == 0) {
		write_phy_mem(vma + 0, LABEL_LABL);	// Label magic.
		write_phy_mem(vma + 1, 000000000001);	// Version.
		write_phy_mem(vma + 2, 000000001000);	// Number of cylinders
		write_phy_mem(vma + 3, 000000000004);	// Number of heades.
		write_phy_mem(vma + 4, 000000000100);	// Number of blocks.
		write_phy_mem(vma + 5, 000000000400);	// Heads * blocks.
		write_phy_mem(vma + 6, 000000001234);	// Name of microcode partition.
		write_phy_mem(vma + 0200, 1);	// Number of partitions.
		write_phy_mem(vma + 0201, 1);	// Words per partition.
		write_phy_mem(vma + 0202, 01234);	// Start of partition info.
		write_phy_mem(vma + 0203, 01000);	// Microcode address.
		write_phy_mem(vma + 0204, 010);	// Number of blocks.

		return 0;
	}

	return -1;
}

int
disk_write_block(unsigned int vma, int cyl, int head, int block)
{
	int block_no;
	int i;
	unsigned int buffer[256];

	block_no = (cyl * blocks_per_track * heads) + (head * blocks_per_track) + block;

	if (disk_fd) {
		for (i = 0; i < 256; i++) {
			read_phy_mem(vma + i, &buffer[i]);
		}
		_disk_write(block_no, buffer);

		return 0;
	}

	return 0;
}

void
disk_throw_interrupt(void)
{
	tracedio("disk: throw interrupt\n");
	disk_status |= 1 << 3;
	assert_xbus_interrupt();
}

int disk_interrupt_delay;

void
disk_future_interrupt(void)
{
	disk_interrupt_delay = 100;
	disk_interrupt_delay = 2500;
}

void
disk_poll(void)
{
	if (disk_interrupt_delay) {
		if (--disk_interrupt_delay == 0) {
			disk_throw_interrupt();
		}
	}
}

void
disk_show_cur_addr(void)
{
	tracedio("disk: unit %d, CHB %o/%o/%o\n", cur_unit, cur_cyl, cur_head, cur_block);
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

	// Process CCW's.
	for (i = 0; i < 65535; i++) {
		int f;

		f = read_phy_mem(disk_clp, &ccw);
		if (f) {
			// Huh. what to do now?
			printf("disk: mem[clp=%o] yielded fault (no page)\n", disk_clp);
			return;
		}

		tracedio("disk: mem[clp=%o] -> ccw %08o\n", disk_clp, ccw);

		vma = ccw & ~0377;
		disk_ma = vma;

		disk_show_cur_addr();

		disk_read_block(vma, cur_unit, cur_cyl, cur_head, cur_block);

		if ((ccw & 1) == 0) {
			tracedio("disk: last ccw\n");
			break;
		}

		disk_incr_block();

		disk_clp++;
	}

	disk_undecode_addr();

	if (disk_cmd & 04000) {
		disk_future_interrupt();
	}
}

void
disk_start_read_compare(void)
{
	printf("disk_start_read_compare!\n");
	disk_decode_addr();
	disk_show_cur_addr();
}

void
disk_start_write(void)
{
	unsigned int ccw;
	unsigned int vma;
	int i;

	disk_decode_addr();

	// Process CCW's.
	for (i = 0; i < 65535; i++) {
		int f;

		f = read_phy_mem(disk_clp, &ccw);
		if (f) {
			// Huh. what to do now?
			printf("disk: mem[clp=%o] yielded fault (no page)\n", disk_clp);
			return;
		}

		tracedio("disk: mem[clp=%o] -> ccw %08o\n", disk_clp, ccw);

		vma = ccw & ~0377;
		disk_ma = vma;

		disk_show_cur_addr();

		disk_write_block(vma, cur_cyl, cur_head, cur_block);

		if ((ccw & 1) == 0) {
			tracedio("disk: last ccw\n");
			break;
		}
		disk_incr_block();

		disk_clp++;
	}

	disk_undecode_addr();

	if (disk_cmd & 04000) {
		disk_future_interrupt();
	}
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
		return -1;
	}

	return 0;
}

void
disk_xbus_write(int offset, unsigned int v)
{
	tracef("disk register write, offset %o <- %o\n", offset, v);

	switch (offset) {
	case 0370:
		tracedio("disk: load status %o\n", v);
		break;
	case 0374:
		disk_cmd = v;
		if ((disk_cmd & 06000) == 0)
			deassert_xbus_interrupt();
		tracedio("disk: load cmd %o\n", v);
		break;
	case 0375:
		tracedio("disk: load clp %o (phys page %o)\n", v, v << 8);
		disk_clp = v;
		break;
	case 0376:
		disk_da = v;
		tracef("disk: load da %o\n", v);
		break;
	case 0377:
		disk_start();
		break;
	default:
		tracedio("disk: unknown reg write %o\n", offset);
		break;
	}
}

void
disk_xbus_read(int offset, unsigned int *pv)
{
	tracef("disk register read, offset %o\n", offset);

	switch (offset) {
	case 0370:
		tracef("disk: read status\n");
		*pv = disk_status;
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
		*pv = disk_status;
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
		break;
	}
}

int
disk_init(char *filename)
{
	unsigned int label[256];
	int ret;

	label[0] = 0;

#ifdef __BIG_ENDIAN__
	disk_byteswap = on;
#endif

	printf("disk: opening %s\n", filename);

	disk_fd = open(filename, O_RDWR | O_BINARY);
	if (disk_fd < 0) {
		disk_fd = 0;
		perror(filename);
		exit(1);
	}

	struct stat s;
	fstat(disk_fd, &s);
	printf("disk: size: %zd bytes\n", s.st_size);
	disk_mm = mmap(NULL, s.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);

	ret = _disk_read(0, label);
	if (ret < 0 || label[0] != LABEL_LABL) {
		printf("disk: invalid pack label - disk image ignored\n");
		printf("label %o\n", label[0]);
		close(disk_fd);
		disk_fd = 0;
		return -1;
	}

	cyls = label[2];
	heads = label[3];
	blocks_per_track = label[4];

	printf("disk: image CHB %o/%o/%o\n", cyls, heads, blocks_per_track);

	// Hack to find MCR symbol file via disk pack label.
	if (label[030] != 0 && label[030] != LABEL_BLANK) {
		char fn[1024];
		char *s;

		memset(fn, 0, sizeof(fn));
		strcpy(fn, (char *) &label[030]);
#ifdef __BIG_ENDIAN__
		memcpy(fn, (char *) &label[030], 32);
		_swaplongbytes((unsigned int *) fn, 8);
#endif
		printf("disk: pack label comment '%s'\n", fn);
		s = strstr(fn, ".mcr.");
		if (s)
			memcpy(s, ".sym.", 5);
		mcrsym_filename = strdup(fn);
	}

	return 0;
}

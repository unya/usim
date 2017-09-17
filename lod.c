#include "usim.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "macroops.h"

char *loadband_filename;
char *disk_filename;

int show_comm;
int show_scratch;
int show_initial_fef;
int show_fef;
int show_initial_sg;
int show_memory;
int set_wide;

struct {
	char *name;
	unsigned int a;
	unsigned int v;
} sys_com[] = {
	{"%SYS-COM-AREA-ORIGIN-PNTR", 0400, 0},
	{"%SYS-COM-VALID-SIZE", 0401, 0},
	{"%SYS-COM-PAGE-TABLE-PNTR", 0402, 0},
	{"%SYS-COM-PAGE-TABLE-SIZE", 0403, 0},
	{"%SYS-COM-OBARRAY-PNTR", 0404, 0},
	{"%SYS-COM-ETHER-FREE-LIST", 0405, 0},
	{"%SYS-COM-ETHER-TRANSMIT-LIST", 0406, 0},
	{"%SYS-COM-ETHER-RECEIVE-LIST", 0407, 0},
	{"%SYS-COM-BAND-FORMAT", 0410, 0},
	{"%SYS-COM-SPARE-2", 0411, 0},
	{"%SYS-COM-UNIBUS-INTERRUPT-LIST", 0412, 0},
	{"%SYS-COM-TEMPORARY", 0413, 0},
	{"%SYS-COM-FREE-AREA/#-LIST", 0414, 0},
	{"%SYS-COM-FREE-REGION/#-LIST", 0415, 0},
	{"%SYS-COM-MEMORY-SIZE", 0416, 0},
	{"%SYS-COM-WIRED-SIZE", 0417, 0},
	{(char *) 0, 0, 0},
};

struct {
	char *name;
	unsigned int a;
	unsigned int v;
} cv[] = {
	{"A-V-RESIDENT-SYMBOL-AREA", 0, 0},
	{"A-V-SYSTEM-COMMUNICATION-AREA", 0, 0},
	{"A-V-SCRATCH-PAD-INIT-AREA", 0, 0},
	{"A-V-MICRO-CODE-SYMBOL-AREA", 0, 0},
	{"A-V-PAGE-TABLE-AREA", 0, 0},
	{"A-V-PHYSICAL-PAGE-DATA", 0, 0},
	{"A-V-REGION-ORIGIN", 0, 0},
	{"A-V-REGION-LENGTH", 0, 0},
	{"A-V-REGION-BITS", 0, 0},
	{"A-V-ADDRESS-SPACE-MAP", 0, 0},
	{"A-V-REGION-FREE-POINTER", 0, 0},
	{"A-V-REGION-GC-POINTER", 0, 0},
	{"A-V-REGION-LIST-THREAD", 0, 0},
	{"A-V-AREA-NAME", 0, 0},
	{"A-V-AREA-REGION-LIST", 0, 0},
	{"A-V-AREA-REGION-SIZE", 0, 0},
	{"A-V-AREA-MAXIMUM-SIZE", 0, 0},
	{"A-V-AREA-SWAP-RECOMMENDATIONS", 0, 0},
	{"A-V-GC-TABLE-AREA", 0, 0},
	{"A-V-SUPPORT-ENTRY-VECTOR", 0, 0},
	{"A-V-CONSTANTS-AREA", 0, 0},
	{"A-V-EXTRA-PDL-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-NAME-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-ARGS-INFO-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-MAX-PDL-USAGE", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-ARGLIST-AREA", 0, 0},
	{"A-V-MICRO-CODE-SYMBOL-NAME-AREA", 0, 0},
	{"A-V-LINEAR-PDL-AREA", 0, 0},
	{"A-V-LINEAR-BIND-PDL-AREA", 0, 0},
	{"A-V-INIT-LIST-AREA", 0, 0},
	{"A-V-FIRST-UNFIXED-AREA", 0, 0},
	{(char *) 0, 0, 0}
};

struct {
	char *name;
	unsigned int a;
	unsigned int v;
} sv[] = {
	{"A-INITIAL-FEF", 0, 0},
	{"A-QTRSTKG", 0, 0},
	{"A-QCSTKG", 0, 0},
	{"A-QISTKG", 0, 0},
	{(char *) 0, 0, 0}
};

int lodfd;
int swapfd;
int partoff;
int bnum = -1;
unsigned int buf[256];

unsigned int
read_virt(int fd, int addr)
{
	int b;
	off_t offset;
	off_t ret;

	addr &= 077777777;
	b = addr / 256;

	offset = (b + partoff) * (256 * 4);

	if (b != bnum) {
		bnum = b;
		ret = lseek(fd, offset, SEEK_SET);
		if (ret != offset) {
			perror("seek");
		}

		ret = read(fd, buf, 256 * 4);
		if (ret != 256 * 4) {
		}
	}

	return buf[addr % 256];
}

unsigned int
vr(int addr)
{
	return read_virt(lodfd, addr);
}

unsigned int
swap_vr(int addr)
{
	return read_virt(swapfd, addr);
}

struct part_s {
	char *name;
	int start;
	int size;
} parts[16];

int part_count;

unsigned long
str4(char *s)
{
	return (s[3] << 24) | (s[2] << 16) | (s[1] << 8) | s[0];
}

char *
unstr4(unsigned long s)
{
	static char b[5];

	b[3] = s >> 24;
	b[2] = s >> 16;
	b[1] = s >> 8;
	b[0] = s;
	b[4] = 0;

	return b;
}

int
read_partition_table(void)
{
	int i;
	int ret;
	int p;
	int count;

	ret = read(swapfd, buf, 256 * 4);
	if (ret != 256 * 4) {
		perror(disk_filename);
		return -1;
	}

	if (buf[0] != str4("LABL")) {
		fprintf(stderr, "%s: no valid disk label found\n", disk_filename);
		return -1;
	}

	if (buf[1] != 1) {
		fprintf(stderr, "%s: label version not 1\n", disk_filename);
		return -1;
	}

	count = buf[0200];
	p = 0202;

	part_count = 0;
	for (i = 0; i < count; i++) {
		parts[part_count].name = strdup(unstr4(buf[p + 0]));
		parts[part_count].start = buf[p + 1];
		parts[part_count].size = buf[p + 2];
		part_count++;
	}

	return 0;
}

void
set_swap(void)
{
	int i;

	// Nice hack, eh? Swap starts at block 0524 - see diskmaker.c.
	partoff = 0524;

	for (i = 0; i < part_count; i++) {
		if (strcmp(parts[i].name, "SWAP") == 0) {
			partoff = parts[i].start;
			break;
		}
	}
	printf("SWAP partoff %o\n", partoff);
	bnum = -1;
}

void
set_lod1(void)
{
	int i;

	bnum = -1;

	// Nice hack, eh? Swap starts at block 0524 - see diskmaker.c.
	partoff = 021210;

	for (i = 0; i < part_count; i++) {
		if (strcmp(parts[i].name, "LOD1") == 0) {
			partoff = parts[i].start;
			break;
		}
	}
	printf("LOD1 partoff %o\n", partoff);
}

unsigned int
_show(int fd, int a, int cr)
{
	unsigned int v;

	v = read_virt(fd, a);
	printf("%011o %011o (0x%08x)", a, v, v);
	if (cr)
		printf("\n");

	return v;
}

unsigned int
_get(int fd, int a)
{
	unsigned int v;

	v = read_virt(fd, a);

	return v;
}

unsigned int
get(int a)
{
	return _get(lodfd, a);
}

unsigned int
show(int a, int cr)
{
	return _show(lodfd, a, cr);
}

unsigned int
showlabel(char *l, int a, int cr)
{
	printf("%s: ", l);
	return _show(lodfd, a, cr);
}

unsigned int
showswap(char *l, int a, int cr)
{
	printf("%s: ", l);
	return _show(swapfd, a, cr);
}

unsigned int
swap_show(int a, int cr)
{
	return _show(swapfd, a, cr);
}

void
showstr(int a, int cr)
{
	int t;
	int i;
	int j;
	unsigned int n;
	char s[256];

	t = get(a) & 0xff;
	j = 0;
	for (i = 0; i < t; i += 4) {
		n = get(a + 1 + (i / 4));
		s[j++] = n >> 0;
		s[j++] = n >> 8;
		s[j++] = n >> 16;
		s[j++] = n >> 24;
	}
	s[t] = 0;
	printf("'%s' ", s);
	if (cr)
		printf("\n");
}

void
show_fef_func_name(unsigned int fefptr, unsigned int width)
{
	unsigned int n;
	unsigned int v;
	int tag;

	n = get(fefptr + 2);
	printf(" ");
	v = get(n);
	tag = (v >> width) & 037;
	if (tag == 3) {
		v = get(v);
		tag = (v >> width) & 037;
	}
	if (tag == 4) {
		printf(" ");
		showstr(v, 0);
	}
}

void
disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst, unsigned int width)
{
	int op;
	int dest;
	int reg;
	int delta;
	int adr;
	int to;
	unsigned int nlc;

	if (!misc_inst_vector_setup) {
		int i;
		int index;
		for (i = 0; i < 1024; i++) {
			if (misc_inst[i].name == 0)
				break;
			index = misc_inst[i].value;
			misc_inst_vector[index] = i;
		}
		misc_inst_vector_setup = 1;
	}
	op = (inst >> 011) & 017;
	dest = (inst >> 015) & 07;
	reg = (inst >> 6) & 07;
	delta = (inst >> 0) & 077;
	printf("%011o%c %06o %s ", loc, even ? 'e' : 'o', inst, op_names[op]);

	switch (op) {
	case 0:		// CALL
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);
		{
			unsigned int v;
			unsigned int tag;

			v = get(fefptr + delta);
			tag = (v >> width) & 037;
			switch (tag) {
			case 3:
				v = get(v);
				showstr(v, 0);
				break;
			case 4:
				showstr(v, 0);
				break;
			case 027:
				break;
			default:
				v = get(v);
				show_fef_func_name(v, width);
			}
		}
		break;
	case 2:		// MOVE.
	case 3:		// CAR
	case 4:		// CDR.
	case 5:		// CADR.
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);
		break;
	case 011:		// ND1.
		printf("%s ", nd1_names[dest]);
		break;
	case 012:		// ND2.
		printf("%s ", nd2_names[dest]);
		break;
	case 013:		// ND3.
		printf("%s ", nd3_names[dest]);
		break;
	case 014:		// BRANCH.
		printf("type %s, ", branch_names[dest]);

		to = (inst & 03777) << 1;
		to |= (inst & 0x8000) ? 1 : 0;

		if (inst & 0400) {
			to = inst & 01777;
			to |= 03000;
			to |= ~01777;
		}

		nlc = (loc * 2 + (even ? 0 : 1)) + to;

		if (to > 0) {
			printf("+%o; %o%c ", to, nlc / 2, (nlc & 1) ? 'o' : 'e');
		} else {
			printf("-%o; %o%c ", -to, nlc / 2, (nlc & 1) ? 'o' : 'e');
		}
		break;
	case 015:		// MISC.
		adr = inst & 0777;
		if (adr < 1024 && misc_inst_vector[adr]) {
			printf("%s ", misc_inst[misc_inst_vector[adr]].name);
		} else {
			printf("%o ", adr);
		}
		printf("dest %s, ", dest_names[dest]);
		break;
	}
	printf("\n");
}

int
find_and_dump_fef(unsigned int pc)
{
	unsigned int addr;
	unsigned int v;
	unsigned int n;
	unsigned int o;
	int i;
	int j;
	int tag;
	int icount;
	int max;
	unsigned short ib[512];
	unsigned int width;

	printf("\n");
	width = set_wide ? 25 : 24;
	addr = pc >> 2;
	printf("pc %o, addr %o\n", pc, addr);

	// Find FEF>
	for (i = 0; i < 512; i--) {
		n = get(addr);
		tag = (n >> width) & 037;
		if (tag == 7)
			break;
		addr--;
	}
	if (tag != 7) {
		printf("couldn't not find FEF\n");
		return -1;
	}

	n = get(addr);
	o = n & 0777;
	printf("code offset %o\n", o);

	max = get(addr + 1) & 07777;
	icount = (max - o / 2) * 2;

	j = 0;
	for (i = 0; i < max; i++) {
		unsigned int loc;
		unsigned int inst;

		loc = addr + i;
		inst = get(loc);
		ib[j++] = inst;
		ib[j++] = inst >> 16;
		if (i < o / 2) {
			show(loc, 1);
		}
		switch (i) {
		case 1:
			break;
		case 2:
			printf(" ");
			v = show(inst, 0);
			tag = (v >> width) & 037;
			if (tag == 3) {
				printf("\n");
				printf(" ");
				v = show(v, 0);
				tag = (v >> 24) & 037;
			}
			if (tag == 4) {
				printf(" ");
				showstr(v, 1);
			}
			break;
		}
	}

	for (i = o; i < o + icount; i++) {
		unsigned int loc;

		loc = addr + i / 2;
		disass(addr, loc, (i % 2) ? 0 : 1, ib[i], width);
	}

	return 0;
}

void
usage(void)
{
	fprintf(stderr, "usage: lod -l FILE [OPTION]...\n");
	fprintf(stderr, "  or:  lod -i FILE [OPTION]...\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -l FILE        LOD band file\n");
	fprintf(stderr, "  -i FILE        disk image\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -c             dump system communication area\n");
	fprintf(stderr, "  -s             dump scratch-pad area\n");
	fprintf(stderr, "  -f             find and disassemble initial FEF\n");
	fprintf(stderr, "  -g             dump initial stack group\n");
	fprintf(stderr, "  -p PC          find and disassemble FEF for given pc\n");
	fprintf(stderr, "  -a addr        find and disassemble FEF for given address\n");
	fprintf(stderr, "  -m ADDR        dump memory\n");
	fprintf(stderr, "  -w             decode 25-bit pointers\n");
	exit(1);
}

extern char *optarg;
extern int optind;

int
main(int argc, char *argv[])
{
	unsigned int com;
	int i;
	int c;
	unsigned int pc = 0;
	unsigned int addr = 0;

	while ((c = getopt(argc, argv, "l:i:csfgp:a:m:w")) != -1) {
		switch (c) {
		case 'l':
			loadband_filename = strdup(optarg);
			break;
		case 'i':
			disk_filename = strdup(optarg);
			break;
		case 'c':
			show_comm++;
			break;
		case 's':
			show_scratch++;
			break;
		case 'f':
			show_initial_fef++;
			break;
		case 'g':
			show_initial_sg++;
			break;
		case 'p':
			sscanf(optarg, "%o", &pc);
			show_fef++;
			break;
		case 'a':
			sscanf(optarg, "%o", &addr);
			pc = addr * 4;
			show_fef++;
			break;
		case 'm':
			sscanf(optarg, "%o", &addr);
			show_memory++;
			break;
		case 'w':
			set_wide++;
			break;
		}
	}

	if (optind < argc || argc == 1) {
		usage();
	}

	// Raw LOD band.
	if (loadband_filename) {
		lodfd = open(loadband_filename, O_RDONLY);
		if (lodfd < 0) {
			perror(loadband_filename);
			exit(1);
		}
	}

	// Optional full disk image (to check swap).
	if (disk_filename) {
		swapfd = open(disk_filename, O_RDONLY);
		if (swapfd < 0) {
			perror(disk_filename);
			exit(1);
		}

		read_partition_table();
		set_swap();
	}

	if (swapfd == 0 && lodfd == 0) {
		fprintf(stderr, "need either -l or -f (or both)\n");
		exit(2);
	}

	if (loadband_filename) {
		com = showlabel("%SYS-COM-AREA-ORIGIN-PNTR", 0400, 1);
		showlabel("%SYS-COM-BAND-FORMAT", 0410, 1);

		if (show_comm) {
			for (i = 0; cv[i].name; i++) {
				printf("%s ", cv[i].name);
				cv[i].a = com + i;
				cv[i].v = show(cv[i].a, 0);
				printf("; ");
				show(cv[i].v, 1);
			}
			printf("\n");
		}

		if (show_scratch) {
			printf("scratch-pad\n");
			for (i = 0; sv[i].name; i++) {
				printf("%s ", sv[i].name);
				sv[i].a = 01000 + i;
				sv[i].v = show(sv[i].a, 0);
				printf("; ");
				show(sv[i].v, 1);
			}
			printf("\n");
		}

		if (show_initial_fef) {
			unsigned int v;

			sv[0].a = 01000 + 0;
			sv[0].v = showlabel(sv[0].name, sv[0].a, 1);
			v = show(sv[0].v, 1);
			find_and_dump_fef(v << 2);
		}

		if (show_fef) {
			find_and_dump_fef(pc);
		}

		if (show_initial_sg) {
			int i;
			unsigned int a;

			sv[3].a = 01000 + 3;
			sv[3].v = showlabel(sv[3].name, sv[3].a, 1);
			a = sv[3].v & 0x00ffffff;
			printf("\ninitial sg:\n");
			for (i = 10; i >= 0; i--) {
				char b[16];
				sprintf(b, "%d", -i);
				show(a - i, 1);
			}
		}

		if (show_memory) {
			printf("memory @ %o:\n", addr);
			for (i = 0; i < 10; i++) {
				show(addr + i, 1);
			}
		}
	}

	if (disk_filename) {
		if (show_comm) {
			for (i = 0; sys_com[i].name; i++) {
				sys_com[i].v = showswap(sys_com[i].name, sys_com[i].a, 1);
			}
		}
	}

	exit(0);
}

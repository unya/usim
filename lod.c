/*
 * feeble program to pull information out of CADR load band 
 * 10/2004
 * brad@heeltoe.com
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

/*
0  TRAP
1  NULL
2  FREE
3  SYMBOL
4  SYMBOL HEADER
5  FIX
6  EXTENDED NUMBER
7  HEADER
10 GC-FORWARD
11 EXTERNAL-VALUE-CELL-POINTER
12 ONE-Q-FORWARD
13 HEADER-FORWARD
14 BODY-FORWARD
15 LOCATIVE
16 LIST -- don't skip
17 U CODE ENTRY
20 FEF
21 ARRAY-POINTER
22 ARRAY-HEADER
23 STACK-GROUP
24 CLOSURE
25 SMALL-FLONUM
26 SELECT-METHOD
27 INSTANCE
0  INSTANCE-HEADER
0  ENTITY
0  STACK-CLOSURE
*/

struct {
	char *name;
	unsigned int a;
	unsigned int v;
} cv[] = {
	{ "A-V-RESIDENT-SYMBOL-AREA", 0 },
	{ "A-V-SYSTEM-COMMUNICATION-AREA", 0 },
	{ "A-V-SCRATCH-PAD-INIT-AREA", 0 },
	{ "A-V-MICRO-CODE-SYMBOL-AREA", 0 },
	{ "A-V-PAGE-TABLE-AREA", 0 },
	{ "A-V-PHYSICAL-PAGE-DATA", 0 },
	{ "A-V-REGION-ORIGIN", 0 },
	{ "A-V-REGION-LENGTH", 0 },
	{ "A-V-REGION-BITS", 0 },
	{ "A-V-ADDRESS-SPACE-MAP", 0 },
	{ "A-V-REGION-FREE-POINTER", 0 },
	{ "A-V-REGION-GC-POINTER", 0 },
	{ "A-V-REGION-LIST-THREAD", 0 },
	{ "A-V-AREA-NAME", 0 },
	{ "A-V-AREA-REGION-LIST", 0 },
	{ "A-V-AREA-REGION-SIZE", 0 },
	{ "A-V-AREA-MAXIMUM-SIZE", 0 },
	{ "A-V-AREA-SWAP-RECOMMENDATIONS", 0 },
	{ "A-V-GC-TABLE-AREA", 0 },
	{ "A-V-SUPPORT-ENTRY-VECTOR", 0 },
	{ "A-V-CONSTANTS-AREA", 0 },
	{ "A-V-EXTRA-PDL-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-NAME-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-ARGS-INFO-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-MAX-PDL-USAGE", 0 },
	{ "A-V-MICRO-CODE-ENTRY-ARGLIST-AREA", 0 },
	{ "A-V-MICRO-CODE-SYMBOL-NAME-AREA", 0 },
	{ "A-V-LINEAR-PDL-AREA", 0 },
	{ "A-V-LINEAR-BIND-PDL-AREA", 0 },
	{ "A-V-INIT-LIST-AREA", 0 },
	{ "A-V-FIRST-UNFIXED-AREA", 0 },
	{ (char *)0, 0 }
};

struct {
	char *name;
	unsigned int a;
	unsigned int v;
} sv[] = {
	{ "A-INITIAL-FEF", 0 },
	{ "A-QTRSTKG", 0 },
	{ "A-QCSTKG", 0 },
	{ "A-QISTKG", 0 },
	{ (char *)0, 0 }
};

int lodfd, swapfd;
int partoff;
int bnum = -1;
unsigned int buf[256];

unsigned int
read_virt(int fd, int addr)
{
	int b;
	off_t offset, ret;

	addr &= 077777777;

	b = addr / 256;

	offset = (b + partoff) * (256*4);

	if (b != bnum) {
		bnum = b;

		if (0) printf("block %d(10)\n", b);

		ret = lseek(fd, offset, SEEK_SET);
		if (ret != offset) {
			perror("seek");
		}

		ret = read(fd, buf, 256*4);
		if (ret != 256*4) {
		}
	}

	return buf[ addr % 256 ];
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

void
set_swap(void)
{
	bnum = -1;

	/* nice hack, eh?  swap starts @ block 0524 - see diskmaker.c */
	partoff = 0524;
}

void
set_lod1(void)
{
	bnum = -1;

	/* nice hack, eh?  swap starts @ block 0524 - see diskmaker.c */
	partoff = 021210;
}

unsigned int
_show(int fd, int a, int cr)
{
	unsigned int v;
	v = read_virt(fd, a);
	printf("%011o %011o", a, v);
	if (cr) printf("\n");
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
swap_show(int a, int cr)
{
	return _show(swapfd, a, cr);
}

void
showstr(int a, int cr)
{
	int t, i, j;
	unsigned int n;
	char s[256];

	t = get(a) & 0xff;
	j = 0;
	for (i = 0; i < t; i += 4) {
		n = get(a+1+(i/4));
		s[j++] = n >> 0;
		s[j++] = n >> 8;
		s[j++] = n >> 16;
		s[j++] = n >> 24;
	}

	s[t] = 0;
	printf("'%s' ", s);
	if (cr) printf("\n");
}

int
show_fef_func_name(unsigned int fefptr)
{
	unsigned int n, v;
	int tag;

	n = get(fefptr+2);

	printf(" "); v = get(n);

	tag = (v >> 24) & 077;
	if (0) printf("tag %o\n", tag);

	if (tag == 3) {
		v = get(v);
		tag = (v >> 24) & 077;
	}

	if (tag == 4) {
		printf(" "); showstr(v, 0);
	}
}

#include "macro.c"

int
find_and_dump_fef(unsigned int pc)
{
	unsigned int addr, v, n, o;
	int i, j, tag, icount, max;
	unsigned short ib[512];

	printf("\n");

	addr = pc >> 2;
	if (1) printf("pc %o, addr %o\n", pc, addr);

	/* find fef */
	for (i = 0; i < 512; i--) {
		n = get(addr);
		tag = (n >> 24) & 077;
		if (tag == 7) break;
		addr--;
	}

	if (tag != 7) {
		printf("couldn't not find FEF\n");
		return -1;
	}

	n = get(addr);
	o = n & 0777;
	printf("code offset %o\n", o);

	max = get(addr+1) & 07777;

	icount = (max - o/2) * 2;

	j = 0;
	for (i = 0; i < max; i++) {
		unsigned int loc, inst;
		loc = addr+i;
		inst = get(loc);

		ib[j++] = inst;
		ib[j++] = inst >> 16;

		if (i < o/2)
		{
			show(loc, 1);
		}

		switch (i) {
		case 1:
			break;
		case 2:
			printf(" "); v = show(inst, 0);

			tag = (v >> 24) & 077;
			if (0) printf("tag %o\n", tag);

			if (tag == 3) {
				printf("\n");
				printf(" "); v = show(v, 0);
				tag = (v >> 24) & 077;
			}
			if (tag == 4) {
				printf(" "); showstr(v, 1);
			}
			break;
		}
	}

	for (i = o; i < o+icount; i++) {
		unsigned int loc;
		loc = addr+i/2;
		disass(addr, loc, (i%2) ? 0 : 1, ib[i]);
	}
}


void
usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "-l <load-band filename>\n");
	fprintf(stderr, "-i <disk-image filename>\n");
	fprintf(stderr, "\n");	
	fprintf(stderr, "-c	dump system communication area\n");
	fprintf(stderr, "-s	dump scratch-pad area\n");
	fprintf(stderr, "-f	find and disassemble initial FEF\n");
	fprintf(stderr, "-g	dump initial stack group\n");
	fprintf(stderr, "-p <pc> find and disassemble FEF for given pc\n");
	fprintf(stderr, "-a <addr> find and disassemble FEF for given address\n");
	exit(1);
}

extern char *optarg;
extern int optind;

char *loadband_filename;
char *disk_filename;
int show_comm;
int show_scratch;
int show_initial_fef;
int show_fef;
int show_initial_sg;
int show_memory;

main(int argc, char *argv[])
{
	unsigned int com;
	int i, n, c;
	unsigned int pc, addr;

	while ((c = getopt(argc, argv, "l:i:csfgp:a:m:")) != -1) {
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
			pc = addr*4;
			show_fef++;
			break;
		case 'm':
			sscanf(optarg, "%o", &addr);
			show_memory++;
			break;
		}
	}

	if (0) printf("optind %d, argc %d\n", optind, argc);
	if (optind < argc || argc == 1) {
		usage();
	}

	if (loadband_filename) {
		/* raw load band file */
		lodfd = open(loadband_filename, O_RDONLY);
		if (lodfd < 0) {
			perror(loadband_filename);
			exit(1);
		}
	}

	if (disk_filename) {
		/* optional full disk image (to check swap) */
		swapfd = open(disk_filename, O_RDONLY);
		if (swapfd < 0) {
			perror(disk_filename);
			exit(1);
		}
	}

	if (swapfd == 0 && lodfd == 0) {
		fprintf(stderr, "need either -l or -f (or both)\n");
		exit(2);
	}

	/* %SYS-COM-AREA-ORIGIN-PNTR */
	com = showlabel("%SYS-COM-AREA-ORIGIN-PNTR", 0400, 1);

	if (show_comm) {
		for (i = 0; cv[i].name; i++) {
			printf("%s ", cv[i].name);
			cv[i].a = com+i;
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
			sv[i].a = 01000+i;
			sv[i].v = show(sv[i].a, 0);
			printf("; ");
			show(sv[i].v, 1);
		}
		printf("\n");
	}

	if (show_initial_fef) {
		unsigned int v;

		sv[0].a = 01000+0;
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

		sv[3].a = 01000+3;
		sv[3].v = showlabel(sv[3].name, sv[3].a, 1);

		a = sv[3].v & 0x00ffffff;

		printf("\ninitial sg:\n");
		for (i = 10; i >= 0; i--) {
			char b[16];
			sprintf(b, "%d", -i);
			show(a-i, 1);
		}
	}

	if (show_memory) {
		printf("memory @ %o:\n", addr);
		for (i = 0; i < 10; i++) {
			show(addr+i, 1);
		}
	}

#if 0
	//xxx need to add code to look at disk label
	//xxx and set partition offsets based no that
	//xxx (instead of the bogus hardcoded offsets in set_swap() and set_lod1()

	if (swapfd)
	{
		int i;
		unsigned int a;

		a = sv[3].v & 0x00ffffff;
		printf("a %o\n", a);

		set_swap();
		printf("sg - swap\n");

		for (i = 10; i >= 0; i--) {
			char b[16];
			sprintf(b, "%d", -i);
			swap_show(a-i, 1);
		}

		set_lod1();
		printf("sg - lod1\n");

		for (i = 10; i >= 0; i--) {
			char b[16];
			sprintf(b, "%d", -i);
			swap_show(a-i, 1);
		}
	}
#endif


}

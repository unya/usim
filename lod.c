#include <stdio.h>
#include <fcntl.h>

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

char *op_names[16] = {
	"CALL",
	"CALL0",
	"MOVE",
	"CAR",
	"CDR",
	"CADR",
	"CDDR",
	"CDAR",
	"CAAR",
	"ND1",
	"ND2",
	"ND3",
	"BRANCH",
	"MISC",
	"16 UNUSED",
	"17 UNUSED"
};

char *reg_names[] = {
	"FEF",
	"FEF+100",
	"FEF+200", 
	"FEF+300",
	"CONSTANTS PAGE", 
	"LOCAL BLOCK",
	"ARG POINTER",
	"PDL"
};

char *dest_names[] = {
	"IGNORE",
	"TO STACK",
	"TO NEXT",
	"TO LAST",
	"TO RETURN",
	"TO NEXT QUOTE=1",
	"TO LAST QUOTE=1",
	"TO NEXT LIST",
	"D-MICRO, POPJ",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal"
};

char *branch_names[] = {
	"BRALW",
	"branch-on-nil",
	"branch-on-not-nil",
	"branch-nil,pop-if-not",
	"branch-not-nil,pop-if",
	"branch-on-atom",
	"branch-on-non-atom",
	"illegal"
};

void
disass(unsigned int loc, int even, unsigned int inst)
{
	int op, dest, reg, delta;
	int to;
	unsigned int nlc;

	op = (inst >> 011) & 017;
	dest = (inst >> 015) & 07;
	reg = (inst >> 6) & 07;
	delta = (inst >> 0) & 077;
	printf("%011o%c %06o %s ", loc, even ? 'e':'o', inst, op_names[op]);

	switch (op) {
	case 0: /* call */
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);

//		nlc = (loc*2 + (even?0:1)) + delta;
//		printf("+%o; %o%c ",
//		       delta, nlc/2, (nlc & 1) ? 'o' : 'e');

		break;
	case 2: /* move */
	case 3:
	case 4:
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);
		break;
	case 014: /* branch */
		printf("type %s, ", branch_names[dest]);
		to = (inst & 03777) << 1;
		to |= (inst & 0x8000) ? 1 : 0;

		if (inst & 0400) {
			to = inst & 01777;
			to |= 03000;
			to |= ~01777;
		}

		nlc = (loc*2 + (even?0:1)) + to;

		if (to > 0) {
			printf("+%o; %o%c ",
			       to, nlc/2, (nlc & 1) ? 'o' : 'e');
		} else {
			printf("-%o; %o%c ",
			       -to, nlc/2, (nlc & 1) ? 'o' : 'e');
		}
		break;
	}

	printf("\n");
}

main(int argc, char *argv[])
{
	unsigned int com;
	int i, n;

	if (argc < 2) {
		exit(1);
	}

	/* raw load band file */
	lodfd = open(argv[1], O_RDONLY);
	if (lodfd < 0) {
		perror(argv[1]);
		exit(1);
	}

	/* optional full disk image (to check swap) */
	if (argc > 2) {
		swapfd = open(argv[2], O_RDONLY);
		if (swapfd < 0) {
			perror(argv[2]);
			exit(1);
		}
	}

	/* %SYS-COM-AREA-ORIGIN-PNTR */
	com = show(0400, 1);

#if 0
	for (i = 0; cv[i].name; i++) {
		printf("%s ", cv[i].name);
		cv[i].a = com+i;
		cv[i].v = show(cv[i].a, 0);
		printf("; ");
		show(cv[i].v, 1);
	}
	printf("\n");
#endif

#if 0
	printf("scratch-pad\n");
	for (i = 0; sv[i].name; i++) {
		printf("%s ", sv[i].name);
		sv[i].a = 01000+i;
		sv[i].v = show(sv[i].a, 0);
		printf("; ");
		show(sv[i].v, 1);
	}

	{
		unsigned int v, pc, n, o;
		int i, j;
		unsigned short ib[256];

		v = show(sv[0].a, 0);
		pc = show(v, 1);

		n = show(pc, 1);
		o = n & 0377;
		printf("offset %o\n", o);

		j = 0;
		for (i = 0; i < 64; i++) {
			unsigned int loc, inst;
			unsigned int a;
			loc = pc+i;
			inst = get(loc);

			ib[j++] = inst;
			ib[j++] = inst >> 16;

			if (i < o/2)
			{
				show(loc, 1);
			}

			if (i == 2) {
				printf(" "); v = show(inst, 0);
				printf(" "); showstr(v, 1);
			}
		}

		for (i = o; i < o+10; i++) {
			unsigned int loc;
			loc = pc+i/2;
			disass(loc, (i%2) ? 0 : 1, ib[i]);
		}
	}

	{
		unsigned int v, pc, n, o;
		int i, j;
		unsigned short ib[512];

		printf("\n");

		pc = show(01722706, 1);
		pc = show(pc, 1);

		n = show(pc, 1);
		o = n & 0777;
		printf("offset %o\n", o);

		j = 0;
		for (i = 0; i < 256; i++) {
			unsigned int loc, inst;
			loc = pc+i;
			inst = get(loc);

			ib[j++] = inst;
			ib[j++] = inst >> 16;

			if (i < o/2)
			{
				show(loc, 1);
			}

			if (i == 2) {
				printf(" "); v = show(inst, 0);
				printf(" "); showstr(v, 1);
			}
		}

		for (i = o; i < o+20; i++) {
			unsigned int loc;
			loc = pc+i/2;
			disass(loc, (i%2) ? 0 : 1, ib[i]);
		}
	}

#endif

#if 1
	{
		unsigned int v, pc, n, o;
		int i, j;
		unsigned short ib[512];

		printf("\n");

		sscanf(argv[3], "%o", &pc);
		pc >>= 2;
///		pc = 011010066774 >> 2;
//		pc = 011047720640 >> 2;

		for (i = 0; i < 512; i--) {
			int tag;
			n = show(pc, 1);
			tag = (n >> 24) & 077;
			if (tag == 7) break;
			pc--;
		}

		n = show(pc, 1);
		o = n & 0777;
		printf("offset %o\n", o);

		j = 0;
		for (i = 0; i < 256; i++) {
			unsigned int loc, inst;
			loc = pc+i;
			inst = get(loc);

			ib[j++] = inst;
			ib[j++] = inst >> 16;

			if (i < o/2)
			{
				show(loc, 1);
			}

			if (i == 2) {
				printf(" "); v = show(inst, 0);
				printf(" "); showstr(v, 1);
			}
		}

		for (i = o; i < o+30; i++) {
			unsigned int loc;
			loc = pc+i/2;
			disass(loc, (i%2) ? 0 : 1, ib[i]);
		}
	}
#endif

#if 0
	{
		int i;
		unsigned int a;

		printf("sg\n");
		a = sv[3].v & 0x00ffffff;
		printf("a %o\n", a);

		for (i = 10; i >= 0; i--) {
			char b[16];
			sprintf(b, "%d", -i);
			show(a-i, 1);
		}
	}
#endif

#if 0
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

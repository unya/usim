#include <stdio.h>
#include <fcntl.h>

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

int fd;
int bnum = -1;
unsigned int buf[256];

unsigned int
vr(int addr)
{
	int b;
	off_t offset, ret;

	addr &= 077777777;

	b = addr / 256;
//if (b != 1) b++;
//if (b != 1) b += 1;
//b += 1;
	offset = b * (256*4);

	if (b != bnum) {
		bnum = b;

		//printf("block %d(10)\n", b);

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
show(int a, int cr)
{
	unsigned int v;
	v = vr(a);
	printf("%011o %011o", a, v);
	if (cr) printf("\n");
	return v;
}

main()
{
	unsigned int com;
	int i, n;

	fd = open("partition.lod1.841", O_RDONLY);
//	fd = open("partition.lod2.841", O_RDONLY);

	/* %SYS-COM-AREA-ORIGIN-PNTR */
	com = show(0400, 1);

#if 0
	for (n = -01000; n <= 01000; n += 0400) {
		for (i = 0; i < 10; i++) {
			show(com+n+i, 1);
		}
	}
#endif

#if 1
	for (i = 0; cv[i].name; i++) {
		printf("%s ", cv[i].name);
		cv[i].a = com+i;
		cv[i].v = show(cv[i].a, 0);
		printf("; ");
		show(cv[i].v, 1);
	}
#endif

	show(01000, 1);
}

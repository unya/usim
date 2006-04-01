/*
 * decode.c
 *
 * disassemble CADR microcode
 * or at least, try to :-)
 *
 * $Id$
 */

#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>

#if defined(LINUX) || defined(OSX)
#include <unistd.h>
#endif

#include "ucode.h"

u_char prom[6][512];
ucw_t prom_ucode[512];

int
read_prom_files(void)
{
	int fd, i, ret;

	for (i = 0; i < 6; i++) {
		char name[256];

		sprintf(name, "cadr_%1d.bin", i+1);

		if (alt_prom_flag) {
			sprintf(name, "../prom/alt_cadr_%1d.bin", i+1);
		}

		printf("%s\n", name);
		fd = open(name, O_RDONLY | O_BINARY);
		if (fd < 0) {
			perror(name);
			exit(1);
		}

		ret = read(fd, prom[i], 512);
		close(fd);
				
		if (ret != 512) {
			fprintf(stderr, "read_prom_files: short read\n");
			exit(1);
		}
	}

	for (i = 0; i < 512; i++) {
		prom_ucode[511-i] =
			((uint64)prom[0][i] << (8*5)) |
			((uint64)prom[1][i] << (8*4)) |
			((uint64)prom[2][i] << (8*3)) |
			((uint64)prom[3][i] << (8*2)) |
			((uint64)prom[4][i] << (8*1)) |
			((uint64)prom[5][i] << (8*0));
	}

	return 0;
}

int
show_prom(void)
{
	int i;

	for (i = 0; i < 16; i++) {
		printf("%03o %016Lo", i, prom_ucode[i]);
		printf(" %02x %02x %02x %02x %02x %02x",
		       prom[0][i], 
		       prom[1][i], 
		       prom[2][i], 
		       prom[3][i], 
		       prom[4][i], 
		       prom[5][i]);
		printf("\n");
	}

	for (i = 0100; i < 0110; i++) {
		printf("%03o %016Lo", i, prom_ucode[i]);
		printf(" %02x %02x %02x %02x %02x %02x",
		       prom[0][i], 
		       prom[1][i], 
		       prom[2][i], 
		       prom[3][i], 
		       prom[4][i], 
		       prom[5][i]);
		printf("\n");
	}

	printf("----\n");
	return 0;
}

#define l48(n)		((uint64)(n))
#define mask(v, n)	(l48(v) << (n))
#define bit(n)		(l48(1) << (n))

char *alu_bool_op[] = {
	"SETZ",
	"AND",
	"ANDCA",
	"SETM",
	"ANDCM",
	"SETA",
	"XOR",
	"IOR",
	"ANDCB",
	"EQV",
	"SETCA",
	"ORCA",
	"SETCM",
	"ORCM",
	"ORCB",
	"SETO"
};

char *alu_arith_op[] = {
	"-1",
	"(M&A)-1",
	"(M&~A)-1",
	"M-1",
	"M|~A",
	"(M|~A)+(M&A)",
	"M-A-1 [M-A-1]",
	"(M|~A)+M",
	"M|A",
	"M+A [ADD]",
	"(M|A)+(M&~A)",
	"(M|A)+M",
	"M",
	"M+(M&A)",
	"M+(M|~A)",
	"M+M"
};

void
disassemble_m_src(ucw_t u, int m_src)
{
	if (m_src & 040) {
		switch (m_src & 037) {
		case 0:
			printf("dispatch-constant "); break;
		case 1:
			printf("SPC-ptr, spc-data ");
			break;
		case 2:
			printf("PDL-ptr %o ", (int)u & 01777);
			break;
		case 3:
			printf("PDL-index %o ", (int)u & 01777);
			break;
		case 5:
			printf("PDL-buffer ");
			break;
		case 6:
			printf("OPC register %o ",
			       (int)u & 017777);
			break;
		case 7:
			printf("Q ");
			break;
		case 010:
			printf("VMA ");
			break;
		case 011:
			printf("MAP[MD] ");
			break;
		case 012:
			printf("MD ");
			break; 
		case 013:
			printf("LC ");
			break; 
		case 014:
			printf("SPC pointer and data, pop ");
			break; 
		case 024:
			printf("PDL[Pointer], pop ");
			break;
		case 025:
			printf("PDL[Pointer] ");
			break; 
		}
	} else {
		printf("m[%o] ", m_src);
	}
}

void
disassemble_dest(int dest)
{
	if (dest & 04000) {
		printf("->a_mem[%o] ", dest & 01777);
	} else {
		switch (dest >> 5) {
		case 0: printf("-><none>"); break;
		case 1: printf("->LC "); break;
		case 2: printf("->IC "); break;
		case 010: printf("->PDL[ptr] "); break;
		case 011: printf("->PDL[ptr],push "); break;
		case 012: printf("->PDL[index] "); break;
		case 013: printf("->PDL index "); break;
		case 014: printf("->PDL ptr "); break;

		case 015: printf("->SPC data,push "); break;

		case 016: printf("->OA-reg-lo "); break;
		case 017: printf("->OA-reg-hi "); break;

		case 020: printf("->VMA "); break;
		case 021: printf("->VMA,start-read "); break;
		case 022: printf("->VMA,start-write "); break;
		case 023: printf("->VMA,write-map "); break;

		case 030: printf("->MD "); break;
		case 031: printf("->MD,start-read "); break;
		case 032: printf("->MD,start-write "); break;
		case 033: printf("->MD,write-map "); break;
		}

		printf(",m[%o] ", dest & 037);
	}
}

void
disassemble_ucode_loc(int loc, ucw_t u)
{
	int a_src, m_src, new_pc, dest, alu_op;
	int r_bit, p_bit, n_bit, ir8, ir7;
	int widthm1, pos;
	int mr_sr_bits;
	int jump_op;

	int disp_cont, disp_addr;
	int map, len, rot;
	int out_bus;

	if ((u >> 42) & 1)
		printf("popj; ");

	switch ((u >> 43) & 03) {
	case 0: /* alu */
		printf("(alu) ");

		if ((u & NOP_MASK) == 0) {
			printf("no-op");
			goto done;
		}

		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;
		dest = (u >> 14) & 07777;
		out_bus = (u >> 12) & 3;
		ir8 = (u >> 8) & 1;
		ir7 = (u >> 7) & 1;

		alu_op = (u >> 3) & 017;
		if (ir8 == 0) {
			if (ir7 == 0) {
				printf("%s ", alu_bool_op[alu_op]);
			} else {
				printf("%s ", alu_arith_op[alu_op]);
			}
		} else {
			switch (alu_op) {
			case 0: printf("mult-step "); break;
			case 1: printf("div-step "); break;
			case 5: printf("rem-corr "); break;
			case 011: printf("init-div-step "); break;
			}
		}

		printf("a=%o m=%o ", a_src, m_src);
		disassemble_m_src(u, m_src);

		if ((u >> 2) & 1)
			printf("C=1 ");
		else
			printf("C=0 ");

		switch (out_bus) {
		case 1: printf("alu-> "); break;
		case 2: printf("alu>>+s "); break;
		case 3: printf("alu<<+q31 "); break;
		}

		switch (u & 3) {
		case 1: printf("<<Q "); break;
		case 2: printf(">>Q "); break;
		case 3: printf("Q-R "); break;
		}

		disassemble_dest(dest);
		break;
	case 1: /* jump */
		printf("(jump) ");

		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;
		new_pc = (u >> 12) & 037777;

		jump_op = (u >> 14) & 3;

		printf("a=%o m=", a_src);
		disassemble_m_src(u, m_src);

		r_bit = (u >> 9) & 1;
		p_bit = (u >> 8) & 1;
		n_bit = (u >> 7) & 1;

		printf("pc %o, %s%s",
		       new_pc,
		       r_bit ? "R " : "",
		       p_bit ? "P " : "");

		if (n_bit)
			/* INHIBIT-XCT-NEXT */
			printf("!next ");
		if (u & (1<<6))
			/* INVERT-JUMP-SENSE */
			printf("!jump ");

		if (u & (1<<5)) {
			switch (u & 017) {
			case 0:
			case 1: printf("M-src < A-src "); break;
			case 2: printf("M-src <= A-src "); break;
			case 3: printf("M-src = A-src "); break;
			case 4: printf("pf "); break;
			case 5: printf("pf/int "); break;
			case 6: printf("pf/int/seq "); break;
			case 7:
				printf("jump-always "); break;
			}
		} else {
			printf("m-rot<< %o", (int)u & 037);
		}

/*
  switch (jump_op) {
  case 0: printf("jump-xct-next "); break;
  case 1: printf("jump "); break;
  case 2: printf("call-xct-next "); break;
  case 3: printf("call "); break;
  }
*/
		break;
	case 2: /* dispatch */
		printf("(dispatch) ");

		disp_cont = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;

		if ((u >> 25) & 1) printf("!N+1 ");
		if ((u >> 24) & 1) printf("ISH ");
		disp_addr = (u >> 12) & 03777;
		map = (u >> 8) & 3;
		len = (u >> 5) & 07;
		rot = u & 037;

		printf("m=%o ", m_src);
		disassemble_m_src(u, m_src);

		printf("disp-const %o, disp-addr %o, map %o, len %o, rot %o ",
		       disp_cont, disp_addr, map, len, rot);
		break;
	case 3: /* byte */
		printf("(byte) ");

		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;
		dest = (u >> 14) & 07777;
		mr_sr_bits = (u >> 12) & 3;

		widthm1 = (u >> 5) & 037;
		pos = u & 037;

		printf("a=%o m=", a_src);
		disassemble_m_src(u, m_src);

		switch (mr_sr_bits) {
		case 0:
			break;
		case 1: /* ldb */
			printf("ldb pos=%o, width=%o ",
			       pos, widthm1+1);
			break;
		case 2:
			printf("sel dep (a<-m&mask) pos=%o, width=%o ",
			       pos, widthm1+1);
			break;
		case 3: /* dpb */
			printf("dpb pos=%o, width=%o ",
			       pos, widthm1+1);
			break;
		}

		disassemble_dest(dest);
		break;
	}

 done:
	printf("\n");
}

void
disassemble_prom(void)
{
	int i, start, finish; 

#if 0
	start = 0100;
	finish = 0110;
#else
	start = 0;
	finish = 512;
#endif

	for (i = start; i < finish; i++) {

		ucw_t u = prom_ucode[i];

		printf("%03o %016Lo ", i, prom_ucode[i]);

		disassemble_ucode_loc(i, u);
	}
}

/* ----------------------------------------------------------------- */

/* see diskmaker.c */
//static int partoff = 046324;
//static int partoff = 0114124;
static int partoff;
static int bnum = -1;
static unsigned int buf[256];

extern int disk_fd;

static unsigned long
str4(char *s)
{
	return (s[3]<<24) | (s[2]<<16) | (s[1]<<8) | s[0];
}


static int
find_disk_partition_table(int fd)
{
	int ret;
	int p, count, nw, i;
	off_t offset;

	printf("looking for partition\n");

	offset = 0;
	ret = lseek(fd, offset, SEEK_SET);
	if (ret != offset) {
		return -1;
	}

	ret = read(fd, buf, 256*4);
	if (ret != 256*4) {
		return -1;
	}

	/* this is so much prettier in lisp... */
	p = 0200;
	count = buf[p++];
	nw = buf[p++];
	if (0) printf("count %d, nw %d\n", count, nw);

	for (i = 0; i < count; i++) {
		if (0) printf("%d %08x %08x\n", i, buf[p], (int)str4("LOD1"));
		if (buf[p] == str4("LOD1")) {
			partoff = buf[p+1];
			if (0) printf("found lod1 %o\n", partoff);
			break;
		}
		p += nw;
	}

	bnum = -1;
	return 0;
}


static unsigned int
read_virt(int fd, int addr, unsigned int *pv)
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
			return -1;
		}

		ret = read(fd, buf, 256*4);
		if (ret != 256*4) {
			return -1;
		}
	}

	*pv = buf[ addr % 256 ];
	return 0;
}

static char *
read_string(unsigned int loc)
{
	unsigned int v;
	int t, i, j;
	static char s[256];

	if (read_virt(disk_fd, loc, &v) == 0) {

		if (0) printf("%o len %o\n", loc, v);

		t = v & 0xff;
		j = 0;
		for (i = 0; i < t; i += 4) {
			unsigned int l;

			l = loc+1+(i/4);
			if (read_virt(disk_fd, l, &v))
				return "<read-failed>";

			if (0) printf("%o %o %08x\n", l, v, v);

			s[j++] = v >> 0;
			s[j++] = v >> 8;
			s[j++] = v >> 16;
			s[j++] = v >> 24;
		}

		s[t] = 0;
		return s;
	}

	return (char *)0;
}

static char *
show_string(unsigned int loc)
{
	char *s;

	s = read_string(loc);
	printf(" '%s' ", s);
	return s;
}

/*
 * a complete hack; given a pc, find the function header and print
 * the function name.  helpful for debugging.
 */
char *
find_function_name(int the_lc)
{
	int i, tag = 0;
	unsigned int loc = the_lc >> 2;
	unsigned int v;

	if (0) printf("find %o\n", loc);

	if (partoff == 0) {
		find_disk_partition_table(disk_fd);
	}

	/* search backward to find the function header */
	for (i = 0; i < 512; i++) {

		if (read_virt(disk_fd, loc, &v))
			break;

		tag = (v >> 24) & 077;
		if (tag == 7) break;
		loc--;
	}

	if (0) printf("%o found header, back %d\n", loc, i);

	if (tag == 7) {
		/* find function symbol ptr */
		if (read_virt(disk_fd, loc+2, &v) == 0) {

			if (0) printf("%o ptr %o\n", loc, v);

			loc = v;
			tag = (v >> 24) & 077;

			if (read_virt(disk_fd, loc, &v))
				return "<read-failed>";

			/* hack - it's a list */
			if (tag == 016) {
				if (read_virt(disk_fd, v, &v))
					return "<read-failed>";
			}

			return read_string(v);
		}
	}

	return "<no-function>";
}

void
show_list(unsigned int lp)
{
	unsigned int loc, l1, v;
	int i;

	loc = 031602653046;
	loc = 001614546634;
	loc = 030301442405;
	read_virt(disk_fd, loc, &v);
//	printf("%011o %011o\n", loc, v);
//	loc = v;

	for (i = 0; i < 10; i++) {
		int tag, cdr, addr;

		read_virt(disk_fd, loc, &v);

		tag = (v >> 24) & 077;
		cdr = (v >> 30) & 3;
		addr = v & 0x00ffffff;

		printf(" %011o %011o %03o %1o\n", loc, v, tag, cdr);

		if (cdr == 2)
			i = 100;

		if (addr != 0)
		switch (tag) {
		case 003:
			l1 = v;
			read_virt(disk_fd, l1, &v);
			tag = (v >> 24) & 077;
			cdr = (v >> 30) & 3;
			printf("  %011o %011o %03o %1o\n", l1, v, tag, cdr);

			l1 = v;
			read_virt(disk_fd, l1, &v);
			tag = (v >> 24) & 077;
			cdr = (v >> 30) & 3;
			printf("   %011o %011o %03o %1o\n", l1, v, tag, cdr);

			show_string(l1);
			printf("\n");
			break;
		case 004:
			l1 = v;
			read_virt(disk_fd, l1, &v);
			tag = (v >> 24) & 077;
			cdr = (v >> 30) & 3;
			printf("  %011o %011o %03o %1o\n", l1, v, tag, cdr);

			show_string(l1);
			printf("\n");
			break;

		case 016:
//			loc = v;
			break;
		case 021:
			l1 = v;
			read_virt(disk_fd, l1, &v);
			tag = (v >> 24) & 077;
			cdr = (v >> 30) & 3;
			printf("  %011o %011o %03o %1o\n", l1, v, tag, cdr);

			show_string(l1);
			printf("\n");
#if 0
			l1 = v;
			read_virt(disk_fd, l1, &v);
			tag = (v >> 24) & 077;
			cdr = (v >> 30) & 3;
			printf("   %011o %011o %03o %1o\n", l1, v, tag, cdr);

			l1++;
			read_virt(disk_fd, l1, &v);
			tag = (v >> 24) & 077;
			cdr = (v >> 30) & 3;
			printf("   %011o %011o %03o %1o\n", l1, v, tag, cdr);

			l1++;
			read_virt(disk_fd, l1, &v);
			tag = (v >> 24) & 077;
			cdr = (v >> 30) & 3;
			printf("   %011o %011o %03o %1o\n", l1, v, tag, cdr);
#endif
			break;
		default:
//			i = 100;
			break;
		}

		loc++;
	}
}

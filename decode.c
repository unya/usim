#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include <unistd.h>

#include "ucode.h"
#include "decode.h"
#include "syms.h"

/*---!!! Should be detected, or supplied via command line. */
int needswap = 1;

ucw_t prom_ucode[512];

/*---!!! read16 and read32 are from readmcr; should be shared. */
unsigned int
read16(int fd)
{
	int fd, i;
	ssize_t ret;

	ret = read(fd, b, 2);
	if (ret < 2) {
		printf("eof!\n");
		exit(1);
	}

	if (needswap)
		return (b[1] << 8) | b[0];

	return (b[0] << 8) | b[1];
}

unsigned int
read32(int fd)
{
	unsigned char b[4];
	int ret;

		ret = read(fd, prom[i], 512);
		close(fd);

		if (ret != 512) {
			fprintf(stderr, "read_prom_files: short read\n");
			exit(1);
		}
	}

	if (needswap)
		return (b[1] << 24) | (b[0] << 16) | (b[3] << 8) | b[2];

	return (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
//	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

int
read_prom_files(void)
{

	char *name = "../../lisp/ubin/promh.mcr";
	int fd;
	unsigned int code, start, size;

	if (alt_prom_flag) {
		sprintf(name, "./promh.mcr");
 	}

	fd = open(name, O_RDONLY | O_BINARY);
	if (fd < 0) {
		perror(name);
		exit(1);
	}

	code = read32(fd);
	start = read32(fd);
	size = read32(fd);

	printf ("prom (%s): code: %d, start: %d, size: %d\n", name, code, start, size);

	int loc = start;
	for (int i = 0; i < size; i++) {
		unsigned int w1, w2, w3, w4;
		w1 = read16(fd);
		w2 = read16(fd);
		w3 = read16(fd);
		w4 = read16(fd);
		prom_ucode[loc] = ((unsigned long long) w1 << 48) |
			((unsigned long long) w2 << 32) |
			((unsigned long long) w3 << 16) |
			((unsigned long long) w4 << 0);
		loc++;
 	}

	return 0;
}

int
show_prom(void)
{
	int i;

	for (i = 0; i < 16; i++) {
		printf("%03o %016llo", i, prom_ucode[i]);
		printf(" %02x %02x %02x %02x %02x %02x",
		       prom[0][i],
		       prom[1][i],
		       prom[2][i],
		       prom[3][i],
		       prom[4][i],
		       prom[5][i]);
		printf("\n");
	}

	for (i = 0; i < 512; i++) {
		printf("%03o %016llo", i, prom_ucode[i]);
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
	unsigned int i, start, finish;

#if 0
	start = 0100;
	finish = 0110;
#else
	start = 0;
	finish = 512;
#endif

	for (i = start; i < finish; i++) {

		ucw_t u = prom_ucode[i];

		printf("%03o %016llo ", i, prom_ucode[i]);

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

static unsigned int
str4(char *s)
{
	return (unsigned int)((s[3]<<24) | (s[2]<<16) | (s[1]<<8) | s[0]);
}


static int
find_disk_partition_table(int fd)
{
	off_t ret;
	unsigned int p, count, nw, i;
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
			partoff = (int)buf[p+1];
			if (0) printf("found lod1 %o\n", partoff);
			break;
		}
		p += nw;
	}

	bnum = -1;
	return 0;
}


static unsigned int
read_virt(int fd, unsigned int addr, unsigned int *pv)
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
			return 1;
		}

		ret = read(fd, buf, 256*4);
		if (ret != 256*4) {
			return 1;
		}
	}

	*pv = buf[ addr % 256 ];
	return 0;
}

static char *
read_string(unsigned int loc)
{
	unsigned int v;
	unsigned int t, i, j;
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

			s[j++] = (char)(v >> 0);
			s[j++] = (char)(v >> 8);
			s[j++] = (char)(v >> 16);
			s[j++] = (char)(v >> 24);
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
find_function_name(unsigned int the_lc)
{
	int i, tag = 0;
	unsigned int loc = (unsigned int)(the_lc >> 2);
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

	//	loc = 031602653046;
	//	loc = 001614546634;
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

static void showstr(char *buffer, unsigned int a, int cr);
static void show_fef_func_name(char *buffer, unsigned int fefptr, unsigned int width);
static unsigned int get(unsigned int a);

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
	"LOCAL",
	"ARG POINTER",
	"PDL"
};

char *dest_names[] = {
	"D-IGNORE",
	"D-PDL",
	"D-NEXT",
	"D-LAST",
	"D-RETURN",
	"D-NEXT-Q",
	"D-LAST-Q",
	"D-NEXT-LIST",
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
	"BR",
	"BR-NIL",
	"BR-NOT-NIL",
	"BR-NIL-POP",
	"BR-NOT-NIL-POP",
	"BR-ATOM",
	"BR-NOT-ATOM",
	"BR-ILL-7"
};

char *nd1_names[] = {
	"ND1-UNUSED",
	"+",
	"-",
	"*",
	"/",
	"LOGAND",
	"LOGXOR",
	"LOGIOR"
};

char *nd2_names[] = {
	"=",
	">",
	"<",
	"EQ",
	"SETE-CDR",
	"SETE-CDDR",
	"SETE-1+",
	"SETE-1-"
};

char *nd3_names[] = {
	"BIND-OBSOLETE?",
	"BIND-NIL",
	"BIND-POP",
	"SET-NIL",
	"SET-ZERO",
	"PUSH-E",
	"MOVEM",
	"POP"
};

struct {
	char *name;
	int value;
} misc_inst[] = {
	{ "", 0 },
	{ "(CAR . M-CAR)", 0242 },
	{ "(CDR . M-CDR)", 0243 },
	{ "(CAAR . M-CAAR)", 0244 },
	{ "(CADR . M-CADR)", 0245 },
	{ "(CDAR . M-CDAR)", 0246 },
	{ "(CDDR . M-CDDR)", 0247 },
	{ "CAAAR", 0250 },
	{ "CAADR", 0251 },
	{ "CADAR", 0252 },
	{ "CADDR", 0253 },
	{ "CDAAR", 0254 },
	{ "CDADR", 0255 },
	{ "CDDAR", 0256 },
	{ "CDDDR", 0257 },
	{ "CAAAAR", 0260 },
	{ "CAAADR", 0261 },
	{ "CAADAR", 0262 },
	{ "CAADDR", 0263 },
	{ "CADAAR", 0264 },
	{ "CADADR", 0265 },
	{ "CADDAR", 0266 },
	{ "CADDDR", 0267 },
	{ "CDAAAR", 0270 },
	{ "CDAADR", 0271 },
	{ "CDADAR", 0272 },
	{ "CDADDR", 0273 },
	{ "CDDAAR", 0274 },
	{ "CDDADR", 0275 },
	{ "CDDDAR", 0276 },
	{ "CDDDDR", 0277 },
	{ "%LOAD-FROM-HIGHER-CONTEXT", 0300 },
	{ "%LOCATE-IN-HIGHER-CONTEXT", 0301 },
	{ "%STORE-IN-HIGHER-CONTEXT", 0302 },
	{ "%DATA-TYPE", 0303 },
	{ "%POINTER", 0304 },
	/* 305-307 FREE */
	{ "%MAKE-POINTER", 0310 },
	{ "%SPREAD", 0311 },
	{ "%P-STORE-CONTENTS", 0312 },
	{ "%LOGLDB", 0313 },
	{ "%LOGDPB", 0314 },
	{ "LDB", 0315 },
	{ "DPB", 0316 },
	{ "%P-STORE-TAG-AND-POINTER", 0317 },
	{ "GET", 0320 },
	{ "GETL", 0321 },
	{ "ASSQ", 0322 },
	{ "LAST", 0323 },
	{ "LENGTH", 0324 },
	{ "1+", 0325 },
	{ "1-", 0326 },
	{ "RPLACA", 0327 },
	{ "RPLACD", 0330 },
	{ "ZEROP", 0331 },
	{ "SET", 0332 },
	{ "FIXP", 0333 },
	{ "FLOATP", 0334 },
	{ "EQUAL", 0335 },
	{ "STORE", 0336 },
	{ "XSTORE", 0337 },
	{ "FALSE", 0340 },
	{ "TRUE", 0341 },
	{ "NOT", 0342 },
	{ "(NULL . NOT)", 0342 },
	{ "ATOM", 0343 },
	{ "ODDP", 0344 },
	{ "EVENP", 0345 },
	{ "%HALT", 0346 },
	{ "GET-PNAME", 0347 },
	{ "LSH", 0350 },
	{ "ROT", 0351 },
	{ "*BOOLE", 0352 },
	{ "NUMBERP", 0353 },
	{ "PLUSP", 0354 },
	{ "MINUSP", 0355 },
	{ "\\", 0356 },
	{ "MINUS", 0357 },
	{ "PRINT-NAME-CELL-LOCATION", 0360 },
	{ "VALUE-CELL-LOCATION", 0361 },
	{ "FUNCTION-CELL-LOCATION", 0362 },
	{ "PROPERTY-CELL-LOCATION", 0363 },
	{ "NCONS", 0364 },
	{ "NCONS-IN-AREA", 0365 },
	{ "CONS", 0366 },
	{ "CONS-IN-AREA", 0367 },
	{ "XCONS", 0370 },
	{ "XCONS-IN-AREA", 0371 },
	{ "%SPREAD-N", 0372 },
	{ "SYMEVAL", 0373 },
	{ "POP-M-FROM-UNDER-N", 0374 },
	{ "%OLD-MAKE-LIST", 0375 },
	{ "%CALL-MULT-VALUE", 0376 },
	{ "%CALL0-MULT-VALUE", 0377 },
	{ "%RETURN-2", 0400 },
	{ "%RETURN-3", 0401 },
	{ "%RETURN-N", 0402 },
	{ "RETURN-NEXT-VALUE", 0403 },
	{ "RETURN-LIST", 0404 },
	{ "UNBIND-TO-INDEX-UNDER-N", 0405 },
	{ "BIND", 0406 },
	{ "%MAKE-LEXICAL-CLOSURE", 0407 },
	{ "MEMQ", 0410 },
	{ "(INTERNAL-< . M-<)", 0411 },
	{ "(INTERNAL-> . M->)", 0412 },
	{ "(= . M-=)", 0413 },
	{ "CHAR-EQUAL", 0414 },
	{ "%STRING-SEARCH-CHAR", 0415 },
	{ "%STRING-EQUAL", 0416 },
	{ "NTH", 0417 },
	{ "NTHCDR", 0420 },
	{ "(*PLUS . M-+)", 0421  },
	{ "(*DIF . M--)", 0422  },
	{ "(*TIMES . M-*)", 0423  },
	{ "(*QUO . M-//)", 0424  },
	{ "(*LOGAND . M-LOGAND)", 0425  },
	{ "(*LOGXOR . M-LOGXOR)", 0426  },
	{ "(*LOGIOR . M-LOGIOR)", 0427  },
	{ "ARRAY-LEADER", 0430 },
	{ "STORE-ARRAY-LEADER", 0431 },
	{ "GET-LIST-POINTER-INTO-ARRAY", 0432 },
	{ "ARRAY-PUSH", 0433 },
	{ "APPLY", 0434 },
	{ "%MAKE-LIST", 0435 },
	{ "LIST", 0436 },
	{ "LIST*", 0437 },
	{ "LIST-IN-AREA", 0440 },
	{ "LIST*-IN-AREA", 0441 },
	{ "%P-FLAG-BIT", 0442 },
	{ "%P-CDR-CODE", 0443 },
	{ "%P-DATA-TYPE", 0444 },
	{ "%P-POINTER", 0445 },
	{ "%PAGE-TRACE", 0446 },
	{ "%P-STORE-FLAG-BIT", 0447 },
	{ "%P-STORE-CDR-CODE", 0450 },
	{ "%P-STORE-DATA-TYPE", 0451 },
	{ "%P-STORE-POINTER", 0452 },
	/* 453-455 FREE */
	{ "%CATCH-OPEN", 0456 },
	{ "%CATCH-OPEN-MV", 0457 },
	/* 461, 0462 FREE */
	{ "%FEXPR-CALL", 0462 },
	{ "%FEXPR-CALL-MV", 0463 },
	{ "%LEXPR-CALL", 0464 },
	{ "%LEXPR-CALL-MV", 0465 },
	{ "*CATCH", 0466 },
	{ "%BLT", 0467 },
	{ "*THROW", 0470 },
	{ "%XBUS-WRITE-SYNC", 0471 },
	{ "%P-LDB", 0472 },
	{ "%P-DPB", 0473 },
	{ "MASK-FIELD", 0474 },
	{ "%P-MASK-FIELD", 0475},
	{ "DEPOSIT-FIELD", 0476 },
	{ "%P-DEPOSIT-FIELD", 0477 },
	{ "COPY-ARRAY-CONTENTS", 0500 },
	{ "COPY-ARRAY-CONTENTS-AND-LEADER", 0501 },
	{ "%FUNCTION-INSIDE-SELF", 0502 },
	{ "ARRAY-HAS-LEADER-P", 0503 },
	{ "COPY-ARRAY-PORTION", 0504 },
	{ "FIND-POSITION-IN-LIST", 0505 },
	{ "FIND-POSITION-IN-LIST-EQUAL", 0506 },
	{ "G-L-P", 0507 },
	{ "FIND-POSITION-IN-VECTOR", 0510 },
	{ "FIND-POSITION-IN-VECTOR-EQUAL", 0511 },
	{ "AR-1", 0512 },
	{ "AR-2", 0513 },
	{ "AR-3", 0514 },
	{ "AS-1", 0515 },
	{ "AS-2", 0516 },
	{ "AS-3", 0517 },
	{ "%INSTANCE-REF", 0520 },
	{ "%INSTANCE-LOC", 0521 },
	{ "%INSTANCE-SET", 0522 },
	{ "%BINDING-INSTANCES", 0523 },
	{ "%INTERNAL-VALUE-CELL", 0524 },
	{ "%USING-BINDING-INSTANCES", 0525 },
	{ "%GC-CONS-WORK", 0526 },
	{ "%P-CONTENTS-OFFSET", 0527 },
	{ "%DISK-RESTORE", 0530 },
	{ "%DISK-SAVE", 0531 },
	{ "%ARGS-INFO", 0532 },
	{ "%OPEN-CALL-BLOCK", 0533 },
	{ "%PUSH", 0534 },
	{ "%ACTIVATE-OPEN-CALL-BLOCK", 0535 },
	{ "%ASSURE-PDL-ROOM", 0536 },
	{ "STACK-GROUP-RETURN", 0537 },
	{ "%STACK-GROUP-RETURN-MULTI", 0540 },
	{ "%MAKE-STACK-LIST", 0541 },
	{ "STACK-GROUP-RESUME", 0542 },
	{ "%CALL-MULT-VALUE-LIST", 0543 },
	{ "%CALL0-MULT-VALUE-LIST", 0544 },
	{ "%GC-SCAV-RESET", 0545 },
	{ "%P-STORE-CONTENTS-OFFSET", 0546 },
	{ "%GC-FREE-REGION", 0547 },
	{ "%GC-FLIP", 0550 },
	{ "ARRAY-LENGTH", 0551 },
	{ "ARRAY-ACTIVE-LENGTH", 0552 },
	{ "%COMPUTE-PAGE-HASH", 0553 },
	{ "GET-LOCATIVE-POINTER-INTO-ARRAY", 0554 },
	{ "%UNIBUS-READ", 0555 },
	{ "%UNIBUS-WRITE", 0556 },
	{ "%GC-SCAVENGE", 0557 },
	{ "%CHAOS-WAKEUP", 0560 },
	{ "%AREA-NUMBER", 0561 },
	{ "*MAX", 0562 },
	{ "*MIN", 0563 },
	{ "CLOSURE", 0565 },
	{ "DOWNWARD-CLOSURE", 0566 },
	{ "LISTP", 0567 },
	{ "NLISTP", 0570 },
	{ "SYMBOLP", 0571 },
	{ "NSYMBOLP", 0572 },
	{ "ARRAYP", 0573 },
	{ "FBOUNDP", 0574 },
	{ "STRINGP", 0575 },
	{ "BOUNDP", 0576 },
	{ "INTERNAL-\\", 0577 },
	{ "FSYMEVAL", 0600 },
	{ "AP-1", 0601 },
	{ "AP-2", 0602 },
	{ "AP-3", 0603 },
	{ "AP-LEADER", 0604 },
	{ "%P-LDB-OFFSET", 0605 },
	{ "%P-DPB-OFFSET", 0606 },
	{ "%P-MASK-FIELD-OFFSET", 0607 },
	{ "%P-DEPOSIT-FIELD-OFFSET", 0610 },
	{ "%MULTIPLY-FRACTIONS", 0611 },
	{ "%DIVIDE-DOUBLE", 0612 },
	{ "%REMAINDER-DOUBLE", 0613 },
	{ "HAULONG", 0614 },
	{ "%ALLOCATE-AND-INITIALIZE", 0615 },
	{ "%ALLOCATE-AND-INITIALIZE-ARRAY", 0616 },
	{ "%MAKE-POINTER-OFFSET", 0617 },
	{ "^", 0620 },
	{ "%CHANGE-PAGE-STATUS", 0621 },
	{ "%CREATE-PHYSICAL-PAGE", 0622 },
	{ "%DELETE-PHYSICAL-PAGE", 0623 },
	{ "%24-BIT-PLUS", 0624 },
	{ "%24-BIT-DIFFERENCE", 0625 },
	{ "%24-BIT-TIMES", 0626 },
	{ "ABS", 0627 },
	{ "%POINTER-DIFFERENCE", 0630 },
	{ "%P-CONTENTS-AS-LOCATIVE", 0631 },
	{ "%P-CONTENTS-AS-LOCATIVE-OFFSET", 0632 },
	{ "(EQ . M-EQ)", 0633 },
	{ "%STORE-CONDITIONAL", 0634 },
	{ "%STACK-FRAME-POINTER", 0635 },
	{ "*UNWIND-STACK", 0636 },
	{ "%XBUS-READ", 0637 },
	{ "%XBUS-WRITE", 0640 },
	{ "PACKAGE-CELL-LOCATION", 0641 },
	{ "MOVE-PDL-TOP", 0642 },
	{ "SHRINK-PDL-SAVE-TOP", 0643 },
	{ "SPECIAL-PDL-INDEX", 0644 },
	{ "UNBIND-TO-INDEX", 0645 },
	{ "UNBIND-TO-INDEX-MOVE", 0646 },
	{ "FIX", 0647 },
	{ "FLOAT", 0650 },
	{ "SMALL-FLOAT", 0651 },
	{ "%FLOAT-DOUBLE", 0652 },
	{ "BIGNUM-TO-ARRAY", 0653 },
	{ "ARRAY-TO-BIGNUM", 0654 },
	{ "%UNWIND-PROTECT-CONTINUE", 0655 },
	{ "%WRITE-INTERNAL-PROCESSOR-MEMORIES", 0656 },
	{ "%PAGE-STATUS", 0657 },
	{ "%REGION-NUMBER", 0660 },
	{ "%FIND-STRUCTURE-HEADER", 0661 },
	{ "%STRUCTURE-BOXED-SIZE", 0662 },
	{ "%STRUCTURE-TOTAL-SIZE", 0663 },
	{ "%MAKE-REGION", 0664 },
	{ "BITBLT", 0665 },
	{ "%DISK-OP", 0666 },
	{ "%PHYSICAL-ADDRESS", 0667 },
	{ "POP-OPEN-CALL", 0670 },
	{ "%BEEP", 0671 },
	{ "%FIND-STRUCTURE-LEADER", 0672 },
	{ "BPT", 0673 },
	{ "%FINDCORE", 0674 },
	{ "%PAGE-IN", 0675 },
	{ "ASH", 0676 },
	{ "%MAKE-EXPLICIT-STACK-LIST", 0677 },
	{ "%DRAW-CHAR", 0700 },
	{ "%DRAW-RECTANGLE", 0701 },
	{ "%DRAW-LINE", 0702 },
	{ "%DRAW-TRIANGLE", 0703 },
	{ "%COLOR-TRANSFORM", 0704 },
	{ "%RECORD-EVENT", 0705 },
	{ "%AOS-TRIANGLE", 0706 },
	{ "%SET-MOUSE-SCREEN", 0707  },
	{ "%OPEN-MOUSE-CURSOR", 0710 },
	{ "%ether-wakeup", 0711 },
	{ "%checksum-pup", 0712 },
	{ "%decode-pup", 0713 },
	{ (char *)0, 0 }
};

char *call_names[] = { "CALL", "CALL0", "MOVE", "CAR", "CDR", "CADR", "CDDR", "CDAR", "CAAR" };


static int misc_inst_vector[1024];
static int misc_inst_vector_setup;

char *disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst, unsigned int width);
extern int read_mem(int vaddr, unsigned int *pv);

static char *
disassemble_address(unsigned int fef, unsigned int reg, unsigned int delta)
{
	static char addr[256];

	if (reg < 4)
	{
		sprintf(addr, "FEF|%o;", delta);
	}
	else if (reg == 4)
	{
		static unsigned int constants_area;
		static int done = 0;

		if (!done)
		{
			int i;

			for (i = 0; i < 1024; i++) {
				char *sym;
				extern unsigned int a_memory[1024];

				sym = sym_find_by_type_val(1, 4/*A-MEM*/, i);
				if (sym && strcasecmp(sym, "A-V-CONSTANTS-AREA") == 0) {
					read_mem(a_memory[i], &constants_area);
					printf("found %o %-40s %o\n",
					       i, sym, a_memory[i]);
				}
			}
			done = 1;
		}

		unsigned int value;

		read_mem(constants_area + (delta & 077), &value);
		sprintf(addr, "/%o", value);
	}
	else if (delta == 0777)
	{
		sprintf(addr, "PDL-POP");
	}
	else if (reg == 5)
	{
		sprintf(addr, "LOCAL|%o", delta & 077);
	}
	else if (reg == 6)
	{
		sprintf(addr, "ARG|%o", delta & 077);
	}
	else
	{
		sprintf(addr, "PDL|%o", delta & 077);
	}

	return addr;
}

char *
disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst, unsigned int width)
{
	unsigned int op, dest, reg, delta, adr;
	int to;
	unsigned int nlc;
	static char buffer[1024];

	// search for fefptr
	{
		int i, tag = 0;
		unsigned int addr = (unsigned int)(loc >> 2);
		unsigned int v;

		if (0) printf("find %o\n", addr);

		if (partoff == 0) {
			find_disk_partition_table(disk_fd);
		}

		/* search backward to find the function header */
		for (i = 0; i < 512; i++) {

			if (read_virt(disk_fd, addr, &v))
				break;

			tag = (v >> width) & 037;
			if (tag == 7) break;
			addr--;
		}

		fefptr = addr;
	}

	if (!misc_inst_vector_setup) {
		int i, index;
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
	delta = (inst >> 0) & 0777;
	//	sprintf(buffer, "%011o%c %06o %s ", loc, even ? 'e':'o', inst, op_names[op]);
	sprintf(buffer, "%011o%c %06o ", loc, even ? 'e':'o', inst);

	switch (op) {
	case 0: /* call */
	case 1: /* call0 */
		sprintf(&buffer[strlen(buffer)], "%s", call_names[op]);
		sprintf(&buffer[strlen(buffer)], " %s ", dest_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(fefptr, reg, delta));

		{
			unsigned int v, tag;

			v = get(fefptr + (delta & 077));
			tag = (v >> width) & 037;
			if (0) printf("(tag%o %o) ", tag, v);
			switch (tag) {
			case 3:
				v = get(v);
				showstr(buffer, v, 0);
				break;
			case 4:
				showstr(buffer, v, 0);
				break;
			case 027:
				break;
			default:
				v = get(v);
				show_fef_func_name(buffer, v, width);
			}
		}
		//		nlc = (loc*2 + (even?0:1)) + delta;
		//		printf("+%o; %o%c ",
		//		       delta, nlc/2, (nlc & 1) ? 'o' : 'e');

		break;
	case 2: /* move */
	case 3: /* car */
	case 4: /* cdr */
	case 5: /* cadr */
	case 6: /* cddr */
	case 7: /* cdar */
	case 010: /* caar */
		sprintf(&buffer[strlen(buffer)], "%s", call_names[op]);
		sprintf(&buffer[strlen(buffer)], " %s ", dest_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(fefptr, reg, delta));
		break;
	case 011: /* nd1 */
		sprintf(&buffer[strlen(buffer)], "%s ", nd1_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(fefptr, reg, delta));
		break;
	case 012: /* nd2 */
		sprintf(&buffer[strlen(buffer)], "%s ", nd2_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(fefptr, reg, delta));
		break;
	case 013: /* nd3 */
		sprintf(&buffer[strlen(buffer)], "%s ", nd3_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(fefptr, reg, delta));
		break;
	case 014: /* branch */
		sprintf(&buffer[strlen(buffer)], "%s ", branch_names[dest]);
		to = ((int)inst & 03777) << 1;
		to |= (inst & 0x8000) ? 1 : 0;

		if (inst & 0400) {
			to = inst & 01777;
			to |= 03000;
			to |= ~01777;
		}

		nlc = (unsigned int)(((int)loc*2 + (even?0:1)) + to);

		if (to > 0) {
			sprintf(&buffer[strlen(buffer)], "+%o; %o%c ",
				to, nlc/2, (nlc & 1) ? 'o' : 'e');
		} else {
			sprintf(&buffer[strlen(buffer)], "-%o; %o%c ",
				-to, nlc/2, (nlc & 1) ? 'o' : 'e');
		}
		break;
	case 015: /* misc */
		sprintf(&buffer[strlen(buffer)], "(MISC) ");
		adr = inst & 0777;
		if (adr < 1024 && misc_inst_vector[adr]) {
			sprintf(&buffer[strlen(buffer)], "%s ", misc_inst[ misc_inst_vector[adr] ].name);
		} else {
			sprintf(&buffer[strlen(buffer)], "%o ", adr);
		}
		sprintf(&buffer[strlen(buffer)], "%s ", dest_names[dest]);
		break;
	}
	return buffer;
}

static void
showstr(char *buffer, unsigned int a, int cr)
{
	int j;
	unsigned int i, t, n;
	char s[256];

	t = get(a) & 0xff;
	j = 0;
	for (i = 0; i < t; i += 4) {
		n = get(a+1+(i/4));
		s[j++] = (char)(n >> 0);
		s[j++] = (char)(n >> 8);
		s[j++] = (char)(n >> 16);
		s[j++] = (char)(n >> 24);
	}

	s[t] = 0;
	sprintf(&buffer[strlen(buffer)], "'%s' ", s);
	if (cr) sprintf(&buffer[strlen(buffer)], " <break> ");
}

static void
show_fef_func_name(char *buffer, unsigned int fefptr, unsigned int width)
{
	unsigned int n, v;
	int tag;

	n = get(fefptr+2);

	sprintf(&buffer[strlen(buffer)], " "); v = get(n);

	tag = (v >> width) & 037;
	if (0) printf("tag %o\n", tag);

	if (tag == 3) {
		v = get(v);
		tag = (v >> width) & 037;
	}

	if (tag == 4) {
		sprintf(&buffer[strlen(buffer)], " "); showstr(buffer, v, 0);
	}
}

static unsigned int
get(unsigned int a)
{
	unsigned int v = 0;

	read_virt(disk_fd, a, &v);
	return v;
}

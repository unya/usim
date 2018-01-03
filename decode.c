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
#include <stdlib.h>

#include "ucode.h"
#include "decode.h"
#include "syms.h"

ucw_t prom_ucode[512];

// ---!!! read16 and read32 are from readmcr; should be shared.
unsigned int
read16(int fd)
{
	unsigned char b[2];
	int ret;

	ret = read(fd, b, 2);
	if (ret < 2) {
		printf("eof!\n");
		exit(1);
	}
	return (b[1] << 8) | b[0];
}

unsigned int
read32(int fd)
{
	unsigned char b[4];
	int ret;

	ret = read(fd, b, 4);
	if (ret < 4) {
		printf("eof!\n");
		exit(1);
	}
	return (b[1] << 24) | (b[0] << 16) | (b[3] << 8) | b[2];
}

int
read_prom_files(void)
{
	char *name = "../bands/promh.mcr.9";
	int fd;
	unsigned int code;
	unsigned int start;
	unsigned int size;

	fd = open(name, O_RDONLY | O_BINARY);
	if (fd < 0) {
		perror(name);
		exit(1);
	}

	code = read32(fd);
	start = read32(fd);
	size = read32(fd);
	printf("prom (%s): code: %d, start: %d, size: %d\n", name, code, start, size);

	int loc = start;
	for (unsigned int i = 0; i < size; i++) {
		unsigned int w1;
		unsigned int w2;
		unsigned int w3;
		unsigned int w4;

		w1 = read16(fd);
		w2 = read16(fd);
		w3 = read16(fd);
		w4 = read16(fd);
		prom_ucode[loc] =
			((unsigned long long) w1 << 48) |
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
	for (int i = 0; i < 512; i++) {
		printf("%03o %016llo", i, prom_ucode[i]);
		printf("\n");
	}
	printf("----\n");

	return 0;
}

#define l48(n) ((uint64)(n))
#define mask(v, n) (l48(v) << (n))
#define bit(n) (l48(1) << (n))

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
			printf("dispatch-constant ");
			break;
		case 1:
			printf("SPC-ptr, spc-data ");
			break;
		case 2:
			printf("PDL-ptr %o ", (int) u & 01777);
			break;
		case 3:
			printf("PDL-index %o ", (int) u & 01777);
			break;
		case 5:
			printf("PDL-buffer ");
			break;
		case 6:
			printf("OPC register %o ", (int) u & 017777);
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
		case 0:
			printf("-><none>");
			break;
		case 1:
			printf("->LC ");
			break;
		case 2:
			printf("->IC ");
			break;
		case 010:
			printf("->PDL[ptr] ");
			break;
		case 011:
			printf("->PDL[ptr],push ");
			break;
		case 012:
			printf("->PDL[index] ");
			break;
		case 013:
			printf("->PDL index ");
			break;
		case 014:
			printf("->PDL ptr ");
			break;
		case 015:
			printf("->SPC data,push ");
			break;
		case 016:
			printf("->OA-reg-lo ");
			break;
		case 017:
			printf("->OA-reg-hi ");
			break;
		case 020:
			printf("->VMA ");
			break;
		case 021:
			printf("->VMA,start-read ");
			break;
		case 022:
			printf("->VMA,start-write ");
			break;
		case 023:
			printf("->VMA,write-map ");
			break;
		case 030:
			printf("->MD ");
			break;
		case 031:
			printf("->MD,start-read ");
			break;
		case 032:
			printf("->MD,start-write ");
			break;
		case 033:
			printf("->MD,write-map ");
			break;
		}
		printf(",m[%o] ", dest & 037);
	}
}

void
disassemble_ucode_loc(ucw_t u)
{
	int a_src;
	int m_src;
	int new_pc;
	int dest;
	int alu_op;
	int r_bit;
	int p_bit;
	int n_bit;
	int ir8;
	int ir7;
	int widthm1;
	int pos;
	int mr_sr_bits;
	int disp_cont;
	int disp_addr;
	int map;
	int len;
	int rot;
	int out_bus;

	if ((u >> 42) & 1)
		printf("popj; ");

	switch ((u >> 43) & 03) {
	case 0:		// ALU.
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
			case 0:
				printf("mult-step ");
				break;
			case 1:
				printf("div-step ");
				break;
			case 5:
				printf("rem-corr ");
				break;
			case 011:
				printf("init-div-step ");
				break;
			}
		}

		printf("a=%o m=%o ", a_src, m_src);
		disassemble_m_src(u, m_src);

		if ((u >> 2) & 1)
			printf("C=1 ");
		else
			printf("C=0 ");

		switch (out_bus) {
		case 1:
			printf("alu-> ");
			break;
		case 2:
			printf("alu>>+s ");
			break;
		case 3:
			printf("alu<<+q31 ");
			break;
		}

		switch (u & 3) {
		case 1:
			printf("<<Q ");
			break;
		case 2:
			printf(">>Q ");
			break;
		case 3:
			printf("Q-R ");
			break;
		}
		disassemble_dest(dest);
		break;
	case 1:		// JUMP.
		printf("(jump) ");
		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;
		new_pc = (u >> 12) & 037777;

		printf("a=%o m=", a_src);
		disassemble_m_src(u, m_src);

		r_bit = (u >> 9) & 1;
		p_bit = (u >> 8) & 1;
		n_bit = (u >> 7) & 1;

		printf("pc %o, %s%s", new_pc, r_bit ? "R " : "", p_bit ? "P " : "");
		if (n_bit)	// INHIBIT-XCT-NEXT
			printf("!next ");

		if (u & (1 << 6))	// INVERT-JUMP-SENSE
			printf("!jump ");

		if (u & (1 << 5)) {
			switch (u & 017) {
			case 0:
			case 1:
				printf("M-src < A-src ");
				break;
			case 2:
				printf("M-src <= A-src ");
				break;
			case 3:
				printf("M-src = A-src ");
				break;
			case 4:
				printf("pf ");
				break;
			case 5:
				printf("pf/int ");
				break;
			case 6:
				printf("pf/int/seq ");
				break;
			case 7:
				printf("jump-always ");
				break;
			}
		} else {
			printf("m-rot<< %o", (int) u & 037);
		}
		break;
	case 2:		// DISPATCH.
		printf("(dispatch) ");
		disp_cont = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;

		if ((u >> 25) & 1)
			printf("!N+1 ");
		if ((u >> 24) & 1)
			printf("ISH ");
		disp_addr = (u >> 12) & 03777;
		map = (u >> 8) & 3;
		len = (u >> 5) & 07;
		rot = u & 037;

		printf("m=%o ", m_src);
		disassemble_m_src(u, m_src);

		printf("disp-const %o, disp-addr %o, map %o, len %o, rot %o ", disp_cont, disp_addr, map, len, rot);
		break;
	case 3:		// BYTE.
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
		case 1:	// LDB.
			printf("ldb pos=%o, width=%o ", pos, widthm1 + 1);
			break;
		case 2:
			printf("sel dep (a<-m&mask) pos=%o, width=%o ", pos, widthm1 + 1);
			break;
		case 3:	// DPB.
			printf("dpb pos=%o, width=%o ", pos, widthm1 + 1);
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
	unsigned int start;
	unsigned int finish;

	start = 0;
	finish = 512;

	for (unsigned int i = start; i < finish; i++) {
		ucw_t u = prom_ucode[i];
		printf("%03o %016llo ", i, prom_ucode[i]);
		disassemble_ucode_loc(u);
	}
}

// ---!!! See about merging with diskmaker.

int partoff;
int bnum = -1;
unsigned int buf[256];

extern int disk_fd;

unsigned int
str4(char *s)
{
	return (unsigned int) ((s[3] << 24) | (s[2] << 16) | (s[1] << 8) | s[0]);
}

int
find_disk_partition_table(int fd)
{
	off_t ret;
	unsigned int p;
	unsigned int count;
	unsigned int nw;
	off_t offset;

	printf("looking for partition\n");

	offset = 0;
	ret = lseek(fd, offset, SEEK_SET);
	if (ret != offset) {
		return -1;
	}
	ret = read(fd, buf, 256 * 4);
	if (ret != 256 * 4) {
		return -1;
	}

	p = 0200;
	count = buf[p++];
	nw = buf[p++];

	for (unsigned int i = 0; i < count; i++) {
		if (buf[p] == str4("LOD1")) {
			partoff = (int) buf[p + 1];
			break;
		}
		p += nw;
	}

	bnum = -1;

	return 0;
}

unsigned int
read_virt(int fd, unsigned int addr, unsigned int *pv)
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
			return 1;
		}

		ret = read(fd, buf, 256 * 4);
		if (ret != 256 * 4) {
			return 1;
		}
	}

	*pv = buf[addr % 256];

	return 0;
}

char *
read_string(unsigned int loc)
{
	unsigned int v;
	static char s[256];

	if (read_virt(disk_fd, loc, &v) == 0) {
		unsigned int j;
		unsigned int t;
	
		t = v & 0xff;
		j = 0;

		for (unsigned int i = 0; i < t; i += 4) {
			unsigned int l;

			l = loc + 1 + (i / 4);
			if (read_virt(disk_fd, l, &v))
				return "<read-failed>";

			s[j++] = (char) (v >> 0);
			s[j++] = (char) (v >> 8);
			s[j++] = (char) (v >> 16);
			s[j++] = (char) (v >> 24);
		}

		s[t] = 0;
		return s;
	}

	return (char *) 0;
}

char *
show_string(unsigned int loc)
{
	char *s;

	s = read_string(loc);
	printf(" '%s' ", s);
	return s;
}

// A complete hack; given a PC, find the function header and print the
// function name.  Helpful for debugging.
char *
find_function_name(unsigned int the_lc)
{
	int tag = 0;
	unsigned int loc = (unsigned int) (the_lc >> 2);
	unsigned int v;

	if (partoff == 0) {
		find_disk_partition_table(disk_fd);
	}

	// Search backward to find the function header.
	for (int i = 0; i < 512; i++) {
		if (read_virt(disk_fd, loc, &v))
			break;
		tag = (v >> 24) & 077;
		if (tag == 7)
			break;
		loc--;
	}

	if (tag == 7) {
		// Find function symbol pointer.
		if (read_virt(disk_fd, loc + 2, &v) == 0) {
			loc = v;
			tag = (v >> 24) & 077;

			if (read_virt(disk_fd, loc, &v))
				return "<read-failed>";

			// Hack - it's a list.
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
show_list(void)
{
	unsigned int loc;
	unsigned int l1;
	unsigned int v = 0;

	loc = 030301442405;
	read_virt(disk_fd, loc, &v);

	for (int i = 0; i < 10; i++) {
		int tag;
		int cdr;
		int addr;

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
				printf(" %011o %011o %03o %1o\n", l1, v, tag, cdr);

				l1 = v;
				read_virt(disk_fd, l1, &v);
				tag = (v >> 24) & 077;
				cdr = (v >> 30) & 3;
				printf(" %011o %011o %03o %1o\n", l1, v, tag, cdr);

				show_string(l1);
				printf("\n");
				break;
			case 004:
				l1 = v;
				read_virt(disk_fd, l1, &v);
				tag = (v >> 24) & 077;
				cdr = (v >> 30) & 3;
				printf(" %011o %011o %03o %1o\n", l1, v, tag, cdr);

				show_string(l1);
				printf("\n");
				break;
			case 016:
				break;
			case 021:
				l1 = v;
				read_virt(disk_fd, l1, &v);
				tag = (v >> 24) & 077;
				cdr = (v >> 30) & 3;
				printf(" %011o %011o %03o %1o\n", l1, v, tag, cdr);

				show_string(l1);
				printf("\n");
				break;
			default:
				break;
			}
		loc++;
	}
}

void showstr(char *buffer, unsigned int a, int cr);
void show_fef_func_name(char *buffer, unsigned int fefptr, unsigned int width);
unsigned int get(unsigned int a);

#include "macroops.h"

char *disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst, unsigned int width);
extern int read_mem(int vaddr, unsigned int *pv);

char *
disassemble_address(unsigned int reg, unsigned int delta)
{
	static char addr[256];

	if (reg < 4) {
		sprintf(addr, "FEF|%o;", delta);
	} else if (reg == 4) {
		static unsigned int constants_area;
		static int done = 0;

		if (!done) {
			for (int i = 0; i < 1024; i++) {
				char *sym;
				extern unsigned int a_memory[1024];

				sym = sym_find_by_type_val(1, 4, i);
				if (sym && strcasecmp(sym, "A-V-CONSTANTS-AREA") == 0) {
					read_mem(a_memory[i], &constants_area);
					printf("found %o %-40s %o\n", i, sym, a_memory[i]);
				}
			}
			done = 1;
		}
		unsigned int value;

		read_mem(constants_area + (delta & 077), &value);
		sprintf(addr, "/%o", value);
	} else if (delta == 0777) {
		sprintf(addr, "PDL-POP");
	} else if (reg == 5) {
		sprintf(addr, "LOCAL|%o", delta & 077);
	} else if (reg == 6) {
		sprintf(addr, "ARG|%o", delta & 077);
	} else {
		sprintf(addr, "PDL|%o", delta & 077);
	}
	return addr;
}

char *
disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst, unsigned int width)
{
	unsigned int op;
	unsigned int dest;
	unsigned int reg;
	unsigned int delta;
	unsigned int adr;
	int to;
	unsigned int nlc;
	static char buffer[1024];

	// Search for FEF pointer.
	{
		int tag = 0;
		unsigned int addr = (unsigned int) (loc >> 2);
		unsigned int v;

		if (partoff == 0) {
			find_disk_partition_table(disk_fd);
		}

		// Search backward to find the function header.
		for (int i = 0; i < 512; i++) {
			if (read_virt(disk_fd, addr, &v))
				break;
			tag = (v >> width) & 037;
			if (tag == 7)
				break;
			addr--;
		}
		fefptr = addr;
	}
	if (!misc_inst_vector_setup) {
		int index;

		for (int i = 0; i < 1024; i++) {
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
	sprintf(buffer, "%011o%c %06o ", loc, even ? 'e' : 'o', inst);

	switch (op) {
	case 0:		// CALL
	case 1:		// CALL0
		sprintf(&buffer[strlen(buffer)], "%s", call_names[op]);
		sprintf(&buffer[strlen(buffer)], " %s ", dest_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(reg, delta));
		{
			unsigned int v;
			unsigned int tag;
			v = get(fefptr + (delta & 077));
			tag = (v >> width) & 037;
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
		break;
	case 2:		// MOVE.
	case 3:		// CAR.
	case 4:		// CDR.
	case 5:		// CADR.
	case 6:		// CDDR.
	case 7:		// CDAR.
	case 010:		// CAAR.
		sprintf(&buffer[strlen(buffer)], "%s", call_names[op]);
		sprintf(&buffer[strlen(buffer)], " %s ", dest_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(reg, delta));
		break;
	case 011:		// ND1.
		sprintf(&buffer[strlen(buffer)], "%s ", nd1_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(reg, delta));
		break;
	case 012:		// ND2.
		sprintf(&buffer[strlen(buffer)], "%s ", nd2_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(reg, delta));
		break;
	case 013:		// ND3.
		sprintf(&buffer[strlen(buffer)], "%s ", nd3_names[dest]);
		sprintf(&buffer[strlen(buffer)], "%s", disassemble_address(reg, delta));
		break;
	case 014:		// BRANCH.
		sprintf(&buffer[strlen(buffer)], "%s ", branch_names[dest]);
		to = ((int) inst & 03777) << 1;
		to |= (inst & 0x8000) ? 1 : 0;
		if (inst & 0400) {
			to = inst & 01777;
			to |= 03000;
			to |= ~01777;
		}

		nlc = (unsigned int) (((int) loc * 2 + (even ? 0 : 1)) + to);

		if (to > 0) {
			sprintf(&buffer[strlen(buffer)], "+%o; %o%c ", to, nlc / 2, (nlc & 1) ? 'o' : 'e');
		} else {
			sprintf(&buffer[strlen(buffer)], "-%o; %o%c ", -to, nlc / 2, (nlc & 1) ? 'o' : 'e');
		}
		break;
	case 015:		// MISC.
		sprintf(&buffer[strlen(buffer)], "(MISC) ");
		adr = inst & 0777;
		if (adr < 1024 && misc_inst_vector[adr]) {
			sprintf(&buffer[strlen(buffer)], "%s ", misc_inst[misc_inst_vector[adr]].name);
		} else {
			sprintf(&buffer[strlen(buffer)], "%o ", adr);
		}
		sprintf(&buffer[strlen(buffer)], "%s ", dest_names[dest]);
		break;
	}
	return buffer;
}

void
showstr(char *buffer, unsigned int a, int cr)
{
	int j;
	unsigned int t;
	char s[256];

	t = get(a) & 0xff;
	j = 0;

	for (unsigned int i = 0; i < t; i += 4) {
		unsigned int n;

		n = get(a + 1 + (i / 4));
		s[j++] = (char) (n >> 0);
		s[j++] = (char) (n >> 8);
		s[j++] = (char) (n >> 16);
		s[j++] = (char) (n >> 24);
	}
	s[t] = 0;

	sprintf(&buffer[strlen(buffer)], "'%s' ", s);
	if (cr)
		sprintf(&buffer[strlen(buffer)], " <break> ");
}

void
show_fef_func_name(char *buffer, unsigned int fefptr, unsigned int width)
{
	unsigned int n;
	unsigned int v;
	int tag;

	n = get(fefptr + 2);

	sprintf(&buffer[strlen(buffer)], " ");

	v = get(n);
	tag = (v >> width) & 037;
	if (tag == 3) {
		v = get(v);
		tag = (v >> width) & 037;
	}
	if (tag == 4) {
		sprintf(&buffer[strlen(buffer)], " ");
		showstr(buffer, v, 0);
	}
}

unsigned int
get(unsigned int a)
{
	unsigned int v = 0;

	read_virt(disk_fd, a, &v);
	return v;
}

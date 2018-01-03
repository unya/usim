// cc --- crude version of CC

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>

#include "ucode.h"
#include "decode.h"

int fd;
int debug;
int verbose;

#define WRITE 1

#define rd_spy_ir_low 0
#define rd_spy_ir_med 1
#define rd_spy_ir_high 2
#define rd_spy_scratch 3
#define rd_spy_opc 4
#define rd_spy_pc 5
#define rd_spy_ob_low 6
#define rd_spy_ob_high 7
#define rd_spy_flag_1 010
#define rd_spy_flag_2 011
#define rd_spy_m_low 012
#define rd_spy_m_high 013
#define rd_spy_a_low 014
#define rd_spy_a_high 015
#define rd_spy_stat_low 016
#define rd_spy_stat_high 017
#define rd_spy_md_low 020
#define rd_spy_md_high 021
#define rd_spy_vma_low 022
#define rd_spy_vma_high 023
#define rd_spy_disk 026
#define rd_spy_bd 027

#define wr_spy_ir_low 0
#define wr_spy_ir_med 1
#define wr_spy_ir_high 2
#define wr_spy_clk 3
#define wr_spy_opc_control 4
#define wr_spy_mode 5
#define wr_spy_scratch 7
#define wr_spy_md_low 010
#define wr_spy_md_high 011
#define wr_spy_vma_low 012
#define wr_spy_vma_high 013

#include "lcadmc.h"

uint32_t cc_read_md(void);

int cc_noop_debug_clock(void);
int cc_debug_clock(void);
int cc_noop_clock(void);
int cc_clock(void);

int
cc_send(unsigned char *b, int len)
{
	int ret;

	// Send slowly so as not to confuse hardware.
	for (int i = 0; i < len; i++) {
		ret = write(fd, b + i, 1);
		if (ret != 1)
			perror("write");
		tcflush(fd, TCOFLUSH);
		usleep(50);
		usleep(2000);
	}

	usleep(10000);

	return len;
}

uint16_t
reg_get(int base, int reg)
{
	unsigned char buffer[64];
	unsigned char nibs[4];
	int ret;
	int mask;
	int loops;
	int off;
	uint16_t v;

again:
	buffer[0] = base | (reg & 0x1f);
	if (debug)
		printf("send %02x\n", buffer[0]);
	cc_send(buffer, 1);
	usleep(1000 * 100);

	memset(buffer, 0, 64);
	loops = 0;
	off = 0;
	while (1) {
		ret = read(fd, buffer + off, 64 - off);
		if (ret > 0)
			off += ret;
		if (off == 4)
			break;
		if (ret < 0 && errno == 11) {
			usleep(100);

			loops++;
			if (loops > 5)
				goto again;
			continue;
		}
	}

	if (debug) {
		printf("response %d\n", ret);
		printf("%02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	}

	if (off < 4) {
		printf("spy register read failed (ret=%d, off=%d, errno=%d)\n", ret, off, errno);
		return -1;
	}

	// Response should be 0x3x, 0x4x, 0x5x, 0x6x, but hardware can
	// sometimes repeat characters.
	mask = 0;
	for (int i = 0; i < ret; i++) {
		int nib;

		nib = (buffer[i] & 0xf0) >> 4;
		switch (nib) {
		case 3:
			mask |= 8;
			nibs[0] = buffer[i];
			break;
		case 4:
			mask |= 4;
			nibs[1] = buffer[i];
			break;
		case 5:
			mask |= 2;
			nibs[2] = buffer[i];
			break;
		case 6:
			mask |= 1;
			nibs[3] = buffer[i];
			break;
		}
	}

	if (mask == 0xf) {
		if (debug)
			printf("response ok\n");
		v = ((nibs[0] & 0x0f) << 12) | ((nibs[1] & 0x0f) << 8) | ((nibs[2] & 0x0f) << 4) | ((nibs[3] & 0x0f) << 0);
		if (debug)
			printf("reg %o = 0x%04x (0%o)\n", reg, v, v);
		return v;
	}

	return 0;
}

int
reg_set(int base, int reg, int v)
{
	unsigned char buffer[64];
	int ret;

	if (debug)
		printf("cc_set(r=%d, v=%o)\n", reg, v);

	buffer[0] = 0x30 | ((v >> 12) & 0xf);
	buffer[1] = 0x40 | ((v >> 8) & 0xf);
	buffer[2] = 0x50 | ((v >> 4) & 0xf);
	buffer[3] = 0x60 | ((v >> 0) & 0xf);
	buffer[4] = base | (reg & 0x1f);
	if (debug) {
		printf("writing, fd=%d\n", fd);
		printf("%02x %02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
	}

	ret = cc_send(buffer, 5);
	if (debug)
		printf("ret %d\n", ret);
	if (ret != 5)
		printf("cc_set: write error%d\n", ret);

	return 0;
}

uint16_t
cc_get(int reg)
{
	return reg_get(0x80, reg);
}

int
cc_set(int reg, int v)
{
	return reg_set(0xa0, reg, v);
}

uint16_t
mmc_get(int reg)
{
	return reg_get(0xc0, reg);
}

int
mmc_set(int reg, int v)
{
	return reg_set(0xd0, reg, v);
}

int
cc_stop(void)
{
	return cc_set(wr_spy_clk, 0);
}

int
cc_go(void)
{
	return cc_set(wr_spy_clk, 0001);
}

uint32_t
_cc_read_pair(int r1, int r2)
{
	uint32_t v1;
	uint32_t v2;

	v1 = cc_get(r1);
	v2 = cc_get(r2);

	return (v1 << 16) | v2;
}

uint64_t
_cc_read_triple(int r1, int r2, int r3)
{
	uint64_t v1;
	uint32_t v2;
	uint32_t v3;

	v1 = cc_get(r1);
	v2 = cc_get(r2);
	v3 = cc_get(r3);

	return (v1 << 32) | (v2 << 16) | v3;
}

// See SYS; LCADR: LCADRD LISP and SYS;LCADR:LCADMC LISP for details.
//
// ---!!! Split this into "lcadrd.c", and add comments from LCADMC LISP.

uint32_t
cc_read_obus(void)
{
	return _cc_read_pair(rd_spy_ob_high, rd_spy_ob_low);
}

uint32_t
cc_read_obus_(void)
{
	return _cc_read_pair(025, 024);
}

uint32_t
cc_read_a_bus(void)
{
	return _cc_read_pair(rd_spy_a_high, rd_spy_a_low);
}

uint32_t
cc_read_m_bus(void)
{
	return _cc_read_pair(rd_spy_m_high, rd_spy_m_low);
}

uint64_t
cc_read_ir(void)
{
	return _cc_read_triple(rd_spy_ir_high, rd_spy_ir_med, rd_spy_ir_low);
}

uint16_t
cc_read_pc(void)
{
	return cc_get(rd_spy_pc);
}

uint16_t
cc_read_scratch(void)
{
	return cc_get(rd_spy_scratch);
}

uint32_t
cc_read_status(void)
{
	uint16_t v1;
	uint16_t v2;
	uint16_t v3;

	v1 = cc_get(rd_spy_flag_1);
	v2 = cc_get(rd_spy_flag_2);
	v3 = cc_get(rd_spy_ir_low);

	if (v3 & 0100)
		v2 ^= 4;	// Hardware reads JC-TRUE incorrectly.

	return (v1 << 16) | v2;
}

int
cc_write_diag_ir(uint64_t ir)
{
	if (debug || verbose)
		disassemble_ucode_loc(ir);

	cc_set(wr_spy_ir_high, (uint16_t) ((ir >> 32) & 0xffff));
	cc_set(wr_spy_ir_med, (uint16_t) ((ir >> 16) & 0xffff));
	cc_set(wr_spy_ir_low, (uint16_t) ((ir >> 0) & 0xffff));

	cc_set(wr_spy_ir_high, (uint16_t) ((ir >> 32) & 0xffff));
	cc_set(wr_spy_ir_med, (uint16_t) ((ir >> 16) & 0xffff));
	cc_set(wr_spy_ir_low, (uint16_t) ((ir >> 0) & 0xffff));

	return 0;
}

int
cc_write_ir(uint64_t ir)
{
	if (debug || verbose)
		printf("ir %" PRIu64 " (0x%016llx)\n", ir, ir);
	cc_write_diag_ir(ir);
	return cc_noop_debug_clock();
}

int
cc_execute_r(uint64_t ir)
{
	int ret;

	if (debug || verbose)
		printf("ir %" PRIu64 " (0x%016" PRIx64 ")\n", ir, ir);

again:
	cc_write_diag_ir(ir);
	cc_noop_debug_clock();
	if (cc_read_ir() != ir) {
		printf("ir reread failed; retry\n");
		goto again;
	}

	ret = cc_debug_clock();
	return ret;
}

int
cc_execute_w(uint64_t ir)
{
	int ret;

	if (debug || verbose)
		printf("ir %" PRIu64 " (0x%016" PRIx64 ")\n", ir, ir);

again:
	cc_write_diag_ir(ir);
	cc_noop_debug_clock();
	if (cc_read_ir() != ir) {
		printf("ir reread failed; retry\n");
		goto again;
	}
	cc_clock();
	ret = cc_noop_clock();

	return ret;
}

int
cc_execute(int op, uint64_t ir)
{
	if (debug)
		disassemble_ucode_loc(ir);

	if (op == WRITE) {
		return cc_execute_w(ir);
	}
	return cc_execute_r(ir);
}

uint32_t
bitmask(int wid)
{
	uint32_t m;

	m = 0;

	for (int i = 0; i < wid; i++) {
		m <<= 1;
		m |= 1;
	}
	return m;
}

uint64_t
ir_pair(int field, uint32_t val)
{
	uint64_t ir;
	uint32_t mask;
	int shift;
	int width;

	shift = field >> 6;
	width = field & 077;
	mask = bitmask(width);

	val &= mask;
	ir = ((uint64_t) val) << shift;

	if (debug)
		printf("ir_pair field=%o, shift=%d, width=%d, mask=%x, val=%x\n", field, shift, width, mask, val);

	return ir;
}

uint32_t
cc_read_md(void)
{
	return _cc_read_pair(rd_spy_md_high, rd_spy_md_low);
}

int
cc_write_md(uint32_t md)
{
	cc_set(wr_spy_md_high, (uint16_t) (md >> 16) & 0xffff);
	cc_set(wr_spy_md_low, (uint16_t) (md >> 0) & 0xffff);

	while (1) {
		uint32_t v;

		v = cc_read_md();
		if (v == md)
			break;

		printf("md readback failed, retry got %x want %x\n", v, md);
		cc_set(wr_spy_md_high, (uint16_t) (md >> 16) & 0xffff);
		cc_set(wr_spy_md_low, (uint16_t) (md >> 0) & 0xffff);
	}

	return 0;
}

uint32_t
cc_read_vma(void)
{
	return _cc_read_pair(rd_spy_vma_high, rd_spy_vma_low);
}

int
cc_write_vma(uint32_t vma)
{
	cc_set(wr_spy_vma_high, (uint16_t) (vma >> 16) & 0xffff);
	cc_set(wr_spy_vma_low, (uint16_t) (vma >> 0) & 0xffff);

	while (cc_read_vma() != vma) {
		printf("vma readback failed, retry\n");
		cc_set(wr_spy_vma_high, (uint16_t) (vma >> 16) & 0xffff);
		cc_set(wr_spy_vma_low, (uint16_t) (vma >> 0) & 0xffff);
	}

	return 0;
}

int
cc_write_md_1s(void)
{
	cc_write_md(0xffffffff);

	return 0;
}

int
cc_write_md_0s(void)
{
	cc_write_md(0x00000000);

	return 0;
}

uint32_t
cc_read_a_mem(uint32_t adr)
{
	cc_execute(0,
		   ir_pair(CONS_IR_A_SRC, adr) |
		   ir_pair(CONS_IR_ALUF, CONS_ALU_SETA) |
		   ir_pair(CONS_IR_OB, CONS_OB_ALU));

	return cc_read_obus();
}

uint32_t
cc_write_a_mem(uint32_t loc, uint32_t val)
{
	uint32_t v2;

	cc_write_md(val);
	v2 = cc_read_md();
	if (v2 != val) {
		printf("cc_write_a_mem; md readback error (got=%o wanted=%o)\n", v2, val);
	}
	cc_execute(WRITE,
		   ir_pair(CONS_IR_M_SRC, CONS_M_SRC_MD) |
		   ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
		   ir_pair(CONS_IR_OB, CONS_OB_ALU) |
		   ir_pair(CONS_IR_A_MEM_DEST, CONS_A_MEM_DEST_INDICATOR + loc));

	return 0;
}

uint32_t
cc_read_m_mem(uint32_t adr)
{
	cc_execute(0,
		   ir_pair(CONS_IR_M_SRC, adr) |
		   ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
		   ir_pair(CONS_IR_OB, CONS_OB_ALU));

	return cc_read_obus();
}

int
cc_debug_clock(void)
{
	cc_set(wr_spy_clk, 012);
	cc_set(wr_spy_clk, 0);

	return 0;
}

int
cc_noop_debug_clock(void)
{
	cc_set(wr_spy_clk, 016);
	cc_set(wr_spy_clk, 0);

	return 0;
}

int
cc_clock(void)
{
	cc_set(wr_spy_clk, 2);
	cc_set(wr_spy_clk, 0);

	return 0;
}

int
cc_noop_clock(void)
{
	cc_set(wr_spy_clk, 6);
	cc_set(wr_spy_clk, 0);

	return 0;
}

int
cc_single_step(void)
{
	cc_set(wr_spy_clk, 6);
	cc_set(wr_spy_clk, 0);

	return 0;
}

int
cc_report_basic_regs(void)
{
	uint32_t A;
	uint32_t M;
	uint32_t PC;
	uint32_t MD;
	uint32_t VMA;

	A = cc_read_a_bus();
	printf("A = %011o (%08x)\n", A, A);

	M = cc_read_m_bus();
	printf("M = %011o (%08x)\n", M, M);

	PC = cc_read_pc();
	printf("PC = %011o (%08x)\n", PC, PC);

	MD = cc_read_md();
	printf("MD = %011o (%08x)\n", MD, MD);

	VMA = cc_read_vma();
	printf("VMA= %011o (%08x)\n", VMA, VMA);

	{
		uint32_t disk;
		uint32_t bd;
		uint32_t mmc;

		disk = cc_get(rd_spy_disk);
		bd = cc_get(rd_spy_bd);
		mmc = bd >> 6;
		bd &= 0x3f;

		printf("disk-state %d (0x%04x)\n", disk, disk);
		printf("bd-state %d (0x%04x)\n", bd, bd);
		printf("mmc-state %d (0x%04x)\n", mmc, mmc);
	}

	return 0;
}

int
cc_report_pc(uint32_t *ppc)
{
	uint32_t PC;

	PC = cc_read_pc();
	printf("PC = %011o (%08x)\n", PC, PC);
	*ppc = PC;

	return 0;
}

int
cc_report_pc_and_md(uint32_t *ppc)
{
	uint32_t PC;
	uint32_t MD;

	PC = cc_read_pc();
	MD = cc_read_md();
	printf("PC=%011o (%08x) MD=%011o (%08x)\n", PC, PC, MD, MD);
	*ppc = PC;

	return 0;
}

int
cc_report_pc_and_ir(uint32_t *ppc)
{
	uint32_t PC;
	uint64_t ir;

	PC = cc_read_pc();
	ir = cc_read_ir();
	printf("PC=%011o (%08x) ir=%" PRIu64 " ", PC, PC, ir);
	disassemble_ucode_loc(ir);
	*ppc = PC;

	return 0;
}

int
cc_report_pc_md_ir(uint32_t *ppc)
{
	uint32_t PC;
	uint32_t MD;
	uint64_t ir;

	PC = cc_read_pc();
	MD = cc_read_md();
	ir = cc_read_ir();
	printf("PC=%011o MD=%011o (%08x) ir=%" PRIu64 " ", PC, MD, MD, ir);
	disassemble_ucode_loc(ir);
	*ppc = PC;

	return 0;
}

int
cc_report_status(void)
{
	uint32_t s;
	uint32_t f1;
	uint32_t f2;

	s = cc_read_status();
	f1 = s >> 16;
	f2 = s & 0xffff;

	printf("flags1: %04x (", f1);
	if (f1 & (1 << 15))
		printf("waiting ");
	if (f1 & (1 << 12))
		printf("promdisable ");
	if (f1 & (1 << 11))
		printf("stathalt ");
	if (f1 & (1 << 10))
		printf("err ");
	if (f1 & (1 << 9))
		printf("ssdone ");
	if (f1 & (1 << 8))
		printf("srun ");
	printf(") ");
	printf("flags2: %04x (", f2);
	if (f2 & (1 << 2))
		printf("jcond ");
	if (f2 & (1 << 3))
		printf("vmaok ");
	if (f2 & (1 << 4))
		printf("nop ");
	printf(") ");

	return 0;
}

int
cc_pipe(void)
{
	uint64_t isn;

	for (int i = 0; i < 8; i++) {
		printf("addr %o:\n", i);
		isn =
			ir_pair(CONS_IR_M_SRC, i) |
			ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
			ir_pair(CONS_IR_OB, CONS_OB_ALU);
		printf("%" PRIu64 " ", isn);
		disassemble_ucode_loc(isn);

		cc_write_diag_ir(isn);
		cc_noop_debug_clock();
		printf(" obus1 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus2 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus3 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_clock();
		printf(" obus4 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_clock();
		printf(" obus5 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());
	}

	return 0;
}

int
cc_pipe2(void)
{
	uint64_t isn;
	uint32_t v2;

	for (int i = 0; i < 8; i++) {
		printf("val %o:\n", i);
		cc_write_md(i);
		v2 = cc_read_md();
		if (v2 != i) {
			printf("cc_pipe2; md readback error (got=%o wanted=%o)\n", v2, i);
		}
		isn =
			ir_pair(CONS_IR_M_SRC, CONS_M_SRC_MD) |
			ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
			ir_pair(CONS_IR_OB, CONS_OB_ALU) |
			ir_pair(CONS_IR_M_MEM_DEST, i);
		printf("%" PRIu64 " ", isn);
		disassemble_ucode_loc(isn);
		cc_write_diag_ir(isn);

		cc_noop_debug_clock();
		printf(" obus1 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus2 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus3 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus4 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus5 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());
	}
	return 0;
}

uint64_t setup_map_inst[] = {
	04000000000110003,	// (alu) SETZ a=0 m=0 m[0] C=0 alu-> Q-R -><none>,m[2]
	00000000000150173,	// (alu) SETO a=0 m=0 m[0] C=0 alu-> Q-R -><none>,m[3]
	00600101602370010,	// (byte) a=2 m=m[3] dpb pos=10, width=1 ->a_mem[47]

	04600101446230166,	// (byte) a=2 m=m[3] dpb pos=26, width=4 ->VMA,write-map ,m[4]
	04600201400270400,	// (byte) a=4 m=m[3] dpb pos=0, width=11 -><none>,m[5]
	00002340060010050,	// (alu) SETA a=47 m=0 m[0] C=0 alu-> ->MD ,m[0]
	00600241446030152,	// (byte) a=5 m=m[3] dpb pos=12, width=4 ->VMA,write-map ,m[0]
	00002365060010310,	// (alu) M+A [ADD] a=47 m=52 MD C=0 alu-> ->MD ,m[0]
	00600201400270041,	// (byte) a=4 m=m[3] dpb pos=1, width=2 -><none>,m[5]
	04600241446030444,	// (byte) a=5 m=m[3] dpb pos=4, width=12 ->VMA,write-map ,m[0]
	00002365060010310,	// (alu) M+A [ADD] a=47 m=52 MD C=0 alu-> ->MD ,m[0]
	04600201446030000,	// (byte) a=4 m=m[3] dpb pos=0, width=1 ->VMA,write-map ,m[0]
	0
};

int
cc_setup_map(void)
{
	for (int i = 0; 1; i++) {
		if (setup_map_inst[i] == 0)
			break;
		printf("%d ", i);
		fflush(stdout);
		cc_execute_r(setup_map_inst[i]);
	}

	return 0;
}

int
cc_report_ide_regs(void)
{
	uint32_t v;

	printf("setting up map...\n");
	cc_setup_map();

	printf("read ide...\n");
	cc_write_a_mem(2, 01333);
	cc_write_a_mem(3, 0773);
	cc_write_a_mem(4, 0774);
	cc_write_a_mem(5, 0777);

	for (int i = 0; i < 8; i++) {
		cc_write_a_mem(1, i | 020);
		cc_execute_r(0000040060010050);	// alu seta a=1 ->md
		cc_execute_r(0000140044010050);	// alu seta a=3 alu-> ->vma+write

		cc_execute_w(0000100060010050);	// alu seta a=2 ->md
		cc_execute_w(0000200044010050);	// alu seta a=4 alu-> ->vma+write

		cc_execute_w(0000240044010050);	// alu seta a=5 alu-> ->vma+write

		cc_execute_w(0000140042010050);	// alu seta a=3 alu-> ->vma+read */
		v = cc_read_md();
		printf("ide[%d] = 0x%08x 0x%02x\n", i, v, v & 0xff);
	}

	printf("a[1]=%0o\n", cc_read_a_mem(1));
	printf("a[2]=%0o\n", cc_read_a_mem(2));
	printf("a[3]=%0o\n", cc_read_a_mem(3));
	printf("a[4]=%0o\n", cc_read_a_mem(4));
	printf("a[5]=%0o\n", cc_read_a_mem(5));
	printf("a[6]=%0o\n", cc_read_a_mem(6));

	return 0;
}

int
_test_scratch(uint16_t v)
{
	uint16_t s1;
	uint16_t s2;

	s1 = cc_read_scratch();
	cc_set(wr_spy_scratch, v);
	s2 = cc_read_scratch();

	printf("write 0%o; scratch %o -> %o (0x%x) ", v, s1, s2, s2);
	if (s2 == v) {
		printf("ok\n");
	} else {
		printf("BAD\n");
		s2 = cc_read_scratch();
		printf(" reread; scratch %o -> %o (0x%x) ", s1, s2, s2);
		if (s2 == v) {
			printf("ok\n");
		} else {
			printf("BAD\n");
			s1 = cc_read_scratch();
			cc_set(wr_spy_scratch, v);
			s2 = cc_read_scratch();

			printf(" rewrite 0%o; scratch %o -> %o (0x%x) ", v, s1, s2, s2);
			if (s2 == v) {
				printf("ok\n");
			} else {
				printf("BAD\n");
				return -1;
			}
		}
	}

	return 0;
}

int vv = 0;

int
cc_test_scratch(void)
{
	_test_scratch(01234);
	_test_scratch(04321);
	_test_scratch(0);
	_test_scratch(07777);
	_test_scratch(0123456);
	_test_scratch(0x2222);
	_test_scratch(++vv);

	return 0;
}

int
_test_ir(uint64_t isn)
{
	uint64_t iv;

	printf("test ir %" PRIu64 " ", isn);

	cc_write_ir(isn);
	iv = cc_read_ir();
	if (iv == isn) {
		printf("ok\n");
	} else {
		printf("bad (want 0x%" PRIx64 " got 0x%" PRIx64 ")\n", isn, iv);
		printf(" reread; ");
		iv = cc_read_ir();
		if (iv == isn) {
			printf("ok\n");
		} else {
			printf("bad\n");
			printf(" rewrite; ");
			cc_write_ir(isn);
			iv = cc_read_ir();
			if (iv == isn) {
				printf("ok\n");
			} else {
				printf("bad\n");
				return -1;
			}
		}
	}

	return 0;
}

int
cc_test_ir(void)
{
	_test_ir(0);
	_test_ir(1);
	_test_ir(0x000022220000);
	_test_ir(0x011133332222);
	_test_ir(2);
	_test_ir(0x011155552222);

	return 0;
}

char *serial_devicename = "/dev/ttyUSB1";

void
usage(void)
{
	fprintf(stderr, "usage: cc [OPTION]...\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -d             extra debug output\n");
}

int
main(int argc, char *argv[])
{
	int ret;
	int done;
	int c;
	struct termios oldtio;
	struct termios newtio;

	while ((c = getopt(argc, argv, "dh")) != -1) {
		switch (c) {
		case 'd':
			debug++;
			break;
		case 'h':
			usage();
			exit(0);
		default:
			continue;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		serial_devicename = strdup (argv[0]);

	fd = open(serial_devicename, O_RDWR | O_NONBLOCK);
	if (debug)
		printf("fd %d\n", fd);
	if (fd < 0) {
		perror(serial_devicename);
		exit(1);
	}

	// Get current port settings.
	ret = tcgetattr(fd, &oldtio);
	if (ret)
		perror("tcgetattr");

	newtio = oldtio;
	cfmakeraw(&newtio);

	// Set new port settings for canonical input processing.
	cfsetspeed(&newtio, B115200);
	newtio.c_cflag |= CS8 | CLOCAL | CREAD;
	newtio.c_iflag |= IGNPAR;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	tcflush(fd, TCIFLUSH);

	ret = tcsetattr(fd, TCSANOW, &newtio);
	if (ret)
		perror("tcsetattr");

	printf("ready\n");

	done = 0;
	while (!done) {
		int c;
		int arg;
		uint32_t PC;
		uint32_t v;
		char line[256];

		printf("> ");
		fflush(stdout);
		if (!fgets(line, sizeof(line), stdin))
			break;
		c = line[0];
		arg = 1;

		if (line[1] == ' ') {
			arg = atoi(&line[2]);
			if (line[2] == '0')
				sscanf(&line[2], "%o", &arg);
		}

		switch (c) {
		case 'p':
			cc_pipe();
			break;
		case 'q':
			cc_pipe2();
			break;
		case 'g':
			cc_go();
			break;
		case 'c':
			cc_single_step();
			cc_report_status();
			cc_report_pc_and_ir(&PC);
			break;
		case 's':	// Step.
			for (int i = 0; i < arg; i++) {
				cc_clock();
				cc_report_status();
				cc_report_pc(&PC);
			}
			break;
		case 'u':	// Step until PC.
			printf("run until PC=%o\n", arg);
			while (1) {
				cc_clock();
				cc_report_pc_md_ir(&PC);
				if (PC == arg)
					break;
			}
			break;
		case 'h':	// Halt.
			cc_stop();
			cc_report_status();
			printf("\n");
			cc_report_basic_regs();
			break;
		case 'S':
			cc_test_scratch();
			break;
		case 'r':
			cc_report_basic_regs();
			break;
		case 'I':
			cc_report_ide_regs();
			break;
		case 'R':
			for (int r = 0; r < 027; r++) {
				uint16_t v;
				v = cc_get(r);
				printf("spy reg %o = %06o (0x%x)\n", r, v, v);
			}
			break;
		case 'n':
			cc_set(wr_spy_clk, 2);
			usleep(1000 * 200);
			cc_set(wr_spy_clk, 0);
			usleep(1000 * 200);
			break;
		case 'x':	// Reset.
			cc_set(wr_spy_mode, 0000);
			cc_set(wr_spy_mode, 0301);
			cc_set(wr_spy_mode, 0001);
			break;
		case 'i':
			cc_test_ir();
			break;
		case 'v':
			cc_write_vma(0123456);
			printf("vma=%011o\n", cc_read_vma());
			break;
		case 'd':
			cc_write_md_1s();
			printf("write md ones MD=%011o\n", cc_read_md());

			cc_write_md_0s();
			printf("write md zeros MD=%011o\n", cc_read_md());

			cc_write_md(01234567);
			printf("write md 01234567 MD=%011o\n", cc_read_md());

			cc_write_md(07654321);
			printf("write md 07654321 MD=%011o\n", cc_read_md());

			cc_write_vma(0);
			printf("write vma 0 VMA=%011o\n", cc_read_vma());

			cc_write_vma(01234567);
			printf("write vma 01234567 VMA=%011o\n", cc_read_vma());
			break;
		case 'm':
			cc_write_a_mem(2, 0);
			cc_execute_r(04000100042310050ULL);	// (alu) SETA a=2 m=0 m[0] C=0 alu-> ->VMA,start-read ,m[6]
			v = cc_read_md();
			printf("@0 MD=%011o (0x%x)\n", v, v);
			break;
		case 'G':
			for (int i = 0; i < 4; i++) {
				cc_write_a_mem(2, i);
				verbose = 1;
				cc_execute_r(04000100042310050ULL);	// (alu) SETA a=2 m=0 m[0] C=0 alu-> ->VMA,start-read ,m[6]
				verbose = 0;
				v = cc_read_md();
				printf("@%o MD=%011o (0x%x)\n", i, v, v);
			}
			for (int i = 0776; i < 01000; i++) {
				cc_write_a_mem(2, i);
				cc_execute_r(04000100042310050ULL);	// (alu) SETA a=2 m=0 m[0] C=0 alu-> ->VMA,start-read ,m[6]
				v = cc_read_md();
				printf("@%o MD=%011o (0x%x)\n", i, v, v);
			}
			break;
		case 'a':
			cc_execute_w(04600101442330007ULL);	// (byte) a=2 m=m[3] dpb pos=7, width=1 ->VMA,start-read ,m[6]
			printf("@200 MD=%011o\n", cc_read_md());

			cc_execute_w(00002003042310310ULL);	// (alu) M+A [ADD] a=40 m=6 m[6] C=0 alu-> ->VMA,start-read ,m[6]
			printf("@201 MD=%011o\n", cc_read_md());

			cc_execute_r(00002003000310310ULL);	// (alu) M+A [ADD] a=40 m=6 m[6] C=0 alu-> -><none>,m[6]
			cc_execute_w(00000003042010030ULL);	// (alu) SETM a=0 m=6 m[6] C=0 alu-> ->VMA,start-read ,m[0]
			printf("@202 MD=%011o\n", cc_read_md());

			printf("VMA= %011o\n", cc_read_vma());
			break;
		case 'A':
			for (int r = 0; r < 010; r++) {
				v = cc_read_a_mem(r);
				printf("A[%o] = %011o (0x%x)\n", r, v, v);
			}
			break;
		case 'M':
			for (int r = 0; r < 010; r++) {
				v = cc_read_m_mem(r);
				printf("M[%o] = %011o (0x%x)\n", r, v, v);
			}
			break;
		case 't':
			cc_write_a_mem(1, 01234567);
			cc_write_a_mem(2, 07654321);
			printf("A[0] = %011o\n", cc_read_a_mem(0));
			printf("A[1] = %011o\n", cc_read_a_mem(1));
			printf("A[2] = %011o\n", cc_read_a_mem(2));
			printf("A[3] = %011o\n", cc_read_a_mem(3));
			break;
		case '/':
			switch (line[1]) {
			case 'r':
				printf("%04x\n", mmc_get(0));
				printf("%04x\n", mmc_get(1));
				break;
			case 'i':
				mmc_set(0, 0x0 | (1 << 2));
				break;
			case 'b':
				mmc_set(0, 0x1 | (1 << 2));
				printf("%04x\n", mmc_get(0));
				printf("%04x\n", mmc_get(0));
				printf("%04x\n", mmc_get(0));
				printf("%04x\n", mmc_get(0));
				printf("%04x\n", mmc_get(0));
				break;
			case 'd':
				mmc_set(0, 1 << 3);
				printf("%04x\n", mmc_get(0));
				printf("%04x\n", mmc_get(0));
				printf("%04x\n", mmc_get(0));
				break;
			}
			break;
		}
	}
	sleep(1);
	close(fd);
	exit(0);
}

// Dummy stuff; not used.

unsigned int a_memory[1024];
int disk_fd;
int alt_prom_flag;

char *
sym_find_by_type_val(int mcr, int t, int v)
{
	// Dummy.
	return NULL;
}

int
read_mem(int vaddr, unsigned int *pv)
{
	// Dummy.
	return 0;
}

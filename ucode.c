/*
 * ucode.c
 *
 * very brute-force CADR simulator
 * from AIM-528, "CADR"
 *
 * please remember this is not ment to be fast or pretty.
 * it's ment to be accurate, however.
 *
 * Brad Parker <brad@heeltoe.com>
 * $Id$
 */

#include "ucode.h"

extern ucw_t prom_ucode[512];
ucw_t ucode[16*1024];

unsigned int a_memory[1024];
unsigned int m_memory[32];
unsigned int dispatch_memory[2048];

unsigned int pdl_memory[1024];
int pdl_ptr;
int pdl_index;

int lc;

int spc_stack[32];
int spc_stack_ptr;

struct page_s {
	unsigned int w[256];
};

struct page_s *phy_pages[16*1024];

int l1_map[2048];
int l2_map[1024];

int u_pc;
int cycles;
int trace_cycles;
int max_cycles;
int max_trace_cycles;

int page_fault_flag;
int interrupt_pending_flag;
int sequence_break_flag;

int prom_enabled_flag;
int run_ucode_flag;
int stop_after_prom_flag;

unsigned int md;
unsigned int vma;
unsigned int q;
unsigned int opc;

int write_fault_bit;
int access_fault_bit;

int alu_carry;
unsigned int alu_out;

unsigned int oa_reg_lo;
unsigned int oa_reg_hi;
int oa_reg_lo_set;
int oa_reg_hi_set;

int interrupt_control;
unsigned int dispatch_constant;

int trace;
#define tracef	if (trace) printf
int trace_mcr_labels_flag;
int trace_prom_flag;

extern char *sym_find_by_val(int, int);
extern char *sym_find_last(int, int, int *);

/*
 * map virtual address to physical address,
 * possibly returning l1 mapping
 * possibly returning offset into page
 */
unsigned int
map_vtop(unsigned int virt, int *pl1_map, int *poffset)
{
	int l1_index, l2_index, l1;
	unsigned int l2;

	/* 22 bit address */
	virt &= 017777777;

	if ((virt & 017777400) == 017377400) {
		printf("forcing xbus mapping for disk\n");
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036777;
	}

	if (virt >= 017051757 && virt <= 017051763) {
		printf("disk run light\n");
	}

	/* 11 bit l1 index */
	l1_index = (virt >> 13) & 03777;
	l1 = l1_map[l1_index] & 037;

	if (pl1_map)
		*pl1_map = l1;

	/* 10 bit l2 index */
	l2_index = (l1 << 5) | ((virt >> 8) & 037);
	l2 = l2_map[l2_index];

	if (poffset)
		*poffset = virt & 0377;

	return l2;
}

int
add_new_page_no(int pn)
{
	struct page_s *page;

	if ((page = phy_pages[pn]) == 0) {

		page = (struct page_s *)malloc(sizeof(struct page_s));
		if (page) {
			memset(page, 0, sizeof(struct page_s));
			phy_pages[pn] = page;

			tracef("add_new_page_no(pn=%o)\n", pn);
		}
	}
}

/*
 * read virtual memory
 * returns -1 on fault
 * returns 0 if ok
 */
int
read_mem(int vaddr, unsigned int *pv)
{
	unsigned int map;
	int pn, offset;
	struct page_s *page;

	access_fault_bit = 0;

	map = map_vtop(vaddr, (int *)0, &offset);

	tracef("read_mem(vaddr=%o)\n", vaddr);

	/* 14 bit page # */
	pn = map & 037777;

	tracef("read_mem(vaddr=%o) -> pn %o, offset %o, map %o (%o)\n",
	       vaddr, pn, offset, map, 1 << 23);

	if ((map & (1 << 23)) == 0) {
		/* no access perm */
		access_fault_bit = 1;
		opc = pn;
		tracef("read_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

	if ((page = phy_pages[pn]) == 0) {
		/* page fault */
		page_fault_flag = 1;
		opc = pn;
		tracef("read_mem(vaddr=%o) page fault\n", vaddr);
		return -1;
	}

	/* simulate no memory abovr 2mb */
//	if (pn > 8192 && pn < 9162)
	if (pn == 8192)
	{
		*pv = 0xffffffff;
		return 0;
	}

	if (pn == 037766) {
		/* unibus */
		int paddr = pn << 10;

//tracef("paddr %o\n", paddr);

		switch (offset) {
		case 040:
			printf("unibus: read interrupt status\n");
			*pv = 0;
			return 0;

		case 044:
			printf("unibus: read error status\n");
			*pv = 0;
			return 0;
		}
	}

	if (pn == 036777) {
		int paddr = pn << 10;

		/* 75776000 */
		//printf("disk; paddr=%o\n", paddr);

		tracef("disk register read, offset %o\n", offset);
//printf("a_memory[065] %o\n", a_memory[065]);

		switch (offset) {
		case 0370:
			tracef("disk: read status\n");
			break;
		case 0371:
			tracef("disk: read ma\n");
			break;
		case 0372:
			tracef("disk: read da\n");
			break;
		case 0373:
			tracef("disk: read ecc\n");
			break;
		case 0374:
			tracef("disk status read\n");
			/* disk ready */
			page->w[offset] = disk_get_status();
			break;
		}
	}

	tracef("read_mem(vaddr=%o) -> %o\n", vaddr, page->w[offset]);

	*pv = page->w[offset];
	return 0;
}

/*
 * write virtual memory
 */
int
write_mem(int vaddr, unsigned int v)
{
	unsigned int map;
	int pn, offset;
	struct page_s *page;

	write_fault_bit = 0;

	map = map_vtop(vaddr, (int *)0, &offset);

	tracef("write_mem(vaddr=%o,v=%o)\n", vaddr, v);

	/* 14 bit page # */
	pn = map & 037777;

	tracef("write_mem(vaddr=%o) -> pn %o, offset %o, map %o (%o)\n",
	       vaddr, pn, offset, map, 1 << 22);

	if ((map & (1 << 22)) == 0) {
		/* no write perm */
		write_fault_bit = 1;
		opc = pn;
		return -1;
	}

	if ((page = phy_pages[pn]) == 0) {
		/* page fault */
		page_fault_flag = 1;
		opc = pn;
		return -1;
	}

	if (pn == 037764) {
		offset <<= 1;

		switch (offset) {
		case 0100:
			printf("unibus: kbd low\n");
			break;
		case 0110:
			printf("unibus: beep\n");
			break;
		case 0112:
			printf("unibus: kbd csr\n");
			break;
		case 0120:
		case 0122:
			printf("unibus: usec clock\n");
			break;
		case 0140:
			printf("unibus: chaos\n");
			break;
		}
	}

	if (pn == 037766) {
		/* unibus */
		int paddr = pn << 12;

		//tracef("paddr %o, offset %o\n", paddr, offset);

		offset <<= 1;

		if (offset <= 036) {
			printf("unibus: spy v %o, offset %o\n",
			       vaddr, offset);

			switch (offset) {
			case 012:
				if ((v & 044) == 044) {
					printf("unibus: "
					       "disabling prom enable flag\n");
					prom_enabled_flag = 0;
				}
				if (v & 2) {
					printf("unibus: normal speed\n");
				}

				break;
			}

			return 0;
		}

		switch (offset) {
		case 040:
			printf("unibus: write interrupt status %o\n", v);
			return 0;

		case 042:
			printf("unibus: write interrupt stim %o\n", v);
			return 0;

		case 044:
			printf("unibus: clear bus error %o\n", v);
			return 0;

		default:
			if (offset >= 0140 && offset <= 0176) {
				printf("unibus: mapping reg \n");
				return 0;
			}

			printf("unibus: write? v %o, offset %o\n",
			       vaddr, offset);
		}

	}

	/* disk controller */
	if (pn == 036777) {

		tracef("disk register write, offset %o <- %o\n", offset, v);

		switch (offset) {
		case 0374:
			disk_set_cmd(v);
			tracef("disk: load cmd %o\n", v);
			break;
		case 0375:
			printf("disk: load clp %o (phys page %o)\n", v, v << 8);
			disk_set_clp(v);
			break;
		case 0376:
			disk_set_da(v);
			tracef("disk: load da %o\n", v);
			break;
		case 0377:
			disk_start();
			break;
		}
	}

	page->w[offset] = v;
	return 0;
}

void
write_ucode(int addr, ucw_t w)
{
	tracef("u-code write; %Lo @ %o\n", w, addr);
	ucode[addr] = w;
}

void
write_a_mem(int loc, unsigned int v)
{
	tracef("a_memory[%o] <- %o\n", loc, v);
	a_memory[loc] = v;
}

unsigned int
read_a_mem(int loc)
{
	return a_memory[loc];
}

unsigned int
read_m_mem(int loc)
{
	if (loc > 32) {
		printf("read m-memory address > 32! (%o)\n", loc);
	}

	return m_memory[loc];
}

void
write_m_mem(int loc, unsigned int v)
{
	m_memory[loc] = v;
	a_memory[loc] = v;
	tracef("a,m_memory[%o] <- %o\n", loc, v);
}

#define USE_PDL_PTR 1
#define USE_PDL_INDEX 2

void
write_pdl_mem(int which, unsigned int v)
{
	switch (which) {
	case USE_PDL_PTR:
		if (pdl_ptr > 1024) {
			printf("pdl ptr %o!\n", pdl_ptr);
			return;
		}
		pdl_memory[pdl_ptr] = v;
		break;
	case USE_PDL_INDEX:
		if (pdl_index > 1024) {
			printf("pdl ptr %o!\n", pdl_index);
			return;
		}
		pdl_memory[pdl_index] = v;
		break;
	}
}

unsigned int
rotate_left(unsigned int v, int rot)
{
	int i, c;

	/* silly, but simple */
	for (i = 0; i < rot; i++) {
		c = v & 0x80000000;
		v <<= 1;
		if (c) v |= 1;
	}

	return v;
}

void
push_spc(int pc)
{
	tracef("writing spc[%o] <- %o\n", spc_stack_ptr, pc);
	spc_stack[spc_stack_ptr++] = pc;
	if (spc_stack_ptr == 32)
		spc_stack_ptr = 0;
}

int
pop_spc(void)
{
	if (spc_stack_ptr == 0)
		spc_stack_ptr = 32;
	tracef("reading spc[%o] -> %o\n", spc_stack_ptr-1, spc_stack[spc_stack_ptr-1]);
	return spc_stack[--spc_stack_ptr];
}

/*
 * write value to decoded destination
 */
void
write_dest(ucw_t u, int dest, unsigned int out_bus)
{
	if (dest & 04000) {
		write_a_mem(dest & 03777, out_bus);
		return;
	}

	switch (dest >> 5) {
		/* case 0: none */
	case 1: /* LC (location counter) */
		tracef("writing LC <- %o\n", out_bus);
		lc = out_bus;
		break;
	case 2: /* interrrupt control <29-26> */
		tracef("writing IC <- %o\n", out_bus);
		interrupt_control = out_bus;
		if (interrupt_control & (1 << 26))
			printf("ic: sequence break request\n");
		if (interrupt_control & (1 << 27))
			printf("ic: interrupt enable\n");
		if (interrupt_control & (1 << 28))
			printf("ic: bus reset\n");
		if (interrupt_control & (1 << 29))
			printf("ic: lc byte mode\n");
		break;
	case 010: /* PDL (addressed by Pointer) */
		tracef("writing pdl[%o] <- %o\n",
		       pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
		break;
	case 011: /* PDL (addressed by pointer, push */
		pdl_ptr = (pdl_ptr + 1) & 01777;
		tracef("writing pdl[%o] <- %o, push\n",
		       pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
		break;
	case 012: /* PDL (addressed by index) */
		tracef("writing pdl[%o] <- %o\n",
		       pdl_index, out_bus);
		write_pdl_mem(USE_PDL_INDEX, out_bus);
		break;
	case 013: /* PDL index */
		tracef("pdl index <- %o\n", out_bus);
		pdl_index = out_bus & 01777;
		break;
	case 014: /* PDL pointer */
		tracef("pdl ptr <- %o\n", out_bus);
		pdl_ptr = out_bus & 01777;
		break;

	case 015: /* SPC data, push */
		push_spc(out_bus);
		break;

	case 016: /* Next instruction modifier (lo) */
		oa_reg_lo = out_bus & 0377777777;
		oa_reg_lo_set = 1;
		tracef("setting oa_reg lo %o\n", oa_reg_lo);
		break;
	case 017: /* Next instruction modifier (hi) */
		oa_reg_hi = out_bus;
		oa_reg_hi_set = 1;
		tracef("setting oa_reg hi %o\n", oa_reg_hi);
		break;

	case 020: /* VMA register (memory address) */
		vma = out_bus;
		break;

	case 021: /* VMA register, start main memory read */
		vma = out_bus;
		if (read_mem(vma, &md)) {
		}
		break;

	case 022: /* VMA register, start main memory write */
		vma = out_bus;
		if (write_mem(vma, md)) {
		}
		break;

	case 023: /* VMA register, write map */
		/* vma-write-map */
		vma = out_bus;

		tracef("vma-write-map md=%o, vma=%o (addr %o)\n",
		       md, vma, md >> 13);

	write_map:
		if ((vma >> 26) & 1) {
			int l1_index, l1_data;
			l1_index = (md >> 13) & 03777;
			l1_data = (vma >> 27) & 037;
			l1_map[l1_index] = l1_data;

//if (l1_index > 03000) printf("l1_map[%o] <- %o\n", l1_index, l1_data);
			tracef("l1_map[%o] <- %o\n", l1_index, l1_data);
		}

		if ((vma >> 25) & 1) {
			int l1_index, l2_index, l1_data;
			unsigned int l2_data;
			l1_index = (md >> 13) & 03777;
			l1_data = l1_map[l1_index];

			l2_index = (l1_data << 5) | ((md >> 8) & 037);
			l2_data = vma;
			l2_map[l2_index] = l2_data;

//if (l1_index > 03000) printf("l2_map[%o] <- %o\n", l2_index, l2_data);
			tracef("l2_map[%o] <- %o\n", l2_index, l2_data);

			add_new_page_no(l2_data & 037777);
		}
		break;

	case 030: /* MD register (memory data) */
		md = out_bus;
		tracef("md<-%o\n", md);
		break;

	case 031:
		md = out_bus;
		if (read_mem(vma, &md)) {
		}
		break;

	case 032:
		md = out_bus;
		if (write_mem(vma, md)) {
		}
		break;

	case 033: /* MD register,write map like 23 */
		/* memory-data-write-map */
		md = out_bus;
		tracef("memory-data-write-map md=%o, vma=%o (addr %o)\n",
		       md, vma, md >> 13);
		goto write_map;
		break;
	}

	write_m_mem(dest & 037, out_bus);
}

int
run(void)
{
	int old_pc;
	int no_incr_pc = 0;
	int fetch_next = 0;
	int trace_pt, trace_pt_count;
	char *sym, *last_sym = 0;

	u_pc = 0;
	prom_enabled_flag = 1;
	run_ucode_flag = 1;
	trace_mcr_labels_flag = 1;

//	trace_prom_flag = 1;
//	stop_after_prom_flag = 1;

	trace_pt = 0;
	trace_pt_count = 0;

//	trace = 1;
//	max_cycles = 300000;
//	max_cycles = 350000;
	max_cycles = 600000;
//	max_trace_cycles = 100;

//	sym_find(1, "INIMAP5", &trace_pt);
//	sym_find(1, "CLEAR-I-MEMORY", &trace_pt);

//	sym_find(1, "MEM-SIZE-LOOP", &trace_pt);
//	sym_find(1, "COLD-SWAP-IN", &trace_pt);

//	sym_find(1, "COLD-READ-LABEL", &trace_pt);
//	max_trace_cycles = 1000;

	sym_find(1, "START-DISK-N-PAGES", &trace_pt);
	trace_pt_count = 3;
	max_trace_cycles = 1000;


	printf("run:\n");

#if 1
	/* short out some really long loops */
	prom_ucode[0244] = 0;
	prom_ucode[0251] = 0;
	prom_ucode[0256] = 0;
#endif

#if 0
	/* test unibus prom enable flag */
	prom_ucode[0504] = 0;
	prom_ucode[0510] = 0;
#endif

#if 1
	prom_ucode[0452] = 04000001000310030; /* m-c <- m-zero */
#endif

	while (run_ucode_flag) {
		int a_src, m_src, new_pc, dest, alu_op;
		int r_bit, p_bit, n_bit, ir8, ir7;
		int invert_sense, take_jump;
		int m_src_value, a_src_value;

		int widthm1, pos;
		int mr_sr_bits;
		unsigned int left_mask, right_mask, mask;
		int left_mask_index, right_mask_index;

		int disp_cont, disp_addr;
		int map, len, rot;
		int out_bus;
		int carry_in, do_add, do_sub;
		int pc;

		long long lv;

		ucw_t u, w;

		int n_plus1, enable_ish;
		int i_long;

#if 0
		/* test unibus prom enable flag */
		if (u_pc == 0421) u_pc = 0477;
#endif

		/* see if we hit a trace point */
		if (u_pc == trace_pt && trace == 0) {
			if (trace_pt_count) {
				if (--trace_pt_count == 0)
					trace = 1;
			} else
				trace = 1;


			if (trace)
				printf("trace on\n");
		}

		if (stop_after_prom_flag) {
			if (prom_enabled_flag == 0) run_ucode_flag = 0;
		}

		if (trace_prom_flag) {
			if (prom_enabled_flag == 1) trace = 1;
		}

		/* we need to execute instruction after jump first */
		if (fetch_next) {
			tracef("fetch_next; old_pc %o, u_pc %o\n",
			       old_pc, u_pc);
			pc = old_pc + 1;
			old_pc--;
			no_incr_pc = 1;
			fetch_next = 0;
		} else {
			old_pc = u_pc;
			pc = u_pc;
		}

		/* fetch from prom or ram */
		if (prom_enabled_flag)
			u = prom_ucode[pc];
		else
			u = ucode[pc];

		/* next-instruction modify */
		if (oa_reg_lo_set) {
			tracef("merging oa lo %o\n", oa_reg_lo);
			oa_reg_lo_set = 0;
			u |= oa_reg_lo;
		}

		if (oa_reg_hi_set) {
			tracef("merging oa hi %o\n", oa_reg_hi);
			oa_reg_hi_set = 0;
			u |= (ucw_t)oa_reg_hi << 26;
		}

		/* enforce max trace count */
		if (trace) {
			if (max_trace_cycles && trace_cycles++ > max_trace_cycles) {
				printf("trace cycle count exceeded, pc %o\n", u_pc);
				break;
			}
		}

		/* enforce max cycles */
		if (max_cycles && cycles++ > max_cycles) {
			int offset;
			printf("cycle count exceeded, pc %o\n", u_pc);

			if (sym = sym_find_last(1, pc, &offset)) {
				if (offset == 0)
					printf("%s:\n", sym);
				else
					printf("%s+%o:\n", sym, offset);
			}

			break;
		}

		i_long = (u >> 45) & 1;

		if (trace) {
			int offset;

			printf("------\n");

#if 1
			if (sym = sym_find_by_val(1, pc)) {
				printf("%s:\n", sym);
			}
#else
			if (sym = sym_find_last(1, pc, &offset)) {
				if (offset == 0)
					printf("%s:\n", sym);
				else
					printf("%s+%o:\n", sym, offset);
			}
#endif

			printf("%03o %016Lo %s\n",
			       pc, u, i_long ? "(i-long)" : "");
			disassemble_ucode_loc(pc, u);
		}

		/* trace label names in mcr */
		if (trace_mcr_labels_flag) {
			if (!prom_enabled_flag) {
				int offset;
				if (sym = sym_find_last(1, pc, &offset)) {
					if (offset == 0 && sym != last_sym) {
						printf("%s:\n", sym);
						last_sym = sym;
					}
				}
			}
		}

		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;

		/* get A source value */
		a_src_value = read_a_mem(a_src);

		/* calculate M source value */
		if (m_src & 040) {
			unsigned int l2_data, l1_data;

			switch (m_src & 037) {
			case 0: /* dispatch constant */
				m_src_value = dispatch_constant;
				break;
			case 1: /* SPC pointer <28-24>, SPC data <18-0> */
				m_src_value = (spc_stack_ptr << 24) |
					(spc_stack[spc_stack_ptr] & 0777777);
				break;
			case 2: /* PDL pointer <9-0> */
				m_src_value = pdl_ptr & 01777;
				break;
			case 3: /* PDL index <9-0> */
				m_src_value = pdl_index & 01777;
				break;
			case 5: /* PDL buffer (addressed by index) */
				tracef("reading pdl[%o] -> %o\n",
				       pdl_index, pdl_memory[pdl_index]);

				m_src_value = pdl_memory[pdl_index];
				break;
			case 6: /* OPC registers <13-0> */
				m_src_value = opc;
				break;
			case 7: /* Q register */
				m_src_value = q;
				break;
			case 010: /* VMA register (memory address) */
				m_src_value = vma;
				break;
			case 011: /* MAP[MD] */
				/* memory-map-data, or "map[MD]" */
				l2_data = map_vtop(md, &l1_data, (int *)0);
				
				m_src_value = 
					(write_fault_bit << 31) |
					(access_fault_bit << 30) |
					(l1_data << 24) |
					(l2_data & 077777777);

				if (trace) {
					printf("l1_data %o, l2_data %o\n",
					       l1_data, l2_data);

					printf("read map[md=%o] -> %o\n",
					       md, m_src_value);
				}
				break;
			case 012:
				m_src_value = md;
				break;
			case 013:
				m_src_value = lc;
				break;
			case 014:
				m_src_value = (spc_stack_ptr << 24) |
					(spc_stack[spc_stack_ptr] & 0777777);
				spc_stack_ptr--;
				break;

			case 024:
				tracef("reading pdl[%o] -> %o, pop\n",
				       pdl_ptr, pdl_memory[pdl_ptr]);

				m_src_value = pdl_memory[pdl_ptr];
				pdl_ptr = (pdl_ptr - 1) & 01777;
				break;
			case 025:
				tracef("reading pdl[%o] -> %o\n",
				       pdl_ptr, pdl_memory[pdl_ptr]);

				m_src_value = pdl_memory[pdl_ptr];
				break;
			}
		} else {
			m_src_value = read_m_mem(m_src);
		}

		/*
		 * decode instruction
		 */

		switch ((u >> 43) & 03) {
		case 0: /* alu */

			/* nop short cut */
			if ((u & 03777777777777777) == 0) {
				goto next;
			}

			dest = (u >> 14) & 07777;
			out_bus = (u >> 12) & 3;
			ir8 = (u >> 8) & 1;
			ir7 = (u >> 7) & 1;
			carry_in = (u >> 2) & 1;

			alu_op = (u >> 3) & 017;

			if (trace) {
				printf("a=%o (%o), m=%o (%o)\n",
				       a_src, a_src_value,
				       m_src, m_src_value);

				printf("alu_op %o, ir8 %o, ir7 %o, c %o, "
				       "dest %o, out_bus %d\n",
				       alu_op, ir8, ir7, carry_in,
				       dest, out_bus);
			}


		        if (ir8 == 0 && ir7 == 0) {
				/* logic */
				alu_carry = 0;
				switch (alu_op) {
				case 0: /* [AND] */
					alu_out = 0;
					break;
				case 1: /* [AND] */
					alu_out = m_src_value & a_src_value;
					break;
				case 2: /* [ANDCA] */
					alu_out = m_src_value & ~a_src_value;
					break;
				case 3: /* [SETM] */
					alu_out = m_src_value;
					break;
				case 4: /* [ANDCM] */
					alu_out = ~m_src_value & a_src_value;
					break;
				case 5: /* [SETA] */
					alu_out = a_src_value;
					break;
				case 6: /* [XOR] */
					alu_out = m_src_value ^ a_src_value;
					break;
				case 7: /* [IOR] */
					alu_out = m_src_value | a_src_value;
					break;
				case 010: /* [ANDCB] */
					alu_out = ~a_src_value & ~m_src_value;
					break;
				case 011: /* [EQV] */
					alu_out = a_src_value == m_src_value;
					break;
				case 012: /* [SETCA] */
					alu_out = ~a_src_value;
					break;
				case 013: /* [ORCA] */
					alu_out = m_src_value | ~a_src_value;
					break;
				case 014: /* [SETCM] */
					alu_out = ~m_src_value;
					break;
				case 015: /* [ORCM] */
					alu_out = ~m_src_value | a_src_value;
					break;
				case 016: /* [ORCB] */
					alu_out = ~m_src_value | ~a_src_value;
					break;
				case 017: /* [ONES] */
					alu_out = ~0;
					break;
				}
			}

			if (ir8 == 0 && ir7 == 1) {
				/* arithmetic */
				switch (alu_op) {
				case 0: /* -1 */
					alu_out = carry_in ? 0 : -1;
					alu_carry = 0;
					break;
				case 1: /* (M&A)-1 */
					lv = (m_src_value & a_src_value) -
						(carry_in ? 0 : 1);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 2: /* (M&~A)-1 */
					lv = (m_src_value & ~a_src_value) -
						(carry_in ? 0 : 1);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 3: /* M-1 */
					lv = m_src_value - (carry_in ? 0 : 1);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 4: /* M|~A */
					lv = (m_src_value | ~a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 5: /* (M|~A)+(M&A) */
					lv = (m_src_value | ~a_src_value) +
						(m_src_value & a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 6: /* M-A-1 [SUB] */
					lv = m_src_value - a_src_value -
						(carry_in ? 0 : 1);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 7: /* (M|~A)+M */
					lv = (m_src_value | ~a_src_value) +
						m_src_value +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 010: /* M|A */
					lv = m_src_value | a_src_value +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 011: /* M+A [ADD] */
					lv = a_src_value + m_src_value +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 012: /* (M|A)+(M&~A) */
					lv = (m_src_value | a_src_value) +
						(m_src_value & ~a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 013: /* (M|A)+M */
					lv = (m_src_value | a_src_value) +
						m_src_value +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 014: /* M */
					lv = m_src_value + (carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
//printf("m_src_value %o, carry_in %d, alu_out %o\n", m_src_value, carry_in, alu_out);
					break;
				case 015: /* M+(M&A) */
					lv = m_src_value +
						(m_src_value & a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 016: /* M+(M|~A) */
					lv = m_src_value +
						(m_src_value | ~a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 017: /* M+M */
					lv = m_src_value + m_src_value +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				}
			}

			if (ir8 == 1) {
				/* conditional alu op code */
				switch (alu_op) {
				case 0: /* multiply step */
					/* ADD if Q<0>=1, else SETM */
					do_add = q & 1;
					if (do_add) {
						lv = a_src_value +
							m_src_value +
							(carry_in ? 1 : 0);
						alu_out = lv;
						alu_carry = (lv >> 32) ? 1 : 0;
					} else {
						alu_out = m_src_value;
						alu_carry = 0;
					}
					break;
				case 1: /* divide step */
					tracef("divide step\n");
if (out_bus == 1) {
goto alu_done;
}
					do_sub = q & 1;
tracef("do_sub %d\n", do_sub);
					if (do_sub) {
						lv =
							m_src_value -
							a_src_value -
							(carry_in ? 1 : 0);
					} else {
						lv =
							m_src_value +
							a_src_value +
							(carry_in ? 1 : 0);
					}
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 5: /* remainder correction */
					tracef("remainder correction\n");
					do_sub = q & 1;
					if (a_src_value & 0x80000000)
						do_add = !do_add;
tracef("do_sub %d\n", do_sub);
					if (do_sub) {
						/* setm */
//						alu_out = m_src_value;
//						alu_out = q;
						alu_carry = 0;
					} else {
						lv =
							alu_out +
							a_src_value +
							(carry_in ? 1 : 0);
						alu_out = lv;
						alu_carry = (lv >> 32) ? 1 : 0;
					}

//					q >>= 1; 
//					alu_out >>= 1;

					break;
				case 011:
					/* initial divide step */
					tracef("divide-first-step\n");
#if 0
q = 4;
q = 8;
a_src_value = 2;
a_memory[011] = 2;

q = 10/2;
a_src_value = 3;
a_memory[011] = 3;
#endif
tracef("divide: %o / %o \n", q, a_src_value);

					lv = m_src_value -
						a_src_value -
						(carry_in ? 1 : 0);

					alu_out = lv;
tracef("alu_out %08x %o %d\n", alu_out, alu_out, alu_out);
					alu_carry = (lv >> 32) ? 1 : 0;
					break;

				default:
					printf("UNKNOWN cond alu op code %o\n",
					       alu_op);
				}
			}

			/* Q control */
			switch (u & 3) {
			case 1:
				tracef("q<<\n");
				q <<= 1;
				/* inverse of alu sign */
				if ((alu_out & 0x80000000) == 0)
					q |= 1;
				break;
			case 2:
				tracef("q>>\n");
				q >>= 1;
				if (alu_out & 1)
					q |= 0x80000000;
				break;
			case 3:
				tracef("q<-alu\n");
				q = alu_out;
				break;
			}

			switch (out_bus) {
			case 1: out_bus = alu_out;
				break;
			case 2: out_bus = (alu_out >> 1) | 
					(alu_out & 0x80000000);
				break;
			case 3: out_bus = (alu_out << 1) | 
					((q & 0x80000000) ? 1 : 0);
				break;
			}

			write_dest(u, dest, out_bus);

			tracef("alu_out 0x%08x, alu_carry %d, q 0x%08x\n",
			       alu_out, alu_carry, q);
		alu_done:
			break;

		case 1: /* jump */
			new_pc = (u >> 12) & 037777;

			tracef("a=%o (%o), m=%o (%o)\n",
			       a_src, a_src_value,
			       m_src, m_src_value);

			r_bit = (u >> 9) & 1;
			p_bit = (u >> 8) & 1;
			n_bit = (u >> 7) & 1;
			invert_sense = (u >> 6) & 1;
			take_jump = 0;

		process_jump:
			if (!n_bit) {
				fetch_next = 1;
			}

#if 0
			if (p_bit) {
				if (fetch_next)
					push_spc(u_pc+2);
				else
					push_spc(u_pc+1);
			}

			if (p_bit && r_bit) {
				w = ((ucw_t)(a_src_value & 0177777) << 32) |
					(unsigned int)m_src_value;
				write_ucode(new_pc, w);
			}
#endif

			if (u & (1<<5)) {
				switch (u & 017) {
				case 0: break;
				case 1:
					take_jump = m_src_value < a_src_value;
					break;
				case 2:
					take_jump = m_src_value <= a_src_value;
//					tracef("%o <= %o; take_jump %o\n",
//					       m_src_value, a_src_value, take_jump);
					break;
				case 3:
					take_jump = m_src_value == a_src_value;
					break;
				case 4: 
					take_jump = page_fault_flag;
					break;
				case 5:
					take_jump = page_fault_flag |
						interrupt_pending_flag;
					break;
				case 6:
					take_jump = page_fault_flag |
						interrupt_pending_flag |
						sequence_break_flag;
					break;
				case 7:
					take_jump = 1;
					break;
				}
			} else {
				rot = u & 037;
				tracef("jump-if-bit; rot %o, before %o ",
				       rot, m_src_value);
				m_src_value = rotate_left(m_src_value, rot);
				tracef("after %o\n", m_src_value);
				take_jump = m_src_value & 1;
			}
 
			if (invert_sense)
				take_jump = !take_jump;

#if 1
			if (p_bit && take_jump) {
				if (fetch_next)
					push_spc(u_pc+2);
				else
					push_spc(u_pc+1);
			}

			if (p_bit && r_bit) {
				w = ((ucw_t)(a_src_value & 0177777) << 32) |
					(unsigned int)m_src_value;
				write_ucode(new_pc, w);
			}
#endif

			if (r_bit && take_jump) {
				new_pc = pop_spc();
				no_incr_pc = 1;
			}

			if (take_jump) {

				if (new_pc == u_pc && !fetch_next) {
					printf("loop detected pc %o\n", u_pc);
					run_ucode_flag = 0;
				}

				old_pc = u_pc;
				u_pc = new_pc;
				no_incr_pc = 1;
			} else {
/* this is bogus; we should only set fetch_next if take_jump and we should
   use !n_bit above, instead of fetch_next */
				fetch_next = 0;
			}
			break;

		case 2: /* dispatch */
			disp_cont = (u >> 32) & 01777;

			n_plus1 = (u >> 25) & 1;
			enable_ish = (u >> 24) & 1;
			disp_addr = (u >> 12) & 03777;
			map = (u >> 8) & 3;
			len = (u >> 5) & 07;
			pos = u & 037;

			/* */
			if (((u >> 10) & 3) == 2) {
				tracef("dispatch_memory[%o] <- %o\n",
				       disp_addr, a_src_value);
				dispatch_memory[disp_addr] = a_src_value;
				goto dispatch_done;
			}

			tracef("addr %o, map %o, len %o, rot %o\n",
			       disp_addr, map, len, rot);

			/* rotate m-source */
			m_src_value = rotate_left(m_src_value, rot);

			/* generate mask */
			left_mask_index = (len - 1) & 037;

			mask = ~0;
			mask >>= 31 - left_mask_index;

			/* put ldb into dispatch-addr */
			disp_addr |= m_src_value & mask;

			/* tweek dispatch-addr with l2 map bits */
			if (map) {
				int l2_map, bit14, bit15;

				disp_addr &= ~1;

				l2_map = map_vtop(md, (int *)0, (int *)0);

				bit14 = (l2_map & (1 << 14)) ? 1 : 0;
				bit15 = (l2_map & (1 << 15)) ? 1 : 0;

				switch (map) {
				case 1: disp_addr |= bit14; break;
				case 2: disp_addr |= bit15; break;
				case 3: disp_addr |= bit14 | bit15; break;
				}
			}

			disp_addr = dispatch_memory[disp_addr];

			dispatch_constant = disp_cont;

			/* xxx - I need page 18! */
			new_pc = disp_addr & 037777;

			r_bit = (disp_addr >> 15) & 1;
			p_bit = (disp_addr >> 16) & 1;
			n_bit = (disp_addr >> 17) & 1;

			invert_sense = 0;
			take_jump = 1;
			u = 1<<5;

			goto process_jump;

		dispatch_done:
			break;

		case 3: /* byte */
			tracef("a=%o (%o), m=%o (%o)\n",
			       a_src, a_src_value,
			       m_src, m_src_value);

			dest = (u >> 14) & 07777;
			mr_sr_bits = (u >> 12) & 3;

			widthm1 = (u >> 5) & 037;
			pos = u & 037;

			if (mr_sr_bits & 2)
				right_mask_index = pos;
			else
				right_mask_index = 0;

			left_mask_index = (right_mask_index + widthm1) & 037;

			left_mask = ~0;
			right_mask = ~0;

			left_mask >>= 31 - left_mask_index;
			right_mask <<= right_mask_index;

			mask = left_mask & right_mask;

			tracef("widthm1 %o, pos %o, mr_sr_bits %o\n",
			       widthm1, pos, mr_sr_bits);

			tracef("left_mask_index %o, right_mask_index %o\n",
				left_mask_index, right_mask_index);

			tracef("left_mask %o, right_mask %o, mask %o\n",
			       left_mask, right_mask, mask);

			out_bus = 0;

			switch (mr_sr_bits) {
			case 0:
				break;
			case 1: /* ldb */
				tracef("ldb; m %o\n", m_src_value);

				m_src_value = rotate_left(m_src_value, pos);

				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);

				tracef("ldb; mask %o, result %o\n", 
				       mask, out_bus);
				break;
			case 2: /* selective desposit */
				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);
				break;
			case 3: /* dpb */
				tracef("dpb; m %o, pos %o\n", 
				       m_src_value, pos);

				/* mask is already rotated */

				m_src_value = rotate_left(m_src_value, pos);

				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);

				tracef("dpb; mask %o, result %o\n", 
				       mask, out_bus);
				break;
			}

			write_dest(u, dest, out_bus);
			break;
		}

		/*
		 * this fetch_next thing is such a hack;  I should
		 * just make a "pc fifo" which simulates the fetch pipe
		 * in the hardware and feed pc's into that...
		 */

		if ((u >> 42) & 1) {
			tracef("popj; ");
			u_pc = pop_spc();
			no_incr_pc = 1;
			fetch_next = 1;
		}
		
	next:
		if (no_incr_pc)
			no_incr_pc = 0;
		else
			u_pc++;
	}
}

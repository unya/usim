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

int page_fault_flag;
int interrupt_pending_flag;
int sequence_break_flag;

int prom_enabled_flag;

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

			printf("add_new_page_no(pn=%o)\n", pn);
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

	printf("read_mem(vaddr=%o)\n", vaddr);

	/* 14 bit page # */
	pn = map & 037777;

	printf("read_mem(vaddr=%o) -> pn %o, offset %o, map %o (%o)\n",
	       vaddr, pn, offset, map, 1 << 23);

	if ((map & (1 << 23)) == 0) {
		/* no access perm */
		access_fault_bit = 1;
		opc = pn;
		printf("read_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

	if ((page = phy_pages[pn]) == 0) {
		/* page fault */
		page_fault_flag = 1;
		opc = pn;
		printf("read_mem(vaddr=%o) page fault\n", vaddr);
		return -1;
	}

	if (pn == 037766) {
		int paddr = pn << 10;
		printf("paddr %o\n", paddr);
	}

	if (pn == 036777) {
		int paddr = pn << 10;

		printf("disk register read, offset %o\n", offset);

		switch (offset) {
		case 0370:
			printf("disk: read status\n");
			break;
		case 0371:
			printf("disk: read ma\n");
			break;
		case 0372:
			printf("disk: read da\n");
			break;
		case 0373:
			printf("disk: read ecc\n");
			break;
		case 0374:
			printf("disk status read\n");
			/* disk ready */
			page->w[offset] = disk_get_status();
			break;
		}
	}

	printf("read_mem(vaddr=%o) -> %o\n", vaddr, page->w[offset]);

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

	printf("write_mem(vaddr=%o,v=%o)\n", vaddr, v);

	/* 14 bit page # */
	pn = map & 037777;

	printf("write_mem(vaddr=%o) -> pn %o, offset %o, map %o (%o)\n",
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

	if (pn == 037766) {
		/* unibus */
		int paddr = pn << 12;

		//printf("paddr %o, offset %o\n", paddr, offset);

		switch (offset) {
		case 5:
			if (v == 044) {
				printf("unibus: disabling prom enable flag\n");
				prom_enabled_flag = 0;
			}
			break;
		}
	}

	/* disk controller */
	if (pn == 036777) {

		printf("disk register write, offset %o <- %o\n", offset, v);

		switch (offset) {
		case 0374:
			disk_set_cmd(v);
			printf("disk: load cmd %o\n", v);
			break;
		case 0375:
			printf("disk: load clp %o\n", v);
			disk_set_clp(v);
			break;
		case 0376:
			disk_set_da(v);
			printf("disk: load da %o\n", v);
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
	printf("u-code write; %Lo @ %o\n", w, addr);
}

void
write_a_mem(int loc, unsigned int v)
{
	printf("a_memory[%o] <- %o\n", loc, v);
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
	if (loc > 32) 
		printf("read m-memory address > 32! (%o)\n", loc);

	return m_memory[loc];
}

void
write_m_mem(int loc, unsigned int v)
{
	m_memory[loc] = v;
	printf("m_memory[%o] <- %o\n", loc, v);
	a_memory[loc] = v;
	printf("a_memory[%o] <- %o\n", loc, v);
}

void
write_pdl_mem(int which, unsigned int v)
{
	switch (which) {
	case 1:
		if (pdl_ptr > 1024) {
			printf("pdl ptr %o!\n", pdl_ptr);
			return;
		}
		pdl_memory[pdl_ptr] = v;
		break;
	case 2:
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
	printf("writing spc[%o] <- %o\n", spc_stack_ptr, pc);
	spc_stack[spc_stack_ptr++] = pc;
	if (spc_stack_ptr == 32)
		spc_stack_ptr = 0;
}

int
pop_spc(void)
{
	if (spc_stack_ptr == 0)
		spc_stack_ptr = 32;
	printf("reading spc[%o] -> %o\n", spc_stack_ptr-1, spc_stack[spc_stack_ptr-1]);
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
	case 1:
		printf("writing LC <- %o\n", out_bus);
		lc = out_bus;
		break;
	case 2:
		printf("writing IC <- %o\n", out_bus);
		interrupt_control = out_bus;
		if (interrupt_control & (1 << 26))
			/* sequence break request */;
		if (interrupt_control & (1 << 27))
			/* interrupt enable */;
		if (interrupt_control & (1 << 28))
			printf("bus reset\n");
		if (interrupt_control & (1 << 29))
			/* lc byte mode */;
		break;
	case 010:
		printf("writing pdl[%o] <- %o\n",
		       pdl_ptr, out_bus);
		write_pdl_mem(1, out_bus);
		break;
	case 011:
		if (pdl_ptr == 0)
			pdl_ptr = 02000;
		pdl_ptr--;
		printf("writing pdl[%o] <- %o, push\n",
		       pdl_ptr, out_bus);
		write_pdl_mem(1, out_bus);
		break;
	case 012:
		printf("writing pdl[%o] <- %o\n",
		       pdl_index, out_bus);
		write_pdl_mem(2, out_bus);
		break;
	case 013:
		printf("pdl index <- %o\n", out_bus);
		pdl_index = out_bus;
		break;
	case 014:
		printf("pdl ptr <- %o\n", out_bus);
		pdl_ptr = out_bus;
		break;
	case 015:
		push_spc(out_bus);
		break;

	case 016:
		oa_reg_lo = out_bus & 0377777777;
		oa_reg_lo_set = 1;
		printf("setting oa_reg lo %o\n", oa_reg_lo);
		break;
	case 017:
		oa_reg_hi = out_bus;
		oa_reg_hi_set = 1;
		printf("setting oa_reg hi %o\n", oa_reg_hi);
		break;

	case 020:
		vma = out_bus;
		break;

	case 021:
		vma = out_bus;
		if (read_mem(vma, &md)) {
		}
		break;

	case 022:
		vma = out_bus;
		if (write_mem(vma, md)) {
		}
		break;

	case 023:
		/* vma-write-map */
		vma = out_bus;

		printf("vma-write-map md=%o, vma=%o (addr %o)\n",
		       md, vma, md >> 13);

	write_map:
		if ((vma >> 26) & 1) {
			int l1_index, l1_data;
			l1_index = (md >> 13) & 03777;
			l1_data = (vma >> 27) & 037;
			l1_map[l1_index] = l1_data;

			printf("l1_map[%o] <- %o\n", l1_index, l1_data);
		}

		if ((vma >> 25) & 1) {
			int l1_index, l2_index, l1_data;
			unsigned int l2_data;
			l1_index = (md >> 13) & 03777;
			l1_data = l1_map[l1_index];

			l2_index = (l1_data << 5) | ((md >> 8) & 037);
			l2_data = vma;
			l2_map[l2_index] = l2_data;

			printf("l2_map[%o] <- %o\n", l2_index, l2_data);

			add_new_page_no(l2_data & 037777);
		}
		break;

	case 030:
		md = out_bus;
		printf("md<-%o\n", md);
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

	case 033:
		/* memory-data-write-map */
		md = out_bus;
		printf("memory-data-write-map md=%o, vma=%o (addr %o)\n",
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

	u_pc = 0;
	prom_enabled_flag = 1;

	printf("run:\n");

#if 1
	/* short out some really long loops */
	prom_ucode[0245] = 0;
	prom_ucode[0252] = 0;
	prom_ucode[0257] = 0;
#endif

#if 0
	/* test unibus prom enable flag */
	prom_ucode[0505] = 0;
	prom_ucode[0511] = 0;
#endif

	while (1) {
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

#if 0
		/* test unibus prom enable flag */
		if (u_pc == 0421) u_pc = 0477;
#endif

		/* do we need to execute instruction after jump first? */
		if (fetch_next) {
printf("fetch_next; old_pc %o, u_pc %o\n", old_pc, u_pc);
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
			printf("merging oa lo %o\n", oa_reg_lo);
			oa_reg_lo_set = 0;
			u |= oa_reg_lo;
		}

		if (oa_reg_hi_set) {
			printf("merging oa hi %o\n", oa_reg_hi);
			oa_reg_hi_set = 0;
			u |= (ucw_t)oa_reg_hi << 26;
		}

		if (cycles++ > 60000/*400000*/) {
			printf("cycle count exceeded\n");
			break;
		}

		printf("------\n");
		printf("%03o %016Lo\n", pc, u);
		disassemble_ucode_loc(pc, u);

		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;

		/* get A source value */
		a_src_value = read_a_mem(a_src);

		/* calculate M source value */
		if (m_src & 040) {
			unsigned int l2_data, l1_data;

			switch (m_src & 037) {
			case 0:
				m_src_value = dispatch_constant;
				break;
			case 1:
				m_src_value = (spc_stack_ptr << 24) |
					(spc_stack[spc_stack_ptr] & 0777777);
				break;
			case 2:
				m_src_value = pdl_ptr;
				break;
			case 3:
				m_src_value = pdl_index;
				break;
			case 5:
				printf("reading pdl[%o] -> %o\n",
				       pdl_index, pdl_memory[pdl_index]);

				m_src_value = pdl_memory[pdl_index];
				break;
			case 6:
				m_src_value = opc;
				break;
			case 7:
				m_src_value = q;
				break;
			case 010:
				m_src_value = vma;
				break;
			case 011:
				/* memory-map-data, or "map[MD]" */
				l2_data = map_vtop(md, &l1_data, (int *)0);
				
				m_src_value = 
					(write_fault_bit << 31) |
					(access_fault_bit << 30) |
					(l1_data << 24) |
					(l2_data & 077777777);

				printf("l1_data %o, l2_data %o\n",
				       l1_data, l2_data);

				printf("read map[md=%o] -> %o\n",
				       md, m_src_value);
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
				printf("reading pdl[%o] -> %o, pop\n",
				       pdl_ptr, pdl_memory[pdl_ptr]);
				m_src_value = pdl_memory[pdl_ptr++];
				break;
			case 025:
				printf("reading pdl[%o] -> %o\n",
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

			printf("a=%o (%o), m=%o (%o)\n",
			       a_src, a_src_value,
			       m_src, m_src_value);

			printf("alu_op %o, ir8 %o, ir7 %o, c %o, dest %o\n",
			       alu_op, ir8, ir7, carry_in, dest);

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
					lv = m_src_value + carry_in ? 1 : 0;
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
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
					printf("divide step\n");
					do_sub = q & 1;
printf("do_sub %d\n", do_sub);
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
					printf("remainder correction\n");
					do_sub = q & 1;
					if (a_src_value & 0x80000000)
						do_add = !do_add;
printf("do_sub %d\n", do_sub);
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
					break;
				case 011:
					/* initial divide step */
					printf("divide-first-step\n");
#if 0
q = 4;
q = 8;
a_src_value = 2;
a_memory[011] = 2;

q = 10/2;
a_src_value = 3;
a_memory[011] = 3;
#endif
printf("divide: %o / %o \n", q, a_src_value);
					q >>= 1; 

					lv = m_src_value -
						a_src_value -
						(carry_in ? 1 : 0);

					alu_out = lv;
printf("alu_out %08x %o %d\n", alu_out, alu_out, alu_out);
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
				printf("q<<\n");
				q <<= 1;
				/* inverse of alu sign */
				if ((alu_out & 0x80000000) == 0)
					q |= 1;
				break;
			case 2:
				printf("q>>\n");
				q >>= 1;
				if (alu_out & 1)
					q |= 0x80000000;
				break;
			case 3:
				printf("q<-alu\n");
				q = alu_out;
				break;
			}

			printf("out_bus %d\n", out_bus);

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

			printf("alu_out 0x%08x, alu_carry %d, q 0x%08x\n",
			       alu_out, alu_carry, q);
			break;

		case 1: /* jump */
			new_pc = (u >> 12) & 037777;

			printf("a=%o (%o), m=%o (%o)\n",
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

			if (u & (1<<5)) {
				switch (u & 017) {
				case 0: break;
				case 1:
					take_jump = m_src_value < a_src_value;
					break;
				case 2:
					take_jump = m_src_value <= a_src_value;
//					printf("%o <= %o; take_jump %o\n",
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
				printf("jump-if-bit; rot %o, before %o ",
				       rot, m_src_value);
				m_src_value = rotate_left(m_src_value, rot);
				printf("after %o\n", m_src_value);
				take_jump = m_src_value & 1;
			}
 
			if (r_bit && take_jump) {
				new_pc = pop_spc();
				no_incr_pc = 1;
			}

			if (invert_sense)
				take_jump = !take_jump;

			if (take_jump) {
				old_pc = u_pc;
				u_pc = new_pc;
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
				printf("dispatch_memory[%o] <- %o\n",
				       disp_addr, a_src_value);
				dispatch_memory[disp_addr] = a_src_value;
				goto dispatch_done;
			}

			printf("addr %o, map %o, len %o, rot %o\n",
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
			printf("a=%o (%o), m=%o (%o)\n",
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

			printf("widthm1 %o, pos %o, mr_sr_bits %o\n",
			       widthm1, pos, mr_sr_bits);

			printf("left_mask_index %o, right_mask_index %o\n",
				left_mask_index, right_mask_index);

			printf("left_mask %o, right_mask %o, mask %o\n",
			       left_mask, right_mask, mask);

			out_bus = 0;

			switch (mr_sr_bits) {
			case 0:
				break;
			case 1: /* ldb */
				printf("ldb; m %o\n", m_src_value);

				m_src_value = rotate_left(m_src_value, pos);

				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);

				printf("ldb; mask %o, result %o\n", 
				       mask, out_bus);
				break;
			case 2: /* selective desposit */
				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);
				break;
			case 3: /* dpb */
				printf("dpb; m %o, pos %o\n", 
				       m_src_value, pos);

				/* mask is already rotated */

				m_src_value = rotate_left(m_src_value, pos);

				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);

				printf("dpb; mask %o, result %o\n", 
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
			printf("popj; ");
			u_pc = pop_spc();
			no_incr_pc = 1;
			fetch_next = 1;
		}
		
	next:
		if (no_incr_pc)
			no_incr_pc = 0;
		else
			u_pc++;

		if (u_pc == old_pc && !fetch_next) {
			printf("loop detected pc %o\n", u_pc);
			break;
		}
	}
}

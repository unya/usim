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

#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "ucode.h"

#if defined(OSX)
#include "Breakpoints.h"
#define STAT_PC_HISTORY
#endif

//#define STAT_PC_HISTORY
//#define STAT_ALU_USE
//#define STAT_PC_HISTOGRAM
//#define TRACE

extern ucw_t prom_ucode[512];
ucw_t ucode[16*1024];

unsigned int a_memory[1024];
static unsigned int m_memory[32];
static unsigned int dispatch_memory[2048];

static unsigned int pdl_memory[1024];
int pdl_ptr;
int pdl_index;

int lc;
//static int lc_mode_flag;

static int spc_stack[32];
int spc_stack_ptr;

struct page_s {
	unsigned int w[256];
};

static struct page_s *phy_pages[16*1024];

static int l1_map[2048];
static int l2_map[1024];

unsigned long cycles;
unsigned long trace_cycles;
unsigned long max_cycles;
unsigned long max_trace_cycles;
unsigned long begin_trace_cycle;

int u_pc;
static int page_fault_flag;
static int interrupt_pending_flag;
static int interrupt_status_reg;

static int sequence_break_flag;
static int interrupt_enable_flag;
static int lc_byte_mode_flag;
static int bus_reset_flag;

int prom_enabled_flag;
int stop_after_prom_flag;
int run_ucode_flag;
int warm_boot_flag;
extern int save_state_flag;
extern int dump_state_flag;

unsigned int md;
unsigned int vma;
unsigned int q;
unsigned int opc;

static unsigned int new_md;
static int new_md_delay;

static int write_fault_bit;
static int access_fault_bit;

static int alu_carry;
static unsigned int alu_out;

static unsigned int oa_reg_lo;
static unsigned int oa_reg_hi;
static int oa_reg_lo_set;
static int oa_reg_hi_set;

static int interrupt_control;
static unsigned int dispatch_constant;

int trace;
int trace_mcr_labels_flag;
int trace_lod_labels_flag;
int trace_prom_flag;
int trace_mcr_flag;
int trace_io_flag;
int trace_vm_flag;
int trace_disk_flag;
int trace_net_flag;
int trace_int_flag;
int trace_late_set;
int trace_after_flag;

static int macro_pc_incrs;

static int phys_ram_pages;

#ifdef STAT_ALU_USE
static unsigned int alu_stat0[16], alu_stat1[16], alu_stat2[16];
#endif

void show_label_closest(unsigned int upc);
void show_label_closest_padded(unsigned int upc);
char *find_function_name(int the_lc);
int restore_state(void);

extern void video_read(int offset, unsigned int *pv);
extern void video_write(int offset, unsigned int bits);
extern void iob_unibus_read(int offset, int *pv);
extern void iob_unibus_write(int offset, int v);
extern int disk_xbus_read(int offset, unsigned int *pv);
extern int disk_xbus_write(int offset, unsigned int v);
extern int tv_xbus_read(int offset, unsigned int *pv);
extern int tv_xbus_write(int offset, unsigned int v);
#ifdef CADR2
extern int ether_xbus_reg_read(int, unsigned int *);
extern int ether_xbus_reg_write(int, unsigned int);
extern int ether_xbus_desc_read(int, unsigned int *);
extern int ether_xbus_desc_write(int, unsigned int);
extern void ether_poll();
extern int uart_xbus_read(int, unsigned int *);
extern int uart_xbus_write(int, unsigned int);
#endif

extern void disassemble_ucode_loc(int loc, ucw_t u);
extern int sym_find(int mcr, char *name, int *pval);
void reset_pc_histogram(void);
#if defined(OSX)
static void record_lc_history(void);
static void show_lc_history(void);
#endif

extern void timing_start();
extern void timing_stop();
extern void iob_poll();
extern void disk_poll();
extern void display_poll();
extern void chaos_poll();

int
get_u_pc(void)
{
  return u_pc;
}

void
set_interrupt_status_reg(int new)
{
	interrupt_status_reg = new;
	interrupt_pending_flag = (interrupt_status_reg & 0140000) ? 1 : 0;
}

void
assert_unibus_interrupt(int vector)
{
	/* unibus interrupts enabeld? */
	if (interrupt_status_reg & 02000) {
		traceint("assert: unibus interrupt (enabled)\n");
		set_interrupt_status_reg(
			(interrupt_status_reg & ~01774) |
			0100000 | (vector & 01774));
	} else {
		traceint("assert: unibus interrupt (disabled)\n");
	}
}

void
deassert_unibus_interrupt(int vector)
{
	if (interrupt_status_reg & 0100000) {
		traceint("deassert: unibus interrupt\n");
		set_interrupt_status_reg(
			interrupt_status_reg & ~(01774 | 0100000));
	}
}

void
assert_xbus_interrupt(void)
{
	traceint("assert: xbus interrupt (%o)\n", interrupt_status_reg);
	set_interrupt_status_reg(interrupt_status_reg | 040000);
}

void
deassert_xbus_interrupt(void)
{
	if (interrupt_status_reg & 040000) {
		traceint("deassert: xbus interrupt\n");
		set_interrupt_status_reg(interrupt_status_reg & ~040000);
	}
}

unsigned int last_virt = 0xffffff00, last_l1, last_l2;

static inline void
invalidate_vtop_cache(void)
{
	last_virt = 0xffffff00;
}

/*
 * map virtual address to physical address,
 * possibly returning l1 mapping
 * possibly returning offset into page
 */
static inline unsigned int
map_vtop(unsigned int virt, int *pl1_map, int *poffset)
{
	int l1_index, l2_index, l1;
	unsigned int l2;

	/* 24 bit address */
	virt &= 077777777;

#if 0
	/* cache */
	if ((virt & 0xffffff00) == last_virt) {
		if (pl1_map)
			*pl1_map = last_l1;
		if (poffset)
			*poffset = virt & 0377;
		return last_l2;
	}
#endif

	/* frame buffer */
	if ((virt & 077700000) == 077000000) {
		/*  077000000, size = 210560(8) */

		if (virt >= 077051757 && virt <= 077051763) {
			traceio("disk run light\n");
		} else {
			if (0) traceio("tv: frame buffer %o\n", virt);
		}

		if (poffset)
			*poffset = virt & 0377;

		return (1 << 22) | (1 << 23) | 036000;
	}

	/* color */
	if ((virt & 077700000) == 077200000) {
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036000;
	}

/* this should be move below - I'm not sure it has to happen anymore */
	if ((virt & 077777400) == 077377400) {
		if (0) traceio("forcing xbus mapping for disk\n");
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036777;
	}

/*
764000-7641777 i/o board
764140 chaos (77772060)
*/

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

	last_virt = virt & 0xffffff00;
	last_l1 = l1;
	last_l2 = l2;

#if 0
	if ((virt & 0077777000) == 0076776000) {
		printf("vtop: pdl? %011o l1 %d %011o l2 %d %011o\n",
		       virt, l1_index, l1, l2_index, l2);
	}
#endif

	return l2;
}

char *page_block;
int page_block_count;

struct page_s *
new_page(void)
{
  struct page_s *page;

  if (page_block == 0) {
    page_block = malloc(sizeof(struct page_s) * 1024);
    page_block_count = 1024;
  }

  page = (struct page_s *)page_block;
  page_block += sizeof(struct page_s);
  page_block_count--;
  if (page_block_count == 0)
    page_block = 0;

  return page;
}

/*
 * add a new physical memory page,
 * generally in response to l2 mapping
 * (but can also be due to phys access to ram)
 */
int
add_new_page_no(int pn)
{
#if 0
	struct page_s *page;

	if (0) printf("new_page %o\n", pn);

	if ((page = phy_pages[pn]) == 0) {

		page = (struct page_s *)malloc(sizeof(struct page_s));
		if (page) {
//#define ZERO_NEW_PAGES
#ifdef ZERO_NEW_PAGES
			memset(page, 0, sizeof(struct page_s));
#endif
			phy_pages[pn] = page;

			tracef("add_new_page_no(pn=%o)\n", pn);
			return 0;
		}
	}

	return -1;
#else
	struct page_s *page;

	if ((page = phy_pages[pn]) == 0) {

		page = new_page();
		if (page) {
			phy_pages[pn] = page;
			return 0;
		}
	}

	return -1;
#endif
}

/*
 * read phys memory, with no virt-to-phys mapping
 * (used by disk controller)
 */
int
read_phy_mem(int paddr, unsigned int *pv)
{
	int pn = paddr >> 8;
	int offset = paddr & 0377;
	struct page_s *page;

	if ((page = phy_pages[pn]) == 0) {
		/* page does not exist */
		if (pn < phys_ram_pages) {
			tracef("[read_phy_mem] "
			       "adding phy ram page %o (address %o)\n",
			       pn, paddr);
			add_new_page_no(pn);
			page = phy_pages[pn];
		} else {
			printf("[read_phy_mem] address %o does not exist\n",
			       paddr);
			return -1;
		}
	}

	*pv = page->w[offset];

	return 0;
}

int
write_phy_mem(int paddr, unsigned int v)
{
	int pn = paddr >> 8;
	int offset = paddr & 0377;
	struct page_s *page;

	if ((page = phy_pages[pn]) == 0) {
		/* page does not exist - add it (probably result of disk write) */
		if (pn < phys_ram_pages) {
			tracef("[write_phy_mem] "
			       "adding phy ram page %o (address %o)\n",
			       pn, paddr);
			add_new_page_no(pn);
			page = phy_pages[pn];
		} else {
			printf("[write_phy_mem] address %o does not exist\n",
			       paddr);
			return -1;
		}
	}

	page->w[offset] = v;

	return 0;
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
	write_fault_bit = 0;
	page_fault_flag = 0;


#if 1
	//tracef("read_mem(vaddr=%o)\n", vaddr);
	map = map_vtop(vaddr, (int *)0, &offset);
#else
	{
		/* additional debugging, but slower */
		int l1;
		map = map_vtop(vaddr, (int *)&l1, &offset);
		tracef("read_mem(vaddr=%o) l1_index %o, l1 %o, l2_index %o, l2 %o\n",
		       vaddr,
		       (vaddr >> 13) & 03777,
		       (l1 << 5) | ((vaddr >> 8) & 037),
		       map);
	}
#endif

	/* 14 bit page # */
	pn = map & 037777;

	//tracef("read_mem(vaddr=%o) -> pn %o, offset %o, map %o (%o)\n",
	//vaddr, pn, offset, map, 1 << 23);

	if ((map & (1 << 23)) == 0) {
		/* no access perm */
		access_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		*pv = 0;
		tracef("read_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

#if 1 //speedup
	if (pn < 020000 && (page = phy_pages[pn])) {
		*pv = page->w[offset];
		return 0;
	}
#endif

	/* simulate fixed number of ram pages (< 2mw?) */
	if (pn >= phys_ram_pages && pn <= 035777)
	{
		*pv = 0xffffffff;
		return 0;
	}

	if (pn == 036000) {
		/* thwart the color probe */
		if ((vaddr & 077700000) == 077200000) {
		  if (0) printf("read from %o\n", vaddr);
		  *pv = 0x0;
		  return 0;
		}

		offset = vaddr & 077777;
		video_read(offset, pv);
		return 0;
	}
	/* Extra xbus devices */

	if (pn == 037764) {
		offset <<= 1;
		iob_unibus_read(offset, (int *)pv);
		return 0;
	}

	if (pn == 037766) {
		/* unibus */
		//int paddr = pn << 10;
		//tracef("paddr %o\n", paddr);

		switch (offset) {
		case 040:
			traceio("unibus: read interrupt status\n");
			*pv = 0;
			return 0;

		case 044:
			traceio("unibus: read error status\n");
			*pv = 0;
			return 0;
		}
	}

	/* disk & tv controller on xbus */
	if (pn == 036777) {
		/*int paddr = pn << 10;*/

		/*
		 * 17377774 disk
		 * 17377760 tv
		 */
		if (offset >= 0370)
			return disk_xbus_read(offset, pv);

		if (offset == 0360)
			return tv_xbus_read(offset, pv);

		printf("xbus read %o %o\n", offset, vaddr);
		*pv = 0;
		return 0;
	}

#ifdef CADR2
	if (pn == 036774) {
		/*
		 * 17376000 ethernet registers
		 */
		return ether_xbus_reg_read(offset, pv);
	}

	if (pn == 036775) {
		/*
		 * 17376400 ethernet descriptors
		 */
		return ether_xbus_desc_read(offset, pv);
	}

	if (pn == 036776) {
		return uart_xbus_read(offset, pv);
	}
#endif

	if ((page = phy_pages[pn]) == 0) {
		/* page fault */
		page_fault_flag = 1;
		opc = pn;
		tracef("read_mem(vaddr=%o) page fault\n", vaddr);
        *pv = 0;
		return -1;
	}

	*pv = page->w[offset];
	return 0;
}

int
read_mem_debug(int vaddr, unsigned int *pv)
{
	int save_access_fault_bit = access_fault_bit;
	int save_write_fault_bit = write_fault_bit;
	int save_page_fault_flag = page_fault_flag;

	read_mem(vaddr, pv);

	access_fault_bit = save_access_fault_bit;
	write_fault_bit = save_write_fault_bit;
	page_fault_flag = save_page_fault_flag;

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
	access_fault_bit = 0;
	page_fault_flag = 0;

	map = map_vtop(vaddr, (int *)0, &offset);

#if 0
	//tracef("write_mem(vaddr=%o,v=%o)\n", vaddr, v);
	printf("write_mem(vaddr=%o,v=%o)\n", vaddr, v);
#endif

	/* 14 bit page # */
	pn = map & 037777;

	//tracef("write_mem(vaddr=%o) -> pn %o, offset %o, map %o (%o)\n",
	//	       vaddr, pn, offset, map, 1 << 22);

	if ((map & (1 << 23)) == 0) {
		/* no access perm */
		access_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		tracef("write_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

	if ((map & (1 << 22)) == 0) {
		/* no write perm */
		write_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		tracef("write_mem(vaddr=%o) write fault\n", vaddr);
		return -1;
	}

#if 1 //speedup
	if (pn < 020000 && (page = phy_pages[pn])) {
		page->w[offset] = v;
		return 0;
	}
#endif

	if (pn == 036000) {
/* thwart the color probe */
if ((vaddr & 077700000) == 077200000) {
	if (0) printf("write to %o\n", vaddr);
	return 0;
}
		offset = vaddr & 077777;
		if (0) traceio("video_write %o %o (%011o)\n", offset, v, vaddr);
		video_write(offset, v);
		return 0;
	}

	if (pn == 037760) {
		printf("tv: reg write %o, offset %o, v %o\n",
		       vaddr, offset, v);
		return 0;
	}

	if (pn == 037764) {
		offset <<= 1;
		traceio("unibus: iob v %o, offset %o\n",
		       vaddr, offset);
		iob_unibus_write(offset, v);
		return 0;
	}

	if (pn == 037766) {
		/* unibus */
		/*int paddr = pn << 12;*/

		offset <<= 1;

		if (offset <= 036) {
			traceio("unibus: spy v %o, offset %o\n",
			       vaddr, offset);

			switch (offset) {
			case 012:
				if ((v & 044) == 044) {
					traceio("unibus: "
					       "disabling prom enable flag\n");
					prom_enabled_flag = 0;

					if (warm_boot_flag) {
						restore_state();
					}


#ifdef STAT_PC_HISTOGRAM
					reset_pc_histogram();
#endif
				}
				if (v & 2) {
					traceio("unibus: normal speed\n");
				}

				break;
			}

			return 0;
		}

		switch (offset) {
		case 040:
			traceio("unibus: write interrupt status %o\n", v);
			set_interrupt_status_reg(
				(interrupt_status_reg & ~0036001) |
				(v & 0036001));
			return 0;

		case 042:
			traceio("unibus: write interrupt stim %o\n", v);
			set_interrupt_status_reg(
				(interrupt_status_reg & ~0101774) |
				(v & 0101774));
			return 0;

		case 044:
			traceio("unibus: clear bus error %o\n", v);
			return 0;

		default:
			if (offset >= 0140 && offset <= 0176) {
				traceio("unibus: mapping reg %o\n", offset);
				return 0;
			}

			traceio("unibus: write? v %o, offset %o\n",
			       vaddr, offset);
		}

	}

	/* disk controller on xbus */
	if (pn == 036777) {
		if (offset >= 0370)
			return disk_xbus_write(offset, v);

		if (offset == 0360)
			return tv_xbus_write(offset, v);

	}

#ifdef CADR2
	if (pn == 036774) {
		/*
		 * 17377000 ethernet
		 */
		return ether_xbus_reg_write(offset, v);
	}

	if (pn == 036775) {
		/*
		 * 17376000 ethernet descs
		 */
		return ether_xbus_desc_write(offset, v);
	}

	if (pn == 036776) {
		return uart_xbus_write(offset, v);
	}
#endif

#if 1
	/* catch questionable accesses */
	if (pn >= 036000) {
		printf("??: reg write vaddr %o, pn %o, offset %o, v %o; u_pc %o\n",
		       vaddr, pn, offset, v, u_pc);
	}
#endif

	if ((page = phy_pages[pn]) == 0) {
		/* page fault */
		page_fault_flag = 1;
		opc = pn;
		//tracef("write_mem(vaddr=%o) page fault\n", vaddr);
		return -1;
	}

	page->w[offset] = v;
	return 0;
}

static inline void
write_ucode(int addr, ucw_t w)
{
	tracef("u-code write; %Lo @ %o\n", w, addr);
	ucode[addr] = w;
}

void
note_location(char *s, unsigned int v)
{
	printf("%s; u_pc %o, v %o\n", s, u_pc, v);
	show_label_closest(u_pc);
	printf("\n");
}

static inline void
write_a_mem(int loc, unsigned int v)
{
	//tracef("a_memory[%o] <- %o\n", loc, v);
	a_memory[loc] = v;
}

unsigned int
read_a_mem(int loc)
{
	return a_memory[loc];
}

static inline unsigned int
read_m_mem(int loc)
{
	if (loc > 32) {
		printf("read m-memory address > 32! (%o)\n", loc);
	}

	return m_memory[loc];
}

static inline void
write_m_mem(int loc, unsigned int v)
{
	m_memory[loc] = v;
	a_memory[loc] = v;
	//tracef("a,m_memory[%o] <- %o\n", loc, v);
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

#if 0
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
#else
static inline unsigned int
rotate_left(unsigned int value, int bitstorotate)
{
	unsigned int tmp;
	int mask;

	/* determine which bits will be impacted by the rotate */
	if (bitstorotate == 0)
		mask = 0;
	else
		mask = (int)0x80000000 >> bitstorotate;
		
	/* save off the affected bits */
	tmp = (value & mask) >> (32 - bitstorotate);
		
	/* perform the actual rotate */
	/* add the rotated bits back in (in the proper location) */
	return (value << bitstorotate) | tmp;
}
#endif

static inline void
push_spc(int pc)
{
	spc_stack_ptr = (spc_stack_ptr + 1) & 037;

	//tracef("writing spc[%o] <- %o\n", spc_stack_ptr, pc);
	spc_stack[spc_stack_ptr] = pc;
}

static inline int
pop_spc(void)
{
	unsigned int v;

	//tracef("reading spc[%o] -> %o\n",
	//     spc_stack_ptr, spc_stack[spc_stack_ptr]);

	v = spc_stack[spc_stack_ptr];
	spc_stack_ptr = (spc_stack_ptr - 1) & 037;

	return v;
}

/*
 * advance the LC register,
 * following the rules; will read next vma if needed
 */
void
advance_lc(int *ppc)
{
	/* lc is 26 bits */
	int old_lc = lc & 0377777777;

	//tracef("advance_lc() byte-mode %d, lc %o, need-fetch %d\n",
	//     lc_byte_mode_flag, lc,
	//     ((lc >> 31) & 1) ? 1 : 0);

	if (lc_byte_mode_flag) {
		/* byte mode */
		lc++;
	} else {
		/* 16 bit mode */
		lc += 2;
	}

	macro_pc_incrs++;

	/* need-fetch? */
	if (lc & (1 << 31)) {
		lc &= ~(1 << 31);
		vma = old_lc >> 2;
		if (read_mem(old_lc >> 2, &new_md)) {
		}
		new_md_delay = 2;
		tracef("advance_lc() read vma %011o -> %011o\n",
		       old_lc >> 2, new_md);
	} else {
		/* force skipping 2 instruction (pf + set-md) */
		if (ppc)
			*ppc |= 2;

		tracef("advance_lc() no read; md = %011o\n", md);
	}

	{
		char lc0b, lc1, last_byte_in_word;

		/*
		 * this is ugly, but follows the hardware logic
		 * (I need to distill it to intent but it seems correct)
		 */
		lc0b =
			/* byte-mode */
			(lc_byte_mode_flag ? 1 : 0) &
			/* lc0 */
			((lc & 1) ? 1 : 0);

		lc1 = (lc & 2) ? 1 : 0;

//		last_byte_in_word = ((~lc0b & ~lc1) & 1) ? 1 : 0;
		last_byte_in_word = (~lc0b & ~lc1) & 1;

		tracef("lc0b %d, lc1 %d, last_byte_in_word %d\n",
		       lc0b, lc1, last_byte_in_word);

		if (last_byte_in_word)
			/* set need-fetch */
			lc |= (1 << 31);
	}

#if defined(OSX)
        {
            extern int macromicro;
            extern void step_poll(unsigned int p0_pc);
            
            record_lc_history();
            if (macromicro)
                step_poll(lc);
        }
#endif

}

void
show_pdl_local(void)
{
	int i, min, max;

	printf("pdl-ptr %o, pdl-index %o\n", pdl_ptr, pdl_index);

	min = pdl_ptr > 4 ? pdl_ptr - 4 : 0;
	max = pdl_ptr < 1024-4 ? pdl_ptr + 4 : 1024;

	if (pdl_index > 0 && pdl_index < pdl_ptr) min = pdl_index;

	/* PDL */
	for (i = min; i < max; i += 4) {
		printf("PDL[%04o] %011o %011o %011o %011o\n",
		       i, pdl_memory[i], pdl_memory[i+1],
		       pdl_memory[i+2], pdl_memory[i+3]);
	}
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
	case 1: /* LC (location counter) 26 bits */
		tracef("writing LC <- %o\n", out_bus);
		lc = (lc & ~0377777777) | (out_bus & 0377777777);

		if (lc_byte_mode_flag) {
			/* not sure about byte mode... */
		} else {
			/* in half word mode low order bit is ignored */
			lc &= ~1;
		}

		/* set need fetch */
		lc |= (1 << 31);

		if (trace_lod_labels_flag) {
			char *s;

			s = find_function_name(lc);
			show_label_closest(u_pc);
			printf(": lc <- %o (%o)", lc, lc>>2);
			if (s) printf(" '%s'", s);
			printf("\n");
		}

#if defined(OSX)
                {
                    extern int macromicro;
                    extern void step_poll(unsigned int p0_pc);
                    
                    record_lc_history();
                    if (macromicro)
                        step_poll(lc);
                }
#endif
                
/* isn't this pretty? :-) XXX add main option to trace on macro function name */
#if 0
{ char *s;

 s = find_function_name(lc);
//if (s && strcmp(s, "SHEET-PREPARE-FOR-EXPOSE") == 0) 
//	trace_lod_labels_flag = 1;

 if (trace_lod_labels_flag) {
	 show_label_closest(u_pc);
	 printf(": lc <- %o (%o)", lc, lc>>2);
	 if (s) printf(" '%s'", s);
	 printf("\n");
 }

// if (pdl_ptr > 01770) trace = 1;
// if (lc == 011030114652) trace=1;

 if (s) {
//if (strcmp(s, "RECEIVE-ANY-FUNCTION") == 0) 
//trace = 1;

//if (strcmp(s, "DISK-RUN") == 0) 
//trace = 1;
//	 trace_disk_flag = 1;

//if (strcmp(s, "FIND-DISK-PARTITION") == 0) 
//trace = 1;
//	 trace = 1;

//if (strcmp(s, "LISP-ERROR-HANDLER") == 0) 
//trace = 1;
//	 trace = 0;
 }
}
#endif

#if 0
show_label_closest(u_pc);
printf(": lc <- %o (%o)", lc, lc>>2);
 { char *s;
s = find_function_name(lc);
if (s) printf("%s", s);
// if (strcmp(s, "INITIALIZATIONS") == 0)
if (strcmp(s, "CLEAR-UNIBUS-MAP") == 0) trace = 1;
if (strcmp(s, "INITIALIZATIONS") == 0) {
dump_pdl_memory();
 show_list(0);
}
 }
printf("\n");

//trace_mcr_labels_flag = 1;
trace_disk_flag = 1;

 if (lc == 011007402704) {
	 trace = 1;
//	 trace_mcr_labels_flag = 1;
 }
#endif

		break;
	case 2: /* interrrupt control <29-26> */
		tracef("writing IC <- %o\n", out_bus);
		interrupt_control = out_bus;

		lc_byte_mode_flag = interrupt_control & (1 << 29);
		bus_reset_flag = interrupt_control & (1 << 28);
		interrupt_enable_flag = interrupt_control & (1 << 27);
		sequence_break_flag = interrupt_control & (1 << 26);

		if (sequence_break_flag) {
			traceint("ic: sequence break request\n");
		}
		if (interrupt_enable_flag) {
			traceint("ic: interrupt enable\n");
		}
		if (bus_reset_flag) {
			traceint("ic: bus reset\n");
		}
		if (lc_byte_mode_flag) {
			traceint("ic: lc byte mode\n");
		}

		/* preserve flags */
		lc = (lc & ~(017 << 26)) |
			(interrupt_control & (017 << 26));

		break;
	case 010: /* PDL (addressed by Pointer) */
		tracef("writing pdl[%o] <- %o\n",
		       pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
if (0) show_pdl_local();
		break;
	case 011: /* PDL (addressed by pointer, push */
		pdl_ptr = (pdl_ptr + 1) & 01777;
		tracef("writing pdl[%o] <- %o, push\n",
		       pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
if (0) show_pdl_local();
		break;
	case 012: /* PDL (addressed by index) */
		tracef("writing pdl[%o] <- %o\n",
		       pdl_index, out_bus);
		write_pdl_mem(USE_PDL_INDEX, out_bus);
if (0) show_pdl_local();
		break;
	case 013: /* PDL index */
		tracef("pdl-index <- %o\n", out_bus);
		pdl_index = out_bus & 01777;
		break;
	case 014: /* PDL pointer */
		tracef("pdl-ptr <- %o\n", out_bus);
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
		if (read_mem(vma, &new_md)) {
		}
		new_md_delay = 2;
		break;

	case 022: /* VMA register, start main memory write */
		vma = out_bus;
		if (write_mem(vma, md)) {
		}
		break;

	case 023: /* VMA register, write map */
		/* vma-write-map */
		vma = out_bus;

		tracevm("vma-write-map md=%o, vma=%o (addr %o)\n",
		       md, vma, md >> 13);

	write_map:
		if ((vma >> 26) & 1) {
			int l1_index, l1_data;
			l1_index = (md >> 13) & 03777;
			l1_data = (vma >> 27) & 037;
			l1_map[l1_index] = l1_data;
			invalidate_vtop_cache();

			tracevm("l1_map[%o] <- %o\n", l1_index, l1_data);
		}

		if ((vma >> 25) & 1) {
			int l1_index, l2_index, l1_data;
			unsigned int l2_data;
			l1_index = (md >> 13) & 03777;
			l1_data = l1_map[l1_index];

			l2_index = (l1_data << 5) | ((md >> 8) & 037);
			l2_data = vma;
			l2_map[l2_index] = l2_data;
			invalidate_vtop_cache();

#if 0
			if (l2_index == 0) 
				printf("l2_map[%o] <- %o\n",
				       l2_index, l2_data);
#endif
			tracevm("l2_map[%o] <- %o\n", l2_index, l2_data);

			add_new_page_no(l2_data & 037777);
		}
		break;

	case 030: /* MD register (memory data) */
		md = out_bus;
		tracef("md<-%o\n", md);
		break;

	case 031:
		md = out_bus;
		if (read_mem(vma, &new_md)) {
		}
		new_md_delay = 2;
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

#define MAX_PC_HISTORY 64
struct {
	unsigned int rpc;
	unsigned int rvma;
	unsigned int rmd;
	int rpf;
	int rpdl_ptr;
	unsigned int rpdl;
	unsigned int rq;
	unsigned int m[8];
} pc_history[MAX_PC_HISTORY];
int pc_history_ptr, pc_history_max, pc_history_stores;

void
record_pc_history(unsigned int pc, unsigned int vma, unsigned int md)
{
	int index;

	pc_history_stores++;

	if (pc_history_max < MAX_PC_HISTORY) {
		index = pc_history_max;
		pc_history_max++;
	} else {
		index = pc_history_ptr;
		pc_history_ptr++;
		if (pc_history_ptr == MAX_PC_HISTORY)
			pc_history_ptr = 0;
	}

	pc_history[index].rpc = pc;
	pc_history[index].rvma = vma;
	pc_history[index].rmd = md;
	pc_history[index].rpf = page_fault_flag;
	pc_history[index].rpdl_ptr = pdl_ptr;
	pc_history[index].rpdl = pdl_memory[pdl_ptr];

	pc_history[index].rq = q;
	pc_history[index].m[0] = m_memory[015];
	pc_history[index].m[1] = m_memory[016];
	pc_history[index].m[2] = m_memory[022];
	pc_history[index].m[3] = m_memory[023];
}

void
show_pc_history(void)
{
	int i;
	unsigned int pc;

	printf("pc history:\n");
	if (0) printf("pc_history_ptr %d, pc_history_max %d, pc_history_stores %d\n",
		      pc_history_ptr, pc_history_max, pc_history_stores);

	for (i = 0; i < MAX_PC_HISTORY; i++) {
		pc = pc_history[pc_history_ptr].rpc;
		if (pc == 0)
			break;
		printf("%2d %011o ", i, pc);
		show_label_closest_padded(pc);

		printf("\tvma %011o md %011o pf%d pdl %o %011o",
		       pc_history[pc_history_ptr].rvma,
		       pc_history[pc_history_ptr].rmd,
		       pc_history[pc_history_ptr].rpf,
		       pc_history[pc_history_ptr].rpdl_ptr,
		       pc_history[pc_history_ptr].rpdl);

#if 1
		printf(" Q %08x M %08x %08x %08x %08x\n",
		       pc_history[pc_history_ptr].rq,
		       pc_history[pc_history_ptr].m[0],
		       pc_history[pc_history_ptr].m[1],
		       pc_history[pc_history_ptr].m[2],
		       pc_history[pc_history_ptr].m[3]);
#endif
		       
		printf("\n");

#if 1
		{
			ucw_t u = ucode[pc];
			disassemble_ucode_loc(pc, u);
		}
#endif

		pc_history_ptr++;
		if (pc_history_ptr == MAX_PC_HISTORY)
			pc_history_ptr = 0;
		
	}

	printf("\n");
    
#if defined(OSX)
        show_lc_history();
#endif
}

#if defined(OSX)

#define MAX_LC_HISTORY 200
struct {
    unsigned short instr;
    unsigned int lc;
} lc_history[MAX_LC_HISTORY];
int lc_history_ptr, lc_history_max, lc_history_stores;

static void
record_lc_history()
{
    int index;
    unsigned int instr;
    
    lc_history_stores++;
    
    if (lc_history_max < MAX_LC_HISTORY) {
        index = lc_history_max;
        lc_history_max++;
    } else {
        index = lc_history_ptr;
        lc_history_ptr++;
        if (lc_history_ptr == MAX_LC_HISTORY)
            lc_history_ptr = 0;
    }
    
    read_mem(lc >> 2, &instr);
    lc_history[index].instr = (lc & 2) ? (instr >> 16) & 0xffff : (instr & 0xffff);
    lc_history[index].lc = lc;
    
    if (breakpoint_count)
    {
        char *name = find_function_name(lc);
        if (name)
        {
            for (int i = 0; i < breakpoint_count; i++)
            {
                if (breakpoints[i].identifier && strcasecmp(name, breakpoints[i].identifier) == 0)
                {
                    extern int runpause;
                    
                    runpause = 0;
                    printf("found it\n");
                }
            }
        }
    }
}

static void
show_lc_history(void)
{
    extern int wide_integer;
    char *disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst, unsigned int width);
    int i;
    unsigned short instr;
    char *decoded;
    
    printf("lc history:\n");
    if (0) printf("lc_history_ptr %d, lc_history_max %d, lc_history_stores %d\n",
                  lc_history_ptr, lc_history_max, lc_history_stores);
    
    for (i = 0; i < MAX_LC_HISTORY; i++) {
        instr = lc_history[lc_history_ptr].instr;
        if (lc_history_max < MAX_LC_HISTORY && lc_history_ptr == lc_history_max)
            break;
        
        decoded = disass(0, lc_history[lc_history_ptr].lc & 0377777777, 0, instr, wide_integer ? 25 : 24);
        
        printf("%s\n", decoded);
        
        lc_history_ptr++;
        if (lc_history_ptr == MAX_LC_HISTORY)
            lc_history_ptr = 0;
        
    }
    
    printf("\n");
}
#endif // defined(OSX)

struct pc_histogram_s {
  unsigned int pc;
  unsigned long long count;
} pc_histogram[16*1024];


void
record_pc_histogram(unsigned int pc)
{
  pc_histogram[pc].count++;
}

void
reset_pc_histogram(void)
{
  unsigned int pc;
  for (pc = 0; pc < 16*1024; pc++)
    pc_histogram[pc].count = 0;
}

int pc_histogram_cmp(const void *n1, const void *n2)
{
  struct pc_histogram_s *e1 = (struct pc_histogram_s *)n1;
  struct pc_histogram_s *e2 = (struct pc_histogram_s *)n2;
  return (int)(e2->count - e1->count);
}

void
show_pc_histogram(void)
{
  unsigned int pc, i, perc;
  unsigned long long count;
  unsigned long long total;

  printf("microcode pc histogram:\n");

  total = 0;
  for (i = 0; i < 16*1024; i++) {
    pc_histogram[i].pc = i;
    total += pc_histogram[i].count;
  }

  printf("total %lld %016llx\n", total, total);

  qsort(pc_histogram, 16*1024,
	sizeof(struct pc_histogram_s), pc_histogram_cmp);

  for (i = 0; i < 16*1024; i++) {
    pc = pc_histogram[i].pc;
    count = pc_histogram[i].count;
    if (count) {
      char *sym;
      int offset;

      perc = (unsigned int)( (count * 100ULL) / total );
      if (count < 100)
	continue;

      sym = sym_find_last(!prom_enabled_flag, pc, &offset);
      if (offset == 0)
	printf("%05o %s: %lld %d%%\n", pc, sym, count, perc);
      else
	printf("%05o %s+%d: %lld %d%%\n", pc, sym, offset, count, perc);
    }
  }
}

void
dump_l1_map()
{
	int i;

#if 0
	for (i = 0; i < 32; i += 4) {
		printf("l1[%02o] %011o %011o %011o %011o\n",
		       i, l1_map[i], l1_map[i+1], l1_map[i+2], l1_map[i+3]);
	}
	printf("...\n");
	for (i = 2048-32; i < 2048; i += 4) {
		printf("l1[%02o] %011o %011o %011o %011o\n",
		       i, l1_map[i], l1_map[i+1], l1_map[i+2], l1_map[i+3]);
	}
	printf("\n");
#else
	for (i = 0; i < 2048; i += 4) {
		int skipped;
		printf("l1[%02o] %011o %011o %011o %011o\n",
		       i, l1_map[i], l1_map[i+1], l1_map[i+2], l1_map[i+3]);

		skipped = 0;
		while (l1_map[i+0] == l1_map[i+0+4] &&
		       l1_map[i+1] == l1_map[i+1+4] &&
		       l1_map[i+2] == l1_map[i+2+4] &&
		       l1_map[i+3] == l1_map[i+3+4] &&
			i < 2048)
		{
			if (skipped++ == 0)
				printf("...\n");
			i += 4;
		}
	}
	printf("\n");
#endif
}

void
dump_l2_map()
{
	int i;
#if 0
	for (i = 0; i < 32; i += 4) {
		printf("l2[%02o] %011o %011o %011o %011o\n",
		       i, l2_map[i], l2_map[i+1], l2_map[i+2], l2_map[i+3]);
	}
	printf("...\n");
	for (i = 1024-32; i < 1024; i += 4) {
		printf("l2[%02o] %011o %011o %011o %011o\n",
		       i, l2_map[i], l2_map[i+1], l2_map[i+2], l2_map[i+3]);
	}
	printf("\n");
#else
	for (i = 0; i < 1024; i += 4) {
		int skipped;
		printf("l2[%02o] %011o %011o %011o %011o\n",
		       i, l2_map[i], l2_map[i+1], l2_map[i+2], l2_map[i+3]);

		skipped = 0;
		while (l2_map[i+0] == l2_map[i+0+4] &&
		       l2_map[i+1] == l2_map[i+1+4] &&
		       l2_map[i+2] == l2_map[i+2+4] &&
		       l2_map[i+3] == l2_map[i+3+4] &&
			i < 1024)
		{
			if (skipped++ == 0)
				printf("...\n");
			i += 4;
		}
	}
	printf("\n");
#endif
}

void
dump_pdl_memory(void)
{
	int i;

	printf("pdl-ptr %o, pdl-index %o\n", pdl_ptr, pdl_index);

	/* PDL */
	for (i = 0; i < 1024; i += 4) {
		int skipped;
		printf("PDL[%04o] %011o %011o %011o %011o\n",
		       i, pdl_memory[i], pdl_memory[i+1],
		       pdl_memory[i+2], pdl_memory[i+3]);

		skipped = 0;
		while (pdl_memory[i+0] == pdl_memory[i+0+4] &&
		       pdl_memory[i+1] == pdl_memory[i+1+4] &&
		       pdl_memory[i+2] == pdl_memory[i+2+4] &&
		       pdl_memory[i+3] == pdl_memory[i+3+4] &&
			i < 1024)
		{
			if (skipped++ == 0)
				printf("...\n");
			i += 4;
		}
	}
	printf("\n");
}

void
dump_state(void)
{
	int i;

	printf("\n-------------------------------------------------\n");
	printf("CADR machine state:\n\n");

	printf("u-code pc %o, lc %o (%o)\n", u_pc, lc, lc>>2);
	printf("vma %o, md %o, q %o, opc %o, disp-const %o\n",
	       vma, md, q, opc, dispatch_constant);
	printf("oa-lo %011o, oa-hi %011o, ", oa_reg_lo, oa_reg_hi);
	printf("pdl-ptr %o, pdl-index %o, spc-ptr %o\n", pdl_ptr, pdl_index, spc_stack_ptr);
	printf("\n");
	printf("lc increments %d (macro instructions executed)\n",
	       macro_pc_incrs);
	printf("\n");

#ifdef STAT_PC_HISTORY
	show_pc_history();
#endif
#ifdef STAT_PC_HISTOGRAM
	show_pc_histogram();
#endif

	for (i = 0; i < 32; i += 4) {
		printf(" spc[%02o] %c%011o %c%011o %c%011o %c%011o\n",
		       i,
		       (i+0 == spc_stack_ptr) ? '*' : ' ',
		       spc_stack[i+0],
		       (i+1 == spc_stack_ptr) ? '*' : ' ',
		       spc_stack[i+1],
		       (i+2 == spc_stack_ptr) ? '*' : ' ',
		       spc_stack[i+2],
		       (i+3 == spc_stack_ptr) ? '*' : ' ',
		       spc_stack[i+3]);
	}
	printf("\n");

	if (spc_stack_ptr > 0) {
		printf("stack backtrace:\n");
		for (i = spc_stack_ptr; i >= 0; i--) {
			char *sym;
			int offset, pc;
			pc = spc_stack[i] & 037777;
			sym = sym_find_last(!prom_enabled_flag, pc, &offset);
			printf("%2o %011o %s+%d\n",
			       i, spc_stack[i], sym, offset);

		}
		printf("\n");
	}

	for (i = 0; i < 32; i += 4) {
		printf("m[%02o] %011o %011o %011o %011o\n",
		       i, m_memory[i], m_memory[i+1], m_memory[i+2], m_memory[i+3]);
	}
	printf("\n");

	if (0) {
		dump_l1_map();
		dump_l2_map();
	}

	dump_pdl_memory();

	/* A-memory */
	for (i = 0; i < /*1024*/01000; i += 4) {
		printf("A[%04o] %011o %011o %011o %011o\n",
		       i, a_memory[i], a_memory[i+1],
		       a_memory[i+2], a_memory[i+3]);
	}
	printf("\n");

#if 0
	for (i = 0; i < 16*1024; i++) {
		if (phy_pages[i] == 0) printf("z %o\n", i);
	}
#endif

	{
		int s, e;
		s = -1;
		for (i = 0; i < 16*1024; i++) {
			if (phy_pages[i] != 0 && s == -1)
				s = i;

			if ((phy_pages[i] == 0 || i == 16*1024-1) && s != -1) {
				e = i-1;
				printf("%o-%o\n", s, e);
				s = -1;
			}
		}
	}


	printf("\n");
	printf("A-memory by symbol:\n");
	{
		int i;
		for (i = 0; i < 1024; i++) {
			char *sym;

			sym = sym_find_by_type_val(1, 4/*A-MEM*/, i);
			if (sym) {
				printf("%o %-40s %o\n",
				       i, sym, a_memory[i]);
			}
		}
	}

	printf("\n");

	printf("trace: %s\n", trace ? "on" : "off");
}

#define PAGES_TO_SAVE	8192

int restored;

int
restore_state(void)
{
	int fd, i;
    ssize_t ret;
	unsigned char version[2];

	if (restored)
	  return 0;
	restored = 1;

	fd = open("usim.state", O_RDONLY);
	if (fd < 0)
	  return -1;

	ret = read(fd, version, 2);
	if (ret < 0 || version[0] != 0 || version[1] != 1) {
	  close(fd);
	  return -1;
	}
	  
	for (i = 0; i < PAGES_TO_SAVE; i++) {
	  add_new_page_no(i);
	  ret = read(fd, (char *)phy_pages[i], sizeof(struct page_s));
      if (ret < 0)
      {
          close(fd);
          return -1;
      }
#ifdef __BIG_ENDIAN__
	  _swaplongbytes((unsigned int *)phy_pages[i], 256);
#endif
	}

	printf("memory state restored\n");

	close(fd);

	return 0;
}

int
save_state(void)
{
	int fd, i;
    ssize_t ret;
	unsigned char version[2];

	fd = open("usim.state", O_RDWR | O_CREAT, 0666);
	if (fd < 0)
	  return -1;

	version[0] = 0;
	version[1] = 1;
	ret = write(fd, version, 2);
    if (ret < 0)
    {
        close(fd);
        return -1;
    }

	for (i = 0; i < PAGES_TO_SAVE; i++) {
#ifdef __BIG_ENDIAN__
	  _swaplongbytes((unsigned int *)phy_pages[i], 256);
#endif
	  ret = write(fd, (char *)phy_pages[i], sizeof(struct page_s));
      if (ret < 0)
      {
          close(fd);
          return -1;
      }
	}

	close(fd);

	printf("memory state saved\n");

	return 0;
}

void
patch_prom_code(void)
{
//#define PATCH_PROM_LOOPS_1
//#define PATCH_PROM_LOOPS_2

#ifdef PATCH_PROM_LOOPS_1
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

#ifdef PATCH_PROM_LOOPS_2
	prom_ucode[0452] = 04000001000310030; /* m-c <- m-zero */
#endif

#if 0
	/* quick hack while debugging verilog */
	prom_ucode[0175] = 0;
	prom_ucode[0202] = 0;
	prom_ucode[0226] = 0;
	prom_ucode[0232] = 0;
	prom_ucode[0236] = 0;
	prom_ucode[0244] = 0;
	prom_ucode[0251] = 0;
	prom_ucode[0256] = 0;
#endif
}

char *breakpoint_name_prom;
char *breakpoint_name_mcr;
int breakpoint_count;
char *tracelabel_name_mcr;

int
breakpoint_set_prom(char *arg)
{
	breakpoint_name_prom = arg;
	return 0;
}

int
breakpoint_set_mcr(char *arg)
{
	breakpoint_name_mcr = arg;
	return 0;
}

int
breakpoint_set_count(int count)
{
	printf("breakpoint: max count %d\n", count);
	breakpoint_count = count;
	return 0; 
}

int
tracelabel_set_mcr(char *arg)
{
	tracelabel_name_mcr = arg;
	return 0;
}

int
set_breakpoints(int *ptrace_pt_prom, int *ptrace_pt, int *ptrace_pt_count, int *ptrace_label_pt)
{
	max_cycles = 0;

#if 0
	trace_disk_flag = 1;
	trace_io_flag = 1;
	trace_int_flag = 1;
#endif

	if (breakpoint_name_prom) {
		if (sym_find(0, breakpoint_name_prom, ptrace_pt_prom)) {
			if (isdigit(breakpoint_name_prom[0])) {
				sscanf(breakpoint_name_prom, "%o", ptrace_pt_prom);
			} else {
				fprintf(stderr,
					"can't find prom breakpoint '%s'\n",
					breakpoint_name_prom);
				return -1;
			}
		}
		printf("breakpoint [prom]: %s %o\n",
		       breakpoint_name_prom, *ptrace_pt_prom);

		*ptrace_pt_count = 1;
	}

	if (breakpoint_name_mcr) {
		if (sym_find(1, breakpoint_name_mcr, ptrace_pt)) {
			if (isdigit(breakpoint_name_mcr[0])) {
				sscanf(breakpoint_name_mcr, "%o", ptrace_pt);
			} else {
				fprintf(stderr,
					"can't find mcr breakpoint '%s'\n",
					breakpoint_name_mcr);
				return -1;
			}
		}
		printf("breakpoint [mcr]: %s %o\n",
		       breakpoint_name_mcr, *ptrace_pt);

		*ptrace_pt_count = 1;
	}

	if (breakpoint_count) {
		*ptrace_pt_count = breakpoint_count;
	}

	if (tracelabel_name_mcr) {
		if (sym_find(1, tracelabel_name_mcr, ptrace_label_pt)) {
			fprintf(stderr, "can't find mcr trace label '%s'\n",
				tracelabel_name_mcr);
			return -1;
		}
		printf("trace label point [mcr]: %s %o\n",
		       tracelabel_name_mcr, *ptrace_label_pt);
	}

	return 0;
}

int
set_late_breakpoint(int *ptrace_pt, int *ptrace_pt_count)
{
	if (breakpoint_name_mcr) {
		if (sym_find(1, breakpoint_name_mcr, ptrace_pt)) {
			if (isdigit(breakpoint_name_mcr[0])) {
				sscanf(breakpoint_name_mcr, "%o", ptrace_pt);
			} else {
				fprintf(stderr,
					"can't find mcr breakpoint '%s'\n",
					breakpoint_name_mcr);
				return -1;
			}
		}
		printf("breakpoint [mcr]: %s %o\n",
		       breakpoint_name_mcr, *ptrace_pt);

		*ptrace_pt_count = 1;
	}

	if (breakpoint_count) {
		*ptrace_pt_count = breakpoint_count;
	}

	return 0;
}

void
show_label_closest(unsigned int upc)
{
	int offset;
	char *sym;

	if ((sym = sym_find_last(!prom_enabled_flag, upc, &offset))) {
		if (offset == 0)
			printf("%s", sym);
		else
			printf("%s+%o", sym, offset);
	}
}

void
show_label_closest_padded(unsigned int upc)
{
	int offset;
	char *sym;

	if ((sym = sym_find_last(!prom_enabled_flag, upc, &offset))) {
		if (offset == 0)
			printf("%-16s  ", sym);
		else
			printf("%-16s+%o", sym, offset);
	}
}

/*
For 32-bit integers, (A + B) & (1 << 32)
will always be zero. Without resorting to 64-bit arithmetic,
you can find the carry by B > ~A.
How does it work? ~A (the complement of A) is the largest possible
number you can add to A without a carry:
A + ~A = (1 << 32) - 1.
If B is any larger, then a carry will be generated from the top bit.
*/

#define add32(a, b, ci, out, co) \
		out = (a) + (b) + ((ci) ? 1 : 0); \
		co = (ci) ? (((b) >= ~(a)) ? 0:1) : (((b) >  ~(a)) ? 0:1) ;

#define sub32(a, b, ci, out, co) \
		out = (a) - (b) - ((ci) ? 0 : 1); \
		co = (unsigned)(out) < (unsigned)(a) ? 1 : 0;

#if 0
/* old, slow, realible version */
#define add32(a, b, ci, out, co) \
		lv = (long long)(a) + (b) + ((ci) ? 1 : 0); \
		out = lv; co = (lv >> 32) ? 1 : 0;

#define sub32(a, b, ci, out, co) \
		lv = (long long)(a) - (b) - ((ci) ? 0 : 1); \
		out = lv; co = (lv >> 32) ? 1 : 0;
#endif


/*
 * 'The time has come,' the Walrus said,
 *   'To talk of many things:
 * Of shoes -- and ships -- and sealing wax --
 *   Of cabbages -- and kings --
 * And why the sea is boiling hot --
 *   And whether pigs have wings.'
 *       -- Lewis Carroll, The Walrus and Carpenter
 *
 * (and then, they ate all the clams :-)
 *
 */

//char no_exec_next;
//int m_src_value, a_src_value;

//unsigned int out_bus;
//ucw_t u, w;
//int64 lv;
//static ucw_t p1;
//static int p0_pc, p1_pc;

int
run(void)
{
	int trace_pt_prom, trace_pt, trace_pt_count, trace_label_pt;
	char *sym, *last_sym = 0;
    ucw_t p1 = 0;
    int p0_pc = 0, p1_pc= 0;
    char no_exec_next = 0;

	/* 2Mwords */
	phys_ram_pages = 8192;

	u_pc = 0;
	prom_enabled_flag = 1;
	run_ucode_flag = 1;

	trace_pt = 0;
	trace_pt_count = 0;
	trace_label_pt = 0;

	set_breakpoints(&trace_pt_prom, &trace_pt, &trace_pt_count, &trace_label_pt);

	printf("run:\n");

	patch_prom_code();

	write_phy_mem(0, 0);

	timing_start();

	while (run_ucode_flag) {
		char op_code;
		char invert_sense, take_jump;
		int a_src, m_src, new_pc, dest, alu_op;
		int r_bit, p_bit, n_bit, ir8, ir7;
		int m_src_value, a_src_value;

		int widthm1, pos;
		int mr_sr_bits;
		unsigned int left_mask, right_mask, mask, old_q;
		int left_mask_index, right_mask_index;

		int disp_const, disp_addr;
		int map, len, rot;
		int carry_in, do_add, do_sub;

		unsigned int out_bus;
		int64 lv;
		ucw_t u, w;
#define p0 u

		char n_plus1, enable_ish;
		char i_long, popj;

		if (cycles == 0) {
			p0 = p1 = 0;
			p1_pc = 0;
			no_exec_next = 0;
		}

	next:
#ifdef LASHUP
		lashup_poll();
#endif
		iob_poll(cycles);

		disk_poll();

		if ((cycles & 0x0ffff) == 0) {
			display_poll();
			chaos_poll();
#ifdef CADR2
			ether_poll();
#endif
		}

#define FETCH()	(prom_enabled_flag ? prom_ucode[u_pc] : ucode[u_pc])

		/* pipeline */
		p0 = p1;
		p0_pc = p1_pc;

		/* fetch next instruction from prom or ram */
		p1 = FETCH();
		p1_pc = u_pc;
		u_pc++;

		if (new_md_delay) {
			new_md_delay--;
			if (new_md_delay == 0)
				md = new_md;
		}

		/* effectively stall pipe for one cycle */
		if (no_exec_next) {
			tracef("no_exec_next; u_pc %o\n", u_pc);
			no_exec_next = 0;

			p0 = p1;
			p0_pc = p1_pc;

			p1 = FETCH();
			p1_pc = u_pc;
			u_pc++;
		}

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

#ifdef LASHUP
		lashup_start(&p0_pc, &p0, &p1_pc, &p1, &u_pc, &trace);
#endif

#if defined(OSX)
                {
                    extern int macromicro;
                    extern void step_poll(unsigned int);

                    if (!macromicro)
                        step_poll((unsigned int)p0_pc);
                }
#endif

		/* ----------- trace ------------- */

#ifdef STAT_PC_HISTORY
		record_pc_history(p0_pc, vma, md);
#endif
#ifdef STAT_PC_HISTOGRAM
		record_pc_histogram(p0_pc);
#endif

#ifdef TRACE
		if (trace_late_set) {
			set_late_breakpoint(&trace_pt, &trace_pt_count);
			trace_late_set = 0;
		}

		/* see if we hit a label trace point */
		if (trace_label_pt &&
		    p0_pc == trace_label_pt)
		{
//			show_pc_history();
//			trace = 1;
			trace_mcr_labels_flag = 1;
		}

		/* see if we hit a trace point */
		if (trace_pt_prom &&
		    p0_pc == trace_pt_prom &&
		    trace == 0 &&
		    prom_enabled_flag == 1)
		{
			if (trace_pt_count) {
				if (--trace_pt_count == 0)
					trace = 1;
			} else {
				trace = 1;
			}

			if (trace)
				printf("trace on\n");
		}

		if (trace_pt &&
		    p0_pc == trace_pt &&
		    trace == 0 &&
		    prom_enabled_flag == 0)
		{
			if (trace_pt_count) {
				if (--trace_pt_count == 0)
					trace = 1;
			} else {
				trace = 1;
			}

			if (trace)
				printf("trace on\n");
		}

		if (stop_after_prom_flag) {
			if (prom_enabled_flag == 0) run_ucode_flag = 0;
		}

		if (trace_prom_flag) {
			if (prom_enabled_flag == 1) trace = 1;
		}

		if (trace_mcr_flag) {
			if (prom_enabled_flag == 0) trace = 1;
		}

		/* enforce max trace count */
		if (trace) {
			if (max_trace_cycles &&
			    trace_cycles++ > max_trace_cycles)
			{
				printf("trace cycle count exceeded, pc %o\n",
				       u_pc);
				break;
			}
		}
#endif

		/* ----------- end trace ------------- */

		/* enforce max cycles */
		cycles++;
		/* glug. overflow */
		if (cycles == 0) cycles = 1;

		if (max_cycles && cycles > max_cycles) {
			int offset;
			printf("cycle count exceeded, pc %o\n", u_pc);

			if ((sym = sym_find_last(!prom_enabled_flag,
						 u_pc, &offset)))
			{
				if (offset == 0)
					printf("%s:\n", sym);
				else
					printf("%s+%o:\n", sym, offset);
			}

			break;
		}

		if (trace_after_flag && (cycles > begin_trace_cycle))
			trace = 1;

		i_long = (u >> 45) & 1;
		popj = (u >> 42) & 1;

		if (trace) {
			printf("------\n");

#if 1
			if ((sym = sym_find_by_val(!prom_enabled_flag, p0_pc)))
			{
				printf("%s:\n", sym);
			}
#else
			printf("\n");
			show_label_closest(p0_pc);
			printf(":\n");
#endif

			printf("%03o %016llo%s",
			       p0_pc, u, i_long ? " (i-long)" : "");

			if (lc != 0) {
				printf(" (lc=%011o %011o)", lc, lc>>2);
			}

			printf("\n");
			disassemble_ucode_loc(p0_pc, u);
		}

		/* trace label names in mcr */
		if (trace_mcr_labels_flag && !trace) {
			if (!prom_enabled_flag) {
				int offset;
				if ((sym = sym_find_last(1, p0_pc, &offset)))
				{
					if (offset == 0 && sym != last_sym) {
					     printf("%s: (lc=%011o %011o)\n",
						    sym, lc, lc>>2);
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
					(spc_stack[spc_stack_ptr] & 01777777);
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
				if (0) show_pdl_local();

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
				l2_data = map_vtop(md, (int *)&l1_data, (int *)0);
				
				m_src_value = 
					(write_fault_bit << 31) |
					(access_fault_bit << 30) |
					((l1_data & 037) << 24) |
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
				if (lc_byte_mode_flag)
					m_src_value = lc;
				else
					m_src_value = lc & ~1;
				break;
			case 014:
				m_src_value = (spc_stack_ptr << 24) |
					(spc_stack[spc_stack_ptr] & 01777777);

				tracef("reading spc[%o] + ptr -> %o\n",
				       spc_stack_ptr, m_src_value);

				spc_stack_ptr = (spc_stack_ptr - 1) & 037;
				break;

			case 024:
				tracef("reading pdl[%o] -> %o, pop\n",
				       pdl_ptr, pdl_memory[pdl_ptr]);
				if (0) show_pdl_local();

				m_src_value = pdl_memory[pdl_ptr];
				pdl_ptr = (pdl_ptr - 1) & 01777;
				break;
			case 025:
				tracef("reading pdl[%o] -> %o\n",
				       pdl_ptr, pdl_memory[pdl_ptr]);
				if (0) show_pdl_local();

				m_src_value = pdl_memory[pdl_ptr];
				break;
			}
		} else {
			m_src_value = read_m_mem(m_src);
		}

		/*
		 * decode instruction
		 */

		switch (op_code = (u >> 43) & 03) {
		case 0: /* alu */

#if 1
			/* nop short cut */
			if ((u & NOP_MASK) == 0) {
				goto next;
			}
#endif

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

			/* (spec) ir7 is backward in memo? */
		        if (ir8 == 0 && ir7 == 0) {
#ifdef STAT_ALU_USE
				alu_stat0[alu_op]++;
#endif
				/* logic */
				alu_carry = 0;
				switch (alu_op) {
				case 0: /* [SETZ] */
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
//new - better?
//					alu_out = ~(a_src_value | m_src_value);
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
#ifdef STAT_ALU_USE
				alu_stat1[alu_op]++;
#endif
				/* arithmetic */
				switch (alu_op) {
				case 0: /* -1 */
					alu_out = carry_in ? 0 : -1;
					alu_carry = 0;
					break;
				case 1: /* (M&A)-1 */
					lv = (long long)(m_src_value & a_src_value) -
						(carry_in ? 0 : 1);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 2: /* (M&~A)-1 */
					lv = (long long)(m_src_value & ~a_src_value) -
						(carry_in ? 0 : 1);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 3: /* M-1 */
					lv = (long long)m_src_value - (carry_in ? 0 : 1);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 4: /* M|~A */
					lv = (long long)(m_src_value | ~a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 5: /* (M|~A)+(M&A) */
					lv = (long long)(m_src_value | ~a_src_value) +
						(m_src_value & a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 6: /* M-A-1 [SUB] */
					sub32(m_src_value,
					      a_src_value,
					      carry_in,
					      alu_out, alu_carry);
					break;
				case 7: /* (M|~A)+M */
					lv = (long long)(m_src_value | ~a_src_value) +
						m_src_value +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 010: /* M|A */
//?? is this right? check 74181
					lv = (long long)(m_src_value |
							 a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 011: /* M+A [ADD] */
					add32(m_src_value, a_src_value,
					      carry_in, alu_out, alu_carry);
					break;
				case 012: /* (M|A)+(M&~A) */
					lv = (long long)(m_src_value | a_src_value) +
						(m_src_value & ~a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 013: /* (M|A)+M */
					lv = (long long)(m_src_value | a_src_value) +
						m_src_value +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 014: /* M */
#if 1
					/* faster */
					alu_out = m_src_value +
						(carry_in ? 1 : 0);
					alu_carry = 0;
					if (m_src_value == 0xffffffff &&
					    carry_in)
						alu_carry = 1;
#else
					lv = (long long)
						m_src_value +
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
#endif
					break;
				case 015: /* M+(M&A) */
					lv = (long long)m_src_value +
						(m_src_value & a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 016: /* M+(M|~A) */
					lv = (long long)m_src_value +
						(m_src_value | ~a_src_value) +
						(carry_in ? 1 : 0);
					alu_out = (unsigned int)lv;
					alu_carry = (lv >> 32) ? 1 : 0;
					break;
				case 017: /* M+M */
					add32(m_src_value, m_src_value,
					      carry_in, alu_out, alu_carry);
//new - better?
//					alu_out = (m_src_value << 1) | 
//						(carry_in ? 1 : 0);
//					alu_carry = (m_src_value & 0x80000000)
//						? 1 : 0;
					break;
				}
			}

			if (ir8 == 1) {
#ifdef STAT_ALU_USE
				alu_stat2[alu_op]++;
#endif

				/* conditional alu op code */
				switch (alu_op) {
				case 0: /* multiply step */
					/* ADD if Q<0>=1, else SETM */
					do_add = q & 1;
					if (do_add) {
						add32(a_src_value,
						      m_src_value, 
						      carry_in,
						      alu_out, alu_carry);
					} else {
						alu_out = m_src_value;
						alu_carry =
							alu_out & 0x80000000 ?
							1 : 0;
					}
					break;
				case 1: /* divide step */
					tracef("divide step\n");
					do_sub = q & 1;
					tracef("do_sub %d\n", do_sub);

#if 1
					if (do_sub) {
						sub32(m_src_value, a_src_value,
						      !carry_in,
						      alu_out, alu_carry);
					} else {
						add32(m_src_value, a_src_value,
						      carry_in,
						      alu_out, alu_carry);
					}
#else
					if (do_sub) {
						lv = (long long)
							m_src_value -
							a_src_value -
							(carry_in ? 1 : 0);
					} else {
						lv = (long long)
							m_src_value +
							a_src_value +
							(carry_in ? 1 : 0);
					}
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
#endif
					break;
				case 5: /* remainder correction */
					tracef("remainder correction\n");
					do_sub = q & 1;

					tracef("do_sub %d\n", do_sub);
					if (do_sub) {
						/* setm */
						alu_carry = 0;
					} else {
#if 1
						add32(alu_out, a_src_value,
						      carry_in,
						      alu_out, alu_carry);
#else
						lv =
							(long long)alu_out +
							a_src_value +
							(carry_in ? 1 : 0);
						alu_out = lv;
						alu_carry = (lv >> 32) ? 1 : 0;
#endif
					}

					break;
				case 011:
					/* initial divide step */
					tracef("divide-first-step\n");
					tracef("divide: %o / %o \n",
					       q, a_src_value);

#if 1
					sub32(m_src_value, a_src_value,
					      !carry_in, alu_out, alu_carry);
#else
					lv = (long long)m_src_value -
						a_src_value -
						(carry_in ? 1 : 0);
					alu_out = lv;
					alu_carry = (lv >> 32) ? 1 : 0;
#endif
					tracef("alu_out %08x %o %d\n",
					       alu_out, alu_out, alu_out);
					break;

				default:
					printf("UNKNOWN cond alu op code %o\n",
					       alu_op);
				}
			}

			take_jump = 0;

			/* Q control */
			old_q = q;
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

			/* output bus control */
			switch (out_bus) {
			case 0:
				printf("out_bus == 0!\n");
out_bus = rotate_left(m_src_value, u & 037);
				break;
			case 1: out_bus = alu_out;
				break;
			case 2:
#if 0
				out_bus = (alu_out >> 1) | 
					(alu_out & 0x80000000);
#else
				/*
				 * "ALU output shifted right one, with
				 * the correct sign shifted in,
				 * regardless of overflow."
				 */
				out_bus = (alu_out >> 1) |
					(alu_carry ? 0x80000000 : 0);
#endif
				break;
			case 3: out_bus = (alu_out << 1) | 
					((old_q & 0x80000000) ? 1 : 0);
				break;
			}

			write_dest(u, dest, out_bus);

			tracef("alu_out 0x%08x, alu_carry %d, q 0x%08x\n",
			       alu_out, alu_carry, q);
//		alu_done:
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

			/* halt-cons? */
			if (((u >> 10) & 3) == 1) {
				printf("halted\n");
				run_ucode_flag = 0;
				break;
			}

		process_jump:
			/* jump condition */
			if (u & (1<<5)) {
				switch (u & 017) {
				case 0:
					if (op_code != 2)
						printf("jump-condition == 0! u_pc=%o\n",
						       p0_pc);
					break;
				case 1:
					take_jump = m_src_value < a_src_value;
					break;
				case 2:
					take_jump = m_src_value <= a_src_value;
#if 0
					tracef("%o <= %o; take_jump %o\n",
					       m_src_value, a_src_value, take_jump);
#endif
					break;
				case 3:
					take_jump = m_src_value == a_src_value;
					break;
				case 4: 
					take_jump = page_fault_flag;
					break;
				case 5:
					tracef("jump i|pf\n");
					take_jump = page_fault_flag |
						(interrupt_enable_flag ?
						 interrupt_pending_flag :0);
					break;
				case 6:
					tracef("jump i|pf|sb\n");
					take_jump = page_fault_flag |
						(interrupt_enable_flag ?
						 interrupt_pending_flag:0) |
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

			if (((u >> 10) & 3) == 3) {
				printf("jump w/misc-3!\n");
			}
 
			if (invert_sense)
				take_jump = !take_jump;

			if (p_bit && take_jump) {
				if (!n_bit)
					push_spc(u_pc);
				else
					push_spc(u_pc-1);
			}

			/* P & R & jump-inst -> write ucode */
			if (p_bit && r_bit && op_code == 1) {
				w = ((ucw_t)(a_src_value & 0177777) << 32) |
					(unsigned int)m_src_value;
				write_ucode(new_pc, w);
			}

			if (r_bit && take_jump) {
				new_pc = pop_spc();

				/* spc<14> */
				if ((new_pc >> 14) & 1) {
					advance_lc(&new_pc);
				}

				new_pc &= 037777;
			}

			if (take_jump) {

//				if (new_pc == u_pc && n_bit && !p_bit) {
//					printf("loop detected pc %o\n", u_pc);
//					run_ucode_flag = 0;
//				}

				if (n_bit)
					no_exec_next = 1;

				u_pc = new_pc;

#if 0
				/* I don't think this ever happens */
				if (popj && r_bit == 0 && p_bit == 0) {
					pop_spc();
				}
#endif

				/* inhibit possible popj */
				popj = 0;
			}

			break;

		case 2: /* dispatch */
			disp_const = (u >> 32) & 01777;

			n_plus1 = (u >> 25) & 1;
			enable_ish = (u >> 24) & 1;
			disp_addr = (u >> 12) & 03777;
			map = (u >> 8) & 3;
			len = (u >> 5) & 07;
			pos = u & 037;

			/* misc function 3 */
			if (((u >> 10) & 3) == 3) {
				if (lc_byte_mode_flag) {
					/* byte mode */
					char ir4, ir3, lc1, lc0;

					ir4 = (u >> 4) & 1;
					ir3 = (u >> 3) & 1;
					lc1 = (lc >> 1) & 1;
					lc0 = (lc >> 0) & 1;

					pos = u & 007;
					pos |= ((ir4 ^ (lc1 ^ lc0)) << 4) |
						((ir3 ^ lc0) << 3);

					tracef("byte-mode, pos %o\n", pos);
				} else {
					/* 16 bit mode */
					char ir4, lc1;

					ir4 = (u >> 4) & 1;
					lc1 = (lc >> 1) & 1;

					pos = u & 017;

					/* (spec) result needs to be inverted*/
					pos |= ((ir4 ^ lc1) ? 0 : 1) << 4;

					tracef("16b-mode, pos %o\n", pos);
				}
			}

			/* misc function 2 */
			if (((u >> 10) & 3) == 2) {
				tracef("dispatch_memory[%o] <- %o\n",
				       disp_addr, a_src_value);
				dispatch_memory[disp_addr] = a_src_value;
				goto dispatch_done;
			}

			tracef("m-src %o, ", m_src_value);

			/* rotate m-source */
			m_src_value = rotate_left(m_src_value, pos);

			/* generate mask */
			left_mask_index = (len - 1) & 037;

			mask = ~0;
			mask >>= 31 - left_mask_index;

			/* len == 0 */
			if (len == 0)
				mask = 0;

			/* put ldb into dispatch-addr */
			disp_addr |= m_src_value & mask;

			tracef("rotated %o, mask %o, result %o\n",
			       m_src_value, mask, m_src_value & mask);

			/* tweek dispatch-addr with l2 map bits */
			if (map) {
				int l2_map, bit18, bit19;

				/* (spec) bit 0 is or'd, not replaced */
				/* disp_addr &= ~1; */

				l2_map = map_vtop(md, (int *)0, (int *)0);

				/* (spec) schematics show this as bit 19,18 */
				bit19 = ((l2_map >> 19) & 1) ? 1 : 0;
				bit18 = ((l2_map >> 18) & 1) ? 1 : 0;

				tracef("md %o, l2_map %o, b19 %o, b18 %o\n",
				       md, l2_map, bit19, bit18);

				switch (map) {
				case 1: disp_addr |= bit18; break;
				case 2: disp_addr |= bit19; break;
				case 3: disp_addr |= bit18 | bit19; break;
				}
			}

			disp_addr &= 03777;

			tracef("dispatch[%o] -> %o ",
			       disp_addr, dispatch_memory[disp_addr]);

			disp_addr = dispatch_memory[disp_addr];

			dispatch_constant = disp_const;

			/* 14 bits */
			new_pc = disp_addr & 037777;

			n_bit = (disp_addr >> 14) & 1;
			p_bit = (disp_addr >> 15) & 1;
			r_bit = (disp_addr >> 16) & 1;

			tracef("%s%s%s\n",
			       n_bit ? "N " : "",
			       p_bit ? "P " : "",
			       r_bit ? "R " : "");

			if (n_plus1 && n_bit) {
				u_pc--;
			}

			invert_sense = 0;
			take_jump = 1;
			u = 1<<5;

			/* enable instruction sequence hardware */
			if (enable_ish) {
				advance_lc((int *)0);
			}

			/* fall-through on dispatch */ 
			if (p_bit && r_bit) {
				if (n_bit)
					no_exec_next = 1;
				goto dispatch_done;
			}

			goto process_jump;

		dispatch_done:
			break;

		case 3: /* byte */
			dest = (u >> 14) & 07777;
			mr_sr_bits = (u >> 12) & 3;

			tracef("a=%o (%o), m=%o (%o), dest=%o\n",
			       a_src, a_src_value,
			       m_src, m_src_value, dest);

			widthm1 = (u >> 5) & 037;
			pos = u & 037;

			/* misc function 3 */
			if (((u >> 10) & 3) == 3) {
				if (lc_byte_mode_flag) {
					/* byte mode */
					char ir4, ir3, lc1, lc0;

					ir4 = (u >> 4) & 1;
					ir3 = (u >> 3) & 1;
					lc1 = (lc >> 1) & 1;
					lc0 = (lc >> 0) & 1;

					pos = u & 007;
					pos |= ((ir4 ^ (lc1 ^ lc0)) << 4) |
						((ir3 ^ lc0) << 3);

					tracef("byte-mode, pos %o\n", pos);
				} else {
					/* 16 bit mode */
					char ir4, lc1;

					ir4 = (u >> 4) & 1;
					lc1 = (lc >> 1) & 1;

					pos = u & 017;

					/* (spec) result needs to be inverted*/
					pos |= ((ir4 ^ lc1) ? 0 : 1) << 4;

					tracef("16b-mode, pos %o\n", pos);
				}
			}

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
				printf("mr_sr_bits == 0!\n");
				break;
			case 1: /* ldb */
				tracef("ldb; m %o\n", m_src_value);

				m_src_value = rotate_left(m_src_value, pos);

				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);

				tracef("ldb; m-rot %o, mask %o, result %o\n", 
				       m_src_value, mask, out_bus);
				break;
			case 2: /* selective desposit */
				out_bus = (m_src_value & mask) |
					(a_src_value & ~mask);
				tracef("sel-dep; a %o, m %o, mask %o -> %o\n", 
				       a_src_value, m_src_value, mask, out_bus);
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

		if (popj) {
			tracef("popj; ");
			u_pc = pop_spc();

			/* spc<14> */
			if ((u_pc >> 14) & 1) {
				advance_lc(&u_pc);
			}

			u_pc &= 037777;
		}
	}

	{
		int offset;
		sym = sym_find_last(!prom_enabled_flag, u_pc, &offset);
		printf("%s+%o:\n", sym, offset);
	}

	timing_stop();

	if (save_state_flag) {
		save_state();
	}

	if (dump_state_flag) {
		dump_state();
	}

#ifdef STAT_PC_HISTOGRAM
	show_pc_histogram();
#endif

#ifdef STAT_ALU_USE
	{
		int i;
		printf("ALU op-code usage:\n");
		for (i = 0; i < 16; i++) {
			printf("%2i %2o %08u %08u %08u\n",
			       i, i, alu_stat0[i], alu_stat1[i], alu_stat2[i]);
		}
	}
#endif

	return 0;
}

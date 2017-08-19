// 'The time has come,' the Walrus said,
//   'To talk of many things:
// Of shoes -- and ships -- and sealing wax --
//   Of cabbages -- and kings --
// And why the sea is boiling hot --
//   And whether pigs have wings.'
//       -- Lewis Carroll, The Walrus and Carpenter
//
// (and then, they ate all the clams :-)

#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "ucode.h"

extern ucw_t prom_ucode[512];

ucw_t ucode[16 * 1024];
unsigned int a_memory[1024];
static unsigned int m_memory[32];
static unsigned int dispatch_memory[2048];

static unsigned int pdl_memory[1024];
int pdl_ptr;
int pdl_index;

int lc;

static int spc_stack[32];
int spc_stack_ptr;

struct page_s {
	unsigned int w[256];
};

static struct page_s *phy_pages[16 * 1024];
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

void show_label_closest(unsigned int upc);
void show_label_closest_padded(unsigned int upc);
char *find_function_name(int the_lc);
int restore_state(void);

extern void video_read(int offset, unsigned int *pv);
extern void video_write(int offset, unsigned int bits);
extern void iob_unibus_read(int offset, int *pv);
extern void iob_unibus_write(int offset, int v);
extern void disk_xbus_read(int offset, unsigned int *pv);
extern void disk_xbus_write(int offset, unsigned int v);
extern void tv_xbus_read(int offset, unsigned int *pv);
extern void tv_xbus_write(int offset, unsigned int v);
extern void uart_xbus_read(int, unsigned int *);
extern void uart_xbus_write(int, unsigned int);

extern void disassemble_ucode_loc(int loc, ucw_t u);
extern int sym_find(int mcr, char *name, int *pval);
void reset_pc_histogram(void);

extern void timing_start();
extern void timing_stop();
extern void iob_poll();
extern void disk_poll();
extern void display_poll();
extern void chaos_poll();

// ---!!! Should expose this some other way.
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
	// Unibus interrupts enabled?
	if (interrupt_status_reg & 02000) {
		traceint("assert: unibus interrupt (enabled)\n");
		set_interrupt_status_reg((interrupt_status_reg & ~01774) | 0100000 | (vector & 01774));
	} else {
		traceint("assert: unibus interrupt (disabled)\n");
	}
}

void
deassert_unibus_interrupt(int vector)
{
	if (interrupt_status_reg & 0100000) {
		traceint("deassert: unibus interrupt\n");
		set_interrupt_status_reg(interrupt_status_reg & ~(01774 | 0100000));
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

unsigned int last_virt = 0xffffff00;
unsigned int last_l1;
unsigned int last_l2;

static inline void
invalidate_vtop_cache(void)
{
	last_virt = 0xffffff00;
}

// Map virtual address to physical address, possibly returning l1
// mapping and possibly returning offset into page.
static inline unsigned int
map_vtop(unsigned int virt, int *pl1_map, int *poffset)
{
	int l1_index;
	int l2_index;
	int l1;
	unsigned int l2;

	virt &= 077777777;	// 24 bit address.

	// Frame buffer.
	if ((virt & 077700000) == 077000000) {
		if (virt >= 077051757 && virt <= 077051763) {
			traceio("disk run light\n");
		}
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036000;
	}

	// Color.
	if ((virt & 077700000) == 077200000) {
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036000;
	}

	// This should be moved below - I'm not sure it has to happen
	// anymore.
	if ((virt & 077777400) == 077377400) {
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036777;
	}

	// 11 bit L1 index.
	l1_index = (virt >> 13) & 03777;
	l1 = l1_map[l1_index] & 037;
	if (pl1_map)
		*pl1_map = l1;

	// 10 bit L2 index.
	l2_index = (l1 << 5) | ((virt >> 8) & 037);
	l2 = l2_map[l2_index];

	if (poffset)
		*poffset = virt & 0377;

	last_virt = virt & 0xffffff00;
	last_l1 = l1;
	last_l2 = l2;

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

	page = (struct page_s *) page_block;

	page_block += sizeof(struct page_s);
	page_block_count--;

	if (page_block_count == 0)
		page_block = 0;

	return page;
}

// Add a new physical memory page, generally in response to L2 mapping
// (but can also be due to physical access to RAM).
int
add_new_page_no(int pn)
{
	struct page_s *page;

	page = phy_pages[pn];
	if (page == 0) {
		page = new_page();
		if (page) {
			phy_pages[pn] = page;
			return 0;
		}
	}
	return -1;
}

// Read physical memory, with no virtual-to-physical mapping (used by
// disk controller).
int
read_phy_mem(int paddr, unsigned int *pv)
{
	int pn;
	int offset;
	struct page_s *page;

	pn = paddr >> 8;
	offset = paddr & 0377;

	page = phy_pages[pn];
	if (page == 0) {
		// Page does not exist.
		if (pn < phys_ram_pages) {
			tracef("[read_phy_mem] adding phy ram page %o (address %o)\n", pn, paddr);
			add_new_page_no(pn);
			page = phy_pages[pn];
		} else {
			printf("[read_phy_mem] address %o does not exist\n", paddr);
			return -1;
		}
	}

	*pv = page->w[offset];

	return 0;
}

int
write_phy_mem(int paddr, unsigned int v)
{
	int pn;
	int offset;
	struct page_s *page;

	pn = paddr >> 8;
	offset = paddr & 0377;

	page = phy_pages[pn];
	if (page == 0) {
		// Page does not exist - add it (probably result of
		// disk write).
		if (pn < phys_ram_pages) {
			tracef("[write_phy_mem] adding phy ram page %o (address %o)\n", pn, paddr);
			add_new_page_no(pn);
			page = phy_pages[pn];
		} else {
			printf("[write_phy_mem] address %o does not exist\n", paddr);
			return -1;
		}
	}

	page->w[offset] = v;

	return 0;
}

// ---!!! read_mem, write_mem: Document each address.

// Read virtual memory, returns -1 on fault and 0 if OK.
int
read_mem(int vaddr, unsigned int *pv)
{
	unsigned int map;
	int pn;
	int offset;
	struct page_s *page;

	access_fault_bit = 0;
	write_fault_bit = 0;
	page_fault_flag = 0;

	// 14 bit page number.
	map = map_vtop(vaddr, (int *) 0, &offset);
	pn = map & 037777;

	if ((map & (1 << 23)) == 0) {
		// No access permission.
		access_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		*pv = 0;
		tracef("read_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

	if (pn < 020000 && (page = phy_pages[pn])) {
		*pv = page->w[offset];
		return 0;
	}

	// Simulate fixed number of RAM pages (< 2MW?).
	if (pn >= phys_ram_pages && pn <= 035777) {
		*pv = 0xffffffff;
		return 0;
	}

	if (pn == 036000) {
		// Inhibit color probe.
		if ((vaddr & 077700000) == 077200000) {
			*pv = 0x0;
			return 0;
		}
		offset = vaddr & 077777;
		video_read(offset, pv);
		return 0;
	}

	// Extra xbus devices.
	if (pn == 037764) {
		offset <<= 1;
		iob_unibus_read(offset, (int *) pv);
		return 0;
	}

	// Unibus.
	if (pn == 037766) {
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

	// Disk & TV controller on XBUS.
	if (pn == 036777) {
		if (offset >= 0370) { // Disk.
			 disk_xbus_read(offset, pv);
			 return 0;
		}
		if (offset == 0360) { // TV.
			 tv_xbus_read(offset, pv);
			 return 0;
		}
		printf("xbus read %o %o\n", offset, vaddr);
		*pv = 0;
		return 0;
	}

	if (pn == 036776) {
		uart_xbus_read(offset, pv);
		return 0;
	}

	// Page fault.
	if ((page = phy_pages[pn]) == 0) {
		page_fault_flag = 1;
		opc = pn;
		tracef("read_mem(vaddr=%o) page fault\n", vaddr);
		*pv = 0;
		return -1;
	}

	*pv = page->w[offset];
	return 0;
}

// Write virtual memory.
int
write_mem(int vaddr, unsigned int v)
{
	unsigned int map;
	int pn;
	int offset;
	struct page_s *page;

	write_fault_bit = 0;
	access_fault_bit = 0;
	page_fault_flag = 0;

	// 14 bit page number.
	map = map_vtop(vaddr, (int *) 0, &offset);
	pn = map & 037777;

	if ((map & (1 << 23)) == 0) {
		// No access permission.
		access_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		tracef("write_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

	if ((map & (1 << 22)) == 0) {
		// No write permission.
		write_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		tracef("write_mem(vaddr=%o) write fault\n", vaddr);
		return -1;
	}

	if (pn < 020000 && (page = phy_pages[pn])) {
		page->w[offset] = v;
		return 0;
	}

	if (pn == 036000) {
		// Inhibit color probe.
		if ((vaddr & 077700000) == 077200000) {
			return 0;
		}
		offset = vaddr & 077777;
		video_write(offset, v);
		return 0;
	}

	if (pn == 037760) {
		printf("tv: reg write %o, offset %o, v %o\n", vaddr, offset, v);
		return 0;
	}

	if (pn == 037764) {
		offset <<= 1;
		traceio("unibus: iob v %o, offset %o\n", vaddr, offset);
		iob_unibus_write(offset, v);
		return 0;
	}

	if (pn == 037766) {
		// Unibus.

		offset <<= 1;
		if (offset <= 036) {
			traceio("unibus: spy v %o, offset %o\n", vaddr, offset);
			switch (offset) {
			case 012:
				if ((v & 044) == 044) {
					traceio("unibus: disabling prom enable flag\n");
					prom_enabled_flag = 0;

					if (warm_boot_flag) {
						restore_state();
					}
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
			set_interrupt_status_reg((interrupt_status_reg & ~0036001) | (v & 0036001));
			return 0;
		case 042:
			traceio("unibus: write interrupt stim %o\n", v);
			set_interrupt_status_reg((interrupt_status_reg & ~0101774) | (v & 0101774));
			return 0;
		case 044:
			traceio("unibus: clear bus error %o\n", v);
			return 0;
		case 0100:
		case 0104:
		case 0110:
		case 0114:
			printf("unibus: cadr debugee (%o) v %o\n", offset, v);
			return 0;
		default:
			if (offset >= 0140 && offset <= 0176) {
				traceio("unibus: mapping reg %o\n", offset);
				return 0;
			}
			traceio("unibus: write? v %o, offset %o\n", vaddr, offset);
		}
	}

	if (pn == 036777) {
		if (offset >= 0370) {
			 disk_xbus_write(offset, v);
			 return 0;
		}
		if (offset == 0360) {
			 tv_xbus_write(offset, v);
			 return 0;
		}
	}

	if (pn == 036776) {
		uart_xbus_write(offset, v);
		return 0;
	}

	// Catch questionable accesses.
	if (pn >= 036000) {
		printf("??: reg write vaddr %o, pn %o, offset %o, v %o; u_pc %o\n", vaddr, pn, offset, v, u_pc);
	}

	if ((page = phy_pages[pn]) == 0) {
		// Page fault.
		page_fault_flag = 1;
		opc = pn;
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

static inline void
write_a_mem(int loc, unsigned int v)
{
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

static inline unsigned int
rotate_left(unsigned int value, int bitstorotate)
{
	unsigned int tmp;
	int mask;

	// Determine which bits will be impacted by the rotate.
	if (bitstorotate == 0)
		mask = 0;
	else
		mask = (int) 0x80000000 >> bitstorotate;
	// Save off the affected bits.
	tmp = (value & mask) >> (32 - bitstorotate);
	// Perform the actual rotate, and add the rotated bits back in
	// (in the proper location).
	return (value << bitstorotate) | tmp;
}

static inline void
push_spc(int pc)
{
	spc_stack_ptr = (spc_stack_ptr + 1) & 037;
	spc_stack[spc_stack_ptr] = pc;
}

static inline int
pop_spc(void)
{
	unsigned int v;

	v = spc_stack[spc_stack_ptr];
	spc_stack_ptr = (spc_stack_ptr - 1) & 037;
	return v;
}

// Advance the LC register, following the rules; will read next VMA if
// needed.
void
advance_lc(int *ppc)
{
	int old_lc;

	old_lc = lc & 0377777777;	// LC is 26 bits.

	if (lc_byte_mode_flag) {
		lc++;		// Byte mode.
	} else {
		lc += 2;	// 16-bit mode.
	}

	macro_pc_incrs++;

	// NEED-FETCH?
	if (lc & (1 << 31)) {
		lc &= ~(1 << 31);
		vma = old_lc >> 2;
		if (read_mem(old_lc >> 2, &new_md)) {
		}
		new_md_delay = 2;
		tracef("advance_lc() read vma %011o -> %011o\n", old_lc >> 2, new_md);
	} else {
		// Force skipping 2 instruction (PF + SET-MD).
		if (ppc)
			*ppc |= 2;
		tracef("advance_lc() no read; md = %011o\n", md);
	}

	{
		char lc0b;
		char lc1;
		char last_byte_in_word;

		// This is ugly, but follows the hardware logic (I
		// need to distill it to intent but it seems correct).
		lc0b =
			(lc_byte_mode_flag ? 1 : 0) & // Byte mode.

			((lc & 1) ? 1 : 0); // LC0.
		lc1 = (lc & 2) ? 1 : 0;
		last_byte_in_word = (~lc0b & ~lc1) & 1;
		tracef("lc0b %d, lc1 %d, last_byte_in_word %d\n", lc0b, lc1, last_byte_in_word);
		if (last_byte_in_word)
			// Set NEED-FETCH.
			lc |= (1 << 31);
	}
}

void
show_pdl_local(void)
{
	int min;
	int max;

	min = pdl_ptr > 4 ? pdl_ptr - 4 : 0;
	max = pdl_ptr < 1024 - 4 ? pdl_ptr + 4 : 1024;

	printf("pdl-ptr %o, pdl-index %o\n", pdl_ptr, pdl_index);

	if (pdl_index > 0 && pdl_index < pdl_ptr)
		min = pdl_index;

	for (int i = min; i < max; i += 4) {
		printf("PDL[%04o] %011o %011o %011o %011o\n", i, pdl_memory[i], pdl_memory[i + 1], pdl_memory[i + 2], pdl_memory[i + 3]);
	}
}

// Write value to decoded destination.
void
write_dest(ucw_t u, int dest, unsigned int out_bus)
{
	if (dest & 04000) {
		write_a_mem(dest & 03777, out_bus);
		return;
	}

	switch (dest >> 5) {
	case 1:			// LC (location counter) 26 bits.
		tracef("writing LC <- %o\n", out_bus);
		lc = (lc & ~0377777777) | (out_bus & 0377777777);

		if (lc_byte_mode_flag) {
			// ---!!! Not sure about byte mode...
		} else {
			// In half word mode, low order bit is
			// ignored.
			lc &= ~1;
		}

		// Set NEED-FETCH.
		lc |= (1 << 31);

		if (trace_lod_labels_flag) {
			char *s;

			show_label_closest(u_pc);
			printf(": lc <- %o (%o)", lc, lc >> 2);
			s = find_function_name(lc);
			if (s)
				printf(" '%s'", s);
			printf("\n");
		}
		break;
	case 2:			// Interrrupt Control <29-26>.
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

		lc = (lc & ~(017 << 26)) | // Preserve flags.
			(interrupt_control & (017 << 26));
		break;
	case 010:		// PDL (addressed by pointer)
		tracef("writing pdl[%o] <- %o\n", pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
		break;
	case 011:		// PDL (addressed by pointer, push)
		pdl_ptr = (pdl_ptr + 1) & 01777;
		tracef("writing pdl[%o] <- %o, push\n", pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
		break;
	case 012:		// PDL (address by index).
		tracef("writing pdl[%o] <- %o\n", pdl_index, out_bus);
		write_pdl_mem(USE_PDL_INDEX, out_bus);
		break;
	case 013:		// PDL index.
		tracef("pdl-index <- %o\n", out_bus);
		pdl_index = out_bus & 01777;
		break;
	case 014:		// PDL pointer.
		tracef("pdl-ptr <- %o\n", out_bus);
		pdl_ptr = out_bus & 01777;
		break;
	case 015:		// SPC data, push.
		push_spc(out_bus);
		break;
	case 016:		// Next instruction modifier (lo).
		oa_reg_lo = out_bus & 0377777777;
		oa_reg_lo_set = 1;
		tracef("setting oa_reg lo %o\n", oa_reg_lo);
		break;
	case 017:		// Next instruction modifier (hi).
		oa_reg_hi = out_bus;
		oa_reg_hi_set = 1;
		tracef("setting oa_reg hi %o\n", oa_reg_hi);
		break;
	case 020:		// VMA register (memory address).
		vma = out_bus;
		break;
	case 021:	      // VMA register, start main memory read.
		vma = out_bus;
		if (read_mem(vma, &new_md)) {
		}
		new_md_delay = 2;
		break;
	case 022:	     // VMA register, start main memory write.
		vma = out_bus;
		write_mem(vma, md);
		break;
	case 023:		// VMA register, write map.
		vma = out_bus;
		tracevm("vma-write-map md=%o, vma=%o (addr %o)\n", md, vma, md >> 13);
	write_map:
		if ((vma >> 26) & 1) {
			int l1_index;
			int l1_data;

			l1_index = (md >> 13) & 03777;
			l1_data = (vma >> 27) & 037;

			l1_map[l1_index] = l1_data;
			invalidate_vtop_cache();
			tracevm("l1_map[%o] <- %o\n", l1_index, l1_data);
		}

		if ((vma >> 25) & 1) {
			int l1_index;
			int l1_data;
			int l2_index;
			unsigned int l2_data;

			l1_index = (md >> 13) & 03777;
			l1_data = l1_map[l1_index];

			l2_index = (l1_data << 5) | ((md >> 8) & 037);
			l2_data = vma;

			l2_map[l2_index] = l2_data;
			invalidate_vtop_cache();
			tracevm("l2_map[%o] <- %o\n", l2_index, l2_data);
			add_new_page_no(l2_data & 037777);
		}
		break;
	case 030:		// MD register (memory data).
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
		write_mem(vma, md);
		break;
	case 033:		// MD register, write map (like 23).
		md = out_bus;
		tracef("memory-data-write-map md=%o, vma=%o (addr %o)\n", md, vma, md >> 13);
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
	pc_history[index].m[0] = read_m_mem(015);
	pc_history[index].m[1] = read_m_mem(016);
	pc_history[index].m[2] = read_m_mem(022);
	pc_history[index].m[3] = read_m_mem(023);
}

void
show_pc_history(void)
{
	unsigned int pc;

	printf("pc history:\n");
	for (int i = 0; i < MAX_PC_HISTORY; i++) {
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
		printf(" Q %08x M %08x %08x %08x %08x\n",
		       pc_history[pc_history_ptr].rq,
		       pc_history[pc_history_ptr].m[0],
		       pc_history[pc_history_ptr].m[1],
		       pc_history[pc_history_ptr].m[2],
		       pc_history[pc_history_ptr].m[3]);
		printf("\n");

		{
			ucw_t u = ucode[pc];

			disassemble_ucode_loc(pc, u);
		}
		pc_history_ptr++;
		if (pc_history_ptr == MAX_PC_HISTORY)
			pc_history_ptr = 0;
	}
	printf("\n");
}

struct pc_histogram_s {
	unsigned int pc;
	unsigned long long count;
} pc_histogram[16 * 1024];

void
record_pc_histogram(unsigned int pc)
{
	pc_histogram[pc].count++;
}

void
reset_pc_histogram(void)
{
	unsigned int pc;

	for (pc = 0; pc < 16 * 1024; pc++)
		pc_histogram[pc].count = 0;
}

int
pc_histogram_cmp(const void *n1, const void *n2)
{
	struct pc_histogram_s *e1;
	struct pc_histogram_s *e2;

	e1 = (struct pc_histogram_s *) n1;
	e2 = (struct pc_histogram_s *) n2;

	return (int) (e2->count - e1->count);
}

void
show_pc_histogram(void)
{
	unsigned int pc;
	unsigned int perc;
	unsigned long long count;
	unsigned long long total;

	total = 0;

	printf("microcode pc histogram:\n");
	for (int i = 0; i < 16 * 1024; i++) {
		pc_histogram[i].pc = i;
		total += pc_histogram[i].count;
	}

	printf("total %lld %016llx\n", total, total);
	qsort(pc_histogram, 16 * 1024, sizeof(struct pc_histogram_s), pc_histogram_cmp);
	for (int i = 0; i < 16 * 1024; i++) {
		pc = pc_histogram[i].pc;
		count = pc_histogram[i].count;
		if (count) {
			char *sym;
			int offset;

			perc = (unsigned int) ((count * 100ULL) / total);
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
	for (int i = 0; i < 2048; i += 4) {
		int skipped;

		skipped = 0;

		printf("l1[%02o] %011o %011o %011o %011o\n", i, l1_map[i], l1_map[i + 1], l1_map[i + 2], l1_map[i + 3]);
		while (l1_map[i + 0] == l1_map[i + 0 + 4] &&
		       l1_map[i + 1] == l1_map[i + 1 + 4] &&
		       l1_map[i + 2] == l1_map[i + 2 + 4] &&
		       l1_map[i + 3] == l1_map[i + 3 + 4] &&
		       i < 2048) {
			if (skipped++ == 0)
				printf("...\n");
			i += 4;
		}
	}
	printf("\n");
}

void
dump_l2_map()
{
	for (int i = 0; i < 1024; i += 4) {
		int skipped;

		skipped = 0;

		printf("l2[%02o] %011o %011o %011o %011o\n", i, l2_map[i], l2_map[i + 1], l2_map[i + 2], l2_map[i + 3]);
		while (l2_map[i + 0] == l2_map[i + 0 + 4] &&
		       l2_map[i + 1] == l2_map[i + 1 + 4] &&
		       l2_map[i + 2] == l2_map[i + 2 + 4] &&
		       l2_map[i + 3] == l2_map[i + 3 + 4] &&
		       i < 1024) {
			if (skipped++ == 0)
				printf("...\n");
			i += 4;
		}
	}
	printf("\n");
}

void
dump_pdl_memory(void)
{
	printf("pdl-ptr %o, pdl-index %o\n", pdl_ptr, pdl_index);
	for (int i = 0; i < 1024; i += 4) {
		int skipped;

		skipped = 0;

		printf("PDL[%04o] %011o %011o %011o %011o\n", i, pdl_memory[i], pdl_memory[i + 1], pdl_memory[i + 2], pdl_memory[i + 3]);
		while (pdl_memory[i + 0] == pdl_memory[i + 0 + 4] &&
		       pdl_memory[i + 1] == pdl_memory[i + 1 + 4] &&
		       pdl_memory[i + 2] == pdl_memory[i + 2 + 4] &&
		       pdl_memory[i + 3] == pdl_memory[i + 3 + 4] &&
		       i < 1024) {
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
	printf("\n-------------------------------------------------\n");
	printf("CADR machine state:\n\n");
	printf("u-code pc %o, lc %o (%o)\n", u_pc, lc, lc >> 2);
	printf("vma %o, md %o, q %o, opc %o, disp-const %o\n", vma, md, q, opc, dispatch_constant);
	printf("oa-lo %011o, oa-hi %011o, ", oa_reg_lo, oa_reg_hi);
	printf("pdl-ptr %o, pdl-index %o, spc-ptr %o\n", pdl_ptr, pdl_index, spc_stack_ptr);
	printf("\n");
	printf("lc increments %d (macro instructions executed)\n", macro_pc_incrs);
	printf("\n");

	for (int i = 0; i < 32; i += 4) {
		printf(" spc[%02o] %c%011o %c%011o %c%011o %c%011o\n",
		       i,
		       (i + 0 == spc_stack_ptr) ? '*' : ' ', spc_stack[i + 0],
		       (i + 1 == spc_stack_ptr) ? '*' : ' ', spc_stack[i + 1],
		       (i + 2 == spc_stack_ptr) ? '*' : ' ', spc_stack[i + 2],
		       (i + 3 == spc_stack_ptr) ? '*' : ' ', spc_stack[i + 3]);
	}
	printf("\n");

	if (spc_stack_ptr > 0) {
		printf("stack backtrace:\n");
		for (int i = spc_stack_ptr; i >= 0; i--) {
			char *sym;
			int offset, pc;
			pc = spc_stack[i] & 037777;
			sym = sym_find_last(!prom_enabled_flag, pc, &offset);
			printf("%2o %011o %s+%d\n", i, spc_stack[i], sym, offset);
		}
		printf("\n");
	}

	for (int i = 0; i < 32; i += 4) {
		printf("m[%02o] %011o %011o %011o %011o\n", i, read_m_mem(i), read_m_mem(i + 1), read_m_mem(i + 2), read_m_mem(i + 3));
	}
	printf("\n");

	dump_pdl_memory();

	for (int i = 0; i < 01000; i += 4) {
		printf("A[%04o] %011o %011o %011o %011o\n", i, read_a_mem(i), read_a_mem(i + 1), read_a_mem(i + 2), read_a_mem(i + 3));
	}
	printf("\n");

	{
		int s;
		int e;

		s = -1;
		
		for (int i = 0; i < 16 * 1024; i++) {
			if (phy_pages[i] != 0 && s == -1)
				s = i;
			if ((phy_pages[i] == 0 || i == 16 * 1024 - 1) && s != -1) {
				e = i - 1;
				printf("%o-%o\n", s, e);
				s = -1;
			}
		}
	}
	printf("\n");

	printf("A-memory by symbol:\n");
	{
		for (int i = 0; i < 1024; i++) {
			char *sym;

			sym = sym_find_by_type_val(1, 4, i);
			if (sym) {
				printf("%o %-40s %o\n", i, sym, read_a_mem(i));
			}
		}
	}
	printf("\n");

	printf("trace: %s\n", trace ? "on" : "off");
}

#define PAGES_TO_SAVE 8192
int restored;

int
restore_state(void)
{
	int fd;
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

	for (int i = 0; i < PAGES_TO_SAVE; i++) {
		add_new_page_no(i);
		ret = read(fd, (char *) phy_pages[i], sizeof(struct page_s));
		if (ret < 0) {
			close(fd);
			return -1;
		}

#ifdef __BIG_ENDIAN__
		_swaplongbytes((unsigned int *) phy_pages[i], 256);
#endif
	}
	close(fd);
	printf("memory state restored\n");

	return 0;
}

int
save_state(void)
{
	int fd;
	ssize_t ret;
	unsigned char version[2];

	fd = open("usim.state", O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		return -1;

	version[0] = 0;
	version[1] = 1;

	ret = write(fd, version, 2);
	if (ret < 0) {
		close(fd);
		return -1;
	}

	for (int i = 0; i < PAGES_TO_SAVE; i++) {
#ifdef __BIG_ENDIAN__
		_swaplongbytes((unsigned int *) phy_pages[i], 256);
#endif

		ret = write(fd, (char *) phy_pages[i], sizeof(struct page_s));
		if (ret < 0) {
			close(fd);
			return -1;
		}
	}
	close(fd);
	printf("memory state saved\n");

	return 0;
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

	if (breakpoint_name_prom) {
		if (sym_find(0, breakpoint_name_prom, ptrace_pt_prom)) {
			if (isdigit(breakpoint_name_prom[0])) {
				sscanf(breakpoint_name_prom, "%o", ptrace_pt_prom);
			} else {
				fprintf(stderr, "can't find prom breakpoint '%s'\n", breakpoint_name_prom);
				return -1;
			}
		}
		printf("breakpoint [prom]: %s %o\n", breakpoint_name_prom, *ptrace_pt_prom);
		*ptrace_pt_count = 1;
	}

	if (breakpoint_name_mcr) {
		if (sym_find(1, breakpoint_name_mcr, ptrace_pt)) {
			if (isdigit(breakpoint_name_mcr[0])) {
				sscanf(breakpoint_name_mcr, "%o", ptrace_pt);
			} else {
				fprintf(stderr, "can't find mcr breakpoint '%s'\n", breakpoint_name_mcr);
				return -1;
			}
		}
		printf("breakpoint [mcr]: %s %o\n", breakpoint_name_mcr, *ptrace_pt);
		*ptrace_pt_count = 1;
	}

	if (breakpoint_count) {
		*ptrace_pt_count = breakpoint_count;
	}

	if (tracelabel_name_mcr) {
		if (sym_find(1, tracelabel_name_mcr, ptrace_label_pt)) {
			fprintf(stderr, "can't find mcr trace label '%s'\n", tracelabel_name_mcr);
			return -1;
		}
		printf("trace label point [mcr]: %s %o\n", tracelabel_name_mcr, *ptrace_label_pt);
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
				fprintf(stderr, "can't find mcr breakpoint '%s'\n", breakpoint_name_mcr);
				return -1;
			}
		}
		printf("breakpoint [mcr]: %s %o\n", breakpoint_name_mcr, *ptrace_pt);
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

	sym = sym_find_last(!prom_enabled_flag, upc, &offset);
	if (sym) {
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

	sym = sym_find_last(!prom_enabled_flag, upc, &offset);
	if (sym) {
		if (offset == 0)
			printf("%-16s ", sym);
		else
			printf("%-16s+%o", sym, offset);
	}
}

// For 32-bit integers, (A + B) & (1 << 32) will always be
// zero. Without resorting to 64-bit arithmetic, you can find the
// carry by B > ~A.  How does it work? ~A (the complement of A) is the
// largest possible number you can add to A without a carry: A + ~A =
// (1 << 32) - 1.  If B is any larger, then a carry will be generated
// from the top bit.
#define add32(a, b, ci, out, co)					\
	out = (a) + (b) + ((ci) ? 1 : 0);				\
	co = (ci) ? (((b) >= ~(a)) ? 0:1) : (((b) > ~(a)) ? 0:1) ;
#define sub32(a, b, ci, out, co)			\
	out = (a) - (b) - ((ci) ? 0 : 1);		\
	co = (unsigned)(out) < (unsigned)(a) ? 1 : 0;

int
run(void)
{
	int trace_pt_prom;
	int trace_pt;
	int trace_pt_count;
	int trace_label_pt;
	char *sym;
	char *last_sym;
	ucw_t p1;
	int p0_pc;
	int p1_pc;
	char no_exec_next;

	phys_ram_pages = 8192;	// 2 MW.

	u_pc = 0;
	prom_enabled_flag = 1;
	run_ucode_flag = 1;
	
	trace_pt = 0;
	trace_pt_count = 0;
	trace_label_pt = 0;
	
	last_sym = 0;

	p1 = 0;
	p0_pc = 0;
	p1_pc = 0;
	no_exec_next = 0;

	set_breakpoints(&trace_pt_prom, &trace_pt, &trace_pt_count, &trace_label_pt);

	printf("run:\n");

	write_phy_mem(0, 0);
	timing_start();

	while (run_ucode_flag) {
		char op_code;
		char invert_sense;
		char take_jump;
		int a_src;
		int m_src;
		int new_pc;
		int dest;
		int aluop;
		int r_bit;
		int p_bit;
		int n_bit;
		int m_src_value;
		int a_src_value;
		int widthm1;
		int pos;
		int mr_sr_bits;
		unsigned int left_mask;
		unsigned int right_mask;
		unsigned int mask;
		unsigned int old_q;
		int left_mask_index;
		int right_mask_index;
		int disp_const;
		int disp_addr;
		int map;
		int len;
		int rot;
		int carry_in;
		int do_add;
		int do_sub;
		unsigned int out_bus;
		int64_t lv;
		ucw_t u;
		ucw_t w;
#define p0 u
		char n_plus1;
		char enable_ish;
		char i_long;
		char popj;
		
		m_src_value = 0;
		
		if (cycles == 0) {
			p0 = p1 = 0;
			p1_pc = 0;
			no_exec_next = 0;
		}
		
	next:
		iob_poll(cycles);

		disk_poll();

		if ((cycles & 0x0ffff) == 0) {
			display_poll();
			chaos_poll();
		}

		// Fetch next instruction from PROM or RAM.
#define FETCH() (prom_enabled_flag ? prom_ucode[u_pc] : ucode[u_pc])

		// CPU pipeline.
		p0 = p1;
		p0_pc = p1_pc;
		p1 = FETCH();
		p1_pc = u_pc;
		u_pc++;

		if (new_md_delay) {
			new_md_delay--;
			if (new_md_delay == 0)
				md = new_md;
		}

		// Stall pipe for one cycle.
		if (no_exec_next) {
			tracef("no_exec_next; u_pc %o\n", u_pc);
			no_exec_next = 0;

			p0 = p1;
			p0_pc = p1_pc;

			p1 = FETCH();
			p1_pc = u_pc;
			u_pc++;
		}

		// Next instruction modify.
		if (oa_reg_lo_set) {
			tracef("merging oa lo %o\n", oa_reg_lo);
			oa_reg_lo_set = 0;
			u |= oa_reg_lo;
		}

		if (oa_reg_hi_set) {
			tracef("merging oa hi %o\n", oa_reg_hi);
			oa_reg_hi_set = 0;
			u |= (ucw_t) oa_reg_hi << 26;
		}

		// Enforce max. cycles.
		cycles++;
		if (cycles == 0)
			// Handle overflow.
			cycles = 1;

		if (max_cycles && cycles > max_cycles) {
			int offset;
			printf("cycle count exceeded, pc %o\n", u_pc);
			if ((sym = sym_find_last(!prom_enabled_flag, u_pc, &offset))) {
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
			if ((sym = sym_find_by_val(!prom_enabled_flag, p0_pc))) {
				printf("%s:\n", sym);
			}
			printf("%03o %016llo%s", p0_pc, u, i_long ? " (i-long)" : "");
			if (lc != 0) {
				printf(" (lc=%011o %011o)", lc, lc >> 2);
			}
			printf("\n");
			disassemble_ucode_loc(p0_pc, u);
		}

		// Trace label names in microcode.
		if (trace_mcr_labels_flag && !trace) {
			if (!prom_enabled_flag) {
				int offset;
				if ((sym = sym_find_last(1, p0_pc, &offset))) {
					if (offset == 0 && sym != last_sym) {
						printf("%s: (lc=%011o %011o)\n", sym, lc, lc >> 2);
						last_sym = sym;
					}
				}
			}
		}

		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;

		a_src_value = read_a_mem(a_src); // Get A source value.

		// Calculate M source value.
		if (m_src & 040) {
			unsigned int l2_data;
			unsigned int l1_data;

			l1_data = 0;

			switch (m_src & 037) {
			case 0:
				m_src_value = dispatch_constant;
				break;
			case 1:
				m_src_value = (spc_stack_ptr << 24) | (spc_stack[spc_stack_ptr] & 01777777);
				break;
			case 2:
				m_src_value = pdl_ptr & 01777;
				break;
			case 3:
				m_src_value = pdl_index & 01777;
				break;
			case 5:
				tracef("reading pdl[%o] -> %o\n", pdl_index, pdl_memory[pdl_index]);
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
				l2_data = map_vtop(md, (int *) &l1_data, (int *) 0);
				m_src_value = (write_fault_bit << 31) | (access_fault_bit << 30) | ((l1_data & 037) << 24) | (l2_data & 077777777);
				if (trace) {
					printf("l1_data %o, l2_data %o\n", l1_data, l2_data);
					printf("read map[md=%o] -> %o\n", md, m_src_value);
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
				m_src_value = (spc_stack_ptr << 24) | (spc_stack[spc_stack_ptr] & 01777777);
				tracef("reading spc[%o] + ptr -> %o\n", spc_stack_ptr, m_src_value);
				spc_stack_ptr = (spc_stack_ptr - 1) & 037;
				break;
			case 024:
				tracef("reading pdl[%o] -> %o, pop\n", pdl_ptr, pdl_memory[pdl_ptr]);
				m_src_value = pdl_memory[pdl_ptr];
				pdl_ptr = (pdl_ptr - 1) & 01777;
				break;
			case 025:
				tracef("reading pdl[%o] -> %o\n", pdl_ptr, pdl_memory[pdl_ptr]);
				m_src_value = pdl_memory[pdl_ptr];
				break;
			}
		} else {
			m_src_value = read_m_mem(m_src);
		}

		// Decode isntruction.
		switch (op_code = (u >> 43) & 03) {
		case 0:		// ALU

			// NOP short cut.
			if ((u & NOP_MASK) == 0) {
				goto next;
			}

			dest = (u >> 14) & 07777;
			out_bus = (u >> 12) & 3;
			carry_in = (u >> 2) & 1;

			aluop = (u >> 3) & 077;

			if (trace) {
				printf("a=%o (%o), m=%o (%o)\n", a_src, a_src_value, m_src, m_src_value);
				printf("aluop %o, c %o,  dest %o, out_bus %d\n", aluop, carry_in, dest, out_bus);
			}

			alu_carry = 0;

			switch (aluop) {
				// Arithmetic.
			case 020:
				alu_out = carry_in ? 0 : -1;
				alu_carry = 0;
				break;
			case 021:
				lv = (long long) (m_src_value & a_src_value) - (carry_in ? 0 : 1);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 022:
				lv = (long long) (m_src_value & ~a_src_value) - (carry_in ? 0 : 1);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 023:
				lv = (long long) m_src_value - (carry_in ? 0 : 1);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 024:
				lv = (long long) (m_src_value | ~a_src_value) + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 025:
				lv = (long long) (m_src_value | ~a_src_value) + (m_src_value & a_src_value) + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 026: // [M-A-1] [SUB]
				sub32(m_src_value, a_src_value, carry_in, alu_out, alu_carry);
				break;
			case 027:
				lv = (long long) (m_src_value | ~a_src_value) + m_src_value + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 030:
				lv = (long long) (m_src_value | a_src_value) + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 031: // [ADD] [M+A+1]
				add32(m_src_value, a_src_value, carry_in, alu_out, alu_carry);
				break;
			case 032:
				lv = (long long) (m_src_value | a_src_value) + (m_src_value & ~a_src_value) + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 033:
				lv = (long long) (m_src_value | a_src_value) + m_src_value + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 034: // [M+1]
				alu_out = m_src_value + (carry_in ? 1 : 0);
				alu_carry = 0;
				if (m_src_value == 0xffffffff && carry_in)
					alu_carry = 1;
				break;
			case 035:
				lv = (long long) m_src_value + (m_src_value & a_src_value) + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 036:
				lv = (long long) m_src_value + (m_src_value | ~a_src_value) + (carry_in ? 1 : 0);
				alu_out = (unsigned int) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 037: // [M+M] [M+M+1]
				add32(m_src_value, m_src_value, carry_in, alu_out, alu_carry);
				break;

				// Boolean.
			case 000: // [SETZ]
				alu_out = 0;
				break;
			case 001: // [AND]
				alu_out = m_src_value & a_src_value;
				break;
			case 002: // [ANDCA]
				alu_out = m_src_value & ~a_src_value;
				break;
			case 003: // [SETM]
				alu_out = m_src_value;
				break;
			case 004: // [ANDCM]
				alu_out = ~m_src_value & a_src_value;
				break;
			case 005: // [SETA]
				alu_out = a_src_value;
				break;
			case 006: // [XOR]
				alu_out = m_src_value ^ a_src_value;
				break;
			case 007: // [IOR]
				alu_out = m_src_value | a_src_value;
				break;
			case 010: // [ANDCB]
				alu_out = ~a_src_value & ~m_src_value;
				break;
			case 011: // [EQV]
				alu_out = a_src_value == m_src_value;
				break;
			case 012: // [SETCA]
				alu_out = ~a_src_value;
				break;
			case 013: // [ORCA]
				alu_out = m_src_value | ~a_src_value;
				break;
			case 014: // [SETCM]
				alu_out = ~m_src_value;
				break;
			case 015: // [ORCM]
				alu_out = ~m_src_value | a_src_value;
				break;
			case 016: // [ORCB]
				alu_out = ~m_src_value | ~a_src_value;
				break;
			case 017: // [SETO]
				alu_out = ~0;
				break;

				// Conditioanl ALU operation.
			case 040: // Multiply step
				do_add = q & 1;
				if (do_add) {
					add32(a_src_value, m_src_value, carry_in, alu_out, alu_carry);
				} else {
					alu_out = m_src_value;
					alu_carry = alu_out & 0x80000000 ? 1 : 0;
				}
				break;
			case 041: // Divide step
				do_sub = q & 1;
				tracef("do_sub %d\n", do_sub);
				if (do_sub) {
					sub32(m_src_value, a_src_value, !carry_in, alu_out, alu_carry);
				} else {
					add32(m_src_value, a_src_value, carry_in, alu_out, alu_carry);
				}
				break;
			case 045: // Remainder correction
				do_sub = q & 1;
				tracef("do_sub %d\n", do_sub);
				if (do_sub) {
					alu_carry = 0;
				} else {
					add32(alu_out, a_src_value, carry_in, alu_out, alu_carry);
				}
				break;
			case 051: // Initial divide step
				tracef("divide-first-step\n");
				tracef("divide: %o / %o \n", q, a_src_value);
				sub32(m_src_value, a_src_value, !carry_in, alu_out, alu_carry);
				tracef("alu_out %08x %o %d\n", alu_out, alu_out, alu_out);
				break;
			}

			// Q control.
			old_q = q;
			switch (u & 3) {
			case 1:
				tracef("q<<\n");
				q <<= 1;
				// Inverse of ALU sign.
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

			// Output bus control.
			switch (out_bus) {
			case 0:
				printf("out_bus == 0!\n");
				out_bus = rotate_left(m_src_value, u & 037);
				break;
			case 1:
				out_bus = alu_out;
				break;
			case 2:
				// "ALU output shifted right one, with
				// the correct sign shifted in,
				// regardless of overflow."
				out_bus = (alu_out >> 1) | (alu_carry ? 0x80000000 : 0);
				break;
			case 3:
				out_bus = (alu_out << 1) | ((old_q & 0x80000000) ? 1 : 0);
				break;
			}
			write_dest(u, dest, out_bus);
			tracef("alu_out 0x%08x, alu_carry %d, q 0x%08x\n", alu_out, alu_carry, q);
			break;
		case 1:		// JUMP
			new_pc = (u >> 12) & 037777;
			tracef("a=%o (%o), m=%o (%o)\n", a_src, a_src_value, m_src, m_src_value);
			r_bit = (u >> 9) & 1;
			p_bit = (u >> 8) & 1;
			n_bit = (u >> 7) & 1;
			invert_sense = (u >> 6) & 1;
			take_jump = 0;

			if (((u >> 10) & 3) == 1) {
				printf("halted\n");
				run_ucode_flag = 0;
				break;
			}

		process_jump:
			// Jump condition.
			if (u & (1 << 5)) {
				switch (u & 017) {
				case 0:
					if (op_code != 2)
						printf("jump-condition == 0! u_pc=%o\n", p0_pc);
					break;
				case 1:
					take_jump = m_src_value < a_src_value;
					break;
				case 2:
					take_jump = m_src_value <= a_src_value;
					break;
				case 3:
					take_jump = m_src_value == a_src_value;
					break;
				case 4:
					take_jump = page_fault_flag;
					break;
				case 5:
					tracef("jump i|pf\n");
					take_jump = page_fault_flag | (interrupt_enable_flag ? interrupt_pending_flag : 0);
					break;
				case 6:
					tracef("jump i|pf|sb\n");
					take_jump = page_fault_flag | (interrupt_enable_flag ? interrupt_pending_flag : 0) | sequence_break_flag;
					break;
				case 7:
					take_jump = 1;
					break;
				}
			} else {
				rot = u & 037;
				tracef("jump-if-bit; rot %o, before %o ", rot, m_src_value);
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
					push_spc(u_pc - 1);
			}

			// P & R & jump-inst -> write ucode.
			if (p_bit && r_bit && op_code == 1) {
				w = ((ucw_t) (a_src_value & 0177777) << 32) | (unsigned int) m_src_value;
				write_ucode(new_pc, w);
			}
			if (r_bit && take_jump) {
				new_pc = pop_spc();
				if ((new_pc >> 14) & 1) {
					advance_lc(&new_pc);
				}
				new_pc &= 037777;
			}
			if (take_jump) {
				if (n_bit)
					no_exec_next = 1;
				u_pc = new_pc;
				// inhibit possible POPJ.
				popj = 0;
			}
			break;
		case 2:		// DISPATCH.
			disp_const = (u >> 32) & 01777;
			n_plus1 = (u >> 25) & 1;
			enable_ish = (u >> 24) & 1;
			disp_addr = (u >> 12) & 03777;
			map = (u >> 8) & 3;
			len = (u >> 5) & 07;
			pos = u & 037;

			// Misc. function 3.
			if (((u >> 10) & 3) == 3) {
				if (lc_byte_mode_flag) {
					// Byte mode.
					char ir4, ir3, lc1, lc0;

					ir4 = (u >> 4) & 1;
					ir3 = (u >> 3) & 1;
					lc1 = (lc >> 1) & 1;
					lc0 = (lc >> 0) & 1;
					pos = u & 007;
					pos |= ((ir4 ^ (lc1 ^ lc0)) << 4) | ((ir3 ^ lc0) << 3);
					tracef("byte-mode, pos %o\n", pos);
				} else {
					// 16 bit mode.
					char ir4, lc1;

					ir4 = (u >> 4) & 1;
					lc1 = (lc >> 1) & 1;

					pos = u & 017;

					pos |= ((ir4 ^ lc1) ? 0 : 1) << 4;
					tracef("16b-mode, pos %o\n", pos);
				}
			}

			// Misc. function 2.
			if (((u >> 10) & 3) == 2) {
				tracef("dispatch_memory[%o] <- %o\n", disp_addr, a_src_value);
				dispatch_memory[disp_addr] = a_src_value;
				goto dispatch_done;
			}

			tracef("m-src %o, ", m_src_value);
			// Rotate M-SOURCE.
			m_src_value = rotate_left(m_src_value, pos);
			// Generate mask.
			left_mask_index = (len - 1) & 037;

			mask = ~0;
			mask >>= 31 - left_mask_index;

			if (len == 0)
				mask = 0;

			// Put LDB into DISPATCH-ADDR.
			disp_addr |= m_src_value & mask;

			tracef("rotated %o, mask %o, result %o\n", m_src_value, mask, m_src_value & mask);

			// Tweak DISPATCH-ADDR with L2 map bits.
			if (map) {
				int l2_map, bit18, bit19;

				l2_map = map_vtop(md, (int *) 0, (int *) 0);
				bit19 = ((l2_map >> 19) & 1) ? 1 : 0;
				bit18 = ((l2_map >> 18) & 1) ? 1 : 0;
				tracef("md %o, l2_map %o, b19 %o, b18 %o\n", md, l2_map, bit19, bit18);
				switch (map) {
				case 1:
					disp_addr |= bit18;
					break;
				case 2:
					disp_addr |= bit19;
					break;
				case 3:
					disp_addr |= bit18 | bit19;
					break;
				}
			}
			disp_addr &= 03777;

			tracef("dispatch[%o] -> %o ", disp_addr, dispatch_memory[disp_addr]);

			disp_addr = dispatch_memory[disp_addr];
			dispatch_constant = disp_const;

			new_pc = disp_addr & 037777; // 14 bits.

			n_bit = (disp_addr >> 14) & 1;
			p_bit = (disp_addr >> 15) & 1;
			r_bit = (disp_addr >> 16) & 1;

			tracef("%s%s%s\n", n_bit ? "N " : "", p_bit ? "P " : "", r_bit ? "R " : "");

			if (n_plus1 && n_bit) {
				u_pc--;
			}

			invert_sense = 0;
			take_jump = 1;
			u = 1 << 5;

			// Enable instruction sequence hardware.
			if (enable_ish) {
				advance_lc((int *) 0);
			}

			// Fall through on dispatch.
			if (p_bit && r_bit) {
				if (n_bit)
					no_exec_next = 1;
				goto dispatch_done;
			}
			goto process_jump;
		dispatch_done:
			break;
		case 3:		// BYTE.
			dest = (u >> 14) & 07777;
			mr_sr_bits = (u >> 12) & 3;
			tracef("a=%o (%o), m=%o (%o), dest=%o\n", a_src, a_src_value, m_src, m_src_value, dest);
			widthm1 = (u >> 5) & 037;
			pos = u & 037;

			// Misc. function 3.
			if (((u >> 10) & 3) == 3) {
				if (lc_byte_mode_flag) {
					// Byte mode.
					char ir4, ir3, lc1, lc0;

					ir4 = (u >> 4) & 1;
					ir3 = (u >> 3) & 1;
					lc1 = (lc >> 1) & 1;
					lc0 = (lc >> 0) & 1;

					pos = u & 007;
					pos |= ((ir4 ^ (lc1 ^ lc0)) << 4) | ((ir3 ^ lc0) << 3);
					tracef("byte-mode, pos %o\n", pos);
				} else {
					// 16-bit mode.
					char ir4, lc1;

					ir4 = (u >> 4) & 1;
					lc1 = (lc >> 1) & 1;

					pos = u & 017;
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

			tracef("widthm1 %o, pos %o, mr_sr_bits %o\n", widthm1, pos, mr_sr_bits);
			tracef("left_mask_index %o, right_mask_index %o\n", left_mask_index, right_mask_index);
			tracef("left_mask %o, right_mask %o, mask %o\n", left_mask, right_mask, mask);

			out_bus = 0;

			switch (mr_sr_bits) {
			case 0:
				printf("mr_sr_bits == 0!\n");
				break;
			case 1: // LDB.
				tracef("ldb; m %o\n", m_src_value);
				m_src_value = rotate_left(m_src_value, pos);
				out_bus = (m_src_value & mask) | (a_src_value & ~mask);
				tracef("ldb; m-rot %o, mask %o, result %o\n", m_src_value, mask, out_bus);
				break;
			case 2: // Selective deposit.
				out_bus = (m_src_value & mask) | (a_src_value & ~mask);
				tracef("sel-dep; a %o, m %o, mask %o -> %o\n", a_src_value, m_src_value, mask, out_bus);
				break;
			case 3: // DPB.
				tracef("dpb; m %o, pos %o\n", m_src_value, pos);
				// Mask is already rotated.
				m_src_value = rotate_left(m_src_value, pos);
				out_bus = (m_src_value & mask) | (a_src_value & ~mask);
				tracef("dpb; mask %o, result %o\n", mask, out_bus);
				break;
			}

			write_dest(u, dest, out_bus);
			break;
		}

		if (popj) {
			tracef("popj; ");
			u_pc = pop_spc();
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

	return 0;
}

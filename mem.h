#ifndef USIM_MEM_H
#define USIM_MEM_H

struct page_s {
	unsigned int w[256];
};

extern struct page_s *phy_pages[16 * 1024];

extern int phys_ram_pages;

extern void invalidate_vtop_cache(void);

extern unsigned int map_vtop(unsigned int virt, int *pl1_map, int *poffset);

extern int write_phy_mem(int paddr, unsigned int v);
extern int add_new_page_no(int pn);
extern int read_phy_mem(int paddr, unsigned int *pv);

extern int restore_state(void);
extern int save_state(void);

extern int l1_map[2048];
extern int l2_map[1024];

extern unsigned int last_virt;
extern unsigned int last_l1;
extern unsigned int last_l2;

#endif

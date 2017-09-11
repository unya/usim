#include <stdint.h>

#define NOP_MASK 03777777777767777LL

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef uint64_t ucw_t;

extern int trace;

#define tracef(...)
#define traceio(...)
#define tracedio(...)
#define tracenet(...)
#define traceint(...)
#define tracevm(...)

extern int run_ucode_flag;
extern int warm_boot_flag;
extern int stop_after_prom_flag;

extern unsigned long cycles;

extern char *sym_find_by_val(int mcr, int v);
extern char *sym_find_last(int mcr, int v, int *poffset);
extern char *sym_find_by_type_val(int mcr, int t, int v);

extern int read_phy_mem(int paddr, unsigned int *pv);
extern int write_phy_mem(int paddr, unsigned int v);

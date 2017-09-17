#ifndef USIM_UCODE_H
#define USIM_UCODE_H

#include <stdint.h>

#define NOP_MASK 03777777777767777LL

typedef uint64_t ucw_t;

extern int run(void);

extern void write_a_mem(int loc, unsigned int v);
extern unsigned int read_a_mem(int loc);

extern void assert_unibus_interrupt(int vector);
extern void deassert_unibus_interrupt(void);

extern void assert_xbus_interrupt(void);
extern void deassert_xbus_interrupt(void);

#endif

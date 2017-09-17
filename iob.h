#ifndef USIM_IOB_H
#define USIM_IOB_H

extern unsigned int iob_csr;

extern int iob_init(void);
extern void iob_poll(int cycles);

extern void iob_unibus_read(int offset, int *pv);
extern void iob_unibus_write(int offset, int v);

#endif

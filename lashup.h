#ifndef USIM_LASHUP_H
#define USIM_LASHUP_H

extern void lashup_init(char *port);
extern void lashup_unibus_read(int offset, unsigned int *pv);
extern void lashup_unibus_write(int offset, unsigned int v);

#endif

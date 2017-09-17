#ifndef USIM_TV_H
#define USIM_TV_H

extern unsigned int tv_bitmap[(768 * 1024)];
extern unsigned int tv_width;
extern unsigned int tv_height;

extern void tv_init(void);
extern void tv_poll(void);
extern void tv_write(int offset, unsigned int bits);
extern void tv_read(int offset, unsigned int *pv);

extern void tv_xbus_read(int offset, unsigned int *pv);
extern void tv_xbus_write(int offset, unsigned int v);

#endif

#ifndef USIM_MOUSE_H
#define USIM_MOUSE_H

extern int mouse_x;
extern int mouse_y;
extern int mouse_head;
extern int mouse_middle;
extern int mouse_tail;
extern int mouse_rawx;
extern int mouse_rawy;
extern int mouse_poll_delay;

extern void iob_mouse_event(int x, int y, int buttons);
extern void mouse_sync_init(void);

#endif

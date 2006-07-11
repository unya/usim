/*
 * usim.h
 * $Id$
 */

#if defined(linux)
#define LINUX
#endif

#if defined(__APPLE__)
#define OSX
#endif

void assert_unibus_interrupt(int vector);
void assert_xbus_interrupt(void);
void deassert_xbus_interrupt(void);

void sdl_set_bow_mode(char new_mode);
void sdl_queue_all_keys_up(void);

void iob_sdl_mouse_event(int x, int y, int dx, int dy, int buttons);
void iob_dequeue_key_event(void);
void kbd_init(void);

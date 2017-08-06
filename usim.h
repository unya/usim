void assert_unibus_interrupt(int vector);
void assert_xbus_interrupt(void);
void deassert_xbus_interrupt(void);

void set_bow_mode(char new_mode);
void queue_all_keys_up(void);

void iob_mouse_event(int x, int y, int dx, int dy, int buttons);
void iob_dequeue_key_event(void);
void kbd_init(void);

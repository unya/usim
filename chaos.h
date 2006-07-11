/*
 * chaos.h
 */

int chaos_get_csr(void);
int chaos_get_addr(void);
int chaos_get_rcv_buffer(void);
int chaos_get_bit_count(void);

int chaos_set_csr(int v);
int chaos_put_xmit_buffer(int v);


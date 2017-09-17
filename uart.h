#ifndef USIM_UART_H
#define USIM_UART_H

extern void uart_xbus_read(int offset, unsigned int *pv);
extern void uart_xbus_write(int offset, unsigned int v);

#endif

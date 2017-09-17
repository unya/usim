#include <stdio.h>

void
uart_xbus_read(int offset, unsigned int *pv)
{
	*pv = 0;
}

void
uart_xbus_write(int offset, unsigned int v)
{
	if (offset == 0) {
		putc(v, stdout);
		fflush(stdout);
	}
}

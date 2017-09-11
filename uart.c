#include "usim.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

extern int trace;

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

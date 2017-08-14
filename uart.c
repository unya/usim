#include "usim.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

extern int trace;

int
uart_xbus_read(int offset, unsigned int *pv)
{
	*pv = 0;
	return 0;
}

int
uart_xbus_write(int offset, unsigned int v)
{
	if (offset == 0) {
		putc(v, stdout);
		fflush(stdout);
	} else if (offset == 1) {
		trace = v;
	}
	return 0;
}

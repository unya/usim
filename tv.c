#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#include "usim.h"
#include "ucode.h"

#include "x11.h"

#define Black 0x000000
#define White 0xffffff

unsigned int tv_width = 768;
unsigned int tv_height = 897;

unsigned int tv_bitmap[(768 * 1024)];

int tv_csr;

void
tv_xbus_read(int offset, unsigned int *pv)
{
	*pv = tv_csr;
}

void
tv_xbus_write(int offset, unsigned int v)
{
	tv_csr = v;
	tv_csr &= ~(1 << 4);
	deassert_xbus_interrupt();
}

void
tv_post_60hz_interrupt(void)
{
	tv_csr |= 1 << 4;
	assert_xbus_interrupt();
}

void
sigalrm_handler(int arg)
{
	tv_post_60hz_interrupt();
}

void
tv_write(int offset, unsigned int bits)
{
	int i;
	int h;
	int v;

	offset *= 32;

	v = offset / tv_width;
	h = offset % tv_width;

	for (i = 0; i < 32; i++) {
		tv_bitmap[offset + i] = (bits & 1) ? Black : White;
		bits >>= 1;
	}

	accumulate_update(h, v, 32, 1);
}

void
tv_poll(void)
{
	x11_event();
}

void
tv_read(int offset, unsigned int *pv)
{
	unsigned long bits;
	int i;

	offset *= 32;

	if (offset > tv_width * tv_height) {
		printf("tv: tv_read past end; " "offset %o\n", offset);
		*pv = 0;
		return;
	}

	bits = 0;
	for (i = 0; i < 32; i++) {
		if (tv_bitmap[offset + i] == Black)
			bits |= 1 << i;
	}
	*pv = bits;
}

void
tv_init(void)
{
	x11_init();

	{
		struct itimerval itimer;
		int usecs;
		signal(SIGVTALRM, sigalrm_handler);
		usecs = 16000;

		itimer.it_interval.tv_sec = 0;
		itimer.it_interval.tv_usec = usecs;
		itimer.it_value.tv_sec = 0;
		itimer.it_value.tv_usec = usecs;
		setitimer(ITIMER_VIRTUAL, &itimer, 0);
	}
}

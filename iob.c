// iob.c --- CADR I/O board
//
// The IOB board consits of the following devices:
//
// - General-Purpose I/O
// - Clocks
// - Command/Status register (CSR)
// - Keyboard
// - Mouse
// - Chaosnet
//
// See SYS:DOC;IOB TEXT for details.

// ---!!! Order this into proper sections: IOB, Mouse, TV, ...

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

#include "usim.h"
#include "ucode.h"
#include "kbd.h"
#include "mouse.h"
#include "chaos.h"

unsigned int iob_csr;

void tv_post_60hz_interrupt(void);

unsigned long
get_us_clock(void)
{
	unsigned long v;
	static struct timeval tv;
	struct timeval tv2;
	unsigned long ds;
	unsigned long du;

	if (tv.tv_sec == 0) {
		gettimeofday(&tv, 0);
		v = 0;
	} else {
		gettimeofday(&tv2, 0);
		if (tv2.tv_usec < tv.tv_usec) {
			tv2.tv_sec--;
			tv2.tv_usec += 1000 * 1000;
		}
		ds = tv2.tv_sec - tv.tv_sec;
		du = tv2.tv_usec - tv.tv_usec;
		v = (ds * 1000 * 1000) + du;
	}

	return v;
}

static unsigned long cv;

unsigned int
get_us_clock_low(void)
{
	cv = get_us_clock();
	return cv & 0xffff;
}

unsigned int
get_us_clock_high(void)
{
	return (unsigned int) (cv >> 16);
}

unsigned int
get_60hz_clock(void)
{
	return 0;
}

void
iob_unibus_read(int offset, int *pv)
{
	*pv = 0;		// For now default to zero.

	switch (offset) {
	case 0100:
		*pv = kbd_key_scan & 0177777;
		traceio("unibus: kbd low %011o\n", *pv);
		iob_csr &= ~(1 << 5);	// Clear CSR<5>.
		break;
	case 0102:
		*pv = (kbd_key_scan >> 16) & 0177777;
		traceio("unibus: kbd high %011o\n", *pv);
		iob_csr &= ~(1 << 5);	// Clear CSR<5>.
		break;
	case 0104:
		*pv = (mouse_tail << 12) | (mouse_middle << 13) | (mouse_head << 14) | (mouse_y & 07777);
		traceio("unibus: mouse y %011o\n", *pv);

		mouse_tail = 0;
		mouse_middle = 0;
		mouse_head = 0;

		iob_csr &= ~(1 << 4);	// Clear CSR<4>.
		break;
	case 0106:
		*pv = (mouse_rawx << 12) | (mouse_rawy << 14) | (mouse_x & 07777);
		traceio("unibus: mouse x %011o\n", *pv);
		break;
	case 0110:
		traceio("unibus: beep\n");
		fprintf(stderr, "\a");	// Beep!
		break;
	case 0112:
		*pv = iob_csr;
		traceio("unibus: kbd csr %011o\n", *pv);
		break;
	case 0120:
		*pv = get_us_clock_low();
		traceio("unibus: usec clock low\n");
		break;
	case 0122:
		*pv = get_us_clock_high();
		traceio("unibus: usec clock high\n");
		break;
	case 0124:
		*pv = get_60hz_clock();
		traceio("unibus: 60hz clock\n");
		break;
	case 0140:
		*pv = chaos_get_csr();
		break;
	case 0142:
		*pv = chaos_get_addr();
		tracenet("unibus: chaos read my-number\n");
		break;
	case 0144:
		*pv = chaos_get_rcv_buffer();
		tracenet("unibus: chaos read rcv buffer %06o\n", *pv);
		break;
	case 0146:
		*pv = chaos_get_bit_count();
		tracenet("unibus: chaos read bit-count 0%o\n", *pv);
		break;
	case 0152:
		*pv = chaos_get_addr();
		tracenet("unibus: chaos read xmt => %o\n", *pv);
		chaos_xmit_pkt();
		break;
	default:
		if (offset > 0140 && offset <= 0153)
			printf("unibus: chaos read other %o\n", offset);
		chaos_xmit_pkt();
		break;
	}
}

void
iob_unibus_write(int offset, int v)
{
	switch (offset) {
	case 0100:
		traceio("unibus: kbd low\n");
		break;
	case 0102:
		traceio("unibus: kbd high\n");
		break;
	case 0104:
		traceio("unibus: mouse y\n");
		break;
	case 0106:
		traceio("unibus: mouse x\n");
		break;
	case 0110:
		traceio("unibus: beep\n");
		break;
	case 0112:
		traceio("unibus: kbd csr\n");
		iob_csr = (iob_csr & ~017) | (v & 017);
		break;
	case 0120:
		traceio("unibus: usec clock\n");
		break;
	case 0122:
		traceio("unibus: usec clock\n");
		break;
	case 0124:
		printf("unibus: START 60hz clock\n");
		break;
	case 0140:
		traceio("unibus: chaos write %011o\n", v);
		chaos_set_csr(v);
		break;
	case 0142:
		traceio("unibus: chaos write-buffer write %011o, u_pc %011o\n", v, get_u_pc());
		chaos_put_xmit_buffer(v);
		break;
	default:
		if (offset > 0140 && offset <= 0152)
			printf("unibus: chaos write other\n");
		break;
	}
}

void
iob_poll(void)
{
}

void
iob_init(void)
{
	kbd_init();
	mouse_sync_init();
}

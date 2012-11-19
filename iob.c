/*
 * iob.c
 *
 * simple CADR i/o board simulation
 * support for mouse, keyboard, clock
 *
 * $Id$
 */

#include "usim.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#if defined(LINUX) || defined(OSX) || defined(BSD)
#include <sys/time.h>
#endif

#ifdef DISPLAY_SDL
#ifdef _WIN32
#include <SDL/SDL_keysym.h>
#else
#include "SDL/SDL_keysym.h"
#endif
#endif /* DISPLAY_SDL */

#include "ucode.h"
#include "chaos.h"

unsigned int iob_key_scan;
unsigned int iob_kbd_csr;

int mouse_x, mouse_y;
int mouse_head, mouse_middle, mouse_tail;
int mouse_rawx, mouse_rawy;
int mouse_poll_delay;

/* location in A memory of micrcode mouse state */
static int mouse_sync_amem_x;
static int mouse_sync_amem_y;

extern int mouse_sync_flag;
extern int get_u_pc();

void tv_post_60hz_interrupt(void);
void chaos_xmit_pkt(void);

/*
 CADR i/o board

 interrupt vectors:
 260 kdb/mouse
 264 serial
 270 chaos int
 274 clock
 400 ether xmit done
 404 ether rcv done
 410 ether collision

764100
0	0 read kbd
2	1 read kbd
4	2 read mouse y (12 bits)
6	3 read mouse x (12 bits)
10	4 click audio
12	5 kbd/mouse csr

csr - write
0 remote mouse enable
1 mouse int enable
2 kbd int enable
3 clock int enable

csr - read
0 remote mouse eable
1 mouse int enable
2 kbd int enable
3 clock int enable
4 mouse ready
5 kbd ready
6 clock ready
7 ser int enable

keyboard
; 		5-0	keycode
; 		7-6	shift
; 		9-8	top
; 		11-10	control
; 		13-12	meta
; 		14	shift-lock
; 		15	unused

*/

/*
764100
0 read kbd
1 read kbd
2 read mouse y (12 bits)
3 read mouse x (12 bits)
4 click audio
5 kbd/mouse csr

csr - write
0 remote mouse enable
1 mouse int enable
2 kbd int enable
3 clock int enable

csr - read
0 remote mouse eable
1 mouse int enable
2 kbd int enable
3 clock int enable
4 mouse ready
5 kbd ready
6 clock ready
7 ser int enable
*/

#define US_CLOCK_IS_WALL_CLOCK
#if defined(linux) || defined(osx)
#define USE_SIGVTARLM_FOR_60HZ
#endif

#ifdef _WIN32
#define USE_US_CLOCK_FOR_60HZ
#endif

unsigned long
get_us_clock()
{
	unsigned long v;
#ifdef US_CLOCK_IS_WALL_CLOCK
	static unsigned long last_hz60;
	static struct timeval tv;
	struct timeval tv2;
	unsigned long ds, du;

	if (tv.tv_sec == 0) {
		gettimeofday(&tv, 0);
		v = 0;
		last_hz60 = 0;
	} else {
		gettimeofday(&tv2, 0);

		if (tv2.tv_usec < tv.tv_usec) {
			tv2.tv_sec--;
			tv2.tv_usec += 1000*1000;
		}
		ds = tv2.tv_sec - tv.tv_sec;
		du = tv2.tv_usec - tv.tv_usec;

//		v = (ds * 100) + (du / 10000);
		v = (ds * 1000*1000) + du;
		if (0) printf("delta %lu\n", v);

#ifdef USE_US_CLOCK_FOR_60HZ
		hz60 = v / 16000;
		if (hz60 > last_hz60) {
			last_hz60 = hz60;
			tv_post_60hz_interrupt();
		}
#endif
	}
#else
	/* assume 200ns cycle, we want 1us */
	extern long cycles;
	v = cycles * (1000/200);
#endif

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
	return cv >> 16;
}

unsigned int get_60hz_clock(void)
{
	return 0;
}


void
iob_unibus_read(int offset, int *pv)
{
	/* default, for now */
	*pv = 0;

	switch (offset) {
	case 0100:
		*pv = iob_key_scan & 0177777;
		traceio("unibus: kbd low %011o\n", *pv);
		iob_kbd_csr &= ~(1 << 5);
		break;
	case 0102:
		*pv = (iob_key_scan >> 16) & 0177777;
		iob_kbd_csr &= ~(1 << 5);
		traceio("unibus: kbd high %011o\n", *pv);
		break;
	case 0104:
		traceio("unibus: mouse y\n");
		 *pv = (mouse_tail << 12) |
			 (mouse_middle << 13) |
			 (mouse_head << 14) |
			 (mouse_y & 07777); 

		 mouse_tail = 0;
		 mouse_middle = 0;
		 mouse_head = 0;

		 iob_kbd_csr &= ~(1 << 4);
		break;
	case 0106:
		traceio("unibus: mouse x\n");
		 *pv = (mouse_rawx << 12) | (mouse_rawy << 14) |
			 (mouse_x & 07777); 
		break;
	case 0110:
		traceio("unibus: beep\n");
		fprintf(stderr,"\a"); /* alert - beep */
		break;
	case 0112:
		*pv = iob_kbd_csr;
		traceio("unibus: kbd csr %011o\n", *pv);
		break;
	case 0120:
		traceio("unibus: usec clock low\n");
		*pv = get_us_clock_low();
		break;
	case 0122:
		traceio("unibus: usec clock high\n");
		*pv = get_us_clock_high();
		break;
	case 0124:
		printf("unibus: 60hz clock\n");
		*pv = get_60hz_clock();
		break;
	case 0140:
		//tracenet("unibus: chaos read\n");
		*pv = chaos_get_csr();
		break;
	case 0142:
		tracenet("unibus: chaos read my-number\n");
		*pv = chaos_get_addr();
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
			printf/*traceio*/("unibus: chaos read other %o\n",
					  offset);
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
		iob_kbd_csr = 
			(iob_kbd_csr & ~017) | (v & 017);
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
		traceio("unibus: chaos write %011o, u_pc %011o ",
				  v, get_u_pc());
#ifdef CHAOS_DEBUG
		show_label_closest(get_u_pc());
		printf("\n");
#endif
		chaos_set_csr(v);
		break;
	case 0142:
		traceio("unibus: chaos write-buffer write %011o, u_pc %011o\n",
			v, get_u_pc());
		chaos_put_xmit_buffer(v);
		break;
	default:
		if (offset > 0140 && offset <= 0152)
			printf/*traceio*/("unibus: chaos write other\n");
		break;
	}
}

void
iob_sdl_mouse_event(int x, int y, int dx, int dy, int buttons)
{
	iob_kbd_csr |= 1 << 4;
	assert_unibus_interrupt(0264);

#if 0
	printf("iob_sdl_mouse_event(dx=%x,dy=%x,buttons=%x) x %o, y %o\n",
	       dx, dy, buttons, mouse_x, mouse_y);
	mouse_x += dx;
	mouse_y += dy;
#endif

	if (0)
		printf("iob_sdl_mouse_event(x=%x,y=%x,buttons=%x)\n",
		       x, y, buttons);

	if (mouse_sync_flag) {
		int mcx, mcy, dx, dy;

		/* move mouse closer to where microcode thinks it is */
		mcx = read_a_mem(mouse_sync_amem_x);
		mcy = read_a_mem(mouse_sync_amem_y);

		dx = x - mcx;
		dy = y - mcy;

		//printf("m %d,%d mc %d,%d  c %d,%d  d %d,%d\n",
		//mouse_x, mouse_y, mcx, mcy, x, y, dx, dy);

		mouse_x += dx;
		mouse_y += dy;
	} else {
		/* convert SDL coods in to mouse loc */
		mouse_x = (x*4)/3;
		mouse_y = (y*5)/3;
	}

	if (buttons & 4)
		mouse_head = 1;
	if (buttons & 2)
		mouse_middle = 1;
	if (buttons & 1)
		mouse_tail = 1;
}

/*
 * create simulated mouse motion to keep SDL cursor
 * and microcode cursor in sync
 */
void
iob_sdl_mouse_poll(int x, int y)
{
	int mcx, mcy, dx, dy;

	if (iob_kbd_csr & (1 << 4))
		return;

	mcx = read_a_mem(mouse_sync_amem_x);
	mcy = read_a_mem(mouse_sync_amem_y);

	if (mcx == 0 && mcy == 0)
		return;

	dx = x - mcx;
	dy = y - mcy;

#define MAX_MOTION 5
#define POLL_DELAY 20

	if (dx || dy) {

		if (mouse_poll_delay) {
			mouse_poll_delay--;
			if (mouse_poll_delay > 0)
				return;
		}

		if (dx > MAX_MOTION)
			dx = MAX_MOTION;
		if (dy > MAX_MOTION)
			dy = MAX_MOTION;

		mouse_x += dx;
		mouse_y += dy;

		//printf("P: m %d,%d mc %d,%d  c %d,%d  d %d,%d\n",
		//mouse_x, mouse_y, mcx, mcy, x, y, dx, dy);

		iob_kbd_csr |= 1 << 4;
		assert_unibus_interrupt(0264);

		mouse_poll_delay = POLL_DELAY;
	}
}


int tv_csr;

int
tv_xbus_read(int offset, unsigned int *pv)
{
	if (0) printf("tv register read, offset %o -> %o\n", offset, tv_csr);
	*pv = tv_csr;
	return 0;
}

int
tv_xbus_write(int offset, unsigned int v)
{
	if (0) printf("tv register write, offset %o, v %o\n", offset, v);
	if ((tv_csr & 4) != (v & 4)) {
#ifdef DISPLAY_SDL
		sdl_set_bow_mode((v & 4)>>2);
#endif
	}
	tv_csr = v;
	tv_csr &= ~(1 << 4);
	deassert_xbus_interrupt();
	return 0;
}

//xxx tv interrupt
// tv csr @ base, 1<<4 = interrupt flag
// writing back clears int
// 60hz

void
tv_post_60hz_interrupt(void)
{
	tv_csr |= 1 << 4;
	assert_xbus_interrupt();
}

void
iob_sdl_clock_event()
{
	iob_kbd_csr |= 1 << 6;
	assert_unibus_interrupt(0274);
}

void
sigalrm_handler(int arg)
{
	if (0) printf("sigalrm_handler()\n");
	tv_post_60hz_interrupt();
}

void
iob_poll(unsigned long cycles)
{
#ifndef USE_SIGVTARLM_FOR_60HZ
	/* assume 200ns cycle, we want 16ms */
	if ((cycles % ((16*1000*1000)/200)) == 0) {
		tv_post_60hz_interrupt();
	}
#endif
}

void
mouse_sync_init(void)
{
	int val;

	//A-MOUSE-CURSOR-X A-MEM 516
	//A-MOUSE-CURSOR-Y A-MEM 517 

	mouse_sync_amem_x = 334;
	mouse_sync_amem_y = 335;

	if (sym_find(1, "A-MOUSE-CURSOR-X", &val)) {
		printf("can't find A-MOUSE-CURSOR-X in microcode symbols\n");
	} else
		mouse_sync_amem_x = val;

	if (sym_find(1, "A-MOUSE-CURSOR-Y", &val)) {
		printf("can't find A-MOUSE-CURSOR-Y in microcode symbols\n");
	} else
		mouse_sync_amem_y = val;

	if (0)
		printf("mouse_sync_amem_x %o, mouse_sync_amem_y %o\n",
		       mouse_sync_amem_x, mouse_sync_amem_y);
}

int
iob_init(void)
{
	kbd_init();

	if (mouse_sync_flag) {
		mouse_sync_init();
	}

#ifdef USE_SIGVTARLM_FOR_60HZ
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
#endif

	return 0;
}


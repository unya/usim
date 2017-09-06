// kbd.c --- keyboard handling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "usim.h"
#include "ucode.h"
#include "kbd.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>

extern unsigned int iob_key_scan;
extern unsigned int iob_kbd_csr;

unsigned short kb_to_scancode[256][4];

#define IOB_KEY_QUEUE_LEN 10

static int iob_key_queue[IOB_KEY_QUEUE_LEN];
static int iob_key_queue_optr = 0;
static int iob_key_queue_iptr = 0;
static int iob_key_queue_free = IOB_KEY_QUEUE_LEN;

void
iob_queue_key_event(int ev)
{
	int v;

	v = (1 << 16) | ev;

	if (iob_key_queue_free > 0) {
		traceio("iob_queue_key_event() - queuing 0%o, q len before %d\n", v, IOB_KEY_QUEUE_LEN - iob_key_queue_free);
		iob_key_queue_free--;
		iob_key_queue[iob_key_queue_optr] = v;
		iob_key_queue_optr = (iob_key_queue_optr + 1) % IOB_KEY_QUEUE_LEN;
	} else {
		fprintf(stderr, "IOB key queue full!\n");
		if (!(iob_kbd_csr & (1 << 5)) && (iob_kbd_csr & (1 << 2))) {
			iob_kbd_csr |= 1 << 5;
			fprintf(stderr, "iob_queue_key_event generating interrupt\n");
			assert_unibus_interrupt(0260);
		}
	}
}

void
iob_dequeue_key_event()
{
	if (iob_kbd_csr & (1 << 5)) // Already something to be read.
		return;

	if (iob_key_queue_free < IOB_KEY_QUEUE_LEN) {
		int v = iob_key_queue[iob_key_queue_iptr];
		traceio("iob_dequeue_key_event() - dequeuing 0%o, q len before %d\n", v, IOB_KEY_QUEUE_LEN - iob_key_queue_free);
		iob_key_queue_iptr = (iob_key_queue_iptr + 1) % IOB_KEY_QUEUE_LEN;
		iob_key_queue_free++;
		iob_key_scan = (1 << 16) | v;
		if (iob_kbd_csr & (1 << 2)) { // Keyboard interrupt enabled?
			iob_kbd_csr |= 1 << 5;
			fprintf(stderr, "iob_dequeue_key_event generating interrupt (q len after %d)\n", IOB_KEY_QUEUE_LEN - iob_key_queue_free);
			assert_unibus_interrupt(0260);
		}
	}
}

void
iob_key_event(int code, int keydown)
{
	int v;

	traceio("iob_key_event(code=%x, keydown=%x)\n", code, keydown);

	v = ((!keydown) << 8) | code;

	if (iob_kbd_csr & (1 << 5))
		iob_queue_key_event(v); // Already something there, queue this.
	else {
		iob_key_scan = (1 << 16) | v;
		traceio("iob_key_event() - 0%o\n", iob_key_scan);
		if (iob_kbd_csr & (1 << 2)) {
			iob_kbd_csr |= 1 << 5;
			assert_unibus_interrupt(0260);
		}
	}
}

void
iob_warm_boot_key(void)
{
	// Send a Return to get the machine booted.
	iob_key_event(50, 0);
}

void
old_kbd_init(void)
{
	// ---!!! Handle multiple modifiers!
	memset((char *) kb_to_scancode, 0, sizeof(kb_to_scancode));

	// Walk unshifted old keyboard table.
	for (int i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_to_scancode[(int) k][0] = i;
	}

	// Modify mapping to match present-day US keyboard layout.
	kb_to_scancode['`'][0] = 015 | (3 << 6);
	kb_to_scancode['`'][1] = 016 | (3 << 6);

	kb_to_scancode['\''][0] = 010 | (3 << 6);
	kb_to_scancode['\''][1] = 3 | (3 << 6);
	kb_to_scancode['='][0] = 014 | (3 << 6);
	kb_to_scancode['2'][1] = 015;

	kb_to_scancode['6'][1] = 016;
	kb_to_scancode['7'][1] = 7 | (3 << 6);
	kb_to_scancode['8'][1] = 061 | (3 << 6);
	kb_to_scancode['9'][1] = 011 | (3 << 6);
	kb_to_scancode['0'][1] = 012 | (3 << 6);
	kb_to_scancode['-'][1] = 013 | (3 << 6);
	kb_to_scancode['='][1] = 060 | (3 << 6);

	kb_to_scancode[';'][1] = 061;
	kb_to_scancode[':'][1] = 061;
	kb_to_scancode['!'][1] = 2 | (3 << 6);
	kb_to_scancode['"'][1] = 3 | (3 << 6);
	kb_to_scancode['#'][1] = 4 | (3 << 6);
	kb_to_scancode['$'][1] = 5 | (3 << 6);
	kb_to_scancode['%'][1] = 6 | (3 << 6);
	kb_to_scancode['&'][1] = 7 | (3 << 6);
	kb_to_scancode['('][1] = 011 | (3 << 6);
	kb_to_scancode[')'][1] = 012 | (3 << 6);
	kb_to_scancode['_'][1] = 013 | (3 << 6);
	kb_to_scancode['~'][1] = 016 | (3 << 6);
	kb_to_scancode['@'][1] = 13;
	kb_to_scancode['^'][1] = 14;

	kb_to_scancode['Q'][1] = 20 | (3 << 6);
	kb_to_scancode['W'][1] = 21 | (3 << 6);
	kb_to_scancode['E'][1] = 22 | (3 << 6);
	kb_to_scancode['R'][1] = 23 | (3 << 6);
	kb_to_scancode['T'][1] = 24 | (3 << 6);
	kb_to_scancode['Y'][1] = 25 | (3 << 6);
	kb_to_scancode['U'][1] = 26 | (3 << 6);
	kb_to_scancode['I'][1] = 27 | (3 << 6);
	kb_to_scancode['O'][1] = 28 | (3 << 6);
	kb_to_scancode['P'][1] = 29 | (3 << 6);
	kb_to_scancode['{'][1] = 30 | (3 << 6);
	kb_to_scancode['}'][1] = 31 | (3 << 6);
	kb_to_scancode['|'][1] = 32 | (3 << 6);

	kb_to_scancode['A'][1] = 39 | (3 << 6);
	kb_to_scancode['S'][1] = 40 | (3 << 6);
	kb_to_scancode['D'][1] = 41 | (3 << 6);
	kb_to_scancode['F'][1] = 42 | (3 << 6);
	kb_to_scancode['G'][1] = 43 | (3 << 6);
	kb_to_scancode['H'][1] = 44 | (3 << 6);
	kb_to_scancode['J'][1] = 45 | (3 << 6);
	kb_to_scancode['K'][1] = 46 | (3 << 6);
	kb_to_scancode['L'][1] = 47 | (3 << 6);
	kb_to_scancode['+'][1] = 48 | (3 << 6);
	kb_to_scancode['*'][1] = 061 | (3 << 6);

	kb_to_scancode['Z'][1] = 53 | (3 << 6);
	kb_to_scancode['X'][1] = 54 | (3 << 6);
	kb_to_scancode['C'][1] = 55 | (3 << 6);
	kb_to_scancode['V'][1] = 56 | (3 << 6);
	kb_to_scancode['B'][1] = 57 | (3 << 6);
	kb_to_scancode['N'][1] = 58 | (3 << 6);
	kb_to_scancode['M'][1] = 59 | (3 << 6);
	kb_to_scancode['<'][1] = 60 | (3 << 6);
	kb_to_scancode['>'][1] = 61 | (3 << 6);
	kb_to_scancode['?'][1] = 62 | (3 << 6);

	// Map Delete to Rubout.
	kb_to_scancode[0x7f][0] = 046;
	kb_to_scancode[0x08][0] = 046;

	// Map Tab to Tab.
	kb_to_scancode[9][0] = 0211;

	// Map Escape to Escape.
	kb_to_scancode[0x1b][0] = 0204;

	// Add shifts.
	for (int i = 0; i < 256; i++) {
		if (kb_to_scancode[i][1] == 0)
			kb_to_scancode[i][1] = kb_to_scancode[i][0] | (3 << 6);
	}
}

void
kbd_init(void)
{
	old_kbd_init();
}

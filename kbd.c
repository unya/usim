// kbd.c --- keyboard handling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "usim.h"
#include "ucode.h"
#include "iob.h"
#include "kbd.h"

// See SYS:LMWIN;COLD LISP for details.
//
// Keyboard translate table is a 3 X 64 array.
// 3 entries for each of 100 keys.  First is vanilla, second shift, third top.
unsigned char kb_old_table[64][3] = {
	{LM_K_BREAK, LM_K_BREAK, LM_K_NETWORK},
	{LM_K_ESC, LM_K_ESC, LM_K_SYSTEM},
	{'1', '!', '!'},
	{'2', '"', '"'},
	{'3', '#', '#'},
	{'4', '$', '$'},
	{'5', '%', '%'},
	{'6', '&', '&'},
	{'7', '\'', '\''},
	{'8', '(', '('},
	{'9', ')', ')'},
	{'0', '_', '_'},
	{'-', '=', '='},
	{'@', '`', '`'},
	{'^', '~', '~'},
	{LM_K_BS, LM_K_BS, LM_K_BS},
	{LM_K_CALL, LM_K_CALL, LM_K_ABORT},
	{LM_K_CLEAR, LM_K_CLEAR, LM_K_CLEAR},
	{LM_K_TAB, LM_K_TAB, LM_K_TAB},
	{'', '', ''},
	{'q', 'Q', ''},
	{'w', 'W', ''},
	{'e', 'E', ''},
	{'r', 'R', ''},
	{'t', 'T', ''},
	{'y', 'Y', ''},
	{'u', 'U', ''},
	{'i', 'I', ''},
	{'o', 'O', ''},
	{'p', 'P', ''},
	{'[', '{', '{'},
	{']', '}', '}'},
	{'\\', '|', '|'},
	{'/', '', ''},
	{'', LM_K_CR, LM_K_NULL},
	{LM_K_CR, LM_K_TAB, LM_K_TAB},
	{LM_K_FORM, LM_K_FORM, LM_K_FORM},
	{LM_K_VT, LM_K_VT, LM_K_VT},
	{LM_K_RUBOUT, LM_K_RUBOUT, LM_K_RUBOUT},
	{'a', 'A', ''},
	{'s', 'S', ''},
	{'d', 'D', ''},
	{'f', 'F', ''},
	{'g', 'G', ''},
	{'h', 'H', LM_K_HELP},
	{'j', 'J', ''},
	{'k', 'K', ''},
	{'l', 'L', ''},
	{';', '+', '+'},
	{':', '*', '*'},
	{LM_K_CR, LM_K_CR, LM_K_END},
	{LM_K_LINE, LM_K_LINE, LM_K_LINE},
	{LM_K_BACK_NEXT, LM_K_BACK_NEXT, LM_K_BACK_NEXT},
	{'z', 'Z', ''},
	{'x', 'X', ''},
	{'c', 'C', ''},
	{'v', 'V', ''},
	{'b', 'B', ''},
	{'n', 'N', ''},
	{'m', 'M', ''},
	{',', '<', '<'},
	{'.', '>', '>'},
	{'/', '?', '?'},
	{LM_K_SP, LM_K_SP, LM_K_SP},
};

unsigned int kbd_key_scan;

unsigned short kb_to_scancode[256][4];

#define KEY_QUEUE_LEN 10

static int key_queue[KEY_QUEUE_LEN];
static int key_queue_optr = 0;
static int key_queue_iptr = 0;
static int key_queue_free = KEY_QUEUE_LEN;

void
queue_key_event(int ev)
{
	int v;

	v = (1 << 16) | ev;

	if (key_queue_free > 0) {
		traceio("queue_key_event() - queuing 0%o, q len before %d\n", v, KEY_QUEUE_LEN - key_queue_free);
		key_queue_free--;
		key_queue[key_queue_optr] = v;
		key_queue_optr = (key_queue_optr + 1) % KEY_QUEUE_LEN;
	} else {
		fprintf(stderr, "IOB key queue full!\n");
		if (!(iob_csr & (1 << 5)) && (iob_csr & (1 << 2))) {
			iob_csr |= 1 << 5;
			fprintf(stderr, "queue_key_event generating interrupt\n");
			assert_unibus_interrupt(0260);
		}
	}
}

void
dequeue_key_event(void)
{
	if (iob_csr & (1 << 5))	// Already something to be read.
		return;

	if (key_queue_free < KEY_QUEUE_LEN) {
		int v = key_queue[key_queue_iptr];
		traceio("dequeue_key_event() - dequeuing 0%o, q len before %d\n", v, KEY_QUEUE_LEN - key_queue_free);
		key_queue_iptr = (key_queue_iptr + 1) % KEY_QUEUE_LEN;
		key_queue_free++;
		kbd_key_scan = (1 << 16) | v;
		if (iob_csr & (1 << 2)) {	// Keyboard interrupt enabled?
			iob_csr |= 1 << 5;
			fprintf(stderr, "dequeue_key_event generating interrupt (q len after %d)\n", KEY_QUEUE_LEN - key_queue_free);
			assert_unibus_interrupt(0260);
		}
	}
}

void
kbd_key_event(int code, int keydown)
{
	int v;

	traceio("key_event(code=%x, keydown=%x)\n", code, keydown);

	v = ((!keydown) << 8) | code;

	if (iob_csr & (1 << 5))
		queue_key_event(v);	// Already something there, queue this.
	else {
		kbd_key_scan = (1 << 16) | v;
		traceio("key_event() - 0%o\n", kbd_key_scan);
		if (iob_csr & (1 << 2)) {
			iob_csr |= 1 << 5;
			assert_unibus_interrupt(0260);
		}
	}
}

void
kbd_warm_boot_key(void)
{
	// Send a Return to get the machine booted.
	kbd_key_event(50, 0);
}

void
kbd_init(void)
{
	// ---!!! Handle multiple modifiers!
	memset((char *) kb_to_scancode, 0, sizeof(kb_to_scancode));

	// Walk unshifted old keyboard table.
	for (int i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_to_scancode[(int) k][0] = i;
	}

	// Add shifts.
	for (int i = 0; i < 256; i++)
		kb_to_scancode[i][1] = kb_to_scancode[i][0] | (3 << 6);

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

	kb_to_scancode['@'][1] = 13;
	kb_to_scancode['^'][1] = 14;

	kb_to_scancode['Q'][1] = 20| (3 << 6);
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
}

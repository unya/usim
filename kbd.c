// kbd.c --- keyboard handling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "usim.h"
#include "ucode.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>

extern unsigned int iob_key_scan;
extern unsigned int iob_kbd_csr;

// See SYS:LMIO;RDDEFS LISP for details.
#define LM_K_NULL 0200
#define LM_K_BREAK 0201
#define LM_K_CLEAR 0202
#define LM_K_CALL 0203
#define LM_K_ESC 0204
#define LM_K_BACK_NEXT 0205
#define LM_K_HELP 0206
#define LM_K_RUBOUT 0207
#define LM_K_BS 0210
#define LM_K_TAB 0211
#define LM_K_LINE 0212
#define LM_K_VT 0213
#define LM_K_FORM 0214
#define LM_K_CR 0215
#define LM_K_ABORT 0221
#define LM_K_END 0224
#define LM_K_SYSTEM 0235 
#define LM_K_NETWORK 0236
#define LM_K_SP 040

// See SYS:LMWIN;COLD LISP for details.
//
// Keyboard translate table is a 3 X 64 array.
// 3 entries for each of 100 keys.  First is vanilla, second shift, third top.
unsigned char kb_old_table[64][3] = {
	LM_K_BREAK, LM_K_BREAK, LM_K_NETWORK,
	LM_K_ESC, LM_K_ESC, LM_K_SYSTEM,
	'1', '!', '!',
	'2', '"', '"',
	'3', '#', '#',
	'4', '$', '$',
	'5', '%', '%',
	'6', '&', '&',
	'7', '\'', '\'',
	'8', '(', '(',
	'9', ')', ')',
	'0', '_', '_',
	'-', '=', '=',
	'@', '`', '`',
	'^', '~', '~',
	LM_K_BS, LM_K_BS, LM_K_BS,
	LM_K_CALL, LM_K_CALL, LM_K_ABORT,
	LM_K_CLEAR, LM_K_CLEAR, LM_K_CLEAR,
	LM_K_TAB, LM_K_TAB, LM_K_TAB,
	'', '', '',
	'q', 'Q', '',
	'w', 'W', '',
	'e', 'E', '',
	'r', 'R', '',
	't', 'T', '',
	'y', 'Y', '',
	'u', 'U', '',
	'i', 'I', '',
	'o', 'O', '',
	'p', 'P', '',
	'[', '{', '{',
	']', '}', '}',
	'\\', '|', '|',
	'/', '', '',
	'', LM_K_CR, LM_K_NULL,
	LM_K_CR, LM_K_TAB, LM_K_TAB,
	LM_K_FORM, LM_K_FORM, LM_K_FORM,
	LM_K_VT, LM_K_VT, LM_K_VT,
	LM_K_RUBOUT, LM_K_RUBOUT, LM_K_RUBOUT,
	'a', 'A', '',
	's', 'S', '',
	'd', 'D', '',
	'f', 'F', '',
	'g', 'G', '',
	'h', 'H', LM_K_HELP,
	'j', 'J', '',
	'k', 'K', '',
	'l', 'L', '',
	';', '+', '+',
	':', '*', '*',
	LM_K_CR, LM_K_CR, LM_K_END,
	LM_K_LINE, LM_K_LINE, LM_K_LINE,
	LM_K_BACK_NEXT, LM_K_BACK_NEXT, LM_K_BACK_NEXT,
	'z', 'Z', '',
	'x', 'X', '',
	'c', 'C', '',
	'v', 'V', '',
	'b', 'B', '',
	'n', 'N', '',
	'm', 'M', '',
	',', '<', '<',
	'.', '>', '>',
	'/', '?', '?',
	LM_K_SP, LM_K_SP, LM_K_SP,
};

unsigned short kb_to_scancode[256][4];

void
iob_key_event(int code, int extra)
{
	if (code == XK_Shift_L ||
	    code == XK_Shift_R ||
	    code == XK_Control_L ||
	    code == XK_Control_R ||
	    code == XK_Alt_L ||
	    code == XK_Alt_R)
		return;

	switch (code) {
	case XK_F1:		// Terminal.
		iob_key_scan = 1;
		break;
	case XK_F2:		// System.
		iob_key_scan = 1 | (3 << 8);
		break;
	case XK_F3:		// Network.
		iob_key_scan = 0 | (3 << 8);
		break;
	case XK_F4:		// Abort.
		iob_key_scan = 16 | (3 << 8);
		break;
	case XK_F5:		// Clear.
		iob_key_scan = 17;
		break;
	case XK_F6:		// Help.
		iob_key_scan = 44 | (3 << 8);
		break;
	case XK_F11:		// End.
		iob_key_scan = 50 | (3 << 8);
		break;
	case XK_F7:		// Call.
		iob_key_scan = 16;
		break;
	case XK_F12:		// Break.
	case XK_Break:
		iob_key_scan = 0;
		break;
	case XK_BackSpace:	// Rubout.
		iob_key_scan = 046;
		break;
	case XK_Return:		// Return.
		iob_key_scan = 50;
		break;
	case XK_Tab:
		iob_key_scan = 18;
		break;
	case XK_Escape:
		iob_key_scan = 1;
		break;
	default:
		if (code > 255) {
			printf("unknown keycode: %d\n", code);
			return;
		}
		iob_key_scan = kb_to_scancode[code][(extra & (3 << 6)) ? 1 : 0];
		break;
	}

	// Keep Control and Meta bits, Shift is in the scancode table.
	iob_key_scan |= extra & ~(3 << 6);
	// ... but if Control or Meta, add in Shift.
	if (extra & (17 << 10)) {
		iob_key_scan |= extra;
	}

	iob_key_scan |= 0xffff0000;

	iob_kbd_csr |= 1 << 5;
	assert_unibus_interrupt(0260);
}

void
iob_warm_boot_key(void)
{
	iob_key_event(XK_Return, 0);
}

void
iob_dequeue_key_event(void)
{
}

void
queue_all_keys_up(void)
{
}

void
kbd_init(void)
{
	int i;

	// ---!!! Handle multiple modifiers!
	memset((char *) kb_to_scancode, 0, sizeof(kb_to_scancode));

	// Walk unshifted old keyboard table.
	for (i = 0; i < 64; i++) {
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
	for (i = 0; i < 256; i++) {
		if (kb_to_scancode[i][1] == 0)
			kb_to_scancode[i][1] = kb_to_scancode[i][0] | (3 << 6);
	}
}

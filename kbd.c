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

/* keycodes produced by LM keyboard */
#define LM_K_BREAK	0201
#define LM_K_CLEAR	0202
#define LM_K_CALL	0203
#define LM_K_ESC	0204
#define LM_K_BACK	0205
#define LM_K_HELP	0206
#define LM_K_RUBOUT	0207
#define LM_K_CR		0215
#define LM_K_ALTMODE 033
#define LM_K_BREAK 0201
#define LM_K_CLEAR_INPUT 0202
#define LM_K_CALL 0203
#define LM_K_TERMINAL 0204
#define LM_K_MACRO 0205
#define LM_K_HELP 0206
#define LM_K_RUBOUT 0207
#define LM_K_OVERSTRIKE 0210
#define LM_K_TAB 0211
#define LM_K_LINE 0212
#define LM_K_DELETE 0213
#define LM_K_PAGE 0214
#define LM_K_CLEAR_SCREEN 0214
#define LM_K_RETURN 0215
#define LM_K_QUOTE 0216
#define LM_K_HOLD_OUTPUT 0217
#define LM_K_STOP_OUTPUT 0220
#define LM_K_ABORT 0221
#define LM_K_RESUME 0222
#define LM_K_STATUS 0223
#define LM_K_END 0224
#define LM_K_ROMAN_I 0225
#define LM_K_ROMAN_II 0226
#define LM_K_ROMAN_III 0227
#define LM_K_ROMAN_IV 0230
#define LM_K_HAND_UP 0231
#define LM_K_HAND_DOWN 0232
#define LM_K_HAND_LEFT 0233
#define LM_K_HAND_RIGHT 0234
#define LM_K_SYSTEM 0235
#define LM_K_NETWORK 0236

extern unsigned int iob_key_scan;
extern unsigned int iob_kbd_csr;


/* ****
 * KEYBOARD TRANSLATE TABLE IS A 3 X 64 ARRAY.
 * 3 ENTRIES FOR EACH OF 100 KEYS.  FIRST IS VANILLA, SECOND SHIFT, THIRD TOP.
 * THE FUNCTION KBD-INITIALIZE IS ONLY CALLED ONCE, IN ORDER TO SET UP THIS ARRAY.
 **** */
unsigned char kb_old_table[64][3] = {
	/* none,shift,top */
	0201,	0201,	LM_K_NETWORK,	//BREAK,BREAK,NETWORK
	0204,	0204,	LM_K_SYSTEM,	//ESC,ESC,SYSTEM
	'1',	'!',	'!',
	'2',	'"',	'"',
	'3',	'#',	'#',
	'4',	'$',	'$',
	'5',	'%',	'%',
	'6',	'&',	'&',
	'7',	'\'',	'\'',
	'8',	'(',	'(',
	'9',	')',	')',
	'0',	'_',	'_',
	'-',	'=',	'=',
	'@',	'`',	'`',
	'^',	'~',	'~',
	0210,	0210,	0210,	//BS,BS,BS
	0203,	0203, 	LM_K_ABORT,	//CALL,CALL,ABORT
	0202,	0202,	0202,	//CLEAR,CLEAR,CLEAR
	0211,	0211,	0211,	//TAB,TAB,TAB
	'',	'',	'',
	'q',	'Q',	'',
	'w',	'W',	'',
	'e',	'E',	'',
	'r',	'R',	'',
	't',	'T',	'',
	'y',	'Y',	'',
	'u',	'U',	'',
	'i',	'I',	'',
	'o',	'O',	'',
	'p',	'P',	'',
	'[',	'{',	'{',
	']',	'}',	'}',
	'\\',	'|',	'|',
	'/',	'',	'',
	'',	0215,	0,	//^L,cr,^@
	0215,	0211,	0211,
	0214,	0214,	0214,	//FORM,FORM,FORM
	0213,	0213,	0213,	//VT,VT,VT
	0207,	0207,	0207,	//RUBOUT,RUBOUT,RUBOUT
	'a',	'A',	'',
	's',	'S',	'',
	'd',	'D',	'',
	'f',	'F',	'',
	'g',	'G',	'\032',
	'h',	'H',	0206,	//h,H,HELP
	'j',	'J',	'',
	'k',	'K',	'',
	'l',	'L',	'',
	';',	'+',	'+',
	':',	'*',	'*',
	0215,	0215,	LM_K_END,	//CR,CR,END
	0212,	0212,	0212,	//LINE,LINE,LINE
	0205,	0205,	0205,	//BACK-NEXT,BACK-NEXT,BACK-NEXT
	'z',	'Z',	'',
	'x',	'X',	'',
	'c',	'C',	'',
	'v',	'V',	'',
	'b',	'B',	'',
	'n',	'N',	'',
	'm',	'M',	'',
	',',	'<',	'<',
	'.',	'>',	'>',
	'/',	'?',	'?',
	' ',	' ',	' '
};

unsigned short kb_to_scancode[256][4];

/****
 * FORMAT OF DATA IN 764100 (IF USING OLD KEYBOARD):
 * 00077   0006	  ;KEY CODE
 * 00300   0602    ;SHIFT LEFT,RIGHT
 * 01400   1002    ;TOP LEFT,RIGHT
 * 06000   1202    ;CONTROL LEFT,RIGHT
 * 30000   1402    ;META LEFT,RIGHT
 * 40000   1601    ;SHIFT LOCK
 ****/
void
iob_key_event(int code, int extra)
{
	int newkbd = 0; // keys found on the "new" keyboard

	if (0) printf("iob_key_event(code=%x,extra=%x)\n", code, extra);

	if (code == XK_Shift_L ||
	    code == XK_Shift_R ||
	    code == XK_Control_L ||
	    code == XK_Control_R ||
	    code == XK_Alt_L ||
	    code == XK_Alt_R)
		return;

	/*
	  network
	  system
	  abort
	  clear
	  help
	*/
	switch(code) {
	case XK_F1:
		iob_key_scan = 1;		/* terminal */
		break;
	case XK_F2:
		iob_key_scan = 1 | (3 << 8);	/* system */
		break;
	case XK_F3:
		iob_key_scan = 0 | (3 << 8);	/* network */
		break;
	case XK_F4:
		iob_key_scan = 16 | (3 << 8);	/* abort */
		break;
	case XK_F5:
		iob_key_scan = 17;		/* clear */
		break;
	case XK_F6:
		iob_key_scan = 44 | (3 << 8); /* help */
		break;
	case XK_F11:
		iob_key_scan = 50 | (3 << 8); /* end */
		break;
	case XK_F7:
		iob_key_scan = 16; /* call */
		break;
	case XK_F12:
	case XK_Break:
		iob_key_scan = 0; /* break */
		break;
	case XK_BackSpace:
		iob_key_scan = 046; /* rubout */
		break;
	case XK_Return:
		iob_key_scan = 50; /* CR */
		break;
	case XK_Down:
		iob_key_scan = 0176;
		newkbd = 1;
		break;
	case XK_Left:
		iob_key_scan = 0117;
		newkbd = 1;
		break;
	case XK_Right:
		iob_key_scan = 017;
		newkbd = 1;
		break;
	case XK_Up:
		iob_key_scan = 0106;
		newkbd = 1;
		break;
	case XK_Tab:
		iob_key_scan = 18;
		break;
	case XK_Escape:
		iob_key_scan = 1;
		break;
	default:
		if (code > 255)
		{
			printf("unknown keycode: %d\n", code);
			return;
		}
		iob_key_scan =
			kb_to_scancode[code][(extra & (3 << 6)) ? 1 : 0];
		break;
	}

	/* keep C/M bits, Shift in scancode tbl */
	iob_key_scan |= extra & ~(3 << 6);
	/* but if Control/Meta, add in Shift */
	if (extra & (17 << 10))
	{
		if (0) printf("extra: %x  17<<10: %x", extra, 17 << 10);
		iob_key_scan |= extra;
	}

	if (0) printf("code 0%o, extra 0%o, scan 0%o\n",
		      code, extra, iob_key_scan);

	if (newkbd)
		iob_key_scan |= 1 << 16;
	else
		iob_key_scan |= 0xffff0000;

	iob_kbd_csr |= 1 << 5;
	assert_unibus_interrupt(0260);
}

void
iob_warm_boot_key(void)
{
	iob_key_event(XK_Return, 0);
}


void iob_dequeue_key_event(void)
{
}


void queue_all_keys_up(void)
{
}

void
kbd_init(void)
{
	int i;

	/* #### bzzt! handle multiple modifiers!! */
	memset((char *)kb_to_scancode, 0, sizeof(kb_to_scancode));

	/* Walk unshifted old kbd table */
	for (i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_to_scancode[k][0] = i;
	}

	/* Modify mapping to match present-day US kbd */
	kb_to_scancode['`'][0] = 015 | (3 << 6); /* ` = Shift @ = ` */
	kb_to_scancode['`'][1] = 016 | (3 << 6); /* Sh-` = Sh-^ = ~*/

	kb_to_scancode['\''][0] = 010 | (3<<6);  /* ' = Sh-7 = ' */
	kb_to_scancode['\''][1] = 3 | (3<<6);    /* Sh-' = Sh-2 = " */
	kb_to_scancode['='][0] = 014 | (3<<6);   /* = = Sh-- = = */
	kb_to_scancode['2'][1] = 015;	     /* Sh-2 = @ (unshifted) */

	kb_to_scancode['6'][1] = 016;	     /* Sh-6 = ^ (unshifted) */
	kb_to_scancode['7'][1] = 7 | (3<<6);     /* Sh-7 = Sh-6 = & */
	kb_to_scancode['8'][1] = 061 | (3<<6);   /* Sh-8 = Sh-: = * */
	kb_to_scancode['9'][1] = 011 | (3<<6);   /* Sh-9 = Sh-8 = ( */
	kb_to_scancode['0'][1] = 012 | (3<<6);   /* Sh-0 = Sh-9 = ) */
	kb_to_scancode['-'][1] = 013 | (3<<6);   /* Sh-- = Sh-0 = _ */
	kb_to_scancode['='][1] = 060 | (3<<6);   /* Sh-= = Sh-; = + */

	kb_to_scancode[';'][1] = 061;	     /* Sh-; = : (unshifted) */
	kb_to_scancode[':'][1] = 061;

	kb_to_scancode['!'][1] = 2 | (3<<6);
	kb_to_scancode['"'][1] = 3 | (3<<6);
	kb_to_scancode['#'][1] = 4 | (3<<6);
	kb_to_scancode['$'][1] = 5 | (3<<6);
	kb_to_scancode['%'][1] = 6 | (3<<6);
	kb_to_scancode['&'][1] = 7 | (3<<6);
	kb_to_scancode['('][1] = 011 | (3<<6);
	kb_to_scancode[')'][1] = 012 | (3<<6);
	kb_to_scancode['_'][1] = 013 | (3<<6);
	kb_to_scancode['~'][1] = 016 | (3<<6);
	kb_to_scancode['@'][1] = 13;
	kb_to_scancode['^'][1] = 14;

	kb_to_scancode['Q'][1] = 20 | (3<<6);
	kb_to_scancode['W'][1] = 21 | (3<<6);
	kb_to_scancode['E'][1] = 22 | (3<<6);
	kb_to_scancode['R'][1] = 23 | (3<<6);
	kb_to_scancode['T'][1] = 24 | (3<<6);
	kb_to_scancode['Y'][1] = 25 | (3<<6);
	kb_to_scancode['U'][1] = 26 | (3<<6);
	kb_to_scancode['I'][1] = 27 | (3<<6);
	kb_to_scancode['O'][1] = 28 | (3<<6);
	kb_to_scancode['P'][1] = 29 | (3<<6);
	kb_to_scancode['{'][1] = 30 | (3<<6);
	kb_to_scancode['}'][1] = 31 | (3<<6);
	kb_to_scancode['|'][1] = 32 | (3<<6);

	kb_to_scancode['A'][1] = 39 | (3<<6);
	kb_to_scancode['S'][1] = 40 | (3<<6);
	kb_to_scancode['D'][1] = 41 | (3<<6);
	kb_to_scancode['F'][1] = 42 | (3<<6);
	kb_to_scancode['G'][1] = 43 | (3<<6);
	kb_to_scancode['H'][1] = 44 | (3<<6);
	kb_to_scancode['J'][1] = 45 | (3<<6);
	kb_to_scancode['K'][1] = 46 | (3<<6);
	kb_to_scancode['L'][1] = 47 | (3<<6);
	kb_to_scancode['+'][1] = 48 | (3<<6);
	kb_to_scancode['*'][1] = 061 | (3<<6);

	kb_to_scancode['Z'][1] = 53 | (3<<6);
	kb_to_scancode['X'][1] = 54 | (3<<6);
	kb_to_scancode['C'][1] = 55 | (3<<6);
	kb_to_scancode['V'][1] = 56 | (3<<6);
	kb_to_scancode['B'][1] = 57 | (3<<6);
	kb_to_scancode['N'][1] = 58 | (3<<6);
	kb_to_scancode['M'][1] = 59 | (3<<6);
	kb_to_scancode['<'][1] = 60 | (3<<6);
	kb_to_scancode['>'][1] = 61 | (3<<6);
	kb_to_scancode['?'][1] = 62 | (3<<6);

	/* map "Delete" to rubout */
	kb_to_scancode[0x7f][0] = 046;	     /* Delete = Rubout */
	kb_to_scancode[0x08][0] = 046;	     /* Delete = Rubout */

	/* map tab to tab */
	kb_to_scancode[9][0] = 0211;	     /* Tab = Tab */

	/* esc = esc */
	kb_to_scancode[0x1b][0] = 0204;	     /* Esc = Esc (Terminal) */

	/* map arrows */
	//    kb_to_scancode[0x2b][2] = LM_K_HAND_DOWN;

	/* Add shifts */
	for (i = 0; i < 256; i++) {
		if (kb_to_scancode[i][1] == 0)
			kb_to_scancode[i][1] = kb_to_scancode[i][0] |
				(3 << 6);
	}

	if (0) printf("kb_to_scancode[';'][1] = %x\n", kb_to_scancode[';'][1]);

#if 0   /* Don't do this */
	/* control keys */
	for (i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_to_scancode[k][2] = i | (3 << 10);
	}

	/* meta keys */
	for (i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_to_scancode[k][3] = i | (3 << 12);
	}
#endif
}

/*
 * iob.c
 *
 * $Id$
 */

#include "ucode.h"

#include <sys/time.h>

#include <SDL/SDL_keysym.h>

int iob_key_scan;

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

/* keycodes produced by LM keyboard */
#define LM_K_BREAK	0201
#define LM_K_CLEAR	0202
#define LM_K_CALL	0203
#define LM_K_ESC	0204
#define LM_K_BACK	0205
#define LM_K_HELP	0206
#define LM_K_RUBOUT	0207
#define LM_K_CR		0215

#define LM_K_NETWORK 0
#define LM_K_SYSTEM 0
#define LM_K_ABORT 0
#define LM_K_END 0

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
	'g',	'G',	'',
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

unsigned short kb_sdl_to_scancode[256][3];

/*
keys we need to map

meta <- alt

top
 network
 system
 abort
 help
 end

call
clear

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

int iob_kbd_csr;

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
//		*pv = (iob_key_scan >> 16) & 0177777;
		*pv = 0177777;
iob_kbd_csr &= ~(1 << 5);
		traceio("unibus: kbd high %011o\n", *pv);
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
		*pv = iob_kbd_csr;
		traceio("unibus: kbd csr %011o\n", *pv);
		break;
	case 0120:
		traceio("unibus: usec clock low\n");
#if 1
	{
		static struct timeval tv;
		struct timeval tv2;
		unsigned long ds, du;
		if (tv.tv_sec == 0) {
			gettimeofday(&tv, 0);
			*pv = 0;
		} else {
			unsigned int newsec;
			gettimeofday(&tv2, 0);

			ds = tv2.tv_sec - tv.tv_sec;
			if (tv2.tv_usec < tv.tv_usec) {
				ds--;
				tv2.tv_usec += 1000*1000;
			}
			du = tv2.tv_usec - tv.tv_usec;

			*pv = (ds * 100) + (du / 10000);
			printf("delta %lu\n", *pv);
		}
	}
#endif
		break;
	case 0122:
		traceio("unibus: usec clock high\n");
		break;
	case 0140:
		traceio("unibus: chaos\n");
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
	case 0140:
		traceio("unibus: chaos\n");
		break;
	}
}

int
iob_get_key_scan(void)
{
	return iob_key_scan;
}

void
iob_sdl_key_event(int code, int extra)
{
	int s, c;

	if (0) printf("iob_sdl_key_event(code=%x,extra=%x)\n", code, extra);

	if (code == 0 || code == 0x130)
		return;

	/*
	  network
	  system
	  abort
	  clear
	  help
	*/
	switch(code) {
	case SDLK_F1:
		iob_key_scan = 0 | (3 << 8);
		break;
	case SDLK_F2:
		iob_key_scan = 1 | (3 << 8);
		break;
	case SDLK_F3:
		iob_key_scan = 16 | (3 << 8);
		break;
	case SDLK_F4:
		iob_key_scan = 17;
		break;
	case SDLK_F5:
		iob_key_scan = 44 | (3 << 8);
		break;
	case SDLK_F6:
		iob_key_scan = 50 | (3 << 8);
		break;
	default:
		s = 0;
		if (extra & (3 << 6))
			s = 1;
		if (extra & (3 << 10))
			s = 2;
		if (extra & (3 << 12))
			s = 3;

		c = kb_sdl_to_scancode[code][s];

		if (0) printf("s %d, c %x, code %x\n",  s, c, code);
		iob_key_scan = c;
		break;
	}

	iob_kbd_csr |= 1 << 5;
	assert_unibus_interrupt(0260);
}

void
iob_sdl_mouse_event(int dx, int dy, int buttons)
{
#if 0
	printf("iob_sdl_mouse_event(dx=%x,dy=%x,buttons=%x)\n",
	       dx, dy, buttons);
	iob_kbd_csr |= 1 << 4;
	assert_unibus_interrupt(0260);
#endif
}

//xxx tv interrupt
// tv csr @ base, 1<<4 = interrupt flag
// writing back clears int
// 60hz

void
iob_sdl_clock_event()
{
	iob_kbd_csr |= 1 << 6;
	assert_unibus_interrupt(0274);
}

int
iob_init(void)
{
	int i, j;

	memset((char *)kb_sdl_to_scancode, 0, sizeof(kb_sdl_to_scancode));

	for (i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_sdl_to_scancode[k][0] = i;
	}

	kb_sdl_to_scancode['`'][0] = 13 | (3 << 6);
	kb_sdl_to_scancode['`'][1] = 14 | (3 << 6);

	kb_sdl_to_scancode['\''][0] = 8 | (3<<6);
	kb_sdl_to_scancode['\''][1] = 3 | (3<<6);
	kb_sdl_to_scancode['='][0] = 12 | (3<<6);
	kb_sdl_to_scancode['2'][1] = 13;

	kb_sdl_to_scancode['6'][1] = 14;
	kb_sdl_to_scancode['7'][1] = 7 | (3<<6);
	kb_sdl_to_scancode['8'][1] = 8 | (3<<6);
	kb_sdl_to_scancode['9'][1] = 9 | (3<<6);
	kb_sdl_to_scancode['0'][1] = 10 | (3<<6);
	kb_sdl_to_scancode['-'][1] = 11 | (3<<6);
	kb_sdl_to_scancode['='][1] = 48 | (3<<6);

	kb_sdl_to_scancode[';'][1] = 49;

	for (i = 0; i < 256; i++) {
		if (kb_sdl_to_scancode[i][1] == 0)
			kb_sdl_to_scancode[i][1] = kb_sdl_to_scancode[i][0] |
				(3 << 6);
	}

	return 0;
}

int
tv_xbus_read(int offset, unsigned int *pv)
{
	tracef("tv register read, offset %o\n", offset);
	*pv = 0;
	return 0;
}


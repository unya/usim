/*
 * kbd_old.c
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if !defined(DISPLAY_X11) && !defined(DISPLAY_OSX)
#include <SDL.h>
#endif

#include "usim.h"
#include "keyboard.h"
#include "ucode.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>

/* this is a hack, but it works */
#define SDLK_LSHIFT	XK_Shift_L
#define SDLK_RSHIFT	XK_Shift_R
#define SDLK_LCTRL	XK_Control_L
#define SDLK_RCTRL	XK_Control_R
#define SDLK_LALT	XK_Alt_L
#define SDLK_RALT	XK_Alt_R
#define SDLK_F1		XK_F1
#define SDLK_F2 	XK_F2
#define SDLK_F3		XK_F3
#define SDLK_F4		XK_F4
#define SDLK_F5		XK_F5
#define SDLK_F6		XK_F6
#define SDLK_F7		XK_F7
#define SDLK_END	XK_F11
#define SDLK_F12	XK_F12
#define SDLK_BACKSPACE	XK_BackSpace
#define SDLK_BREAK	XK_Break
#define SDLK_RETURN	XK_Return
#define SDLK_DOWN	XK_Down
#define SDLK_LEFT	XK_Left
#define SDLK_RIGHT	XK_Right
#define SDLK_UP		XK_Up
#define SDLK_TAB	XK_Tab
#define SDLK_ESC	XK_Escape

#ifdef DISPLAY_OSX
#define SDLK_LSHIFT	56
#define SDLK_RSHIFT	56
#define SDLK_LCTRL	59
#define SDLK_RCTRL	59
#define SDLK_LALT	55
#define SDLK_RALT	55
#define SDLK_F1		122
#define SDLK_F2 	120
#define SDLK_F3		99
#define SDLK_F4		118
#define SDLK_F5		96
#define SDLK_F6		97
#define SDLK_F7		98
#define SDLK_END	119
#define SDLK_F12	111
#define SDLK_BACKSPACE	51
#define SDLK_BREAK	71
#define SDLK_RETURN	36
#define SDLK_DOWN	125
#define SDLK_LEFT	123
#define SDLK_RIGHT	124
#define SDLK_UP		126
#define SDLK_TAB	48
#define SDLK_ESC	53
#define SDLK_a      0
#define SDLK_b      11
#define SDLK_c      8
#define SDLK_d      2
#define SDLK_e      14
#define SDLK_f      3
#define SDLK_g      5
#define SDLK_h      4
#define SDLK_i      34
#define SDLK_j      38
#define SDLK_k      40
#define SDLK_l      37
#define SDLK_m      46
#define SDLK_n      45
#define SDLK_o      31
#define SDLK_p      35
#define SDLK_q      12
#define SDLK_r      15
#define SDLK_s      1
#define SDLK_t      17
#define SDLK_u      32
#define SDLK_v      9
#define SDLK_w      13
#define SDLK_x      7
#define SDLK_y      16
#define SDLK_z      6
#define SDLK_0      29
#define SDLK_1      18
#define SDLK_2      19
#define SDLK_3      20
#define SDLK_4      21
#define SDLK_5      23
#define SDLK_6      22
#define SDLK_7      26
#define SDLK_8      28
#define SDLK_9      25
#define SDLK_EQUALS 24
#define SDLK_MINUS  27
#define SDLK_RIGHTBRACKET   30
#define SDLK_LEFTBRACKET    33
#define SDLK_QUOTE 39
#define SDLK_SEMICOLON 41
#define SDLK_SLASH  44
#define SDLK_COMMA  43
#define SDLK_BACKSLASH 42
#define SDLK_PERIOD 47
#define SDLK_BACKQUOTE 50
#define SDLK_SPACE  49
#define SDLK_COLON  41
#define SDLK_LEFTPAREN  25
#define SDLK_RIGHTPAREN 29
#define SDLK_CARET  22
#define SDLK_QUESTION 44
#define SDLK_LESS   43
#define SDLK_GREATER 47
#define SDLK_PLUS   24
#define SDLK_ASTERISK 28
#define SDLK_HASH   20
#define SDLK_EXCLAIM 18
#define SDLK_QUOTEDBL 39
#define SDLK_AT     19
#define SDLK_LEFTBRACKET 33
#define SDLK_RIGHTBRACKET 30
#define SDLK_LEFTBRACE 33
#define SDLK_RIGHTBRACE 30
#define SDLK_BAR    42
#define SDLK_UNDERSCORE 27
#define SDLK_TILDE  50
#define SDLK_AMPERSAND 26

#endif


extern unsigned int iob_key_scan;
extern unsigned int iob_kbd_csr;


/* ****
;KEYBOARD TRANSLATE TABLE IS A 3 X 64 ARRAY.
;3 ENTRIES FOR EACH OF 100 KEYS.  FIRST IS VANILLA, SECOND SHIFT, THIRD TOP.
;THE FUNCTION KBD-INITIALIZE IS ONLY CALLED ONCE, IN ORDER TO SET UP THIS ARRAY.
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

unsigned short kb_sdl_to_scancode[256][4];

/****
;FORMAT OF DATA IN 764100 (IF USING OLD KEYBOARD):
; 00077   0006	  ;KEY CODE
; 00300   0602    ;SHIFT LEFT,RIGHT
; 01400   1002    ;TOP LEFT,RIGHT
; 06000   1202    ;CONTROL LEFT,RIGHT
; 30000   1402    ;META LEFT,RIGHT
; 40000   1601    ;SHIFT LOCK
****/
void
iob_sdl_key_event(int code, int extra)
{
    int newkbd = 0; // keys found on the "new" keyboard

	if (0) printf("iob_sdl_key_event(code=%x,extra=%x)\n", code, extra);

	if (code == SDLK_LSHIFT ||
	    code == SDLK_RSHIFT ||
	    code == SDLK_LCTRL ||
	    code == SDLK_RCTRL ||
	    code == SDLK_LALT ||
	    code == SDLK_RALT)
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
		iob_key_scan = 1;		/* terminal */
		break;
	case SDLK_F2:
		iob_key_scan = 1 | (3 << 8);	/* system */
		break;
	case SDLK_F3:
		iob_key_scan = 0 | (3 << 8);	/* network */
		break;
	case SDLK_F4:
		iob_key_scan = 16 | (3 << 8);	/* abort */
		break;
	case SDLK_F5:
		iob_key_scan = 17;		/* clear */
		break;
	case SDLK_F6:
		iob_key_scan = 44 | (3 << 8); /* help */
		break;
	case SDLK_END:
		iob_key_scan = 50 | (3 << 8); /* end */
		break;
	case SDLK_F7:
		iob_key_scan = 16; /* call */
		break;
	case SDLK_F12:
	case SDLK_BREAK:
		iob_key_scan = 0; /* break */
		break;
	case SDLK_BACKSPACE:
		iob_key_scan = 046; /* rubout */
		break;
	case SDLK_RETURN:
		iob_key_scan = 50; /* CR */
		break;
	case SDLK_DOWN:
        iob_key_scan = 0176;
        newkbd = 1;
		break;
	case SDLK_LEFT:
        iob_key_scan = 0117;
        newkbd = 1;
		break;
	case SDLK_RIGHT:
        iob_key_scan = 017;
        newkbd = 1;
        break;
	case SDLK_UP:
        iob_key_scan = 0106;
        newkbd = 1;
        break;
	case SDLK_TAB:
		iob_key_scan = 18;
		break;
	case SDLK_ESC:
		iob_key_scan = 1;
		break;
	default:
		if (code > 255)
		{
			printf("unknown keycode: %d\n", code);
			return;
		}
		iob_key_scan =
			kb_sdl_to_scancode[code][(extra & (3 << 6)) ? 1 : 0];
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

#ifdef DISPLAY_SDL
/*
 * called by sdl.c:sdl_refresh() with key up/down events
 */
void sdl_process_key(SDL_KeyboardEvent *ev, int updown)
{
	int mod_state, extra;

	mod_state = SDL_GetModState();

	extra = 0;
	if (mod_state & (KMOD_LMETA | KMOD_LALT))
		extra |= 2 << 12;
	if (mod_state & (KMOD_RMETA | KMOD_RALT))
		extra |= 1 << 12;

	if (mod_state & KMOD_LSHIFT)
		extra |= 2 << 6;
	if (mod_state & KMOD_RSHIFT)
		extra |= 1 << 6;

	if (mod_state & KMOD_LCTRL)
		extra |= 2 << 10;
	if (mod_state & KMOD_RCTRL)
		extra |= 1 << 10;

	if (updown) {
#if 0
		printf("scancode %x, sym %x, mod %x, "
		       "modstate %x, extra %x, unicode %x\n",
		       ev->keysym.scancode,
		       ev->keysym.sym,
		       ev->keysym.mod,
		       mod_state,
		       extra,
		       ev->keysym.unicode);
#endif
		iob_sdl_key_event(ev->keysym.sym, extra);
	}
}
#endif /* DISPLAY_SDL */

void
iob_warm_boot_key(void)
{
	iob_sdl_key_event(SDLK_RETURN, 0);
}


void iob_dequeue_key_event(void)
{
}


void sdl_queue_all_keys_up(void)
{
}

void
kbd_init(void)
{
	int i;

	/* #### bzzt! handle multiple modifiers!! */
	memset((char *)kb_sdl_to_scancode, 0, sizeof(kb_sdl_to_scancode));

	/* Walk unshifted old kbd table */
	for (i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_sdl_to_scancode[k][0] = i;
	}

#ifdef DISPLAY_X11
	/* Modify mapping to match present-day US kbd */
	kb_sdl_to_scancode['`'][0] = 015 | (3 << 6); /* ` = Shift @ = ` */
	kb_sdl_to_scancode['`'][1] = 016 | (3 << 6); /* Sh-` = Sh-^ = ~*/
	
	kb_sdl_to_scancode['\''][0] = 010 | (3<<6);  /* ' = Sh-7 = ' */
	kb_sdl_to_scancode['\''][1] = 3 | (3<<6);    /* Sh-' = Sh-2 = " */
	kb_sdl_to_scancode['='][0] = 014 | (3<<6);   /* = = Sh-- = = */
	kb_sdl_to_scancode['2'][1] = 015;	     /* Sh-2 = @ (unshifted) */

	kb_sdl_to_scancode['6'][1] = 016;	     /* Sh-6 = ^ (unshifted) */
	kb_sdl_to_scancode['7'][1] = 7 | (3<<6);     /* Sh-7 = Sh-6 = & */
	kb_sdl_to_scancode['8'][1] = 061 | (3<<6);   /* Sh-8 = Sh-: = * */
	kb_sdl_to_scancode['9'][1] = 011 | (3<<6);   /* Sh-9 = Sh-8 = ( */
	kb_sdl_to_scancode['0'][1] = 012 | (3<<6);   /* Sh-0 = Sh-9 = ) */
	kb_sdl_to_scancode['-'][1] = 013 | (3<<6);   /* Sh-- = Sh-0 = _ */
	kb_sdl_to_scancode['='][1] = 060 | (3<<6);   /* Sh-= = Sh-; = + */

	kb_sdl_to_scancode[';'][1] = 061;	     /* Sh-; = : (unshifted) */
	kb_sdl_to_scancode[':'][1] = 061;

	kb_sdl_to_scancode['!'][1] = 2 | (3<<6);
	kb_sdl_to_scancode['"'][1] = 3 | (3<<6);
	kb_sdl_to_scancode['#'][1] = 4 | (3<<6);
	kb_sdl_to_scancode['$'][1] = 5 | (3<<6);
	kb_sdl_to_scancode['%'][1] = 6 | (3<<6);
	kb_sdl_to_scancode['&'][1] = 7 | (3<<6);
	kb_sdl_to_scancode['('][1] = 011 | (3<<6);
	kb_sdl_to_scancode[')'][1] = 012 | (3<<6);
	kb_sdl_to_scancode['_'][1] = 013 | (3<<6);
	kb_sdl_to_scancode['~'][1] = 016 | (3<<6);
	kb_sdl_to_scancode['@'][1] = 13;
	kb_sdl_to_scancode['^'][1] = 14;

	kb_sdl_to_scancode['Q'][1] = 20 | (3<<6);
	kb_sdl_to_scancode['W'][1] = 21 | (3<<6);
	kb_sdl_to_scancode['E'][1] = 22 | (3<<6);
	kb_sdl_to_scancode['R'][1] = 23 | (3<<6);
	kb_sdl_to_scancode['T'][1] = 24 | (3<<6);
	kb_sdl_to_scancode['Y'][1] = 25 | (3<<6);
	kb_sdl_to_scancode['U'][1] = 26 | (3<<6);
	kb_sdl_to_scancode['I'][1] = 27 | (3<<6);
	kb_sdl_to_scancode['O'][1] = 28 | (3<<6);
	kb_sdl_to_scancode['P'][1] = 29 | (3<<6);
	kb_sdl_to_scancode['{'][1] = 30 | (3<<6);
	kb_sdl_to_scancode['}'][1] = 31 | (3<<6);
	kb_sdl_to_scancode['|'][1] = 32 | (3<<6);

	kb_sdl_to_scancode['A'][1] = 39 | (3<<6);
	kb_sdl_to_scancode['S'][1] = 40 | (3<<6);
	kb_sdl_to_scancode['D'][1] = 41 | (3<<6);
	kb_sdl_to_scancode['F'][1] = 42 | (3<<6);
	kb_sdl_to_scancode['G'][1] = 43 | (3<<6);
	kb_sdl_to_scancode['H'][1] = 44 | (3<<6);
	kb_sdl_to_scancode['J'][1] = 45 | (3<<6);
	kb_sdl_to_scancode['K'][1] = 46 | (3<<6);
	kb_sdl_to_scancode['L'][1] = 47 | (3<<6);
	kb_sdl_to_scancode['+'][1] = 48 | (3<<6);
	kb_sdl_to_scancode['*'][1] = 061 | (3<<6);

	kb_sdl_to_scancode['Z'][1] = 53 | (3<<6);
	kb_sdl_to_scancode['X'][1] = 54 | (3<<6);
	kb_sdl_to_scancode['C'][1] = 55 | (3<<6);
	kb_sdl_to_scancode['V'][1] = 56 | (3<<6);
	kb_sdl_to_scancode['B'][1] = 57 | (3<<6);
	kb_sdl_to_scancode['N'][1] = 58 | (3<<6);
	kb_sdl_to_scancode['M'][1] = 59 | (3<<6);
	kb_sdl_to_scancode['<'][1] = 60 | (3<<6);
	kb_sdl_to_scancode['>'][1] = 61 | (3<<6);
	kb_sdl_to_scancode['?'][1] = 62 | (3<<6);

	/* map "Delete" to rubout */
	kb_sdl_to_scancode[0x7f][0] = 046;	     /* Delete = Rubout */
	kb_sdl_to_scancode[0x08][0] = 046;	     /* Delete = Rubout */

	/* map tab to tab */
	kb_sdl_to_scancode[9][0] = 0211;	     /* Tab = Tab */

	/* esc = esc */
	kb_sdl_to_scancode[0x1b][0] = 0204;	     /* Esc = Esc (Terminal) */
    
    /* map arrows */
//    kb_sdl_to_scancode[0x2b][2] = LM_K_HAND_DOWN;
#endif // DISPLAY_X11

#if DISPLAY_OSX
    kb_sdl_to_scancode[SDLK_a][0] = 39;
    kb_sdl_to_scancode[SDLK_b][0] = 57;
    kb_sdl_to_scancode[SDLK_c][0] = 55;
    kb_sdl_to_scancode[SDLK_d][0] = 41;
    kb_sdl_to_scancode[SDLK_e][0] = 22;
    kb_sdl_to_scancode[SDLK_f][0] = 42;
    kb_sdl_to_scancode[SDLK_g][0] = 43;
    kb_sdl_to_scancode[SDLK_h][0] = 44;
    kb_sdl_to_scancode[SDLK_i][0] = 27;
    kb_sdl_to_scancode[SDLK_j][0] = 45;
    kb_sdl_to_scancode[SDLK_k][0] = 46;
    kb_sdl_to_scancode[SDLK_l][0] = 47;
    kb_sdl_to_scancode[SDLK_m][0] = 59;
    kb_sdl_to_scancode[SDLK_n][0] = 58;
    kb_sdl_to_scancode[SDLK_o][0] = 28;
    kb_sdl_to_scancode[SDLK_p][0] = 29;
    kb_sdl_to_scancode[SDLK_q][0] = 20;
    kb_sdl_to_scancode[SDLK_r][0] = 23;
    kb_sdl_to_scancode[SDLK_s][0] = 40;
    kb_sdl_to_scancode[SDLK_t][0] = 24;
    kb_sdl_to_scancode[SDLK_u][0] = 26;
    kb_sdl_to_scancode[SDLK_v][0] = 56;
    kb_sdl_to_scancode[SDLK_w][0] = 21;
    kb_sdl_to_scancode[SDLK_x][0] = 54;
    kb_sdl_to_scancode[SDLK_y][0] = 25;
    kb_sdl_to_scancode[SDLK_z][0] = 53;
    
    kb_sdl_to_scancode[SDLK_1][0] = 2;
    kb_sdl_to_scancode[SDLK_2][0] = 3;
    kb_sdl_to_scancode[SDLK_3][0] = 4;
    kb_sdl_to_scancode[SDLK_4][0] = 5;
    kb_sdl_to_scancode[SDLK_5][0] = 6;
    kb_sdl_to_scancode[SDLK_6][0] = 7;
    kb_sdl_to_scancode[SDLK_7][0] = 8;
    kb_sdl_to_scancode[SDLK_8][0] = 9;
    kb_sdl_to_scancode[SDLK_9][0] = 10;
    kb_sdl_to_scancode[SDLK_0][0] = 11;
    kb_sdl_to_scancode[SDLK_SLASH][0] = 33;
    kb_sdl_to_scancode[SDLK_SPACE][0] = 63;
    kb_sdl_to_scancode[SDLK_SEMICOLON][0] = 48;
    kb_sdl_to_scancode[SDLK_COLON][1] = 49;
    kb_sdl_to_scancode[SDLK_LEFTPAREN][1] = 9 | (3 << 6);
    kb_sdl_to_scancode[SDLK_RIGHTPAREN][1] = 10 | (3 << 6);
    kb_sdl_to_scancode[SDLK_MINUS][0] = 12;
    kb_sdl_to_scancode[SDLK_AT][1] = 13;
    kb_sdl_to_scancode[SDLK_CARET][1] = 14;
    kb_sdl_to_scancode[SDLK_QUESTION][1] = 62 | (3<<6);
    kb_sdl_to_scancode[SDLK_LESS][1] =  60 | (3<<6);
    kb_sdl_to_scancode[SDLK_GREATER][1] =  61 | (3<<6);
    kb_sdl_to_scancode[SDLK_PLUS][1] = 48 | (3<<6);
    kb_sdl_to_scancode[SDLK_ASTERISK][1] = 061 | (3<<6);
    kb_sdl_to_scancode[SDLK_HASH][1] = 4 | (3<<6);
    kb_sdl_to_scancode[SDLK_EXCLAIM][1] = 2 | (3<<6);
    kb_sdl_to_scancode[SDLK_QUOTEDBL][1] = 3 | (3<<6);
    kb_sdl_to_scancode[SDLK_LEFTBRACKET][0] = 30;
    kb_sdl_to_scancode[SDLK_RIGHTBRACKET][0] = 31;
    kb_sdl_to_scancode[SDLK_LEFTBRACE][1] = 30 | (3<<6);
    kb_sdl_to_scancode[SDLK_RIGHTBRACE][1] = 31 | (3<<6);
    kb_sdl_to_scancode[SDLK_BAR][1] = 32 | (3<<6);
    kb_sdl_to_scancode[SDLK_UNDERSCORE][1] = 013 | (3<<6);
    kb_sdl_to_scancode[SDLK_BACKQUOTE][0] = 015 | (3 << 6);
    kb_sdl_to_scancode[SDLK_TILDE][1] = 016 | (3 << 6);
    kb_sdl_to_scancode[SDLK_EQUALS][0] = 014 | (3<<6);
    kb_sdl_to_scancode[SDLK_BACKSLASH][0] = 32;
    kb_sdl_to_scancode[SDLK_COMMA][0] = 60;
    kb_sdl_to_scancode[SDLK_PERIOD][0] = 61;
    kb_sdl_to_scancode[SDLK_AMPERSAND][1] = 7 | (3<<6);
    kb_sdl_to_scancode[SDLK_QUOTE][0] = 8 | (3 << 6);
#endif // DISPLAY_OSX

	/* Add shifts */
	for (i = 0; i < 256; i++) {
		if (kb_sdl_to_scancode[i][1] == 0)
			kb_sdl_to_scancode[i][1] = kb_sdl_to_scancode[i][0] |
				(3 << 6);
	}

	if (0) printf("kb_sdl_to_scancode[';'][1] = %x\n", kb_sdl_to_scancode[';'][1]);

#if 0   /* Don't do this */
	/* control keys */
	for (i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_sdl_to_scancode[k][2] = i | (3 << 10);
	}

	/* meta keys */
	for (i = 0; i < 64; i++) {
		char k;
		k = kb_old_table[i][0];
		kb_sdl_to_scancode[k][3] = i | (3 << 12);
	}
#endif
}

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

	// #\SHIFT-LOCK
	// #\LEFT-TOP
	// #\LEFT-SHIFT
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
	// #\RIGHT-SHIFT
	// #\RIGHT-TOP

	// #\LEFT-META
	// #\LEFT-CTRL
	{LM_K_SP, LM_K_SP, LM_K_SP},
	// #\RIGHT-CTRL
	// #\RIGHT-META
};

// The second dimension is 200 long and indexed by keycode.
// The first dimension is the shifts:
//  0 unshifted
//  1 shift
//  2 top
//  3 greek
//  4 shift greek
// Elements in the table are 16-bit unsigned numbers.
// Bit 15 on and bit 14 on means undefined code, ignore and beep.
// Bit 15 on and bit 14 off means low bits are shift for bit in KBD-SHIFTS
//    (40 octal for right-hand key of a pair)
// Bit 15 off is ordinary code.
unsigned char kb_new_table[128][4] = {
	// #\BACK-NEXT                           ;100 macro
	// #\ESC                                 ;40 terminal
	// #\QUOTE                               ;120 Quote
	// #\BS                                  ;160 Overstrike
	// #\CLEAR                               ;110 Clear Input
	// #\FORM                                ;50 clear screen
	// #\HOLD-OUTPUT                         ;30 hold output
	// #\STOP-OUTPUT                         ;170 Stop Output
	// #\ABORT                               ;67 Abort
	// #\BREAK                               ;167 Break
	// #\RESUME                              ;47 resume
	// #\CALL                                ;107 Call

	// #\ROMAN-I                             ;101 Roman I
	// #\ROMAN-II                            ;1 Roman II
	// #\SYSTEM                              ;141 system
	// (#/: 14 14)                           ;21 plus-minus
	// (#/1 #/! #/!)                         ;121 One
	// (#/2 #/@ #/@)                         ;61 Two
	// (#/3 #/# #/#)                         ;161 Three
	// (#/4 #/$ #/$)                         ;11 Four
	// (#/5 #/% #/%)                         ;111 Five
	// (#/6 #/^ #/^)                         ;51 Six
	// (#/7 #/& #/&)                         ;151 Seven
	// (#/8 #/* #/*)                         ;31 Eight
	// (#/9 #/( #/( )                        ;71 Nine
	// (#/0 #/) #/))                         ;171 Zero
	// (#/- #/_ #/_)                         ;131 Minus
	// (#/= #/+ #/+)                         ;126 Equals
	// (#/{ 140000 140000)                   ;166 {
	// (#/} 140000 140000)                   ;146 }
	// #\STATUS                              ;46 status
	// #\HAND-UP                             ;106 Up Thumb
	// (#\HAND-DOWN #\HAND-DOWN #\HAND-DOWN #/  #/) ;176 Down Thumb

	// #\ROMAN-III                           ;102 Roman III
	// #\ROMAN-IV                            ;2 Roman IV
	// #\NETWORK                             ;42 network
	// #\TAB                                 ;22 tab
	// (#/q #/Q #/)                        ;122 Q
	// (#/w #/W #/)                        ;62 W
	// (#/e #/E #/ #/)                   ;162 E
	// (#/r #/R #/)                        ;12 R
	// (#/t #/T #/)                        ;112 T
	// (#/y #/Y #/)                        ;52 Y
	// (#/u #/U #/)                        ;152 U
	// (#/i #/I #/)                        ;32 I
	// (#/o #/O #/)                        ;72 O
	// (#/p #/P #/ #/)                   ;172 P
	// (#/( #/[ #/[)                         ;132 Open parenthesis
	// (#/) #/] #/])                         ;137 Close parenthesis
	// (#/` #/~ #/~ #/)                    ;77 back quote
	// (#/\ #/| #/|)                         ;37 Backslash
	// #\DELETE                              ;157 Delete
	// (#\HAND-LEFT #\HAND-LEFT #\HAND-LEFT #/ #/) ;117 Hand Left
	// #\HAND-RIGHT                          ;17 Hand Right

	// 100011                                ;3 Mode lock
	// #/                                  ;143 Alt Mode
	// #\RUBOUT                              ;23 rubout
	// (#/a #/A 140000 #/)                 ;123 A
	// (#/s #/S)                             ;63 S
	// (#/d #/D 140000 12)                   ;163 D/delta
	// (#/f #/F)                             ;13 F
	// (#/g #/G #/ 11)                     ;113 G/gamma
	// (#/h #/H #/)                        ;53 H
	// (#/j #/J #/)                        ;153 J
	// (#/k #/K #/)                        ;33 K
	// (#/l #/L #/ 10)                     ;73 L/lambda
	// (#/; #/: #/:)                         ;173 Semicolon
	// (#/' #/" #/" 0)                       ;133 Apostrophe/center-dot
	// #\CR                                  ;136 Return
	// #\LINE                                ;36 Line
	// #\END                                 ;156 End
	// #\HELP                                ;116 Help

	// 100003                                ;125 Caps Lock
	// 100002                                ;104 Left Top
	// 100001                                ;44 Left Greek
	// 100000                                ;24 Left Shift
	// (#/z #/Z)                             ;124 Z
	// (#/x #/X)                             ;64 X
	// (#/c #/C #/)                        ;164 C
	// (#/v #/V)                             ;14 V
	// (#/b #/B #/ #/)                   ;114 B
	// (#/n #/N #/)                        ;54 N
	// (#/m #/M #/)                        ;154 M
	// (#/, #/< #/<)                         ;34 comma
	// (#/. #/> #/>)                         ;74 period
	// (#// #/? #/? 177)                     ;174 Question/Integral
	// 100040                                ;25 Right Shift
	// 100041                                ;35 Right Greek
	// 100042                                ;155 Right Top
	// ()                                    ;115 Repeat
	// 100008                                ;15 Alt Lock

	// 100007                                ;145 Left Hyper
	// 100006                                ;5 Left super
	// 100005                                ;45 Left Meta
	// 100004                                ;20 Left control
	// #\SP                                  ;134 Space
	// 100044                                ;26 Right control
	// 100045                                ;165 Right Meta
	// 100046                                ;65 Right Super
	// 100047                                ;175 Right Hyper
};

unsigned int kbd_key_scan;

unsigned short okb_to_scancode[256][4];

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
        if (iob_csr & (1 << 5)) // Already something to be read.
                return;

        if (key_queue_free < KEY_QUEUE_LEN) {
                int v = key_queue[key_queue_iptr];
                traceio("dequeue_key_event() - dequeuing 0%o, q len before %d\n", v, KEY_QUEUE_LEN - key_queue_free);
                key_queue_iptr = (key_queue_iptr + 1) % KEY_QUEUE_LEN;
                key_queue_free++;
                kbd_key_scan = (1 << 16) | v;
                if (iob_csr & (1 << 2)) {       // Keyboard interrupt enabled?
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
                queue_key_event(v);     // Already something there, queue this.
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
        memset((char *) okb_to_scancode, 0, sizeof(okb_to_scancode));

        // Walk unshifted old keyboard table.
        for (int i = 0; i < 64; i++) {
                char k;
                k = kb_old_table[i][0];
                okb_to_scancode[(int) k][0] = i;
        }

        // Add shifts.
        for (int i = 0; i < 256; i++)
                okb_to_scancode[i][1] = okb_to_scancode[i][0] | (3 << 6);

        // Modify mapping to match present-day US keyboard layout.
        okb_to_scancode['`'][0] = 015 | (3 << 6);
        okb_to_scancode['`'][1] = 016 | (3 << 6);

        okb_to_scancode['\''][0] = 010 | (3 << 6);
        okb_to_scancode['\''][1] = 3 | (3 << 6);
        okb_to_scancode['='][0] = 014 | (3 << 6);
        okb_to_scancode['2'][1] = 015;

        okb_to_scancode['6'][1] = 016;
        okb_to_scancode['7'][1] = 7 | (3 << 6);
        okb_to_scancode['8'][1] = 061 | (3 << 6);
        okb_to_scancode['9'][1] = 011 | (3 << 6);
        okb_to_scancode['0'][1] = 012 | (3 << 6);
        okb_to_scancode['-'][1] = 013 | (3 << 6);
        okb_to_scancode['='][1] = 060 | (3 << 6);

        okb_to_scancode[';'][1] = 061;
        okb_to_scancode[':'][1] = 061;
        okb_to_scancode['!'][1] = 2 | (3 << 6);
        okb_to_scancode['"'][1] = 3 | (3 << 6);
        okb_to_scancode['#'][1] = 4 | (3 << 6);
        okb_to_scancode['$'][1] = 5 | (3 << 6);
        okb_to_scancode['%'][1] = 6 | (3 << 6);
        okb_to_scancode['&'][1] = 7 | (3 << 6);
        okb_to_scancode['('][1] = 011 | (3 << 6);
        okb_to_scancode[')'][1] = 012 | (3 << 6);
        okb_to_scancode['_'][1] = 013 | (3 << 6);
        okb_to_scancode['~'][1] = 016 | (3 << 6);
        okb_to_scancode['@'][1] = 13;
        okb_to_scancode['^'][1] = 14;

        okb_to_scancode['Q'][1] = 20 | (3 << 6);
        okb_to_scancode['W'][1] = 21 | (3 << 6);
        okb_to_scancode['E'][1] = 22 | (3 << 6);
        okb_to_scancode['R'][1] = 23 | (3 << 6);
        okb_to_scancode['T'][1] = 24 | (3 << 6);
        okb_to_scancode['Y'][1] = 25 | (3 << 6);
        okb_to_scancode['U'][1] = 26 | (3 << 6);
        okb_to_scancode['I'][1] = 27 | (3 << 6);
        okb_to_scancode['O'][1] = 28 | (3 << 6);
        okb_to_scancode['P'][1] = 29 | (3 << 6);
        okb_to_scancode['{'][1] = 30 | (3 << 6);
        okb_to_scancode['}'][1] = 31 | (3 << 6);
        okb_to_scancode['|'][1] = 32 | (3 << 6);

        okb_to_scancode['A'][1] = 39 | (3 << 6);
        okb_to_scancode['S'][1] = 40 | (3 << 6);
        okb_to_scancode['D'][1] = 41 | (3 << 6);
        okb_to_scancode['F'][1] = 42 | (3 << 6);
        okb_to_scancode['G'][1] = 43 | (3 << 6);
        okb_to_scancode['H'][1] = 44 | (3 << 6);
        okb_to_scancode['J'][1] = 45 | (3 << 6);
        okb_to_scancode['K'][1] = 46 | (3 << 6);
        okb_to_scancode['L'][1] = 47 | (3 << 6);
        okb_to_scancode['+'][1] = 48 | (3 << 6);
        okb_to_scancode['*'][1] = 061 | (3 << 6);

        okb_to_scancode['Z'][1] = 53 | (3 << 6);
        okb_to_scancode['X'][1] = 54 | (3 << 6);
        okb_to_scancode['C'][1] = 55 | (3 << 6);
        okb_to_scancode['V'][1] = 56 | (3 << 6);
        okb_to_scancode['B'][1] = 57 | (3 << 6);
        okb_to_scancode['N'][1] = 58 | (3 << 6);
        okb_to_scancode['M'][1] = 59 | (3 << 6);
        okb_to_scancode['<'][1] = 60 | (3 << 6);
        okb_to_scancode['>'][1] = 61 | (3 << 6);
        okb_to_scancode['?'][1] = 62 | (3 << 6);

        // Map Delete to Rubout.
        okb_to_scancode[0x7f][0] = 046;
        okb_to_scancode[0x08][0] = 046;

        // Map Tab to Tab.
        okb_to_scancode[9][0] = 0211;

        // Map Escape to Escape.
        okb_to_scancode[0x1b][0] = 0204;
}

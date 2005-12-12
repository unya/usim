/*
 * keyboard.h
 *
 * $Id$
 */

#ifdef DISPLAY_SDL
#ifdef _WIN32
#include "SDL/SDL_keysym.h"
#else
#include <SDL/SDL_keysym.h>
#endif
#endif /* USE_SDL */

/* keycodes produced by LM keyboard */
#define LM_K_BREAK	0201
#define LM_K_CLEAR	0202
#define LM_K_CALL	0203
#define LM_K_ESC	0204
#define LM_K_BACK	0205
#define LM_K_HELP	0206
#define LM_K_RUBOUT	0207
#define LM_K_CR		0215

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

#define KBD_CONFIG_FILE "keyboard.cfg"

/* all-up shift codes */
#define KB_ALLUP_SHIFT 1
#define KB_ALLUP_GREEK (1<<1)
#define KB_ALLUP_TOP (1<<2)
#define KB_ALLUP_CAPS_LOCK (1<<3)
#define KB_ALLUP_CONTROL (1<<4)
#define KB_ALLUP_META (1<<5)
#define KB_ALLUP_SUPER (1<<6)
#define KB_ALLUP_HYPER (1<<7)
#define KB_ALLUP_ALTLOCK (1<<8)
#define KB_ALLUP_MODELOCK (1<<9)
#define KB_ALLUP_REPEAT (1<<10)

/* Shift codes */
#define KB_SH_LEFT_SHIFT 024
#define KB_SH_LEFT_GREEK 044
#define KB_SH_LEFT_TOP 0104
#define KB_SH_LEFT_CONTROL 020
#define KB_SH_LEFT_META 045
#define KB_SH_LEFT_SUPER 05
#define KB_SH_LEFT_HYPER 0145
#define KB_SH_RIGHT_SHIFT 025
#define KB_SH_RIGHT_GREEK 035
#define KB_SH_RIGHT_TOP 0155
#define KB_SH_RIGHT_CONTROL 026
#define KB_SH_RIGHT_META 0165
#define KB_SH_RIGHT_SUPER 065
#define KB_SH_RIGHT_HYPER 0175
#define KB_SH_CAPSLOCK 0125
#define KB_SH_ALTLOCK 015
#define KB_SH_MODELOCK 03

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

struct lmkey {
  char *lmkey_name;
  unsigned char lmkey_num;
  char lmkey_is_shift;
} lm_key_names[] = {
  { "Left_Shift", 024, 1},
  { "Left_Greek", 044, 1 },
  { "Left_Top", 0104, 1 },
  { "Left_Control", 020, 1 },
  { "Left_Meta", 045, 1 },
  { "Left_Super", 05, 1 },
  { "Left_Hyper", 0145, 1 },
  { "Right_Shift", 025, 1 },
  { "Right_Greek", 035, 1 },
  { "Right_Top", 0155, 1 },
  { "Right_Control", 026, 1 },
  { "Right_Meta", 0165, 1 },
  { "Right_Super", 065, 1 },
  { "Right_Hyper", 0175, 1 },
  { "Caps", 0125, 1 },
  { "Altlock", 015, 1 },
  { "Modelock", 03, 1 },

  { "Altmode", 033 },
  { "Break", 0201  },
  { "Clear_Input", 0202 },
  { "Call", 0203  },
  { "Terminal", 0204  },
  { "Macro", 0205  },
  { "Help", 0206  },
  { "Rubout", 0207  },
  { "Overstrike", 0210  },
  { "Tab", 0211  },
  { "Line", 0212  },
  { "Delete", 0213  },
  { "Page", 0214  },
  { "Clear_Screen", 0214  },
  { "Return", 0215  },
  { "Quote", 0216  },
  { "Hold_Output", 0217  },
  { "Stop_Output", 0220 },
  { "Abort", 0221  },
  { "Resume", 0222  },
  { "Status", 0223  },
  { "End", 0224  },
  { "Roman_I", 0225  },
  { "Roman_II", 0226  },
  { "Roman_III", 0227  },
  { "Roman_IV", 0230  },
  { "Hand_Up", 0231  },
  { "Hand_Down", 0232  },
  { "Hand_Left", 0233  },
  { "Hand_Right", 0234  },
  { "System", 0235  },
  { "Network", 0236 },

  { NULL, -1 }
};  

/* second index in kb_new_table gives which shift must be generated */
#define KBNEW_IX_UNSHIFT 0
#define KBNEW_IX_SHIFT 1
#define KBNEW_IX_TOP 2
#define KBNEW_IX_GREEK 3

/* This is generated from SI:KBD-NEW-TABLE */
/* Key table: key -> (code, shift): */
unsigned char kb_new_table[240][2] = {
    0133, 3,     /* 00: Circle-dot */
    0053, 2,     /* 01: Downarrow */
    0123, 3,     /* 02: Alpha */
    0114, 3,     /* 03: Beta */
    0122, 2,     /* 04: logical And */
    0077, 3,     /* 05: logical Not */
    0162, 3,     /* 06: Epsilon */
    0172, 3,     /* 07: Pi */
    0073, 3,     /* 010: Lambda */
    0113, 3,     /* 011: Gamma */
    0163, 3,     /* 012: Delta */
    0113, 2,     /* 013: Uparrow */
    0021, 1,     /* 014: Plus-minus */
    0176, 3,     /* 015: Circle-plus */
    0032, 2,     /* 016: infinity */
    0172, 2,     /* 017: partial delta */
    0112, 2,     /* 020: left horseshoe (proper subset) */
    0052, 2,     /* 021: right horseshoe (proper superset) */
    0162, 2,     /* 022: up horseshoe (intersection) */
    0012, 2,     /* 023: down horseshoe (union) */
    0152, 2,     /* 024: fowall */
    0072, 2,     /* 025: exists */
    0117, 3,     /* 026: circle-X */
    0073, 2,     /* 027: double arrow (left-right) */
    0153, 2,     /* 030: left arrow */
    0033, 2,     /* 031: right arrow */
    0164, 2,     /* 032: not equal */
    0143, 0,     /* 033: Altmode (lozenge, diamond) */
    0054, 2,     /* 034: less-than-or-equal */
    0154, 2,     /* 035: greater-than-or-equal */
    0114, 2,     /* 036: equivalence (triple bars) */
    0062, 2,     /* 037: logical Or */
    0134, 0,     /* 040: Space */
    0121, 1,     /* 041: ! */
    0133, 1,     /* 042: " */
    0161, 1,     /* 043: # */
    0011, 1,     /* 044: $ */
    0111, 1,     /* 045: % */
    0151, 1,     /* 046: & */
    0133, 0,     /* 047: ' */
//    0132, 0,     /* 050: ( */
    0071, 1,     /* 050: ( */
//    0137, 0,     /* 051: ) */
    0171, 1,     /* 051: ) */
    0031, 1,     /* 052: * */
    0126, 1,     /* 053: + */
    0034, 0,     /* 054: , */
    0131, 0,     /* 055: - */
    0074, 0,     /* 056: . */
    0174, 0,     /* 057: / */
    0171, 0,     /* 060: 0 */
    0121, 0,     /* 061: 1 */
    0061, 0,     /* 062: 2 */
    0161, 0,     /* 063: 3 */
    0011, 0,     /* 064: 4 */
    0111, 0,     /* 065: 5 */
    0051, 0,     /* 066: 6 */
    0151, 0,     /* 067: 7 */
    0031, 0,     /* 070: 8 */
    0071, 0,     /* 071: 9 */
//    0021, 0,     /* 072: : */
    0173, 1,     /* 072: : */
    0173, 0,     /* 073: ; */
    0034, 1,     /* 074: < */
    0126, 0,     /* 075: = */
    0074, 1,     /* 076: > */
    0174, 1,     /* 077: ? */
    0061, 1,     /* 0100: @ */
    0123, 1,     /* 0101: A */
    0114, 1,     /* 0102: B */
    0164, 1,     /* 0103: C */
    0163, 1,     /* 0104: D */
    0162, 1,     /* 0105: E */
    0013, 1,     /* 0106: F */
    0113, 1,     /* 0107: G */
    0053, 1,     /* 0110: H */
    0032, 1,     /* 0111: I */
    0153, 1,     /* 0112: J */
    0033, 1,     /* 0113: K */
    0073, 1,     /* 0114: L */
    0154, 1,     /* 0115: M */
    0054, 1,     /* 0116: N */
    0072, 1,     /* 0117: O */
    0172, 1,     /* 0120: P */
    0122, 1,     /* 0121: Q */
    0012, 1,     /* 0122: R */
    0063, 1,     /* 0123: S */
    0112, 1,     /* 0124: T */
    0152, 1,     /* 0125: U */
    0014, 1,     /* 0126: V */
    0062, 1,     /* 0127: W */
    0064, 1,     /* 0130: X */
    0052, 1,     /* 0131: Y */
    0124, 1,     /* 0132: Z */
    0132, 1,     /* 0133: [ */
    0037, 0,     /* 0134: \ */
    0137, 1,     /* 0135: ] */
    0051, 1,     /* 0136: ^ */
    0131, 1,     /* 0137: _ */
    0077, 0,     /* 0140: ` */
    0123, 0,     /* 0141: a */
    0114, 0,     /* 0142: b */
    0164, 0,     /* 0143: c */
    0163, 0,     /* 0144: d */
    0162, 0,     /* 0145: e */
    0013, 0,     /* 0146: f */
    0113, 0,     /* 0147: g */
    0053, 0,     /* 0150: h */
    0032, 0,     /* 0151: i */
    0153, 0,     /* 0152: j */
    0033, 0,     /* 0153: k */
    0073, 0,     /* 0154: l */
    0154, 0,     /* 0155: m */
    0054, 0,     /* 0156: n */
    0072, 0,     /* 0157: o */
    0172, 0,     /* 0160: p */
    0122, 0,     /* 0161: q */
    0012, 0,     /* 0162: r */
    0063, 0,     /* 0163: s */
    0112, 0,     /* 0164: t */
    0152, 0,     /* 0165: u */
    0014, 0,     /* 0166: v */
    0062, 0,     /* 0167: w */
    0064, 0,     /* 0170: x */
    0052, 0,     /* 0171: y */
    0124, 0,     /* 0172: z */
    0166, 0,     /* 0173: { */
    0037, 1,     /* 0174: | */
    0146, 0,     /* 0175: } */
    0077, 1,     /* 0176: ~ */
    0174, 3,     /* 0177: Integral */
    -1,-1,	 /* 0200: Null */
    0167, 0,     /* 0201: Break */
    0110, 0,     /* 0202: Clear-input */
    0107, 0,     /* 0203: Call */
    0040, 0,     /* 0204: Terminal */
    0100, 0,     /* 0205: Macro */
    0116, 0,     /* 0206: Help */
    0023, 0,     /* 0207: Rubout */
    0160, 0,     /* 0210: Overstrike */
    0022, 0,     /* 0211: Tab */
    0036, 0,     /* 0212: Line */
    0157, 0,     /* 0213: Delete */
    0050, 0,     /* 0214: Page (Clear screen) */
    0136, 0,     /* 0215: Return */
    0120, 0,     /* 0216: Quote */
    0030, 0,     /* 0217: Hold-output */
    0170, 0,     /* 0220: Stop-output */
    0067, 0,     /* 0221: Abort */
    0047, 0,     /* 0222: Resume */
    0046, 0,     /* 0223: Status */
    0156, 0,     /* 0224: End */
    0101, 0,     /* 0225: Roman-i */
    0001, 0,     /* 0226: Roman-ii */
    0102, 0,     /* 0227: Roman-iii */
    0002, 0,     /* 0230: Roman-iv */
    0106, 0,     /* 0231: Hand-up */
    0176, 0,     /* 0232: Hand-down */
    0117, 0,     /* 0233: Hand-left */
    0017, 0,     /* 0234: Hand-right */
    0141, 0,     /* 0235: System */
    0042, 0,     /* 0236: Network */
    -1,-1,	 /* 0237: unused */
};


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

#ifndef USIM_KBD_H
#define USIM_KBD_H

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

extern unsigned char kb_old_table[64][3];
extern unsigned short okb_to_scancode[256][4];

extern void kbd_key_event(int code, int keydown);
extern void kbd_init(void);
extern void kbd_warm_boot_key(void);

extern unsigned int kbd_key_scan;

#endif

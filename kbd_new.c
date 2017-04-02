/*
 * kbd_new.c
 *
 * $Id$
 */

#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "keyboard.h"

#ifndef BUFSIZE
# define BUFSIZE 512
#endif

extern void assert_unibus_interrupt(int v);

/* Map of keys generating shifts */
#define SHIFTPRESS_MAP_LEN 20
static int shiftpress_map[SHIFTPRESS_MAP_LEN][2] = {
  { SDLK_LSHIFT, KB_SH_LEFT_SHIFT },
  { SDLK_RSHIFT, KB_SH_RIGHT_SHIFT },
  { SDLK_LCTRL, KB_SH_LEFT_CONTROL },
  { SDLK_RCTRL, KB_SH_RIGHT_CONTROL },
  { SDLK_LALT, KB_SH_LEFT_META },
  { SDLK_LMETA, KB_SH_LEFT_META },
  { SDLK_RALT, KB_SH_RIGHT_META },
  { SDLK_RMETA, KB_SH_RIGHT_META },
  { SDLK_LSUPER, KB_SH_LEFT_SUPER },
  { SDLK_RSUPER, KB_SH_RIGHT_SUPER },
  { SDLK_MODE, KB_SH_LEFT_GREEK },	/* AltGr = Left Greek */
  { SDLK_MENU, KB_SH_LEFT_TOP },
  { SDLK_COMPOSE, KB_SH_LEFT_TOP },	/* Multi-key compose = Left Top */
  { SDLK_CAPSLOCK, KB_SH_CAPSLOCK },
  { -1, -1 }
};

/* Map of keypresses to lispm chars */
#define KEYPRESS_MAP_LEN SDLK_LAST
static int keypress_map[KEYPRESS_MAP_LEN][2] = {
  { SDLK_F1, LM_K_NETWORK },
  { SDLK_F2, LM_K_SYSTEM },
  { SDLK_F3, LM_K_ABORT },
  { SDLK_F4, LM_K_CLEAR_INPUT },
  { SDLK_F5, LM_K_HELP },
  { SDLK_HELP, LM_K_HELP },
  { SDLK_F6, LM_K_END },
  { SDLK_END, LM_K_END },
  { SDLK_F7, LM_K_CALL },
  { SDLK_BREAK, LM_K_BREAK },
  { SDLK_INSERT, LM_K_RESUME },
  { SDLK_BACKSPACE, LM_K_OVERSTRIKE },
  { SDLK_RETURN, LM_K_RETURN },
  { SDLK_TAB, LM_K_TAB },
  { SDLK_DELETE, LM_K_RUBOUT },
  { SDLK_ESCAPE, LM_K_TERMINAL },
  { SDLK_UP, LM_K_HAND_UP },
  { SDLK_DOWN, LM_K_HAND_DOWN },
  { SDLK_LEFT, LM_K_HAND_LEFT },
  { SDLK_RIGHT, LM_K_HAND_RIGHT },
  { -1, -1 }
};

/* Starting at space, give the shifted chars (on host keyboard) */
#define KBD_SHIFT_MAP_BASE 040
#define KBD_SHIFT_MAP_LEN (0177-KBD_SHIFT_MAP_BASE)
static unsigned char kbd_shift_map[KBD_SHIFT_MAP_LEN] = {
  040, '!', '"', '#', '$', '%', '&',
  '"', '(', ')', '*', '+', '<', '_',
  '>', '?', ')', '!', '@', '#', '$',
  '%', '^', '&', '*', '(', ':', ':',
  '<', '+', '>', '?', '@', 'A', 'B',
  'C', 'D', 'E', 'F', 'G', 'H', 'I',
  'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W',
  'X', 'Y', 'Z', '{', '|', '}', '^',
  '_', '~', 'A', 'B', 'C', 'D', 'E',
  'F', 'G', 'H', 'I', 'J', 'K', 'L',
  'M', 'N', 'O', 'P', 'Q', 'R', 'S',
  'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '{', '|', '}', '~'
};

struct sdl_key_name_to_num {
  char *sdl_key_name;
  int sdl_key_num;
};

struct sdl_key_name_to_num *sdl_key_names;

/* keyboard input queue */

#define IOB_KEY_QUEUE_LEN 10	/* Sufficient? */
static int iob_key_queue[IOB_KEY_QUEUE_LEN];
static int iob_key_queue_optr = 0;
static int iob_key_queue_iptr = 0;
static int iob_key_queue_free = IOB_KEY_QUEUE_LEN;

extern unsigned int iob_key_scan;
extern unsigned int iob_kbd_csr;


void iob_queue_key_event(int ev)
{
  int v = (1<<16) | ev;

  if (iob_key_queue_free > 0) {
#if KBD_DEBUG
    printf("iob_queue_key_event() - queuing 0%o, q len before %d\n", v,
	   IOB_KEY_QUEUE_LEN - iob_key_queue_free);
#endif
    iob_key_queue_free--;
    iob_key_queue[iob_key_queue_optr] = v;
//    iob_key_queue_optr = (iob_key_queue_optr++)%IOB_KEY_QUEUE_LEN;
    iob_key_queue_optr = (iob_key_queue_optr + 1)%IOB_KEY_QUEUE_LEN;
  } else {
    fprintf(stderr,"IOB key queue full!\n");
    if (!(iob_kbd_csr & (1<<5)) && (iob_kbd_csr & (1<<2))) {
      iob_kbd_csr |= 1<<5;
#if KBD_DEBUG
      fprintf(stderr,"iob_queue_key_event generating interrupt\n");
#endif
      assert_unibus_interrupt(0260);
    }
  }
}
void iob_dequeue_key_event()
{
  int v;
  if (iob_kbd_csr & (1<<5))	/* something to be read already */
    return;
  if (iob_key_queue_free < IOB_KEY_QUEUE_LEN) {
    v = iob_key_queue[iob_key_queue_iptr];
#if KBD_DEBUG
    printf("iob_dequeue_key_event() - dequeuing 0%o, q len before %d\n", v,
	   IOB_KEY_QUEUE_LEN - iob_key_queue_free);
#endif
//    iob_key_queue_iptr = (iob_key_queue_iptr++)%IOB_KEY_QUEUE_LEN;
    iob_key_queue_iptr = (iob_key_queue_iptr + 1)%IOB_KEY_QUEUE_LEN;
    iob_key_queue_free++;
    iob_key_scan = (1<<16) | v;
    if (iob_kbd_csr & (1<<2)) {	/* kbd interrupt enabled? */
      iob_kbd_csr |= 1<<5;
#if KBD_DEBUG
      fprintf(stderr,"iob_dequeue_key_event generating interrupt (q len after %d)\n",
	      IOB_KEY_QUEUE_LEN - iob_key_queue_free);
#endif
      assert_unibus_interrupt(0260);
    }
  }
}

void
iob_sdl_key_event(int code, int keydown)
{
  int v = ((!keydown)<<8) | code;
  if (iob_kbd_csr & (1 << 5))
    iob_queue_key_event(v);	/* already something there, queue this */
  else {
    iob_key_scan = (1<<16) | v;
#if KBD_DEBUG
    printf("iob_sdl_key_event() - 0%o\n", iob_key_scan);
#endif
    if (iob_kbd_csr & (1<<2)) {
      iob_kbd_csr |= 1 << 5;
      assert_unibus_interrupt(0260);
    }
  }
}

void
iob_sdl_allup_event(int mods)
{
  int v = (1<<15) | (mods & 01777);
  if (iob_kbd_csr & (1 << 5))
    iob_queue_key_event(v);	/* already something there, queue this */
  else {
    iob_key_scan = (1<<16)|v;
#if KBD_DEBUG
    printf("iob_sdl_key_event() - 0%o\n", iob_key_scan);
#endif
    if (iob_kbd_csr & (1<<2)) {
      iob_kbd_csr |= 1 << 5;
      assert_unibus_interrupt(0260);
    }
  }
}

static void
init_sdl_key_names() {
  int i;
  sdl_key_names = calloc(SDLK_LAST, sizeof(struct sdl_key_name_to_num));

  for (i = SDLK_UNKNOWN; i < SDLK_LAST; i++) {
    char *name = SDL_GetKeyName(i);
    sdl_key_names[i].sdl_key_name = strdup(name);
    sdl_key_names[i].sdl_key_num = i;
  }
}

int find_sdl_key_name(char *name) {
  int i;
  for (i = SDLK_UNKNOWN; i < SDLK_LAST; i++) {
    if ((sdl_key_names[i].sdl_key_name != NULL) &&
	(strcasecmp(sdl_key_names[i].sdl_key_name,name) == 0))
      return sdl_key_names[i].sdl_key_num;
  }
  return -1;
}

int find_lm_key_name(char *name, char is_shift) {
  int i;
  for (i = 0; lm_key_names[i].lmkey_name != NULL; i++) {
    if ((strcasecmp(lm_key_names[i].lmkey_name,name) == 0)
	&& (lm_key_names[i].lmkey_is_shift == is_shift))
      return lm_key_names[i].lmkey_num;
  }
  return -1;
}

char *find_lm_key_name_name(int num, char is_shift) {
  int i;
  for (i = 0; lm_key_names[i].lmkey_name != NULL; i++) {
    if ((lm_key_names[i].lmkey_num == num)
	&& (lm_key_names[i].lmkey_is_shift == is_shift))
      return lm_key_names[i].lmkey_name;
  }
  return NULL;
}

static void
read_kbd_shiftpress_config(FILE *cf, char *buf, int buflen) {
  char *eq;
  int sk, lk, i, max = 0;
  memset(&shiftpress_map, 0, SHIFTPRESS_MAP_LEN);
  shiftpress_map[0][0] = -1;
  while (max < SHIFTPRESS_MAP_LEN && (fgets(buf, buflen, cf) != NULL)) {
    i = strlen(buf);
    if (buf[i-1] == '\n')
      buf[--i] = '\0';
    if (buf[0] == '%' || buf[0] == ';')
      continue;
    else if ((eq = strchr(buf,'=')) != NULL) {
      *eq = '\0';
      if ((sk = find_sdl_key_name(buf)) != -1) {
	lk = find_lm_key_name(eq+1,1);
	if (lk != -1) {
	  shiftpress_map[max][0] = sk;
	  shiftpress_map[max++][1] = lk;
	  shiftpress_map[max][0] = -1;
	  continue;
	} else
	  fprintf(stderr,"Can't find LispM key '%s'\n", eq+1);
      } else
	fprintf(stderr,"Can't find SDL key '%s'\n", buf);
    } else {
      if (i > 0)
	fprintf(stderr,"Bad [ShiftKeys] config line '%s'\n", buf);
      break;
    }
  }
}

static void
read_kbd_keypress_config(FILE *cf, char *buf, int buflen) {
  char *eq;
  int sk, lk, i, max = 0;
  memset(&keypress_map, 0, KEYPRESS_MAP_LEN);
  keypress_map[0][0] = -1;
  while (max < KEYPRESS_MAP_LEN && (fgets(buf, buflen, cf) != NULL)) {
    i = strlen(buf);
    if (buf[i-1] == '\n')
      buf[--i] = '\0';
    if (buf[0] == '%' || buf[0] == ';')
      continue;
    else if ((eq = strchr(buf,'=')) != NULL) {
      *eq = '\0';
      if ((sk = find_sdl_key_name(buf)) != -1) {
	lk = find_lm_key_name(eq+1,0);
	if (lk != -1) {
	  keypress_map[max][0] = sk;
	  keypress_map[max++][1] = lk;
	  keypress_map[max][0] = -1;
	  continue;
	} else
	  fprintf(stderr,"Can't find LispM key '%s'\n", eq+1);
      } else
	fprintf(stderr,"Can't find SDL key '%s'\n", buf);
    } else {
      if (i > 0)
	fprintf(stderr,"Bad [Keys] config line '%s'\n", buf);
      break;
    }
  }
}

static void
read_kbd_keyshift_config(FILE *cf, char *buf, int buflen) {
  unsigned char sk, lk;
  int i, max = 0;
  memset(&kbd_shift_map, 0, KBD_SHIFT_MAP_LEN);
  while (max < KBD_SHIFT_MAP_LEN && (fgets(buf, buflen, cf) != NULL)) {
    i = strlen(buf);
    if (buf[i-1] == '\n')
      buf[--i] = '\0';
    /* Hack: semi and percent are comments only if line can't be a config line */
    if ((buf[0] == '%' || buf[0] == ';') && ((i != 3) || (buf[1] != '=')))
      continue;
    else if (i >= 3 && (buf[1] == '=')) {
      sk = buf[0];
      lk = buf[2];
      if (sk >= KBD_SHIFT_MAP_BASE && (sk < KBD_SHIFT_MAP_LEN + KBD_SHIFT_MAP_BASE))
	kbd_shift_map[sk-KBD_SHIFT_MAP_BASE] = lk;
      else {
	if (i > 0) 
	  fprintf(stderr,"Bad [ShiftMap] config line '%s'\n", buf);
	break;
      }
    }
  }
}

static void
print_kbd_config() {
  int i;
  printf(";;Host key => LispM shift key map\n[ShiftKeys]\n");
  for (i = 0; i < SHIFTPRESS_MAP_LEN && (shiftpress_map[i][0] != -1); i++)
    printf("%s=%s\n", SDL_GetKeyName(shiftpress_map[i][0]),
	   find_lm_key_name_name(shiftpress_map[i][1],1));
  printf("\n;;Host key => LispM key map\n[Keys]\n");
  for (i = 0; i < KEYPRESS_MAP_LEN && (keypress_map[i][0] != -1); i++)
    printf("%s=%s\n", SDL_GetKeyName(keypress_map[i][0]),
	   find_lm_key_name_name(keypress_map[i][1],0));
  printf("\n;;Host key shift map\n[ShiftMap]\n");
  for (i = 0; i < KBD_SHIFT_MAP_LEN; i++)
    printf("%c=%c\n", i+KBD_SHIFT_MAP_BASE, kbd_shift_map[i]);
}

static void
read_kbd_config() {
  FILE *cf;
  int i;
  char buf[BUFSIZE];
  if ((cf = fopen(KBD_CONFIG_FILE,"r")) != NULL) {
    if (fgets(buf, sizeof(buf), cf) != NULL)
      do {
	i = strlen(buf);
	if (buf[i-1] == '\n')
	  buf[--i] = '\0';
	if (buf[0] == '%' || buf[0] == ';')
	  continue;
	else if (strcasecmp(buf,"[ShiftKeys]") == 0) {
	  read_kbd_shiftpress_config(cf,buf,sizeof(buf));
	  continue;
	} else if (strcasecmp(buf,"[Keys]") == 0) {
	  read_kbd_keypress_config(cf,buf,sizeof(buf));
	  continue;
	} else if (strcasecmp(buf,"[ShiftMap]") == 0) {
	  read_kbd_keyshift_config(cf,buf,sizeof(buf));
	  continue;
	} else {
	  if (i > 0)
	    fprintf(stderr,"Unknown keyboard config line: \"%s\"\n", buf);
	  return;
	}
      } while (fgets(buf, sizeof(buf), cf) != NULL);
  }
}

static int
kbd_shiftpress_to_lmcode(SDLKey press)
{
  int i;
  for (i = 0; i < SHIFTPRESS_MAP_LEN && (shiftpress_map[i][0] != -1); i++)
    if (shiftpress_map[i][0] == press)
      return shiftpress_map[i][1];
  return -1;
}

static int
kbd_keypress_to_lmkey(SDLKey press)
{
  int ms, i;
  for (i = 0; i < KEYPRESS_MAP_LEN && (keypress_map[i][0] != -1); i++)
    if (keypress_map[i][0] == press)
      return keypress_map[i][1];
  if ((press >= KBD_SHIFT_MAP_BASE) && (press < 0176)) { /* normal keys */
    ms = SDL_GetModState();
#if KBD_DEBUG
    printf("kbd_keypress_to_lmkey(0%o) => ",press);
#endif
    if (ms & KMOD_SHIFT) {
      press = kbd_shift_map[press-KBD_SHIFT_MAP_BASE];
    }
#if KBD_DEBUG
    printf("0%o\n",press);
#endif
    return press;
  } else
    return -1;
}

static int kbd_shifts = 0; // 1<<KBNEW_IX_UNSHIFT;

void sdl_queue_all_keys_up(void)
{
  kbd_shifts = 0;		/* hmm */
  iob_queue_key_event((1<<15)|0);
}


/*
 * called by sdl.c:sdl_refresh() with key up/down events
 */
void sdl_process_key(SDL_KeyboardEvent *ev, int keydown)
{
  int lmkey = -1;
  int lmcode = -1;
  unsigned char wantshift = 0;


  /* Handle key mapping here, let iob only post the events. */

  /* Check if all keys are up - too expensive? */
  if (!keydown) {
    int statesize, i, allup = 1, mods = 0, shifts = 0;
    Uint8 *state = SDL_GetKeyState(&statesize);
#if KBD_DEBUG
    printf("sdl_process_key: key up - checking %d SDL key states\n", statesize);
#endif
    for (i = 0; allup && i < statesize; i++)
      if (state[i] == 1) {
	switch (kbd_shiftpress_to_lmcode(i)) {
	case KB_SH_LEFT_SHIFT:
	case KB_SH_RIGHT_SHIFT:
	  mods |= KB_ALLUP_SHIFT; /* keep mods for all-up event */
	  shifts |= (1<<KBNEW_IX_SHIFT); /* keep track of shift bits */
	  break;
	case KB_SH_LEFT_GREEK:
	case KB_SH_RIGHT_GREEK:
	  mods |= KB_ALLUP_GREEK;
	  shifts |= (1<<KBNEW_IX_GREEK);
	  break;
	case KB_SH_LEFT_TOP:
	case KB_SH_RIGHT_TOP:
	  mods |= KB_ALLUP_TOP;
	  shifts |= (1<<KBNEW_IX_TOP);
	  break;
	case KB_SH_CAPSLOCK:
	  mods |= KB_ALLUP_CAPS_LOCK; break;
	case KB_SH_LEFT_CONTROL:
	case KB_SH_RIGHT_CONTROL:
	  mods |= KB_ALLUP_CONTROL; break;
	case KB_SH_LEFT_META:
	case KB_SH_RIGHT_META:
	  mods |= KB_ALLUP_META; break;
	case KB_SH_LEFT_SUPER:
	case KB_SH_RIGHT_SUPER:
	  mods |= KB_ALLUP_SUPER; break;
	case KB_SH_LEFT_HYPER:
	case KB_SH_RIGHT_HYPER:
	  mods |= KB_ALLUP_HYPER; break;
	case KB_SH_ALTLOCK:
	  mods |= KB_ALLUP_ALTLOCK; break;
	case KB_SH_MODELOCK:
	  mods |= KB_ALLUP_MODELOCK; break;
	default:
	  /* Hack: ignore modifier key which isn't bound */
	  if (i >= SDLK_NUMLOCK && i <= SDLK_COMPOSE)
	    break;
	  allup = 0;
#if KBD_DEBUG
	  printf("- key %d (%s) is down\n", i, SDL_GetKeyName(i));
#endif
	  break;
	}
      }
    if (allup) {
#if KBD_DEBUG
      printf("sdl_process_keys: all keys up, mods 0%o, shifts 0%o\n",
	     mods, shifts);
#endif
      kbd_shifts = shifts;	/* keep track of shifts */
      iob_sdl_allup_event(mods);	/* generate all-up event */
      return;
    }
  }

  /* Look up keypress in key mapping */
  lmcode = kbd_shiftpress_to_lmcode(ev->keysym.sym);
  if (lmcode != -1) {
#if KBD_DEBUG
    printf("sdl_process_key(0%o, %d) [shift] => 0%o, old shifts 0%o",
	   ev->keysym.sym, keydown, lmcode, kbd_shifts);
#endif
    if (keydown) {
      if ((lmcode == KB_SH_LEFT_GREEK) || (lmcode == KB_SH_RIGHT_GREEK))
	kbd_shifts |= (1<<KBNEW_IX_GREEK);
      else if ((lmcode == KB_SH_LEFT_TOP) || (lmcode == KB_SH_RIGHT_TOP))
	kbd_shifts |= (1<<KBNEW_IX_TOP);
      else if ((lmcode == KB_SH_LEFT_SHIFT) || (lmcode == KB_SH_RIGHT_SHIFT))
	kbd_shifts |= (1<<KBNEW_IX_SHIFT);
    } else {
      if ((lmcode == KB_SH_LEFT_GREEK) || (lmcode == KB_SH_RIGHT_GREEK))
	kbd_shifts &= ~(1<<KBNEW_IX_GREEK);
      else if ((lmcode == KB_SH_LEFT_TOP) || (lmcode == KB_SH_RIGHT_TOP))
	kbd_shifts &= ~(1<<KBNEW_IX_TOP);
      else if ((lmcode == KB_SH_LEFT_SHIFT) || (lmcode == KB_SH_RIGHT_SHIFT))
	kbd_shifts &= ~(1<<KBNEW_IX_SHIFT);
    }
#if 0
    if (kbd_shifts == 0)
      kbd_shifts = 1<<KBNEW_IX_UNSHIFT;
    else if (kbd_shifts != (1<<KBNEW_IX_UNSHIFT))
      kbd_shifts &= ~(1<<KBNEW_IX_UNSHIFT);
#endif
#if KBD_DEBUG
    printf(", new shifts 0%o\n", kbd_shifts);
#endif
    iob_sdl_key_event(lmcode, keydown);
    return;
  } else {
    lmkey = kbd_keypress_to_lmkey(ev->keysym.sym);
    if (lmkey != -1) {
      /* Look up key in kb_new_table. */
      lmcode = kb_new_table[lmkey][0];
      wantshift = kb_new_table[lmkey][1];
#if KBD_DEBUG
      printf("sdl_process_key(0%o, %d) => 0%o, %d (current shifts 0%o)\n",
	     ev->keysym.sym, keydown, lmcode, wantshift, kbd_shifts);
#endif
      /* If modifiers correct, just post the event,
	 else queue the event and post the appropriate shifts.
      */
      if (((wantshift == KBNEW_IX_UNSHIFT) && ((kbd_shifts & (1<<KBNEW_IX_SHIFT)) == 0))
	  || (kbd_shifts & (1<<wantshift)))
	iob_sdl_key_event(lmcode, keydown);
      else {
	/* Messy case: LM wants other shifts than we have pressed */
	int shkey = 0, oshift = kbd_shifts;

	/* All keys up. */
	sdl_queue_all_keys_up();
	/* Press the right key */
	if (wantshift != KBNEW_IX_UNSHIFT) {
	  shkey = (wantshift == KBNEW_IX_SHIFT ?
		   KB_SH_LEFT_SHIFT :
		   (wantshift == KBNEW_IX_TOP ?
		    KB_SH_LEFT_TOP : KB_SH_LEFT_GREEK));
#if KBD_DEBUG
	  printf("sdl_process_key() - simulating key 0%o press\n", shkey);
#endif
	  iob_queue_key_event(shkey);
	}
	/* press/lift the key itself */
	iob_queue_key_event(lmcode|(!keydown)<<8);
	if (shkey) {
#if KBD_DEBUG
	  printf("sdl_process_key() - simulating key 0%o up\n", shkey);
#endif
	  iob_queue_key_event(shkey|1<<8); /* lift the shift */
	}
	/* Re-press the previous keys */
#if KBD_DEBUG
	printf("sdl_process_key() - simulating key re-presses for shifts 0%o\n",
	       oshift);
#endif
	if (oshift & (1<<KBNEW_IX_TOP))
	  iob_queue_key_event(KB_SH_LEFT_TOP);
	if (oshift & (1<<KBNEW_IX_SHIFT))
	  iob_queue_key_event(KB_SH_LEFT_SHIFT);
	if (oshift & (1<<KBNEW_IX_GREEK))
	  iob_queue_key_event(KB_SH_LEFT_GREEK);
	kbd_shifts = oshift;
      }
      return;
    }
    else {
#if KBD_DEBUG
      printf("sdl_process_key: can't find key 0x%x, 0%o, %d.\n",
	     ev->keysym.sym, ev->keysym.sym, ev->keysym.sym);
#endif
#if 0
      beep();
#endif
    }
    return;
  }
  /* Also check for "all keys up" */
  /* Also check for C-M-C-M-Return/Rubout? */
}

void
iob_warm_boot_key()
{
#if 0
	iob_sdl_key_event(LM_K_RETURN, 1);
#else
	iob_key_scan = (1<<16) | ((0)<<8) | LM_K_TAB/*RETURN*/;
	iob_kbd_csr |= 1 << 5;
//	assert_unibus_interrupt(0260);
#endif
}

void
kbd_init()
{
	init_sdl_key_names();

	read_kbd_config();

	if (0) print_kbd_config();
}

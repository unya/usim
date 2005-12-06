/*
 * sdl.c
 * interface to sdl (x-window) video/keyboard emulation
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include "SDL/SDL.h"
#else
#include <SDL/SDL.h>
#endif
//#include <SDL/SDL_image.h>

#include "logo.h"

extern int run_ucode_flag;

#ifndef VIDEO_HEIGHT
# define VIDEO_HEIGHT 896 /* 1024 */
#endif
#ifndef VIDEO_WIDTH
# define VIDEO_WIDTH 768
#endif


static SDL_Surface *screen;
static int video_width = VIDEO_WIDTH;
static int video_height = VIDEO_HEIGHT;

#define MAX_BITMAP_OFFSET	((VIDEO_WIDTH * VIDEO_HEIGHT) / 32)
unsigned int tv_bitmap[MAX_BITMAP_OFFSET];

typedef struct DisplayState {
    unsigned char *data;
    int linesize;
    int depth;
    int width;
    int height;
} DisplayState;

static DisplayState display_state;
static DisplayState *ds = &display_state;

#define MOUSE_EVENT_LBUTTON 1
#define MOUSE_EVENT_MBUTTON 2
#define MOUSE_EVENT_RBUTTON 4

static int old_run_state;

static void sdl_process_key(SDL_KeyboardEvent *ev, int updown)
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
		printf("scancode %x, sym %x, mod %x, modstate %x, extra %x, unicode %x\n",
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

static void sdl_send_mouse_event(void)
{
	int x, y, dx, dy, dz, state, buttons;

	state = SDL_GetRelativeMouseState(&dx, &dy);

	buttons = 0;
	if (state & SDL_BUTTON(SDL_BUTTON_LEFT))
		buttons |= MOUSE_EVENT_LBUTTON;

	if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
		buttons |= MOUSE_EVENT_MBUTTON;

	if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
		buttons |= MOUSE_EVENT_RBUTTON;

	state = SDL_GetMouseState(&x, &y);

	iob_sdl_mouse_event(x, y, dx, dy, buttons);
}

static void sdl_update(DisplayState *ds, int x, int y, int w, int h)
{
    SDL_UpdateRect(screen, x, y, w, h);
}

void
sdl_system_shutdown_request(void)
{
	exit(0);
}

int u_minh = 0x7fffffff, u_maxh, u_minv = 0x7fffffff, u_maxv;

void
accumulate_update(int h, int v, int hs, int vs)
{
	if (h < u_minh) u_minh = h;
	if (h+hs > u_maxh) u_maxh = h+hs;
	if (v < u_minv) u_minv = v;
	if (v+vs > u_maxv) u_maxv = v+vs;
}

void
send_accumulated_updates(void)
{
	int hs, vs;

	hs = u_maxh - u_minh;
	vs = u_maxv - u_minv;
	if (u_minh != 0x7fffffff && u_minv != 0x7fffffff && u_maxh && u_maxv)
		SDL_UpdateRect(screen, u_minh, u_minv, hs, vs);

	u_minh = 0x7fffffff;
	u_maxh = 0;
	u_minv = 0x7fffffff;
	u_maxv = 0;
}

void
sdl_refresh(void)
{
	SDL_Event ev1, *ev = &ev1;
	int mod_state;
	int keysym;

#if 1
	send_accumulated_updates();
#endif

	while (SDL_PollEvent(ev)) {

		switch (ev->type) {
		case SDL_VIDEOEXPOSE:
			sdl_update(ds, 0, 0, screen->w, screen->h);
			break;

		case SDL_KEYDOWN:
			mod_state = (SDL_GetModState() &
				     (KMOD_LSHIFT | KMOD_LCTRL)) ==
				(KMOD_LSHIFT | KMOD_LCTRL);

			if (mod_state) {
				switch(ev->key.keysym.sym) {
				case SDLK_F1 ... SDLK_F12:
					break;
				default:
					break;
				}
			}

			sdl_process_key(&ev->key, 1);
			break;

		case SDL_KEYUP:
			sdl_process_key(&ev->key, 0);
			break;
		case SDL_QUIT:
			sdl_system_shutdown_request();
			break;
		case SDL_MOUSEMOTION:
			sdl_send_mouse_event();
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			SDL_MouseButtonEvent *bev = &ev->button;
			sdl_send_mouse_event();
		}
		break;

		case SDL_ACTIVEEVENT:
			break;
		default:
			break;
		}
	}
}

static void sdl_resize(DisplayState *ds, int w, int h)
{
    int flags;

    flags = SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL;
//    flags |= SDL_RESIZABLE;
//    flags |= SDL_FULLSCREEN;
//    screen = SDL_SetVideoMode(w, h, 0, flags);
//    screen = SDL_SetVideoMode(w, h, 8, flags);
    screen = SDL_SetVideoMode(w, h, 8, flags);

    if (!screen) {
        fprintf(stderr, "Could not open SDL display\n");
        exit(1);
    }

    ds->data = screen->pixels;
    ds->linesize = screen->pitch;
    ds->depth = screen->format->BitsPerPixel;
    ds->width = w;
    ds->height = h;
}

static void sdl_update_caption(void)
{
    char buf[1024];

    strcpy(buf, "CADR");
    if (run_ucode_flag) {
        strcat(buf, " [Running]");
    } else {
        strcat(buf, " [Stopped]");
    }

    SDL_WM_SetCaption(buf, "CADR");
}

static void sdl_cleanup(void) 
{
    SDL_Quit();
}

#define COLOR_WHITE	0xff
#define COLOR_BLACK	0

char video_bow_mode = 1;	/* 1 => White on Black, 0 => Black on White */


void
sdl_set_bow_mode(char new_mode)
{
  unsigned char *p = screen->pixels;
  int i, j;

#if 1
  printf("Setting Black-on-White mode: was %d, setting %d\n",
	 video_bow_mode, new_mode);
#endif
  if (video_bow_mode == new_mode)
    return;

  /* Need to complement it */
  video_bow_mode = new_mode;

  for (i = 0; i < video_width; i++)
    for (j = 0; j < video_height; j++) {
      *p = ~*p;
      p++;
    }

  SDL_UpdateRect(screen, 0, 0, video_width, video_height);
}

void
sdl_setup_display(void)
{
	SDL_Surface *logo;
	unsigned char *p = screen->pixels;
	int i, j;

	for (i = 0; i < video_width; i++) {
		for (j = 0; j < video_height; j++)
			*p++ = COLOR_WHITE;
	}

#if 0
	logo = IMG_ReadXPMFromArray(logo_xpm);
	SDL_BlitSurface(logo, NULL, screen, NULL);
#else
	{
		char *p;
		unsigned char *ps = screen->pixels;
		for (j = 0; j < 116; j++) {
			p = logo_xpm[3+j];
			for (i = 0; i < 432; i++) {
				if (p[i] != '.')
					ps[i] = COLOR_BLACK;
			}
			ps += video_width;
		}
	}
#endif

	SDL_UpdateRect(screen, 0, 0, video_width, video_height);
}

void
video_read(int offset, unsigned int *pv)
{
	*pv = 0;

	/* the real h/w has memory for 768x1024 */
	if (offset < MAX_BITMAP_OFFSET)
		*pv = tv_bitmap[offset];
#if 0
	if (screen) {
		unsigned char *ps = screen->pixels;
		unsigned long bits;
		int i, h, v, n;

//		offset /= 2;
//		offset *= 16;
		offset *= 32;

		v = offset / video_width;
		h = offset % video_width;

		if (offset > video_width*video_height) {
			printf("video: video_read past end; offset %o\n",
			       offset);
			*pv = 0;
			return;
		}

#if 0
		if (v >= video_height) {
			v -= 400;
			offset = v*video_width + h;
		}
#endif

		bits = 0;
		for (i = 0; i < 32; i++)
		{
			if (ps[offset + i] == COLOR_WHITE)
				bits |= 1 << i;
		}

		*pv = bits;
	}
#endif
}

void
video_write(int offset, unsigned int bits)
{
	if (screen) {
		unsigned char *ps = screen->pixels;
		int i, h, v, n;

		tv_bitmap[offset] = bits;

//		offset /= 2;
//		offset *= 16;
		offset *= 32;

		v = offset / video_width;
		h = offset % video_width;

		if (0) printf("v,h %d,%d <- %o (offset %d)\n", v, h, bits, offset);

#if 0
		if (v != 895) {
			printf("bits %3d %3d ", v, h);
			for (i = 0; i < 32; i++)
				if (bits & (1 << (31-i)))
					printf("1");
				else
					printf(".");
			printf("\n");
		}
#endif


#if 0
//		if (v >= video_height) return;
		if (v >= video_height) {
			v -= 400;
			offset = v*video_width + h;
		}
#endif

		for (i = 0; i < 32; i++)
		{
			ps[offset + i] =
//				(bits & 1) ? COLOR_BLACK : COLOR_WHITE;
				(bits & 1) ? COLOR_WHITE : COLOR_BLACK;
			if (video_bow_mode == 0)
			  ps[offset + i] ^= ~0;
			bits >>= 1;
		}

#if 0
		SDL_UpdateRect(screen, h, v, 32, 1);
#else
		accumulate_update(h, v, 32, 1);
#endif
	}
}

static void sdl_display_init(void)
{
    int flags;

    flags = SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE;

    if (SDL_Init(flags)) {
        fprintf(stderr, "SDL initialization failed\n");
        exit(1);
    }

    /* NOTE: we still want Ctrl-C to work - undo the SDL redirections*/
    signal(SIGINT, SIG_DFL);
#ifdef linux
    signal(SIGQUIT, SIG_DFL);
#endif

    sdl_resize(ds, video_width, video_height);

    sdl_update_caption();

    SDL_EnableKeyRepeat(250, 50);
//    SDL_EnableUNICODE(1);

    sdl_setup_display();

    SDL_ShowCursor(0);

    atexit(sdl_cleanup);
}

int
display_init(void)
{
	sdl_display_init();
	return 0;
}

void
display_poll(void)
{
	sdl_refresh();

	if (old_run_state != run_ucode_flag) {
		old_run_state = run_ucode_flag;
		sdl_update_caption();
	}
}


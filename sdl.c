/*
 * sdl.c
 * interface to sdl (x-window) video/keyboard emulation
 *
 * $Id$
 */

#include <stdio.h>
#include <signal.h>

#include <SDL/SDL.h>
//#include <SDL/SDL_image.h>

#include "logo.h"

static SDL_Surface *screen;
static int video_width = 768;
static int video_height = 512/*1024*/;

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
#define MOUSE_EVENT_RBUTTON 3

static void sdl_process_key(SDL_KeyboardEvent *ev)
{
	int mod_state, extra;

	mod_state = SDL_GetModState();

	extra = 0;
	if (mod_state & (KMOD_LMETA | KMOD_RMETA))
		extra |= 3 << 12;

	if (mod_state & (KMOD_LSHIFT | KMOD_RSHIFT))
		extra |= 3 << 6;

	if (mod_state & (KMOD_LCTRL | KMOD_RCTRL))
		extra |= 3 << 10;

	iob_sdl_key_event(ev->keysym.sym, extra);
}

static void sdl_send_mouse_event(void)
{
	int dx, dy, dz, state, buttons;

	state = SDL_GetRelativeMouseState(&dx, &dy);

	buttons = 0;
	if (state & SDL_BUTTON(SDL_BUTTON_LEFT))
		buttons |= MOUSE_EVENT_LBUTTON;

	if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
		buttons |= MOUSE_EVENT_RBUTTON;

	if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
		buttons |= MOUSE_EVENT_MBUTTON;

	iob_sdl_mouse_event(dx, dy, buttons);
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

void
sdl_refresh(void)
{
	SDL_Event ev1, *ev = &ev1;
	int mod_state;
	int keysym;

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

			sdl_process_key(&ev->key);
			break;

		case SDL_KEYUP:
			sdl_process_key(&ev->key);
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
    extern int run_ucode_flag;

    strcpy(buf, "CADR");
    if (!run_ucode_flag) {
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

void
sdl_blah(void)
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
	if (screen) {
		unsigned char *ps = screen->pixels;
		unsigned long bits;
		int i, h, v, n;

//		offset /= 2;
//		offset *= 16;
		offset *= 32;

		v = offset / video_width;
		h = offset % video_width;

		if (v >= video_height) {
			v -= 400;
			offset = v*video_width + h;
		}

		bits = 0;
		for (i = 0; i < 32; i++)
		{
			if (ps[offset + i] == COLOR_WHITE)
				bits |= 1 << i;
		}

		*pv = bits;
	}
}

void
video_write(int offset, unsigned int bits)
{
	if (screen) {
		unsigned char *ps = screen->pixels;
		int i, h, v, n;

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


//		if (v >= video_height) return;
		if (v >= video_height) {
			v -= 400;
			offset = v*video_width + h;
		}

		for (i = 0; i < 32; i++)
		{
			ps[offset + i] =
//				(bits & 1) ? COLOR_BLACK : COLOR_WHITE;
				(bits & 1) ? COLOR_WHITE : COLOR_BLACK;
			bits >>= 1;
		}

		SDL_UpdateRect(screen, h, v, 32, 1);
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
    signal(SIGQUIT, SIG_DFL);

    sdl_resize(ds, video_width, video_height);

    sdl_update_caption();

    SDL_EnableKeyRepeat(250, 50);
//    SDL_EnableUNICODE(1);

    sdl_blah();

    atexit(sdl_cleanup);
}

int
display_init(void)
{
	sdl_display_init();
}

void
display_poll(void)
{
	sdl_refresh();
}


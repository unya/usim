#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

extern int run_ucode_flag;
extern void iob_key_event(int code, int extra);
extern void iob_mouse_event(int x, int y, int dx, int dy, int buttons);

static unsigned int video_width = 768;
static unsigned int video_height = 897 /*1024*/;

unsigned int tv_bitmap[(768 * 1024)];

typedef struct DisplayState {
	unsigned char *data;
	int linesize;
	int depth;
	int width;
	int height;
} DisplayState;

static DisplayState display_state;
static DisplayState *ds = &display_state;

Display *display;
Window window;
static int bitmap_order;
static int color_depth;
static Visual *visual = NULL;
static GC gc;

static XImage *ximage;

#define USIM_EVENT_MASK ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | KeyReleaseMask

#define MOUSE_EVENT_LBUTTON 1
#define MOUSE_EVENT_MBUTTON 2
#define MOUSE_EVENT_RBUTTON 4

#define X_SHIFT            1
#define X_CAPS             2
#define X_CTRL             4
#define X_ALT              8
#define X_META            16

unsigned long Black, White;

static int old_run_state;
static XComposeStatus status;

static void x11_process_key(XEvent *e, int updown)
{
	KeySym keysym;
	unsigned char buffer[5];
	int extra, ret;

	extra = 0;
	if (e->xkey.state & X_META)
		extra |= 3 << 12;

	if (e->xkey.state & X_ALT)
		extra |= 3 << 12;

	if (e->xkey.state & X_SHIFT)
		extra |= 3 << 6;

	if (e->xkey.state & X_CAPS)
		extra ^= 3 << 6;

	if (e->xkey.state & X_CTRL)
		extra |= 3 << 10;

	if (updown) {
		ret = XLookupString(&e->xkey, (char *) buffer, 5, &keysym,
				    &status);
		iob_key_event(keysym, extra);
	}
}

void
display_poll(void)
{
	XEvent e;
	int mod_state;
	int keysym;
	void send_accumulated_updates(void);

	send_accumulated_updates();

	while (XCheckWindowEvent(display, window, USIM_EVENT_MASK, &e)) {

		switch (e.type) {
		case Expose:
			XPutImage(display, window, gc, ximage, 0, 0, 0, 0,
				  video_width, video_height);
			XFlush(display);
			break;

		case KeyPress:
			x11_process_key(&e, 1);
			break;

		case KeyRelease:
			x11_process_key(&e, 0);
			break;

		case MotionNotify:
		case ButtonPress:
		case ButtonRelease:
			iob_mouse_event(e.xbutton.x, e.xbutton.y, 0, 0,
					    e.xbutton.button);
			break;

		default:
			break;
		}
	}

	if (old_run_state != run_ucode_flag) {
		old_run_state = run_ucode_flag;
		/*update_caption();*/
	}
}

void
video_read(int offset, unsigned int *pv)
{
	if (ximage) {
		unsigned long bits;
		int i, h, v, n;

		offset *= 32;

		v = offset / video_width;
		h = offset % video_width;

		if (offset > video_width*video_height) {
			if (1) printf("video: video_read past end; "
				      "offset %o\n", offset);
			*pv = 0;
			return;
		}

		bits = 0;
		for (i = 0; i < 32; i++)
		{
			if (tv_bitmap[offset + i] == Black)
				bits |= 1 << i;
		}

		*pv = bits;
	}
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
	{
		XPutImage(display, window, gc, ximage, u_minh, u_minv, u_minh, u_minv, hs, vs);
		XFlush(display);
	}

	u_minh = 0x7fffffff;
	u_maxh = 0;
	u_minv = 0x7fffffff;
	u_maxv = 0;
}

void
video_write(int offset, unsigned int bits)
{
	if (ximage) {
		int i, h, v, n;

		offset *= 32;

		v = offset / video_width;
		h = offset % video_width;

		for (i = 0; i < 32; i++)
		{
			tv_bitmap[offset + i] =
				(bits & 1) ? Black : White;
			bits >>= 1;
		}

		accumulate_update(h, v, 32, 1);
	}
}

int
display_init(void)
{
	char *displayname;
	unsigned long bg_pixel = 0L;
	int pad = 0;
	int xscreen;
	Window root;
	XEvent e;
	XGCValues gcvalues;
	XSetWindowAttributes attr;
	XSizeHints *size_hints;
	XTextProperty windowName;
	XTextProperty *pWindowName = &windowName;
	XTextProperty iconName;
	XTextProperty *pIconName = &iconName;
	XWMHints *wm_hints;

	char *window_name = (char *)"CADR Emulator";
	char *icon_name = (char *)"CADR";

	displayname = getenv("DISPLAY");

	display = XOpenDisplay(displayname);

	if (display == NULL) {
		fprintf(stderr, "usim: failed to open display.\n");
		exit(-1);
	}

	bitmap_order = BitmapBitOrder(display);
	xscreen = DefaultScreen(display);
	color_depth = DisplayPlanes(display, xscreen);

	Black = BlackPixel(display, xscreen);
	White = WhitePixel(display, xscreen);
	root = RootWindow(display, xscreen);

	attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

	window = XCreateWindow(display, root, 0, 0, video_width, video_height,
			       0, color_depth, InputOutput, visual,
			       CWBorderPixel |  CWEventMask, &attr);

	if (window == None) {
		fprintf(stderr, "usim: failed to open window.\n");
		exit(-1);
	}

	if (!XStringListToTextProperty((char **) &window_name, 1, pWindowName)) {
		pWindowName = NULL;
	}

	if (!XStringListToTextProperty((char **) &icon_name, 1, pIconName)) {
		pIconName = NULL;
	}

	if ((size_hints = XAllocSizeHints()) != NULL) {

		/*
		 * The window will not be resizable:
		 */

		size_hints->flags = PMinSize | PMaxSize;
		size_hints->min_width = size_hints->max_width = video_width;
		size_hints->min_height = size_hints->max_height = video_height;
	}

	if ((wm_hints = XAllocWMHints()) != NULL) {
		wm_hints->initial_state = NormalState;
		wm_hints->input = True;
		wm_hints->flags = StateHint | InputHint;
	}

	XSetWMProperties(display, window, pWindowName, pIconName, NULL, 0,
			 size_hints, wm_hints, NULL);

	XMapWindow(display, window);

	gc = XCreateGC(display, window, 0, &gcvalues);

	/*
	 * Fill window with the specified background color:
	 */

	bg_pixel = 0;
	XSetForeground(display, gc, bg_pixel);
	XFillRectangle(display, window, gc, 0, 0, video_width, video_height);

	/*
	 * Wait for first Expose event to do any drawing, then flush:
	 */

	do {
		XNextEvent(display, &e);
	}
	while (e.type != Expose || e.xexpose.count);

	XFlush(display);

	ximage = XCreateImage(display, visual, (unsigned)color_depth, ZPixmap, 0,
			      (char *) tv_bitmap, video_width, video_height, 32, 0);
	ximage->byte_order = LSBFirst;
	return 0;
}

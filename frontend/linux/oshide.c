/*
 * (C) Gražvydas "notaz" Ignotas, 2009-2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <linux/kd.h>

#define PFX "oshide: "

#define FPTR(f) typeof(f) * p##f
#define FPTR_LINK(xf, dl, f) { \
	xf.p##f = dlsym(dl, #f); \
	if (xf.p##f == NULL) { \
		fprintf(stderr, "missing symbol: %s\n", #f); \
		goto fail; \
	} \
}

struct xfuncs {
FPTR(XCreateBitmapFromData);
FPTR(XCreatePixmapCursor);
FPTR(XFreePixmap);
FPTR(XOpenDisplay);
FPTR(XDisplayName);
FPTR(XCloseDisplay);
FPTR(XCreateSimpleWindow);
FPTR(XChangeWindowAttributes);
FPTR(XSelectInput);
FPTR(XMapWindow);
FPTR(XNextEvent);
FPTR(XCheckTypedEvent);
FPTR(XUnmapWindow);
FPTR(XGrabKeyboard);
};


static Cursor transparent_cursor(struct xfuncs *xf, Display *display, Window win)
{
	Cursor cursor;
	Pixmap pix;
	XColor dummy;
	char d = 0;

	memset(&dummy, 0, sizeof(dummy));
	pix = xf->pXCreateBitmapFromData(display, win, &d, 1, 1);
	cursor = xf->pXCreatePixmapCursor(display, pix, pix,
			&dummy, &dummy, 0, 0);
	xf->pXFreePixmap(display, pix);
	return cursor;
}

static void *x11h_handler(void *arg)
{
	struct xfuncs xf;
	unsigned int display_width, display_height;
	XSetWindowAttributes attributes;
	Window win;
	XEvent report;
	Display *display;
	Visual *visual;
	void *x11lib;
	int screen;

	memset(&xf, 0, sizeof(xf));
	x11lib = dlopen("libX11.so.6", RTLD_LAZY);
	if (x11lib == NULL) {
		fprintf(stderr, "libX11.so load failed:\n%s\n", dlerror());
		goto fail;
	}
	FPTR_LINK(xf, x11lib, XCreateBitmapFromData);
	FPTR_LINK(xf, x11lib, XCreatePixmapCursor);
	FPTR_LINK(xf, x11lib, XFreePixmap);
	FPTR_LINK(xf, x11lib, XOpenDisplay);
	FPTR_LINK(xf, x11lib, XDisplayName);
	FPTR_LINK(xf, x11lib, XCloseDisplay);
	FPTR_LINK(xf, x11lib, XCreateSimpleWindow);
	FPTR_LINK(xf, x11lib, XChangeWindowAttributes);
	FPTR_LINK(xf, x11lib, XSelectInput);
	FPTR_LINK(xf, x11lib, XMapWindow);
	FPTR_LINK(xf, x11lib, XNextEvent);
	FPTR_LINK(xf, x11lib, XCheckTypedEvent);
	FPTR_LINK(xf, x11lib, XUnmapWindow);
	FPTR_LINK(xf, x11lib, XGrabKeyboard);

	//XInitThreads();

	display = xf.pXOpenDisplay(NULL);
	if (display == NULL)
	{
		fprintf(stderr, "cannot connect to X server %s, X handling disabled.\n",
				xf.pXDisplayName(NULL));
		goto fail2;
	}

	visual = DefaultVisual(display, 0);
	if (visual->class != TrueColor)
		fprintf(stderr, PFX "warning: non true color visual\n");

	printf(PFX "X vendor: %s, rel: %d, display: %s, protocol ver: %d.%d\n", ServerVendor(display),
		VendorRelease(display), DisplayString(display), ProtocolVersion(display),
		ProtocolRevision(display));

	screen = DefaultScreen(display);

	display_width = DisplayWidth(display, screen);
	display_height = DisplayHeight(display, screen);
	printf(PFX "display is %dx%d\n", display_width, display_height);

	win = xf.pXCreateSimpleWindow(display,
			RootWindow(display, screen),
			0, 0, display_width, display_height, 0,
			BlackPixel(display, screen),
			BlackPixel(display, screen));

	attributes.override_redirect = True;
	attributes.cursor = transparent_cursor(&xf, display, win);
	xf.pXChangeWindowAttributes(display, win, CWOverrideRedirect | CWCursor, &attributes);

	xf.pXSelectInput(display, win, ExposureMask | FocusChangeMask | KeyPressMask | KeyReleaseMask);
	xf.pXMapWindow(display, win);
	xf.pXGrabKeyboard(display, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
	// XSetIOErrorHandler

	while (1)
	{
		xf.pXNextEvent(display, &report);
		switch (report.type)
		{
			case Expose:
				while (xf.pXCheckTypedEvent(display, Expose, &report))
					;
				break;

			case FocusOut:
				// XFocusChangeEvent
				// printf("focus out\n");
				// xf.pXUnmapWindow(display, win);
				break;

			case KeyPress:
				// printf("press %d\n", report.xkey.keycode);
				break;

			default:
				break;
		}
	}

fail2:
	dlclose(x11lib);
fail:
	fprintf(stderr, "x11 handling disabled.\n");
	return NULL;
}

static struct termios g_kbd_termios_saved;
static int g_kbdfd;

static void hidecon_start(void)
{
	struct termios kbd_termios;
	int mode;

	g_kbdfd = open("/dev/tty", O_RDWR);
	if (g_kbdfd == -1) {
		perror(PFX "open /dev/tty");
		return;
	}

	if (ioctl(g_kbdfd, KDGETMODE, &mode) == -1) {
		perror(PFX "(not hiding FB): KDGETMODE");
		goto fail;
	}

	if (tcgetattr(g_kbdfd, &kbd_termios) == -1) {
		perror(PFX "tcgetattr");
		goto fail;
	}

	g_kbd_termios_saved = kbd_termios;
	kbd_termios.c_lflag &= ~(ICANON | ECHO); // | ISIG);
	kbd_termios.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
	kbd_termios.c_cc[VMIN] = 0;
	kbd_termios.c_cc[VTIME] = 0;

	if (tcsetattr(g_kbdfd, TCSAFLUSH, &kbd_termios) == -1) {
		perror(PFX "tcsetattr");
		goto fail;
	}

	if (ioctl(g_kbdfd, KDSETMODE, KD_GRAPHICS) == -1) {
		perror(PFX "KDSETMODE KD_GRAPHICS");
		tcsetattr(g_kbdfd, TCSAFLUSH, &g_kbd_termios_saved);
		goto fail;
	}

	return;

fail:
	close(g_kbdfd);
	g_kbdfd = -1;
}

static void hidecon_end(void)
{
	if (g_kbdfd < 0)
		return;

	if (ioctl(g_kbdfd, KDSETMODE, KD_TEXT) == -1)
		perror(PFX "KDSETMODE KD_TEXT");

	if (tcsetattr(g_kbdfd, TCSAFLUSH, &g_kbd_termios_saved) == -1)
		perror(PFX "tcsetattr");

	close(g_kbdfd);
	g_kbdfd = -1;
}

int oshide_init(void)
{
	pthread_t tid;
	int ret;

	ret = pthread_create(&tid, NULL, x11h_handler, NULL);
	if (ret != 0) {
		fprintf(stderr, PFX "failed to create thread: %d\n", ret);
		return ret;
	}
	pthread_detach(tid);

	hidecon_start();

	return 0;
}

void oshide_finish(void)
{
	/* XXX: the X thread.. */

	hidecon_end();
}

#if 0
int main()
{
	x11h_init();
	sleep(5);
}
#endif

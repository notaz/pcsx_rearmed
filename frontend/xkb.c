/*
 * Copyright (c) 2009, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include "main.h"
#include "plugin_lib.h"

static const struct {
	uint16_t xkey, psxkey;
} keymap[] = {
	{ XK_Left,	DKEY_LEFT },
	{ XK_Right,	DKEY_RIGHT },
	{ XK_Up,	DKEY_UP },
	{ XK_Down,	DKEY_DOWN },
	{ XK_z,		DKEY_CROSS },
	{ XK_s,		DKEY_SQUARE },
	{ XK_x,		DKEY_CIRCLE },
	{ XK_d,		DKEY_TRIANGLE },
	{ XK_w,		DKEY_L1 },
	{ XK_r,		DKEY_R1 },
	{ XK_e,		DKEY_L2 },
	{ XK_t,		DKEY_R2 },
	{ XK_c,		DKEY_SELECT },
	{ XK_v,		DKEY_START },
};

static Atom wmprotocols, wmdelwindow;
static int initialized;

static void InitKeyboard(void) {
	Display *disp = (Display *)gpuDisp;
	if (disp == NULL) {
		fprintf(stderr, "xkb: null display\n");
		exit(1);
	}

	wmprotocols = XInternAtom(disp, "WM_PROTOCOLS", 0);
	wmdelwindow = XInternAtom(disp, "WM_DELETE_WINDOW", 0);

	XkbSetDetectableAutoRepeat(disp, 1, NULL);
}

static void DestroyKeyboard(void) {
	Display *disp = (Display *)gpuDisp;
	if (disp)
		XkbSetDetectableAutoRepeat(disp, 0, NULL);
}

int x11_update_keys(void) {
	uint8_t					i;
	XEvent					evt;
	XClientMessageEvent		*xce;
	uint16_t				Key;
	static int keystate_x11;
	int psxkey, leave = 0;
	Display *disp = (Display *)gpuDisp;

	if (!disp)
		return 0;

	if (!initialized) {
		initialized++;
		InitKeyboard();
	}

	while (XPending(disp)) {
		XNextEvent(disp, &evt);
		switch (evt.type) {
			case KeyPress:
			case KeyRelease:
				Key = XLookupKeysym((XKeyEvent *)&evt, 0);
				//printf("%s %x\n", evt.type == KeyPress ? "press" : "rel  ", Key);
				psxkey = -1;
				for (i = 0; i < ARRAY_SIZE(keymap); i++) {
					if (keymap[i].xkey == Key) {
						psxkey = keymap[i].psxkey;
						break;
					}
				}

				if (psxkey >= 0) {
					if (evt.type == KeyPress)
						keystate_x11 |= 1 << psxkey;
					else
						keystate_x11 &= ~(1 << psxkey);
				}
				if (evt.type == KeyPress && Key == XK_Escape)
					leave = 1;
				break;

			case ClientMessage:
				xce = (XClientMessageEvent *)&evt;
				if (xce->message_type == wmprotocols && (Atom)xce->data.l[0] == wmdelwindow)
					leave = 1;
				break;
		}
	}

	if (leave) {
		DestroyKeyboard();
		exit(1);
	}

	return keystate_x11;
}

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

#include "../frontend/main.h"
#include "../frontend/plugin_lib.h"

static Atom wmprotocols, wmdelwindow;
static int initialized;



static void InitKeyboard(void) {
	Display *disp = (Display *)gpuDisp;
	if (disp){
		wmprotocols = XInternAtom(disp, "WM_PROTOCOLS", 0);
		wmdelwindow = XInternAtom(disp, "WM_DELETE_WINDOW", 0);
		XkbSetDetectableAutoRepeat(disp, 1, NULL);
	}
}

static void DestroyKeyboard(void) {
	Display *disp = (Display *)gpuDisp;
	if (disp)
		XkbSetDetectableAutoRepeat(disp, 0, NULL);
}
#include "maemo_common.h"

int maemo_x11_update_keys() {

	XEvent					evt;
	XClientMessageEvent		*xce;
	int leave = 0;
	Display *disp = (Display *)gpuDisp;
	
	if (!disp)
		return 0;
		
	if (!initialized) {
		initialized++;
		InitKeyboard();
	}

	while (XPending(disp)>0) {
		XNextEvent(disp, &evt);
		switch (evt.type) {
			case KeyPress:
			case KeyRelease:
				key_press_event(evt.xkey.keycode, evt.type==KeyPress ? 1 : (evt.type==KeyRelease ? 2 : 0) );
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

	return 0;
}

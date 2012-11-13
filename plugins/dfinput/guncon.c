/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <string.h>
#include "main.h"

static unsigned char buf[8];

unsigned char PADpoll_guncon(unsigned char value)
{
	if (CurByte == 0) {
		CurCmd = value;
		CurByte++;
		return 0x63;	// regardless of cmd
	}

	if (CurCmd != 0x42 || CurByte >= 8)
		return 0xff;	// verified

	return buf[CurByte++];
}

unsigned char PADstartPoll_guncon(int pad)
{
	int x, y, xn = 0, yn = 0, in = 0, xres = 256, yres = 240;
	CurByte = 0;

	buf[2] = buf[3] = 0xff;
	pl_update_gun(&xn, &yn, &xres, &yres, &in);

	// while y = const + line counter, what is x?
	// for 256 mode, hw dumped offsets x, y: 0x5a, 0x20
	//x = 0x5a + (356 * xn >> 10);
	x = 0x5a - (xres - 256) / 3 + (((xres - 256) / 3 + 356) * xn >> 10);
	y = 0x20 + (yres * yn >> 10);

	if (in & GUNIN_TRIGGER)
		buf[3] &= ~0x20;
	if (in & GUNIN_BTNA)
		buf[2] &= ~0x08;
	if (in & GUNIN_BTNB)
		buf[3] &= ~0x40;
	if (in & GUNIN_TRIGGER2) {
		buf[3] &= ~0x20;
		x = 1;
		y = 10;
	}
	buf[4] = x;
	buf[5] = x >> 8;
	buf[6] = y;
	buf[7] = y >> 8;

	return 0xff;
}

void guncon_init(void)
{
	memset(buf, 0xff, sizeof(buf));
	buf[1] = 0x5a;
}


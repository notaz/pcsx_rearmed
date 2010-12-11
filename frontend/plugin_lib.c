/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "linux/fbdev.h"
#include "common/fonts.h"
#include "common/input.h"
#include "omap.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"

void *pl_fbdev_buf;
int keystate;
static int pl_fbdev_w;

int pl_fbdev_init(void)
{
	pl_fbdev_buf = vout_fbdev_flip(layer_fb);
	return 0;
}

int pl_fbdev_set_mode(int w, int h, int bpp)
{
	void *ret;

	pl_fbdev_w = w;

	vout_fbdev_clear(layer_fb);
	ret = vout_fbdev_resize(layer_fb, w, h, bpp, 0, 0, 0, 0, 3);
	if (ret == NULL)
		fprintf(stderr, "failed to set mode\n");
	else
		pl_fbdev_buf = ret;

	return (ret != NULL) ? 0 : -1;
}

void *pl_fbdev_flip(void)
{
	/* doing input here because the pad is polled
	 * thousands of times for some reason */
	int actions[IN_BINDTYPE_COUNT] = { 0, };

	in_update(actions);
	if (actions[IN_BINDTYPE_EMU] & PEV_MENU)
		stop = 1;
	keystate = actions[IN_BINDTYPE_PLAYER12];

	// let's flip now
	pl_fbdev_buf = vout_fbdev_flip(layer_fb);
	return pl_fbdev_buf;
}

void pl_fbdev_finish(void)
{
}

static void pl_text_out16_(int x, int y, const char *text)
{
	int i, l, len = strlen(text), w = pl_fbdev_w;
	unsigned short *screen = (unsigned short *)pl_fbdev_buf + x + y * w;
	unsigned short val = 0xffff;

	for (i = 0; i < len; i++, screen += 8)
	{
		for (l = 0; l < 8; l++)
		{
			unsigned char fd = fontdata8x8[text[i] * 8 + l];
			unsigned short *s = screen + l * w;
			if (fd&0x80) s[0] = val;
			if (fd&0x40) s[1] = val;
			if (fd&0x20) s[2] = val;
			if (fd&0x10) s[3] = val;
			if (fd&0x08) s[4] = val;
			if (fd&0x04) s[5] = val;
			if (fd&0x02) s[6] = val;
			if (fd&0x01) s[7] = val;
		}
	}
}

void pl_text_out16(int x, int y, const char *texto, ...)
{
	va_list args;
	char    buffer[256];

	va_start(args, texto);
	vsnprintf(buffer, sizeof(buffer), texto, args);
	va_end(args);

	pl_text_out16_(x, y, buffer);
}


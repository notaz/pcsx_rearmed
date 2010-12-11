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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "plugin_lib.h"
#include "linux/fbdev.h"
#include "common/fonts.h"
#include "common/input.h"
#include "omap.h"
#include "menu.h"
#include "pcnt.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"

void *pl_fbdev_buf;
int keystate;
static int pl_fbdev_w, pl_fbdev_h, pl_fbdev_bpp;
static int flip_cnt, flips_per_sec, tick_per_sec;
extern float fps_cur; // XXX

static int get_cpu_ticks(void)
{
	static unsigned long last_utime;
	static int fd;
	unsigned long utime, ret;
	char buf[128];

	if (fd == 0)
		fd = open("/proc/self/stat", O_RDONLY);
	lseek(fd, 0, SEEK_SET);
	buf[0] = 0;
	read(fd, buf, sizeof(buf));
	buf[sizeof(buf) - 1] = 0;

	sscanf(buf, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu", &utime);
	ret = utime - last_utime;
	last_utime = utime;
	return ret;
}

static void print_fps(void)
{
	if (pl_fbdev_bpp == 16)
		pl_text_out16(2, pl_fbdev_h - 10, "%2d %4.1f", flips_per_sec, fps_cur);
}

static void print_cpu_usage(void)
{
	if (pl_fbdev_bpp == 16)
		pl_text_out16(pl_fbdev_w - 28, pl_fbdev_h - 10, "%3d", tick_per_sec);
}

int pl_fbdev_init(void)
{
	pl_fbdev_buf = vout_fbdev_flip(layer_fb);
	return 0;
}

int pl_fbdev_set_mode(int w, int h, int bpp)
{
	void *ret;

	if (w == pl_fbdev_w && h == pl_fbdev_h && bpp == pl_fbdev_bpp)
		return 0;

	pl_fbdev_w = w;
	pl_fbdev_h = h;
	pl_fbdev_bpp = bpp;

	vout_fbdev_clear(layer_fb);
	ret = vout_fbdev_resize(layer_fb, w, h, bpp, 0, 0, 0, 0, 3);
	if (ret == NULL)
		fprintf(stderr, "failed to set mode\n");
	else
		pl_fbdev_buf = ret;

	menu_notify_mode_change(w, h);

	return (ret != NULL) ? 0 : -1;
}

void pl_fbdev_flip(void)
{
	/* doing input here because the pad is polled
	 * thousands of times for some reason */
	int actions[IN_BINDTYPE_COUNT] = { 0, };

	in_update(actions);
	if (actions[IN_BINDTYPE_EMU] & PEV_MENU)
		stop = 1;
	keystate = actions[IN_BINDTYPE_PLAYER12];

	flip_cnt++;
	print_fps();
	print_cpu_usage();

	// let's flip now
	pl_fbdev_buf = vout_fbdev_flip(layer_fb);
}

void pl_fbdev_finish(void)
{
}

/* called on every vsync */
void pl_frame_limit(void)
{
	extern void CheckFrameRate(void);
	static int oldsec;
	struct timeval tv;

	pcnt_end(PCNT_ALL);
	gettimeofday(&tv, 0);

	if (tv.tv_sec != oldsec) {
		flips_per_sec = flip_cnt;
		flip_cnt = 0;
		tick_per_sec = get_cpu_ticks();
		oldsec = tv.tv_sec;
	}
#ifdef PCNT
	static int ya_vsync_count;
	if (++ya_vsync_count == PCNT_FRAMES) {
		pcnt_print(fps_cur);
		ya_vsync_count = 0;
	}
#endif

	CheckFrameRate();

	pcnt_start(PCNT_ALL);
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


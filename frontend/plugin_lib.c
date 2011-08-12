/*
 * (C) notaz, 2010-2011
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "plugin_lib.h"
#include "linux/fbdev.h"
#include "common/fonts.h"
#include "common/input.h"
#include "omap.h"
#include "menu.h"
#include "main.h"
#include "pcnt.h"
#include "pl_gun_ts.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"
#include "../libpcsxcore/psemu_plugin_defs.h"

int in_type1, in_type2;
int in_a1[2] = { 127, 127 }, in_a2[2] = { 127, 127 };
int in_keystate, in_state_gun;
static void *ts;
void *pl_vout_buf;
static int pl_vout_w, pl_vout_h, pl_vout_bpp;
static int flip_cnt, vsync_cnt, flips_per_sec, tick_per_sec;
static float vsps_cur;
static int frame_interval, frame_interval1024, vsync_usec_time;


static __attribute__((noinline)) int get_cpu_ticks(void)
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

static void print_hud(void)
{
	if (pl_vout_bpp == 16)
		pl_text_out16(2, pl_vout_h - 10, "%s", hud_msg);
}

static void print_fps(void)
{
	if (pl_vout_bpp == 16)
		pl_text_out16(2, pl_vout_h - 10, "%2d %4.1f", flips_per_sec, vsps_cur);
}

static void print_cpu_usage(void)
{
	if (pl_vout_bpp == 16)
		pl_text_out16(pl_vout_w - 28, pl_vout_h - 10, "%3d", tick_per_sec);
}

// draw 192x8 status of 24 sound channels
static __attribute__((noinline)) void draw_active_chans(void)
{
	extern void spu_get_debug_info(int *chans_out, int *fmod_chans_out, int *noise_chans_out); // hack
	int live_chans, fmod_chans, noise_chans;

	static const unsigned short colors[2] = { 0x1fe3, 0x0700 };
	unsigned short *dest = (unsigned short *)pl_vout_buf +
		pl_vout_w * (pl_vout_h - 10) + pl_vout_w / 2 - 192/2;
	unsigned short *d, p;
	int c, x, y;

	if (pl_vout_bpp != 16)
		return;

	spu_get_debug_info(&live_chans, &fmod_chans, &noise_chans);

	for (c = 0; c < 24; c++) {
		d = dest + c * 8;
		p = !(live_chans & (1<<c)) ? 0 :
		     (fmod_chans & (1<<c)) ? 0xf000 :
		     (noise_chans & (1<<c)) ? 0x001f :
		     colors[c & 1];
		for (y = 0; y < 8; y++, d += pl_vout_w)
			for (x = 0; x < 8; x++)
				d[x] = p;
	}
}

static void *pl_vout_set_mode(int w, int h, int bpp)
{
	// special h handling, Wipeout likes to change it by 1-6
	h = (h + 7) & ~7;

	if (w == pl_vout_w && h == pl_vout_h && bpp == pl_vout_bpp)
		return pl_vout_buf;

	pl_vout_w = w;
	pl_vout_h = h;
	pl_vout_bpp = bpp;

#if defined(VOUT_FBDEV)
	vout_fbdev_clear(layer_fb);
	pl_vout_buf = vout_fbdev_resize(layer_fb, w, h, bpp, 0, 0, 0, 0, 3);
#elif defined(MAEMO)
	extern void *hildon_set_mode(int w, int h);
	pl_vout_buf = hildon_set_mode(w, h);
#endif

	if (pl_vout_buf == NULL)
		fprintf(stderr, "failed to set mode\n");

	// menu decides on layer size, we commit it
	menu_notify_mode_change(w, h, bpp);
	omap_enable_layer(1);

	return pl_vout_buf;
}

static void *pl_vout_flip(void)
{
	flip_cnt++;

	if (pl_vout_buf != NULL) {
		if (g_opts & OPT_SHOWSPU)
			draw_active_chans();

		if (hud_msg[0] != 0)
			print_hud();
		else if (g_opts & OPT_SHOWFPS)
			print_fps();

		if (g_opts & OPT_SHOWCPU)
			print_cpu_usage();
	}

	// let's flip now
#if defined(VOUT_FBDEV)
	pl_vout_buf = vout_fbdev_flip(layer_fb);
#elif defined(MAEMO)
	extern void *hildon_flip(void);
	pl_vout_buf = hildon_flip();
#endif
	return pl_vout_buf;
}

static int pl_vout_open(void)
{
	struct timeval now;

	omap_enable_layer(1);
#if defined(VOUT_FBDEV)
	pl_vout_buf = vout_fbdev_flip(layer_fb);

	// try to align redraws to vsync
	vout_fbdev_wait_vsync(layer_fb);
#elif defined(MAEMO)
	extern void *hildon_flip(void);
	pl_vout_buf = hildon_flip();
#endif

	gettimeofday(&now, 0);
	vsync_usec_time = now.tv_usec;
	while (vsync_usec_time >= frame_interval)
		vsync_usec_time -= frame_interval;

	return 0;
}

static void pl_vout_close(void)
{
	omap_enable_layer(0);
}

void *pl_prepare_screenshot(int *w, int *h, int *bpp)
{
	*w = pl_vout_w;
	*h = pl_vout_h;
	*bpp = pl_vout_bpp;

	return pl_vout_buf;
}

static void update_input(void)
{
#ifndef MAEMO
	int actions[IN_BINDTYPE_COUNT] = { 0, };
	unsigned int emu_act;

	in_update(actions);
	if (in_type1 == PSE_PAD_TYPE_ANALOGPAD)
		in_update_analogs();
	emu_act = actions[IN_BINDTYPE_EMU];
	in_state_gun = (emu_act & SACTION_GUN_MASK) >> SACTION_GUN_TRIGGER;

	emu_act &= ~SACTION_GUN_MASK;
	if (emu_act) {
		int which = 0;
		for (; !(emu_act & 1); emu_act >>= 1, which++)
			;
		emu_act = which;
	}
	emu_set_action(emu_act);

	in_keystate = actions[IN_BINDTYPE_PLAYER12];
#endif
#ifdef X11
	extern int x11_update_keys(unsigned int *action);
	in_keystate |= x11_update_keys(&emu_act);
	emu_set_action(emu_act);
#endif
}

void pl_update_gun(int *xn, int *xres, int *y, int *in)
{
	if (ts)
		pl_gun_ts_update(ts, xn, y, in);

	*xres = pl_vout_w;
	*y = *y * pl_vout_h >> 10;
}

#define MAX_LAG_FRAMES 3

#define tvdiff(tv, tv_old) \
	((tv.tv_sec - tv_old.tv_sec) * 1000000 + tv.tv_usec - tv_old.tv_usec)

/* called on every vsync */
void pl_frame_limit(void)
{
	static struct timeval tv_old, tv_expect;
	static int vsync_cnt_prev;
	struct timeval now;
	int diff, usadj;

	vsync_cnt++;

	/* doing input here because the pad is polled
	 * thousands of times per frame for some reason */
	update_input();

	pcnt_end(PCNT_ALL);
	gettimeofday(&now, 0);

	if (now.tv_sec != tv_old.tv_sec) {
		diff = tvdiff(now, tv_old);
		vsps_cur = 0.0f;
		if (0 < diff && diff < 2000000)
			vsps_cur = 1000000.0f * (vsync_cnt - vsync_cnt_prev) / diff;
		vsync_cnt_prev = vsync_cnt;
		flips_per_sec = flip_cnt;
		flip_cnt = 0;
		tv_old = now;
		if (g_opts & OPT_SHOWCPU)
			tick_per_sec = get_cpu_ticks();

		if (hud_new_msg > 0) {
			hud_new_msg--;
			if (hud_new_msg == 0)
				hud_msg[0] = 0;
		}
	}
#ifdef PCNT
	static int ya_vsync_count;
	if (++ya_vsync_count == PCNT_FRAMES) {
		pcnt_print(vsps_cur);
		ya_vsync_count = 0;
	}
#endif

	// tv_expect uses usec*1024 units instead of usecs for better accuracy
	tv_expect.tv_usec += frame_interval1024;
	if (tv_expect.tv_usec >= (1000000 << 10)) {
		tv_expect.tv_usec -= (1000000 << 10);
		tv_expect.tv_sec++;
	}
	diff = (tv_expect.tv_sec - now.tv_sec) * 1000000 + (tv_expect.tv_usec >> 10) - now.tv_usec;

	if (diff > MAX_LAG_FRAMES * frame_interval || diff < -MAX_LAG_FRAMES * frame_interval) {
		//printf("pl_frame_limit reset, diff=%d, iv %d\n", diff, frame_interval);
		tv_expect = now;
		diff = 0;
		// try to align with vsync
		usadj = vsync_usec_time;
		while (usadj < tv_expect.tv_usec - frame_interval)
			usadj += frame_interval;
		tv_expect.tv_usec = usadj << 10;
	}

	if (!(g_opts & OPT_NO_FRAMELIM) && diff > frame_interval) {
		// yay for working usleep on pandora!
		//printf("usleep %d\n", diff - frame_interval / 2);
		usleep(diff - frame_interval / 2);
	}

	if (pl_rearmed_cbs.frameskip) {
		if (diff < -frame_interval)
			pl_rearmed_cbs.fskip_advice = 1;
		else if (diff >= 0)
			pl_rearmed_cbs.fskip_advice = 0;
	}

	pcnt_start(PCNT_ALL);
}

void pl_timing_prepare(int is_pal)
{
	pl_rearmed_cbs.fskip_advice = 0;

	frame_interval = is_pal ? 20000 : 16667;
	frame_interval1024 = is_pal ? 20000*1024 : 17066667;

	// used by P.E.Op.S. frameskip code
	pl_rearmed_cbs.gpu_peops.fFrameRateHz = is_pal ? 50.0f : 59.94f;
	pl_rearmed_cbs.gpu_peops.dwFrameRateTicks =
		(100000*100 / (unsigned long)(pl_rearmed_cbs.gpu_peops.fFrameRateHz*100));
}

static void pl_text_out16_(int x, int y, const char *text)
{
	int i, l, len = strlen(text), w = pl_vout_w;
	unsigned short *screen = (unsigned short *)pl_vout_buf + x + y * w;
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

static void pl_get_layer_pos(int *x, int *y, int *w, int *h)
{
	*x = g_layer_x;
	*y = g_layer_y;
	*w = g_layer_w;
	*h = g_layer_h;
}

struct rearmed_cbs pl_rearmed_cbs = {
	pl_get_layer_pos,
	pl_vout_open,
	pl_vout_set_mode,
	pl_vout_flip,
	pl_vout_close,
};

/* watchdog */
static void *watchdog_thread(void *unused)
{
	int vsync_cnt_old = 0;
	int seen_dead = 0;
	int sleep_time = 5;

#ifndef NDEBUG
	// don't interfere with debug
	return NULL;
#endif
	while (1)
	{
		sleep(sleep_time);

		if (stop) {
			seen_dead = 0;
			sleep_time = 5;
			continue;
		}
		if (vsync_cnt != vsync_cnt_old) {
			vsync_cnt_old = vsync_cnt;
			seen_dead = 0;
			sleep_time = 2;
			continue;
		}

		seen_dead++;
		sleep_time = 1;
		if (seen_dead > 1)
			fprintf(stderr, "watchdog: seen_dead %d\n", seen_dead);
		if (seen_dead > 4) {
			fprintf(stderr, "watchdog: lockup detected, aborting\n");
			// we can't do any cleanup here really, the main thread is
			// likely touching resources and would crash anyway
			abort();
		}
	}
}

void pl_start_watchdog(void)
{
	pthread_attr_t attr;
	pthread_t tid;
	int ret;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	ret = pthread_create(&tid, &attr, watchdog_thread, NULL);
	if (ret != 0)
		fprintf(stderr, "could not start watchdog: %d\n", ret);
}

void pl_init(void)
{
	ts = pl_gun_ts_init();
}

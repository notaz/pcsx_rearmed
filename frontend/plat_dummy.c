/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "common/input.h"
#include "linux/fbdev.h"
#include "plat.h"

struct vout_fbdev *layer_fb;
int g_layer_x, g_layer_y, g_layer_w, g_layer_h;
struct in_default_bind in_evdev_defbinds[] = {
	{ 0, 0, 0 },
};

int omap_enable_layer(int enabled)
{
	return 0;
}

void plat_video_menu_enter(int is_rom_loaded)
{
}

void plat_video_menu_begin(void)
{
}

void plat_video_menu_end(void)
{
}

void plat_video_menu_leave(void)
{
}

void plat_init(void)
{
}

void plat_finish(void)
{
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	return 0;
}

int plat_cpu_clock_get(void)
{
	return -1;
}

int plat_cpu_clock_apply(int cpu_clock)
{
	return -1;
}

int plat_get_bat_capacity(void)
{
	return -1;
}

void plat_step_volume(int is_up)
{
}

void plat_trigger_vibrate(void)
{
}

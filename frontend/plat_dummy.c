/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "common/input.h"
#include "linux/fbdev.h"

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

void bgr555_to_rgb565(void *d, void *s, int len)
{
}

void bgr888_to_rgb888(void *d, void *s, int len)
{
}

void in_update_analogs(void)
{
}

void pandora_rescan_inputs(void)
{
}

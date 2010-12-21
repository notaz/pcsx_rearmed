/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/omapfb.h>

#include "common/input.h"
#include "common/menu.h"
#include "linux/fbdev.h"
#include "linux/oshide.h"
#include "plugin_lib.h"
#include "omap.h"


static struct vout_fbdev *main_fb;
int g_layer_x = 80, g_layer_y = 0;
int g_layer_w = 640, g_layer_h = 480;

struct vout_fbdev *layer_fb;

static const char * const pandora_gpio_keys[KEY_MAX + 1] = {
	[0 ... KEY_MAX] = NULL,
	[KEY_UP]	= "Up",
	[KEY_LEFT]	= "Left",
	[KEY_RIGHT]	= "Right",
	[KEY_DOWN]	= "Down",
	[KEY_HOME]	= "A",
	[KEY_PAGEDOWN]	= "X",
	[KEY_END]	= "B",
	[KEY_PAGEUP]	= "Y",
	[KEY_RIGHTSHIFT]= "L",
	[KEY_RIGHTCTRL]	= "R",
	[KEY_LEFTALT]	= "Start",
	[KEY_LEFTCTRL]	= "Select",
	[KEY_MENU]	= "Pandora",
};

struct in_default_bind in_evdev_defbinds[] = {
	{ KEY_UP,	IN_BINDTYPE_PLAYER12, DKEY_UP },
	{ KEY_DOWN,	IN_BINDTYPE_PLAYER12, DKEY_DOWN },
	{ KEY_LEFT,	IN_BINDTYPE_PLAYER12, DKEY_LEFT },
	{ KEY_RIGHT,	IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
	{ KEY_SPACE,    IN_BINDTYPE_EMU, PEVB_MENU },
	{ KEY_PAGEUP,	IN_BINDTYPE_PLAYER12, DKEY_TRIANGLE },
	{ KEY_PAGEDOWN,	IN_BINDTYPE_PLAYER12, DKEY_CROSS },
	{ KEY_END,	IN_BINDTYPE_PLAYER12, DKEY_CIRCLE },
	{ KEY_HOME,	IN_BINDTYPE_PLAYER12, DKEY_SQUARE },
	{ KEY_LEFTALT,	IN_BINDTYPE_PLAYER12, DKEY_START },
	{ KEY_LEFTCTRL,	IN_BINDTYPE_PLAYER12, DKEY_SELECT },
	{ KEY_RIGHTSHIFT,IN_BINDTYPE_PLAYER12, DKEY_L1 },
	{ KEY_RIGHTCTRL, IN_BINDTYPE_PLAYER12, DKEY_R1 },
	{ KEY_Q,	IN_BINDTYPE_PLAYER12, DKEY_L2 },
	{ KEY_P,	IN_BINDTYPE_PLAYER12, DKEY_R2 },
	{ 0, 0, 0 }
};

static int omap_setup_layer_(int fd, int enabled, int x, int y, int w, int h, int first_call)
{
	struct omapfb_plane_info pi;
	struct omapfb_mem_info mi;
	int ret;

	ret = ioctl(fd, OMAPFB_QUERY_PLANE, &pi);
	if (ret != 0) {
		perror("QUERY_PLANE");
		return -1;
	}

	ret = ioctl(fd, OMAPFB_QUERY_MEM, &mi);
	if (ret != 0) {
		perror("QUERY_MEM");
		return -1;
	}

	/* must disable when changing stuff */
	if (pi.enabled) {
		pi.enabled = 0;
		ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
		if (ret != 0)
			perror("SETUP_PLANE");
	}

	if (first_call) {
		mi.size = 640*512*2*3;
		ret = ioctl(fd, OMAPFB_SETUP_MEM, &mi);
		if (ret != 0) {
			perror("SETUP_MEM");
			return -1;
		}
	}

	pi.pos_x = x;
	pi.pos_y = y;
	pi.out_width = w;
	pi.out_height = h;
	pi.enabled = enabled;

	ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
	if (ret != 0) {
		perror("SETUP_PLANE");
		return -1;
	}

	return 0;
}

int omap_enable_layer(int enabled)
{
	return omap_setup_layer_(vout_fbdev_get_fd(layer_fb), enabled,
		g_layer_x, g_layer_y, g_layer_w, g_layer_h, 0);
}

void plat_video_menu_enter(int is_rom_loaded)
{
}

void plat_video_menu_begin(void)
{
}

void plat_video_menu_end(void)
{
	g_menuscreen_ptr = vout_fbdev_flip(main_fb);
}

void plat_init(void)
{
	const char *main_fb_name, *layer_fb_name;
	void *temp_frame;
	int fd, ret, w, h;

	main_fb_name = getenv("FBDEV_MAIN");
	if (main_fb_name == NULL)
		main_fb_name = "/dev/fb0";

	layer_fb_name = getenv("FBDEV_LAYER");
	if (layer_fb_name == NULL)
		layer_fb_name = "/dev/fb1";

	// must set the layer up first to be able to use it
	fd = open(layer_fb_name, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "%s: ", layer_fb_name);
		perror("open");
		exit(1);
	}

	ret = omap_setup_layer_(fd, 1, g_layer_x, g_layer_y, g_layer_w, g_layer_h, 1);
	close(fd);
	if (ret != 0) {
		fprintf(stderr, "failed to set up layer, exiting.\n");
		exit(1);
	}

	oshide_init();

	w = h = 0;
	main_fb = vout_fbdev_init(main_fb_name, &w, &h, 16, 2);
	if (main_fb == NULL) {
		fprintf(stderr, "couldn't init fb: %s\n", main_fb_name);
		exit(1);
	}

	g_menuscreen_w = w;
	g_menuscreen_h = h;
	g_menuscreen_ptr = vout_fbdev_flip(main_fb);

	w = 640;
	h = 512; // ??
	layer_fb = vout_fbdev_init(layer_fb_name, &w, &h, 16, 3);
	if (layer_fb == NULL) {
		fprintf(stderr, "couldn't init fb: %s\n", layer_fb_name);
		goto fail0;
	}

	temp_frame = calloc(g_menuscreen_w * g_menuscreen_h * 2, 1);
	if (temp_frame == NULL) {
		fprintf(stderr, "OOM\n");
		goto fail1;
	}
	g_menubg_ptr = temp_frame;
	g_menubg_src_ptr = temp_frame;

	in_set_config(in_name_to_id("evdev:gpio-keys"), IN_CFG_KEY_NAMES,
		      pandora_gpio_keys, sizeof(pandora_gpio_keys));
	return;

fail1:
	vout_fbdev_finish(layer_fb);
fail0:
	vout_fbdev_finish(main_fb);
	exit(1);

}


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
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/omapfb.h>

#include "common/menu.h"
#include "linux/fbdev.h"
#include "linux/oshide.h"
#include "plugin_lib.h"
#include "pl_gun_ts.h"
#include "omap.h"
#include "plat.h"


static struct vout_fbdev *main_fb;
int g_layer_x = 80, g_layer_y = 0;
int g_layer_w = 640, g_layer_h = 480;

struct vout_fbdev *layer_fb;

static int omap_setup_layer_(int fd, int enabled, int x, int y, int w, int h)
{
	struct omapfb_plane_info pi = { 0, };
	struct omapfb_mem_info mi = { 0, };
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

	if (mi.size < 640*512*3*3) {
		mi.size = 640*512*3*3;
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
	if (enabled)
		pl_set_gun_rect(g_layer_x, g_layer_y, g_layer_w, g_layer_h);

	return omap_setup_layer_(vout_fbdev_get_fd(layer_fb), enabled,
		g_layer_x, g_layer_y, g_layer_w, g_layer_h);
}

void plat_video_menu_enter(int is_rom_loaded)
{
	g_menuscreen_ptr = vout_fbdev_resize(main_fb,
		g_menuscreen_w, g_menuscreen_h, 16, 0, 0, 0, 0, 3);
	if (g_menuscreen_ptr == NULL)
		fprintf(stderr, "warning: vout_fbdev_resize failed\n");
}

void plat_video_menu_begin(void)
{
}

void plat_video_menu_end(void)
{
	g_menuscreen_ptr = vout_fbdev_flip(main_fb);
}

void plat_video_menu_leave(void)
{
	/* have to get rid of panning so that plugins that
	 * use fb0 and don't ever pan can work. */
	vout_fbdev_clear(main_fb);
	g_menuscreen_ptr = vout_fbdev_resize(main_fb,
		g_menuscreen_w, g_menuscreen_h, 16, 0, 0, 0, 0, 1);
	if (g_menuscreen_ptr == NULL)
		fprintf(stderr, "warning: vout_fbdev_resize failed\n");
}

void plat_step_volume(int is_up)
{
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

	ret = omap_setup_layer_(fd, 0, g_layer_x, g_layer_y, g_layer_w, g_layer_h);
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

	plat_pandora_init(); // XXX

	return;

fail1:
	vout_fbdev_finish(layer_fb);
fail0:
	vout_fbdev_finish(main_fb);
	exit(1);

}

void plat_finish(void)
{
	omap_enable_layer(0);
	vout_fbdev_finish(layer_fb);
	vout_fbdev_finish(main_fb);
	oshide_finish();
}


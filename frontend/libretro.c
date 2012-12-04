/*
 * (C) notaz, 2012
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libpcsxcore/misc.h"
#include "../libpcsxcore/psxcounters.h"
#include "../libpcsxcore/psxmem_map.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"
#include "../plugins/dfsound/out.h"
#include "../plugins/gpulib/cspace.h"
#include "main.h"
#include "plugin.h"
#include "plugin_lib.h"
#include "revision.h"
#include "libretro.h"

static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;
static retro_audio_sample_batch_t audio_batch_cb;

static void *vout_buf;
static int samples_sent, samples_to_send;
static int plugins_opened;

/* memory card data */
extern char Mcd1Data[MCD_SIZE];
extern char McdDisable[2];

/* PCSX ReARMed core calls and stuff */
int in_type1, in_type2;
int in_a1[2] = { 127, 127 }, in_a2[2] = { 127, 127 };
int in_keystate;
int in_enable_vibration;

static int vout_open(void)
{
	return 0;
}

static void vout_set_mode(int w, int h, int raw_w, int raw_h, int bpp)
{
}

#ifdef FRONTEND_SUPPORTS_RGB565
static void convert(void *buf, size_t bytes)
{
	unsigned int i, v, *p = buf;

	for (i = 0; i < bytes / 4; i++) {
		v = p[i];
		p[i] = (v & 0x001f001f) | ((v >> 1) & 0x7fe07fe0);
	}
}
#endif

static unsigned game_width;
static unsigned game_height;

static void vout_flip(const void *vram, int stride, int bgr24, int w, int h)
{
	unsigned short *dest = vout_buf;
	const unsigned short *src = vram;
	int dstride = w, h1 = h;

	if (vram == NULL) {
		// blanking
		memset(vout_buf, 0, dstride * h * 2);
		goto out;
	}

	if (bgr24)
	{
		// XXX: could we switch to RETRO_PIXEL_FORMAT_XRGB8888 here?
		for (; h1-- > 0; dest += dstride, src += stride)
		{
			bgr888_to_rgb565(dest, src, w * 3);
		}
	}
	else
	{
		for (; h1-- > 0; dest += dstride, src += stride)
		{
			bgr555_to_rgb565(dest, src, w * 2);
		}
	}

out:
#ifndef FRONTEND_SUPPORTS_RGB565
   convert(vout_buf, w * h * 2);
#endif
   game_width = w;
   game_height = h;
	pl_rearmed_cbs.flip_cnt++;
}

static void vout_close(void)
{
}

static void *pl_mmap(unsigned int size)
{
	return psxMap(0, size, 0, MAP_TAG_VRAM);
}

static void pl_munmap(void *ptr, unsigned int size)
{
	psxUnmap(ptr, size, MAP_TAG_VRAM);
}

struct rearmed_cbs pl_rearmed_cbs = {
	.pl_vout_open = vout_open,
	.pl_vout_set_mode = vout_set_mode,
	.pl_vout_flip = vout_flip,
	.pl_vout_close = vout_close,
	.mmap = pl_mmap,
	.munmap = pl_munmap,
	/* from psxcounters */
	.gpu_hcnt = &hSyncCount,
	.gpu_frame_count = &frame_counter,
};

void pl_frame_limit(void)
{
	/* called once per frame, make psxCpu->Execute() above return */
	stop = 1;
}

void pl_timing_prepare(int is_pal)
{
}

void plat_trigger_vibrate(int is_strong)
{
}

void pl_update_gun(int *xn, int *yn, int *xres, int *yres, int *in)
{
}

/* sound calls */
static int snd_init(void)
{
	return 0;
}

static void snd_finish(void)
{
}

static int snd_busy(void)
{
	if (samples_to_send > samples_sent)
		return 0; /* give more samples */
	else
		return 1;
}

static void snd_feed(void *buf, int bytes)
{
	audio_batch_cb(buf, bytes / 4);
	samples_sent += bytes / 4;
}

void out_register_libretro(struct out_driver *drv)
{
	drv->name = "libretro";
	drv->init = snd_init;
	drv->finish = snd_finish;
	drv->busy = snd_busy;
	drv->feed = snd_feed;
}

/* libretro */
void retro_set_environment(retro_environment_t cb) { environ_cb = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
}

void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name = "PCSX-ReARMed";
	info->library_version = REV;
	info->valid_extensions = "bin|cue|img|mdf|pbp|cbn";
	info->need_fullpath = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	memset(info, 0, sizeof(*info));
	info->timing.fps            = 60;
	info->timing.sample_rate    = 44100;
	info->geometry.base_width   = 320;
	info->geometry.base_height  = 240;
	info->geometry.max_width    = 640;
	info->geometry.max_height   = 512;
	info->geometry.aspect_ratio = 4.0 / 3.0;
}

/* TODO */
size_t retro_serialize_size(void) 
{ 
	return 0;
}

bool retro_serialize(void *data, size_t size)
{ 
	return false;
}

bool retro_unserialize(const void *data, size_t size)
{
	return false;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
}

bool retro_load_game(const struct retro_game_info *info)
{
#ifdef FRONTEND_SUPPORTS_RGB565
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
	if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
		fprintf(stderr, "RGB565 supported, using it\n");
	}
#endif

	if (plugins_opened) {
		ClosePlugins();
		plugins_opened = 0;
	}

	set_cd_image(info->path);

	/* have to reload after set_cd_image for correct cdr plugin */
	if (LoadPlugins() == -1) {
		printf("faled to load plugins\n");
		return false;
	}

	plugins_opened = 1;
	NetOpened = 0;

	if (OpenPlugins() == -1) {
		printf("faled to open plugins\n");
		return false;
	}

	plugin_call_rearmed_cbs();

	Config.PsxAuto = 1;
	if (CheckCdrom() == -1) {
		printf("unsupported/invalid CD image: %s\n", info->path);
		return false;
	}

	SysReset();

	if (LoadCdrom() == -1) {
		printf("could not load CD-ROM!\n");
		return false;
	}
	emu_on_new_cd(0);

	return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
	return false;
}

void retro_unload_game(void) 
{
}

unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
	return Mcd1Data;
}

size_t retro_get_memory_size(unsigned id)
{
	return MCD_SIZE;
}

void retro_reset(void)
{
	SysReset();
}

static const unsigned short retro_psx_map[] = {
	[RETRO_DEVICE_ID_JOYPAD_B]	= 1 << DKEY_CROSS,
	[RETRO_DEVICE_ID_JOYPAD_Y]	= 1 << DKEY_SQUARE,
	[RETRO_DEVICE_ID_JOYPAD_SELECT]	= 1 << DKEY_SELECT,
	[RETRO_DEVICE_ID_JOYPAD_START]	= 1 << DKEY_START,
	[RETRO_DEVICE_ID_JOYPAD_UP]	= 1 << DKEY_UP,
	[RETRO_DEVICE_ID_JOYPAD_DOWN]	= 1 << DKEY_DOWN,
	[RETRO_DEVICE_ID_JOYPAD_LEFT]	= 1 << DKEY_LEFT,
	[RETRO_DEVICE_ID_JOYPAD_RIGHT]	= 1 << DKEY_RIGHT,
	[RETRO_DEVICE_ID_JOYPAD_A]	= 1 << DKEY_CIRCLE,
	[RETRO_DEVICE_ID_JOYPAD_X]	= 1 << DKEY_TRIANGLE,
	[RETRO_DEVICE_ID_JOYPAD_L]	= 1 << DKEY_L1,
	[RETRO_DEVICE_ID_JOYPAD_R]	= 1 << DKEY_R1,
	[RETRO_DEVICE_ID_JOYPAD_L2]	= 1 << DKEY_L2,
	[RETRO_DEVICE_ID_JOYPAD_R2]	= 1 << DKEY_R2,
	[RETRO_DEVICE_ID_JOYPAD_L3]	= 1 << DKEY_L3,
	[RETRO_DEVICE_ID_JOYPAD_R3]	= 1 << DKEY_R3,
};
#define RETRO_PSX_MAP_LEN (sizeof(retro_psx_map) / sizeof(retro_psx_map[0]))

void retro_run(void) 
{
	int i;

	input_poll_cb();
	in_keystate = 0;
	for (i = 0; i < RETRO_PSX_MAP_LEN; i++)
		if (input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i))
			in_keystate |= retro_psx_map[i];
	in_keystate <<= 16;
	for (i = 0; i < RETRO_PSX_MAP_LEN; i++)
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
			in_keystate |= retro_psx_map[i];

	stop = 0;
	psxCpu->Execute();

	samples_to_send += 44100 / 60;
	video_cb(vout_buf, game_width, game_height, game_width * 2);
}

void retro_init(void)
{
	const char *bios[] = { "scph1001", "scph5501", "scph7001" };
	const char *dir;
	char path[256];
	FILE *f = NULL;
	int i, ret, level;

	ret = emu_core_preinit();
	ret |= emu_core_init();
	if (ret != 0) {
		printf("PCSX init failed, sorry\n");
		exit(1);
	}

	vout_buf = malloc(640 * 512 * 2);

	if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
	{
		snprintf(Config.BiosDir, sizeof(Config.BiosDir), "%s/", dir);

		for (i = 0; i < sizeof(bios) / sizeof(bios[0]); i++) {
			snprintf(path, sizeof(path), "%s/%s.bin", dir, bios[i]);
			f = fopen(path, "r");
			if (f != NULL) {
				snprintf(Config.Bios, sizeof(Config.Bios), "%s.bin", bios[i]);
				break;
			}
		}
	}
	if (f != NULL) {
		printf("found BIOS file: %s\n", Config.Bios);
		fclose(f);
	}
	else
		printf("no BIOS files found.\n");

	level = 1;
	environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);

	McdDisable[0] = 0;
	McdDisable[1] = 1;
}

void retro_deinit(void)
{
	SysClose();
	free(vout_buf);
	vout_buf = NULL;
}

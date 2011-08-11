/*
 * (C) Gražvydas "notaz" Ignotas, 2010-2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <zlib.h>

#include "main.h"
#include "menu.h"
#include "config.h"
#include "plugin.h"
#include "plugin_lib.h"
#include "omap.h"
#include "pandora.h"
#include "pcnt.h"
#include "arm_utils.h"
#include "common/plat.h"
#include "common/input.h"
#include "linux/in_evdev.h"
#include "../libpcsxcore/misc.h"
#include "../libpcsxcore/cdrom.h"
#include "../libpcsxcore/psemu_plugin_defs.h"
#include "../libpcsxcore/new_dynarec/new_dynarec.h"
#include "../plugins/dfinput/main.h"
#include "revision.h"

#define MENU_X2 1
#define array_size(x) (sizeof(x) / sizeof(x[0]))

typedef enum
{
	MA_NONE = 1,
	MA_MAIN_RESUME_GAME,
	MA_MAIN_SAVE_STATE,
	MA_MAIN_LOAD_STATE,
	MA_MAIN_RESET_GAME,
	MA_MAIN_LOAD_ROM,
	MA_MAIN_SWAP_CD,
	MA_MAIN_RUN_BIOS,
	MA_MAIN_RUN_EXE,
	MA_MAIN_CONTROLS,
	MA_MAIN_CREDITS,
	MA_MAIN_EXIT,
	MA_CTRL_PLAYER1,
	MA_CTRL_PLAYER2,
	MA_CTRL_EMU,
	MA_CTRL_DEV_FIRST,
	MA_CTRL_DEV_NEXT,
	MA_CTRL_DONE,
	MA_OPT_SAVECFG,
	MA_OPT_SAVECFG_GAME,
	MA_OPT_CPU_CLOCKS,
	MA_OPT_FILTERING,
} menu_id;

enum {
	SCALE_1_1,
	SCALE_4_3,
	SCALE_FULLSCREEN,
	SCALE_CUSTOM,
};

static int last_psx_w, last_psx_h, last_psx_bpp;
static int scaling, filter, cpu_clock, cpu_clock_st, volume_boost;
static char rom_fname_reload[MAXPATHLEN];
static char last_selected_fname[MAXPATHLEN];
static int warned_about_bios, region, in_type_sel1, in_type_sel2;
static int memcard1_sel, memcard2_sel;
int g_opts;

// sound plugin
extern int iUseReverb;
extern int iUseInterpolation;
extern int iXAPitch;
extern int iSPUIRQWait;
extern int iUseTimer;
extern int iVolume;

static const char *bioses[24];
static const char *gpu_plugins[16];
static const char *spu_plugins[16];
static const char *memcards[32];
static int bios_sel, gpu_plugsel, spu_plugsel;


static int min(int x, int y) { return x < y ? x : y; }
static int max(int x, int y) { return x > y ? x : y; }

void emu_make_path(char *buff, const char *end, int size)
{
	int pos, end_len;

	end_len = strlen(end);
	pos = plat_get_root_dir(buff, size);
	strncpy(buff + pos, end, size - pos);
	buff[size - 1] = 0;
	if (pos + end_len > size - 1)
		printf("Warning: path truncated: %s\n", buff);
}

static int emu_check_save_file(int slot)
{
	int ret = emu_check_state(slot);
	return ret == 0 ? 1 : 0;
}

static int emu_save_load_game(int load, int unused)
{
	int ret;

	if (load) {
		ret = emu_load_state(state_slot);

		// reflect hle/bios mode from savestate
		if (Config.HLE)
			bios_sel = 0;
		else if (bios_sel == 0 && bioses[1] != NULL)
			// XXX: maybe find the right bios instead
			bios_sel = 1;
	}
	else
		ret = emu_save_state(state_slot);

	return ret;
}

// propagate menu settings to the emu vars
static void menu_sync_config(void)
{
	static int allow_abs_only_old;

	Config.PsxAuto = 1;
	if (region > 0) {
		Config.PsxAuto = 0;
		Config.PsxType = region - 1;
	}
	switch (in_type_sel1) {
	case 1:  in_type1 = PSE_PAD_TYPE_ANALOGPAD; break;
	case 2:  in_type1 = PSE_PAD_TYPE_GUNCON;    break;
	default: in_type1 = PSE_PAD_TYPE_STANDARD;
	}
	switch (in_type_sel2) {
	case 1:  in_type2 = PSE_PAD_TYPE_ANALOGPAD; break;
	case 2:  in_type2 = PSE_PAD_TYPE_GUNCON;    break;
	default: in_type2 = PSE_PAD_TYPE_STANDARD;
	}
	if (in_evdev_allow_abs_only != allow_abs_only_old) {
		pandora_rescan_inputs();
		allow_abs_only_old = in_evdev_allow_abs_only;
	}

	iVolume = 768 + 128 * volume_boost;
	pl_timing_prepare(Config.PsxType);
}

static void menu_set_defconfig(void)
{
	g_opts = 0;
	scaling = SCALE_4_3;
	volume_boost = 0;

	region = 0;
	in_type_sel1 = in_type_sel2 = 0;
	in_evdev_allow_abs_only = 0;
	Config.Xa = Config.Cdda = Config.Sio =
	Config.SpuIrq = Config.RCntFix = Config.VSyncWA = 0;
	Config.CdrReschedule = 0;

	pl_rearmed_cbs.frameskip = 0;
	pl_rearmed_cbs.gpu_peops.iUseDither = 0;
	pl_rearmed_cbs.gpu_peops.dwActFixes = 1<<7;

	iUseReverb = 2;
	iUseInterpolation = 1;
	iXAPitch = 0;
	iSPUIRQWait = 1;
	iUseTimer = 2;

	menu_sync_config();
}

#define CE_CONFIG_STR(val) \
	{ #val, 0, Config.val }

#define CE_CONFIG_VAL(val) \
	{ #val, sizeof(Config.val), &Config.val }

#define CE_STR(val) \
	{ #val, 0, val }

#define CE_INTVAL(val) \
	{ #val, sizeof(val), &val }

#define CE_INTVAL_P(val) \
	{ #val, sizeof(pl_rearmed_cbs.val), &pl_rearmed_cbs.val }

// 'versioned' var, used when defaults change
#define CE_INTVAL_V(val, ver) \
	{ #val #ver, sizeof(val), &val }

static const struct {
	const char *name;
	size_t len;
	void *val;
} config_data[] = {
	CE_CONFIG_STR(Bios),
	CE_CONFIG_STR(Gpu),
	CE_CONFIG_STR(Spu),
//	CE_CONFIG_STR(Cdr),
	CE_CONFIG_VAL(Xa),
	CE_CONFIG_VAL(Sio),
	CE_CONFIG_VAL(Mdec),
	CE_CONFIG_VAL(Cdda),
	CE_CONFIG_VAL(Debug),
	CE_CONFIG_VAL(PsxOut),
	CE_CONFIG_VAL(SpuIrq),
	CE_CONFIG_VAL(RCntFix),
	CE_CONFIG_VAL(VSyncWA),
	CE_CONFIG_VAL(Cpu),
	CE_CONFIG_VAL(CdrReschedule),
	CE_INTVAL(region),
	CE_INTVAL(scaling),
	CE_INTVAL(g_layer_x),
	CE_INTVAL(g_layer_y),
	CE_INTVAL(g_layer_w),
	CE_INTVAL(g_layer_h),
	CE_INTVAL(filter),
	CE_INTVAL(state_slot),
	CE_INTVAL(cpu_clock),
	CE_INTVAL(g_opts),
	CE_INTVAL(in_type_sel1),
	CE_INTVAL(in_type_sel2),
	CE_INTVAL_P(frameskip),
	CE_INTVAL_P(gpu_peops.iUseDither),
	CE_INTVAL_P(gpu_peops.dwActFixes),
	CE_INTVAL(iUseReverb),
	CE_INTVAL(iXAPitch),
	CE_INTVAL_V(iUseInterpolation, 2),
	CE_INTVAL_V(iSPUIRQWait, 2),
	CE_INTVAL(iUseTimer),
	CE_INTVAL(warned_about_bios),
	CE_INTVAL(in_evdev_allow_abs_only),
	CE_INTVAL(volume_boost),
};

static char *get_cd_label(void)
{
	static char trimlabel[33];
	int j;

	strncpy(trimlabel, CdromLabel, 32);
	trimlabel[32] = 0;
	for (j = 31; j >= 0; j--)
		if (trimlabel[j] == ' ')
			trimlabel[j] = 0;

	return trimlabel;
}

static void make_cfg_fname(char *buf, size_t size, int is_game)
{
	if (is_game)
		snprintf(buf, size, "." PCSX_DOT_DIR "cfg/%.32s-%.9s.cfg", get_cd_label(), CdromId);
	else
		snprintf(buf, size, "." PCSX_DOT_DIR "%s", cfgfile_basename);
}

static void keys_write_all(FILE *f);

static int menu_write_config(int is_game)
{
	char cfgfile[MAXPATHLEN];
	FILE *f;
	int i;

	make_cfg_fname(cfgfile, sizeof(cfgfile), is_game);
	f = fopen(cfgfile, "w");
	if (f == NULL) {
		printf("menu_write_config: failed to open: %s\n", cfgfile);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(config_data); i++) {
		fprintf(f, "%s = ", config_data[i].name);
		switch (config_data[i].len) {
		case 0:
			fprintf(f, "%s\n", (char *)config_data[i].val);
			break;
		case 1:
			fprintf(f, "%x\n", *(u8 *)config_data[i].val);
			break;
		case 2:
			fprintf(f, "%x\n", *(u16 *)config_data[i].val);
			break;
		case 4:
			fprintf(f, "%x\n", *(u32 *)config_data[i].val);
			break;
		default:
			printf("menu_write_config: unhandled len %d for %s\n",
				 config_data[i].len, config_data[i].name);
			break;
		}
	}

	if (!is_game)
		fprintf(f, "lastcdimg = %s\n", last_selected_fname);

	keys_write_all(f);
	fclose(f);

	return 0;
}

static void parse_str_val(char *cval, const char *src)
{
	char *tmp;
	strncpy(cval, src, MAXPATHLEN);
	cval[MAXPATHLEN - 1] = 0;
	tmp = strchr(cval, '\n');
	if (tmp == NULL)
		tmp = strchr(cval, '\r');
	if (tmp != NULL)
		*tmp = 0;
}

static void keys_load_all(const char *cfg);

static int menu_load_config(int is_game)
{
	char cfgfile[MAXPATHLEN];
	int i, ret = -1;
	long size;
	char *cfg;
	FILE *f;

	make_cfg_fname(cfgfile, sizeof(cfgfile), is_game);
	f = fopen(cfgfile, "r");
	if (f == NULL) {
		printf("menu_load_config: failed to open: %s\n", cfgfile);
		return -1;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	if (size <= 0) {
		printf("bad size %ld: %s\n", size, cfgfile);
		goto fail;
	}

	cfg = malloc(size + 1);
	if (cfg == NULL)
		goto fail;

	fseek(f, 0, SEEK_SET);
	if (fread(cfg, 1, size, f) != size) {
		printf("failed to read: %s\n", cfgfile);
		goto fail_read;
	}
	cfg[size] = 0;

	for (i = 0; i < ARRAY_SIZE(config_data); i++) {
		char *tmp, *tmp2;
		u32 val;

		tmp = strstr(cfg, config_data[i].name);
		if (tmp == NULL)
			continue;
		tmp += strlen(config_data[i].name);
		if (strncmp(tmp, " = ", 3) != 0)
			continue;
		tmp += 3;

		if (config_data[i].len == 0) {
			parse_str_val(config_data[i].val, tmp);
			continue;
		}

		tmp2 = NULL;
		val = strtoul(tmp, &tmp2, 16);
		if (tmp2 == NULL || tmp == tmp2)
			continue; // parse failed

		switch (config_data[i].len) {
		case 1:
			*(u8 *)config_data[i].val = val;
			break;
		case 2:
			*(u16 *)config_data[i].val = val;
			break;
		case 4:
			*(u32 *)config_data[i].val = val;
			break;
		default:
			printf("menu_load_config: unhandled len %d for %s\n",
				 config_data[i].len, config_data[i].name);
			break;
		}
	}

	if (!is_game) {
		char *tmp = strstr(cfg, "lastcdimg = ");
		if (tmp != NULL) {
			tmp += 12;
			parse_str_val(last_selected_fname, tmp);
		}
	}

	menu_sync_config();

	// sync plugins
	for (i = bios_sel = 0; bioses[i] != NULL; i++)
		if (strcmp(Config.Bios, bioses[i]) == 0)
			{ bios_sel = i; break; }

	for (i = gpu_plugsel = 0; gpu_plugins[i] != NULL; i++)
		if (strcmp(Config.Gpu, gpu_plugins[i]) == 0)
			{ gpu_plugsel = i; break; }

	for (i = spu_plugsel = 0; spu_plugins[i] != NULL; i++)
		if (strcmp(Config.Spu, spu_plugins[i]) == 0)
			{ spu_plugsel = i; break; }

	keys_load_all(cfg);
	ret = 0;
fail_read:
	free(cfg);
fail:
	fclose(f);
	return ret;
}

// rrrr rggg gggb bbbb
static unsigned short fname2color(const char *fname)
{
	static const char *cdimg_exts[] = { ".bin", ".img", ".mdf", ".iso", ".cue", ".z", ".bz", ".znx", ".pbp" };
	static const char *other_exts[] = { ".ccd", ".toc", ".mds", ".sub", ".table", ".index", ".sbi" };
	const char *ext = strrchr(fname, '.');
	int i;

	if (ext == NULL)
		return 0xffff;
	for (i = 0; i < array_size(cdimg_exts); i++)
		if (strcasecmp(ext, cdimg_exts[i]) == 0)
			return 0x7bff;
	for (i = 0; i < array_size(other_exts); i++)
		if (strcasecmp(ext, other_exts[i]) == 0)
			return 0xa514;
	return 0xffff;
}

static void draw_savestate_bg(int slot);

static const char *filter_exts[] = {
	".mp3", ".MP3", ".txt", ".htm", "html",	".jpg", ".pnd"
};

#define MENU_ALIGN_LEFT
#define menu_init menu_init_common
#include "common/menu.c"
#undef menu_init

// a bit of black magic here
static void draw_savestate_bg(int slot)
{
	static const int psx_widths[8]  = { 256, 368, 320, 384, 512, 512, 640, 640 };
	int x, y, w, h;
	char fname[MAXPATHLEN];
	GPUFreeze_t *gpu;
	u16 *s, *d;
	gzFile f;
	int ret;
	u32 tmp;

	ret = get_state_filename(fname, sizeof(fname), slot);
	if (ret != 0)
		return;

	f = gzopen(fname, "rb");
	if (f == NULL)
		return;

	if (gzseek(f, 0x29933d, SEEK_SET) != 0x29933d) {
		fprintf(stderr, "gzseek failed\n");
		gzclose(f);
		return;
	}

	gpu = malloc(sizeof(*gpu));
	if (gpu == NULL) {
		gzclose(f);
		return;
	}

	ret = gzread(f, gpu, sizeof(*gpu));
	gzclose(f);
	if (ret != sizeof(*gpu)) {
		fprintf(stderr, "gzread failed\n");
		goto out;
	}

	memcpy(g_menubg_ptr, g_menubg_src_ptr, g_menuscreen_w * g_menuscreen_h * 2);

	if (gpu->ulStatus & 0x800000)
		goto out; // disabled

	x = gpu->ulControl[5] & 0x3ff;
	y = (gpu->ulControl[5] >> 10) & 0x1ff;
	s = (u16 *)gpu->psxVRam + y * 1024 + (x & ~1);
	w = psx_widths[(gpu->ulStatus >> 16) & 7];
	tmp = gpu->ulControl[7];
	h = ((tmp >> 10) & 0x3ff) - (tmp & 0x3ff);
	if (gpu->ulStatus & 0x80000) // doubleheight
		h *= 2;

	x = max(0, g_menuscreen_w - w) & ~3;
	y = max(0, g_menuscreen_h / 2 - h / 2);
	w = min(g_menuscreen_w, w);
	h = min(g_menuscreen_h, h);
	d = (u16 *)g_menubg_ptr + g_menuscreen_w * y + x;

	for (; h > 0; h--, d += g_menuscreen_w, s += 1024)
		if (gpu->ulStatus & 0x200000)
			bgr888_to_rgb565(d, s, w * 3);
		else
			bgr555_to_rgb565(d, s, w * 2);

out:
	free(gpu);
}

// ---------- pandora specific -----------

static const char pnd_script_base[] = "sudo -n /usr/pandora/scripts";
static char **pnd_filter_list;

static int get_cpu_clock(void)
{
	FILE *f;
	int ret = 0;
	f = fopen("/proc/pandora/cpu_mhz_max", "r");
	if (f) {
		fscanf(f, "%d", &ret);
		fclose(f);
	}
	return ret;
}

static void apply_cpu_clock(void)
{
	char buf[128];

	if (cpu_clock != 0 && cpu_clock != get_cpu_clock()) {
		snprintf(buf, sizeof(buf), "unset DISPLAY; echo y | %s/op_cpuspeed.sh %d",
			 pnd_script_base, cpu_clock);
		system(buf);
	}
}

static void apply_filter(int which)
{
	static int old = -1;
	char buf[128];
	int i;

	if (pnd_filter_list == NULL || which == old)
		return;

	for (i = 0; i < which; i++)
		if (pnd_filter_list[i] == NULL)
			return;

	if (pnd_filter_list[i] == NULL)
		return;

	snprintf(buf, sizeof(buf), "%s/op_videofir.sh %s", pnd_script_base, pnd_filter_list[i]);
	system(buf);
	old = which;
}

static void apply_lcdrate(int pal)
{
	static int old = -1;
	char buf[128];

	if (pal == old)
		return;

	snprintf(buf, sizeof(buf), "%s/op_lcdrate.sh %d",
			pnd_script_base, pal ? 50 : 60);
	system(buf);
	old = pal;
}

static menu_entry e_menu_gfx_options[];

static void pnd_menu_init(void)
{
	struct dirent *ent;
	int i, count = 0;
	char **mfilters;
	char buff[64];
	DIR *dir;

	cpu_clock_st = cpu_clock = get_cpu_clock();

	dir = opendir("/etc/pandora/conf/dss_fir");
	if (dir == NULL) {
		perror("filter opendir");
		return;
	}

	while (1) {
		errno = 0;
		ent = readdir(dir);
		if (ent == NULL) {
			if (errno != 0)
				perror("readdir");
			break;
		}

		if (ent->d_type != DT_REG && ent->d_type != DT_LNK)
			continue;

		count++;
	}

	if (count == 0)
		return;

	mfilters = calloc(count + 1, sizeof(mfilters[0]));
	if (mfilters == NULL)
		return;

	rewinddir(dir);
	for (i = 0; (ent = readdir(dir)); ) {
		size_t len;

		if (ent->d_type != DT_REG && ent->d_type != DT_LNK)
			continue;

		len = strlen(ent->d_name);

		// skip pre-HF5 extra files
		if (len >= 3 && strcmp(ent->d_name + len - 3, "_v3") == 0)
			continue;
		if (len >= 3 && strcmp(ent->d_name + len - 3, "_v5") == 0)
			continue;

		// have to cut "_up_h" for pre-HF5
		if (len > 5 && strcmp(ent->d_name + len - 5, "_up_h") == 0)
			len -= 5;

		if (len > sizeof(buff) - 1)
			continue;

		strncpy(buff, ent->d_name, len);
		buff[len] = 0;
		mfilters[i] = strdup(buff);
		if (mfilters[i] != NULL)
			i++;
	}
	closedir(dir);

	i = me_id2offset(e_menu_gfx_options, MA_OPT_FILTERING);
	e_menu_gfx_options[i].data = (void *)mfilters;
	pnd_filter_list = mfilters;
}

void menu_finish(void)
{
	cpu_clock = cpu_clock_st;
	apply_cpu_clock();
}

// -------------- key config --------------

me_bind_action me_ctrl_actions[] =
{
	{ "UP      ", 1 << DKEY_UP},
	{ "DOWN    ", 1 << DKEY_DOWN },
	{ "LEFT    ", 1 << DKEY_LEFT },
	{ "RIGHT   ", 1 << DKEY_RIGHT },
	{ "TRIANGLE", 1 << DKEY_TRIANGLE },
	{ "CIRCLE  ", 1 << DKEY_CIRCLE },
	{ "CROSS   ", 1 << DKEY_CROSS },
	{ "SQUARE  ", 1 << DKEY_SQUARE },
	{ "L1      ", 1 << DKEY_L1 },
	{ "R1      ", 1 << DKEY_R1 },
	{ "L2      ", 1 << DKEY_L2 },
	{ "R2      ", 1 << DKEY_R2 },
	{ "L3      ", 1 << DKEY_L3 },
	{ "R3      ", 1 << DKEY_R3 },
	{ "START   ", 1 << DKEY_START },
	{ "SELECT  ", 1 << DKEY_SELECT },
	{ NULL,       0 }
};

me_bind_action emuctrl_actions[] =
{
	{ "Save State       ", 1 << SACTION_SAVE_STATE },
	{ "Load State       ", 1 << SACTION_LOAD_STATE },
	{ "Prev Save Slot   ", 1 << SACTION_PREV_SSLOT },
	{ "Next Save Slot   ", 1 << SACTION_NEXT_SSLOT },
	{ "Toggle Frameskip ", 1 << SACTION_TOGGLE_FSKIP },
	{ "Take Screenshot  ", 1 << SACTION_SCREENSHOT },
	{ "Enter Menu       ", 1 << SACTION_ENTER_MENU },
	{ "Gun Trigger      ", 1 << SACTION_GUN_TRIGGER },
	{ "Gun A button     ", 1 << SACTION_GUN_A },
	{ "Gun B button     ", 1 << SACTION_GUN_B },
	{ "Gun Offscreen Trigger", 1 << SACTION_GUN_TRIGGER2 },
	{ NULL,                0 }
};

static char *mystrip(char *str)
{
	int i, len;

	len = strlen(str);
	for (i = 0; i < len; i++)
		if (str[i] != ' ') break;
	if (i > 0) memmove(str, str + i, len - i + 1);

	len = strlen(str);
	for (i = len - 1; i >= 0; i--)
		if (str[i] != ' ') break;
	str[i+1] = 0;

	return str;
}

static void get_line(char *d, size_t size, const char *s)
{
	const char *pe;
	size_t len;

	for (pe = s; *pe != '\r' && *pe != '\n' && *pe != 0; pe++)
		;
	len = pe - s;
	if (len > size - 1)
		len = size - 1;
	strncpy(d, s, len);
	d[len] = 0;

	mystrip(d);
}

static void keys_write_all(FILE *f)
{
	int d;

	for (d = 0; d < IN_MAX_DEVS; d++)
	{
		const int *binds = in_get_dev_binds(d);
		const char *name = in_get_dev_name(d, 0, 0);
		int k, count = 0;

		if (binds == NULL || name == NULL)
			continue;

		fprintf(f, "binddev = %s\n", name);
		in_get_config(d, IN_CFG_BIND_COUNT, &count);

		for (k = 0; k < count; k++)
		{
			int i, kbinds, mask;
			char act[32];

			act[0] = act[31] = 0;
			name = in_get_key_name(d, k);

			kbinds = binds[IN_BIND_OFFS(k, IN_BINDTYPE_PLAYER12)];
			for (i = 0; kbinds && i < ARRAY_SIZE(me_ctrl_actions) - 1; i++) {
				mask = me_ctrl_actions[i].mask;
				if (mask & kbinds) {
					strncpy(act, me_ctrl_actions[i].name, 31);
					fprintf(f, "bind %s = player1 %s\n", name, mystrip(act));
					kbinds &= ~mask;
				}
				mask = me_ctrl_actions[i].mask << 16;
				if (mask & kbinds) {
					strncpy(act, me_ctrl_actions[i].name, 31);
					fprintf(f, "bind %s = player2 %s\n", name, mystrip(act));
					kbinds &= ~mask;
				}
			}

			kbinds = binds[IN_BIND_OFFS(k, IN_BINDTYPE_EMU)];
			for (i = 0; kbinds && i < ARRAY_SIZE(emuctrl_actions) - 1; i++) {
				mask = emuctrl_actions[i].mask;
				if (mask & kbinds) {
					strncpy(act, emuctrl_actions[i].name, 31);
					fprintf(f, "bind %s = %s\n", name, mystrip(act));
					kbinds &= ~mask;
				}
			}
		}
	}
}

static int parse_bind_val(const char *val, int *type)
{
	int i;

	*type = IN_BINDTYPE_NONE;
	if (val[0] == 0)
		return 0;
	
	if (strncasecmp(val, "player", 6) == 0)
	{
		int player, shift = 0;
		player = atoi(val + 6) - 1;

		if ((unsigned int)player > 1)
			return -1;
		if (player == 1)
			shift = 16;

		*type = IN_BINDTYPE_PLAYER12;
		for (i = 0; me_ctrl_actions[i].name != NULL; i++) {
			if (strncasecmp(me_ctrl_actions[i].name, val + 8, strlen(val + 8)) == 0)
				return me_ctrl_actions[i].mask << shift;
		}
	}
	for (i = 0; emuctrl_actions[i].name != NULL; i++) {
		if (strncasecmp(emuctrl_actions[i].name, val, strlen(val)) == 0) {
			*type = IN_BINDTYPE_EMU;
			return emuctrl_actions[i].mask;
		}
	}

	return -1;
}

static void keys_load_all(const char *cfg)
{
	char dev[256], key[128], *act;
	const char *p;
	int bind, bindtype;
	int dev_id;

	p = cfg;
	while (p != NULL && (p = strstr(p, "binddev = ")) != NULL) {
		p += 10;

		get_line(dev, sizeof(dev), p);
		dev_id = in_config_parse_dev(dev);
		if (dev_id < 0) {
			printf("input: can't handle dev: %s\n", dev);
			continue;
		}

		in_unbind_all(dev_id, -1, -1);
		while ((p = strstr(p, "bind"))) {
			if (strncmp(p, "binddev = ", 10) == 0)
				break;

			p += 4;
			if (*p != ' ') {
				printf("input: parse error: %16s..\n", p);
				continue;
			}

			get_line(key, sizeof(key), p);
			act = strchr(key, '=');
			if (act == NULL) {
				printf("parse failed: %16s..\n", p);
				continue;
			}
			*act = 0;
			act++;
			mystrip(key);
			mystrip(act);

			bind = parse_bind_val(act, &bindtype);
			if (bind != -1 && bind != 0) {
				//printf("bind #%d '%s' %08x (%s)\n", dev_id, key, bind, act);
				in_config_bind_key(dev_id, key, bind, bindtype);
			}
			else
				lprintf("config: unhandled action \"%s\"\n", act);
		}
	}
	in_clean_binds();
}

static int key_config_loop_wrap(int id, int keys)
{
	switch (id) {
		case MA_CTRL_PLAYER1:
			key_config_loop(me_ctrl_actions, array_size(me_ctrl_actions) - 1, 0);
			break;
		case MA_CTRL_PLAYER2:
			key_config_loop(me_ctrl_actions, array_size(me_ctrl_actions) - 1, 1);
			break;
		case MA_CTRL_EMU:
			key_config_loop(emuctrl_actions, array_size(emuctrl_actions) - 1, -1);
			break;
		default:
			break;
	}
	return 0;
}

static const char *mgn_dev_name(int id, int *offs)
{
	const char *name = NULL;
	static int it = 0;

	if (id == MA_CTRL_DEV_FIRST)
		it = 0;

	for (; it < IN_MAX_DEVS; it++) {
		name = in_get_dev_name(it, 1, 1);
		if (name != NULL)
			break;
	}

	it++;
	return name;
}

static const char *mgn_saveloadcfg(int id, int *offs)
{
	return "";
}

static int mh_savecfg(int id, int keys)
{
	if (menu_write_config(id == MA_OPT_SAVECFG_GAME ? 1 : 0) == 0)
		me_update_msg("config saved");
	else
		me_update_msg("failed to write config");

	return 1;
}

static int mh_input_rescan(int id, int keys)
{
	//menu_sync_config();
	pandora_rescan_inputs();
	me_update_msg("rescan complete.");

	return 0;
}

static const char *men_in_type_sel[] = {
	"Standard (SCPH-1080)",
	"Analog (SCPH-1150)",
	"GunCon",
	NULL
};
static const char h_nub_btns[] = "Experimental, keep this OFF if unsure. Select rescan after change.";
static const char h_notsgun[] =  "Don't trigger (shoot) when touching screen in gun games.";

static menu_entry e_menu_keyconfig[] =
{
	mee_handler_id("Player 1",              MA_CTRL_PLAYER1,    key_config_loop_wrap),
	mee_handler_id("Player 2",              MA_CTRL_PLAYER2,    key_config_loop_wrap),
	mee_handler_id("Emulator/Gun controls", MA_CTRL_EMU,        key_config_loop_wrap),
	mee_label     (""),
	mee_enum      ("Port 1 device",     0, in_type_sel1,    men_in_type_sel),
	mee_enum      ("Port 2 device",     0, in_type_sel2,    men_in_type_sel),
	mee_onoff_h   ("Nubs as buttons",   0, in_evdev_allow_abs_only, 1, h_nub_btns),
	mee_onoff_h   ("No TS Gun trigger", 0, g_opts, OPT_TSGUN_NOTRIGGER, h_notsgun),
	mee_cust_nosave("Save global config",       MA_OPT_SAVECFG,      mh_savecfg, mgn_saveloadcfg),
	mee_cust_nosave("Save cfg for loaded game", MA_OPT_SAVECFG_GAME, mh_savecfg, mgn_saveloadcfg),
	mee_handler   ("Rescan devices",  mh_input_rescan),
	mee_label     (""),
	mee_label     ("Input devices:"),
	mee_label_mk  (MA_CTRL_DEV_FIRST, mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_end,
};

static int menu_loop_keyconfig(int id, int keys)
{
	static int sel = 0;

//	me_enable(e_menu_keyconfig, MA_OPT_SAVECFG_GAME, ready_to_go && CdromId[0]);
	me_loop(e_menu_keyconfig, &sel);
	return 0;
}

// ------------ gfx options menu ------------

static const char *men_scaler[] = { "1x1", "scaled 4:3", "fullscreen", "custom", NULL };
static const char h_cscaler[]   = "Displays the scaler layer, you can resize it\n"
				  "using d-pad or move it using R+d-pad";
static const char *men_dummy[] = { NULL };

static int menu_loop_cscaler(int id, int keys)
{
	unsigned int inp;

	scaling = SCALE_CUSTOM;

	omap_enable_layer(1);

	for (;;)
	{
		menu_draw_begin(0);
		memset(g_menuscreen_ptr, 4, g_menuscreen_w * g_menuscreen_h * 2);
		text_out16(2, 2, "%d,%d", g_layer_x, g_layer_y);
		text_out16(2, 480 - 18, "%dx%d | d-pad: resize, R+d-pad: move",	g_layer_w, g_layer_h);
		menu_draw_end();

		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_R|PBTN_MOK|PBTN_MBACK, 40);
		if (inp & PBTN_UP)    g_layer_y--;
		if (inp & PBTN_DOWN)  g_layer_y++;
		if (inp & PBTN_LEFT)  g_layer_x--;
		if (inp & PBTN_RIGHT) g_layer_x++;
		if (!(inp & PBTN_R)) {
			if (inp & PBTN_UP)    g_layer_h += 2;
			if (inp & PBTN_DOWN)  g_layer_h -= 2;
			if (inp & PBTN_LEFT)  g_layer_w += 2;
			if (inp & PBTN_RIGHT) g_layer_w -= 2;
		}
		if (inp & (PBTN_MOK|PBTN_MBACK))
			break;

		if (inp & (PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT)) {
			if (g_layer_x < 0)   g_layer_x = 0;
			if (g_layer_x > 640) g_layer_x = 640;
			if (g_layer_y < 0)   g_layer_y = 0;
			if (g_layer_y > 420) g_layer_y = 420;
			if (g_layer_w < 160) g_layer_w = 160;
			if (g_layer_h < 60)  g_layer_h = 60;
			if (g_layer_x + g_layer_w > 800)
				g_layer_w = 800 - g_layer_x;
			if (g_layer_y + g_layer_h > 480)
				g_layer_h = 480 - g_layer_y;
			omap_enable_layer(1);
		}
	}

	omap_enable_layer(0);

	return 0;
}

static menu_entry e_menu_gfx_options[] =
{
	mee_enum      ("Scaler",                   0, scaling, men_scaler),
	mee_enum      ("Filter",                   MA_OPT_FILTERING, filter, men_dummy),
//	mee_onoff     ("Vsync",                    0, vsync, 1),
	mee_cust_h    ("Setup custom scaler",      0, menu_loop_cscaler, NULL, h_cscaler),
	mee_end,
};

static int menu_loop_gfx_options(int id, int keys)
{
	static int sel = 0;

	me_loop(e_menu_gfx_options, &sel);

	return 0;
}

// ------------ bios/plugins ------------

static const char *men_gpu_dithering[] = { "None", "Game dependant", "Always", NULL };
static const char h_gpu_0[]            = "Needed for Chrono Cross";
static const char h_gpu_1[]            = "Capcom fighting games";
static const char h_gpu_2[]            = "Black screens in Lunar";
static const char h_gpu_3[]            = "Compatibility mode";
static const char h_gpu_6[]            = "Pandemonium 2";
static const char h_gpu_7[]            = "Skip every second frame";
static const char h_gpu_8[]            = "Needed by Dark Forces";
static const char h_gpu_9[]            = "better g-colors, worse textures";
static const char h_gpu_10[]           = "Toggle busy flags after drawing";

static menu_entry e_menu_plugin_gpu[] =
{
	mee_enum      ("Dithering",                  0, pl_rearmed_cbs.gpu_peops.iUseDither, men_gpu_dithering),
	mee_onoff_h   ("Odd/even bit hack",          0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<0, h_gpu_0),
	mee_onoff_h   ("Expand screen width",        0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<1, h_gpu_1),
	mee_onoff_h   ("Ignore brightness color",    0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<2, h_gpu_2),
	mee_onoff_h   ("Disable coordinate check",   0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<3, h_gpu_3),
	mee_onoff_h   ("Lazy screen update",         0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<6, h_gpu_6),
	mee_onoff_h   ("Old frame skipping",         0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<7, h_gpu_7),
	mee_onoff_h   ("Repeated flat tex triangles ",0,pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<8, h_gpu_8),
	mee_onoff_h   ("Draw quads with triangles",  0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<9, h_gpu_9),
	mee_onoff_h   ("Fake 'gpu busy' states",     0, pl_rearmed_cbs.gpu_peops.dwActFixes, 1<<10, h_gpu_10),
	mee_end,
};

static int menu_loop_plugin_gpu(int id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_plugin_gpu, &sel);
	return 0;
}

static const char *men_spu_interp[] = { "None", "Simple", "Gaussian", "Cubic", NULL };
static const char h_spu_volboost[]  = "Large values cause distortion";
static const char h_spu_irq_wait[]  = "Wait for CPU (recommended set to ON)";
static const char h_spu_thread[]    = "Run sound emulation in main thread (recommended)";

static menu_entry e_menu_plugin_spu[] =
{
	mee_range_h   ("Volume boost",              0, volume_boost, -5, 30, h_spu_volboost),
	mee_onoff     ("Reverb",                    0, iUseReverb, 2),
	mee_enum      ("Interpolation",             0, iUseInterpolation, men_spu_interp),
	mee_onoff     ("Adjust XA pitch",           0, iXAPitch, 1),
	mee_onoff_h   ("SPU IRQ Wait",              0, iSPUIRQWait, 1, h_spu_irq_wait),
	mee_onoff_h   ("Sound in main thread",      0, iUseTimer, 2, h_spu_thread),
	mee_end,
};

static int menu_loop_plugin_spu(int id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_plugin_spu, &sel);
	return 0;
}

static const char h_bios[]       = "HLE is simulated BIOS. BIOS selection is saved in savestates\n"
				   "and can't be changed there. Must save config and reload\n"
				   "the game for change to take effect";
static const char h_plugin_xpu[] = "Must save config and reload the game\n"
				   "for plugin change to take effect";
static const char h_gpu[]        = "Configure P.E.Op.S. SoftGL Driver V1.17";
static const char h_spu[]        = "Configure built-in P.E.Op.S. Sound Driver V1.7";

static menu_entry e_menu_plugin_options[] =
{
	mee_enum_h    ("BIOS",                          0, bios_sel, bioses, h_bios),
	mee_enum_h    ("GPU plugin",                    0, gpu_plugsel, gpu_plugins, h_plugin_xpu),
	mee_enum_h    ("SPU plugin",                    0, spu_plugsel, spu_plugins, h_plugin_xpu),
	mee_handler_h ("Configure gpu_peops plugin",    menu_loop_plugin_gpu, h_gpu),
	mee_handler_h ("Configure built-in SPU plugin", menu_loop_plugin_spu, h_spu),
	mee_end,
};

static menu_entry e_menu_main2[];

static int menu_loop_plugin_options(int id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_plugin_options, &sel);

	// sync BIOS/plugins
	snprintf(Config.Bios, sizeof(Config.Bios), "%s", bioses[bios_sel]);
	snprintf(Config.Gpu, sizeof(Config.Gpu), "%s", gpu_plugins[gpu_plugsel]);
	snprintf(Config.Spu, sizeof(Config.Spu), "%s", spu_plugins[spu_plugsel]);
	me_enable(e_menu_main2, MA_MAIN_RUN_BIOS, bios_sel != 0);

	return 0;
}

// ------------ adv options menu ------------

static const char *men_cfg_cdrr[] = { "Auto", "ON", "OFF", NULL };
static const char h_cfg_cpul[]   = "Shows CPU usage in %";
static const char h_cfg_spu[]    = "Shows active SPU channels\n"
				   "(green: normal, red: fmod, blue: noise)";
static const char h_cfg_fl[]     = "Frame Limiter keeps the game from running too fast";
static const char h_cfg_xa[]     = "Disables XA sound, which can sometimes improve performance";
static const char h_cfg_cdda[]   = "Disable CD Audio for a performance boost\n"
				   "(proper .cue/.bin dump is needed otherwise)";
static const char h_cfg_sio[]    = "You should not need this, breaks games";
static const char h_cfg_spuirq[] = "Compatibility tweak; should be left off";
static const char h_cfg_rcnt1[]  = "Parasite Eve 2, Vandal Hearts 1/2 Fix\n"
				   "(timing hack, breaks other games)";
static const char h_cfg_rcnt2[]  = "InuYasha Sengoku Battle Fix\n"
				   "(timing hack, breaks other games)";
static const char h_cfg_cdrr[]   = "Compatibility tweak (fixes Team Buddies, maybe more)\n"
				   "(CD timing hack, breaks FMVs)";
static const char h_cfg_nodrc[]  = "Disable dynamic recompiler and use interpreter\n"
				   "Might be useful to overcome some dynarec bugs";

static menu_entry e_menu_adv_options[] =
{
	mee_onoff_h   ("Show CPU load",          0, g_opts, OPT_SHOWCPU, h_cfg_cpul),
	mee_onoff_h   ("Show SPU channels",      0, g_opts, OPT_SHOWSPU, h_cfg_spu),
	mee_onoff_h   ("Disable Frame Limiter",  0, g_opts, OPT_NO_FRAMELIM, h_cfg_fl),
	mee_onoff_h   ("Disable XA Decoding",    0, Config.Xa, 1, h_cfg_xa),
	mee_onoff_h   ("Disable CD Audio",       0, Config.Cdda, 1, h_cfg_cdda),
	mee_onoff_h   ("SIO IRQ Always Enabled", 0, Config.Sio, 1, h_cfg_sio),
	mee_onoff_h   ("SPU IRQ Always Enabled", 0, Config.SpuIrq, 1, h_cfg_spuirq),
	mee_onoff_h   ("Rootcounter hack",       0, Config.RCntFix, 1, h_cfg_rcnt1),
	mee_onoff_h   ("Rootcounter hack 2",     0, Config.VSyncWA, 1, h_cfg_rcnt2),
	mee_enum_h    ("CD read reschedule hack",0, Config.CdrReschedule, men_cfg_cdrr, h_cfg_cdrr),
	mee_onoff_h   ("Disable dynarec (slow!)",0, Config.Cpu, 1, h_cfg_nodrc),
	mee_end,
};

static int menu_loop_adv_options(int id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_adv_options, &sel);
	return 0;
}

// ------------ options menu ------------

static int mh_restore_defaults(int id, int keys)
{
	menu_set_defconfig();
	me_update_msg("defaults restored");
	return 1;
}

static const char *men_region[]       = { "Auto", "NTSC", "PAL", NULL };
/*
static const char *men_confirm_save[] = { "OFF", "writes", "loads", "both", NULL };
static const char h_confirm_save[]    = "Ask for confirmation when overwriting save,\n"
					"loading state or both";
*/
static const char h_restore_def[]     = "Switches back to default / recommended\n"
					"configuration";
static const char h_frameskip[]       = "Warning: frameskip sometimes causes glitches\n";

static menu_entry e_menu_options[] =
{
//	mee_range     ("Save slot",                0, state_slot, 0, 9),
//	mee_enum_h    ("Confirm savestate",        0, dummy, men_confirm_save, h_confirm_save),
	mee_onoff_h   ("Frameskip",                0, pl_rearmed_cbs.frameskip, 1, h_frameskip),
	mee_onoff     ("Show FPS",                 0, g_opts, OPT_SHOWFPS),
	mee_enum      ("Region",                   0, region, men_region),
	mee_range     ("CPU clock",                MA_OPT_CPU_CLOCKS, cpu_clock, 20, 5000),
	mee_handler   ("[Display]",                menu_loop_gfx_options),
	mee_handler   ("[BIOS/Plugins]",           menu_loop_plugin_options),
	mee_handler   ("[Advanced]",               menu_loop_adv_options),
	mee_cust_nosave("Save global config",      MA_OPT_SAVECFG,      mh_savecfg, mgn_saveloadcfg),
	mee_cust_nosave("Save cfg for loaded game",MA_OPT_SAVECFG_GAME, mh_savecfg, mgn_saveloadcfg),
	mee_handler_h ("Restore default config",   mh_restore_defaults, h_restore_def),
	mee_end,
};

static int menu_loop_options(int id, int keys)
{
	static int sel = 0;
	int i;

	i = me_id2offset(e_menu_options, MA_OPT_CPU_CLOCKS);
	e_menu_options[i].enabled = cpu_clock != 0 ? 1 : 0;
	me_enable(e_menu_options, MA_OPT_SAVECFG_GAME, ready_to_go && CdromId[0]);

	me_loop(e_menu_options, &sel);

	return 0;
}

// ------------ debug menu ------------

static void draw_frame_debug(GPUFreeze_t *gpuf)
{
	int w = min(g_menuscreen_w, 1024);
	int h = min(g_menuscreen_h, 512);
	u16 *d = g_menuscreen_ptr;
	u16 *s = (u16 *)gpuf->psxVRam;
	char buff[64];
	int ty = 1;

	gpuf->ulFreezeVersion = 1;
	if (GPU_freeze != NULL)
		GPU_freeze(1, gpuf);

	for (; h > 0; h--, d += g_menuscreen_w, s += 1024)
		bgr555_to_rgb565(d, s, w * 2);

	smalltext_out16(4, 1, "build: "__DATE__ " " __TIME__ " " REV, 0xe7fc);
	snprintf(buff, sizeof(buff), "GPU sr: %08x", gpuf->ulStatus);
	smalltext_out16(4, (ty += me_sfont_h), buff, 0xe7fc);
	snprintf(buff, sizeof(buff), "PC/SP: %08x %08x", psxRegs.pc, psxRegs.GPR.n.sp);
	smalltext_out16(4, (ty += me_sfont_h), buff, 0xe7fc);
}

static void debug_menu_loop(void)
{
	GPUFreeze_t *gpuf;
	int inp;

	gpuf = malloc(sizeof(*gpuf));
	if (gpuf == NULL)
		return;

	while (1)
	{
		menu_draw_begin(0);
		draw_frame_debug(gpuf);
		menu_draw_end();

		inp = in_menu_wait(PBTN_MOK|PBTN_MBACK|PBTN_MA2|PBTN_MA3|PBTN_L|PBTN_R |
					PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT, 70);
		if (inp & PBTN_MBACK)
			break;
	}

	free(gpuf);
}

// --------- memcard manager ---------

static void draw_mc_icon(int dx, int dy, const u16 *s)
{
	u16 *d;
	int x, y, l, p;
	
	d = (u16 *)g_menuscreen_ptr + g_menuscreen_w * dy + dx;

	for (y = 0; y < 16; y++, s += 16) {
		for (l = 0; l < 2; l++, d += g_menuscreen_w) {
			for (x = 0; x < 16; x++) {
				p = s[x];
				d[x*2] = d[x*2 + 1] = ((p & 0x7c00) >> 10)
					| ((p & 0x03e0) << 1) | ((p & 0x1f) << 11);
			}
		}
	}
}

static void draw_mc_bg(void)
{
	McdBlock *blocks1, *blocks2;
	int maxicons = 15;
	int i, y, row2;

	blocks1 = malloc(15 * sizeof(blocks1[0]));
	blocks2 = malloc(15 * sizeof(blocks1[0]));
	if (blocks1 == NULL || blocks2 == NULL)
		goto out;

	for (i = 0; i < 15; i++) {
		GetMcdBlockInfo(1, i + 1, &blocks1[i]);
		GetMcdBlockInfo(2, i + 1, &blocks2[i]);
	}

	menu_draw_begin(1);

	memcpy(g_menuscreen_ptr, g_menubg_src_ptr, g_menuscreen_w * g_menuscreen_h * 2);

	y = g_menuscreen_h / 2 - 15 * 32 / 2;
	if (y < 0) {
		// doesn't fit..
		y = 0;
		maxicons = g_menuscreen_h / 32;
	}

	row2 = g_menuscreen_w / 2;
	for (i = 0; i < maxicons; i++) {
		draw_mc_icon(8, y + i * 32, (u16 *)blocks1[i].Icon);
		smalltext_out16(10+32, y + i * 32 + 8, blocks1[i].sTitle, 0xf71e);

		draw_mc_icon(row2 + 8, y + i * 32, (u16 *)blocks2[i].Icon);
		smalltext_out16(row2 + 10+32, y + i * 32 + 8, blocks2[i].sTitle, 0xf71e);
	}

	menu_darken_bg(g_menubg_ptr, g_menuscreen_ptr, g_menuscreen_w * g_menuscreen_h, 0);

	menu_draw_end();
out:
	free(blocks1);
	free(blocks2);
}

static void handle_memcard_sel(void)
{
	Config.Mcd1[0] = 0;
	if (memcard1_sel != 0)
		snprintf(Config.Mcd1, sizeof(Config.Mcd1), ".%s%s", MEMCARD_DIR, memcards[memcard1_sel]);
	Config.Mcd2[0] = 0;
	if (memcard2_sel != 0)
		snprintf(Config.Mcd2, sizeof(Config.Mcd2), ".%s%s", MEMCARD_DIR, memcards[memcard2_sel]);
	LoadMcds(Config.Mcd1, Config.Mcd2);
	draw_mc_bg();
}

static menu_entry e_memcard_options[] =
{
	mee_enum("Memory card 1", 0, memcard1_sel, memcards),
	mee_enum("Memory card 2", 0, memcard2_sel, memcards),
	mee_end,
};

static int menu_loop_memcards(int id, int keys)
{
	static int sel = 0;
	char *p;
	int i;

	memcard1_sel = memcard2_sel = 0;
	p = strrchr(Config.Mcd1, '/');
	if (p != NULL)
		for (i = 0; memcards[i] != NULL; i++)
			if (strcmp(p + 1, memcards[i]) == 0)
				{ memcard1_sel = i; break; }
	p = strrchr(Config.Mcd2, '/');
	if (p != NULL)
		for (i = 0; memcards[i] != NULL; i++)
			if (strcmp(p + 1, memcards[i]) == 0)
				{ memcard2_sel = i; break; }

	me_loop_d(e_memcard_options, &sel, handle_memcard_sel, NULL);

	memcpy(g_menubg_ptr, g_menubg_src_ptr, g_menuscreen_w * g_menuscreen_h * 2);

	return 0;
}

// --------- main menu help ----------

static void menu_bios_warn(void)
{
	int inp;
	static const char msg[] =
		"You don't seem to have copied any BIOS files to\n"
		"<SD card>/pandora/appdata/pcsx_rearmed/bios/\n\n"
		"While many games work fine with fake (HLE) BIOS,\n"
		"others (like MGS and FF8) require BIOS to work.\n"
		"After copying the file, you'll also need to\n"
		"select it in the emu's options->[BIOS/Plugins]\n\n"
		"The file is usually named SCPH1001.BIN, but\n"
		"other not compressed files can be used too.\n\n"
		"Press (B) or (X) to continue";

	while (1)
	{
		draw_menu_message(msg, NULL);

		inp = in_menu_wait(PBTN_MOK|PBTN_MBACK, 70);
		if (inp & (PBTN_MBACK|PBTN_MOK))
			return;
	}
}

// ------------ main menu ------------

void OnFile_Exit();

static void draw_frame_main(void)
{
	if (CdromId[0] != 0) {
		char buff[64];
		snprintf(buff, sizeof(buff), "%.32s/%.9s (running as %s, with %s)",
			 get_cd_label(), CdromId, Config.PsxType ? "PAL" : "NTSC",
			 Config.HLE ? "HLE" : "BIOS");
		smalltext_out16(4, 1, buff, 0x105f);
	}
}

static void draw_frame_credits(void)
{
	smalltext_out16(4, 1, "build: "__DATE__ " " __TIME__ " " REV, 0xe7fc);
}

static const char credits_text[] = 
	"PCSX-ReARMed\n\n"
	"(C) 1999-2003 PCSX Team\n"
	"(C) 2005-2009 PCSX-df Team\n"
	"(C) 2009-2011 PCSX-Reloaded Team\n\n"
	"GPU and SPU code by Pete Bernert\n"
	"  and the P.E.Op.S. team\n"
	"ARM recompiler (C) 2009-2011 Ari64\n"
	"PCSX4ALL plugins by PCSX4ALL team\n"
	"  Chui, Franxis, Unai\n\n"
	"integration, optimization and\n"
	"  frontend (C) 2010-2011 notaz\n";

static int reset_game(void)
{
	// sanity check
	if (bios_sel == 0 && !Config.HLE)
		return -1;

	ClosePlugins();
	OpenPlugins();
	SysReset();
	if (CheckCdrom() != -1) {
		LoadCdrom();
	}
	return 0;
}

static int reload_plugins(const char *cdimg)
{
	pl_vout_buf = NULL;

	ClosePlugins();

	set_cd_image(cdimg);
	LoadPlugins();
	pcnt_hook_plugins();
	NetOpened = 0;
	if (OpenPlugins() == -1) {
		me_update_msg("failed to open plugins");
		return -1;
	}
	plugin_call_rearmed_cbs();

	CdromId[0] = '\0';
	CdromLabel[0] = '\0';

	return 0;
}

static int run_bios(void)
{
	if (bios_sel == 0)
		return -1;

	ready_to_go = 0;
	if (reload_plugins(NULL) != 0)
		return -1;
	SysReset();

	ready_to_go = 1;
	return 0;
}

static int run_exe(void)
{
	const char *fname;

	fname = menu_loop_romsel(last_selected_fname, sizeof(last_selected_fname));
	if (fname == NULL)
		return -1;

	ready_to_go = 0;
	if (reload_plugins(NULL) != 0)
		return -1;

	SysReset();
	if (Load(fname) != 0) {
		me_update_msg("exe load failed, bad file?");
		printf("meh\n");
		return -1;
	}

	ready_to_go = 1;
	return 0;
}

static int run_cd_image(const char *fname)
{
	ready_to_go = 0;
	reload_plugins(fname);

	if (CheckCdrom() == -1) {
		// Only check the CD if we are starting the console with a CD
		ClosePlugins();
		me_update_msg("unsupported/invalid CD image");
		return -1;
	}

	SysReset();

	// Read main executable directly from CDRom and start it
	if (LoadCdrom() == -1) {
		ClosePlugins();
		me_update_msg("failed to load CD image");
		return -1;
	}

	ready_to_go = 1;
	return 0;
}

static int romsel_run(void)
{
	int prev_gpu, prev_spu;
	const char *fname;

	fname = menu_loop_romsel(last_selected_fname, sizeof(last_selected_fname));
	if (fname == NULL)
		return -1;

	printf("selected file: %s\n", fname);

	new_dynarec_clear_full();

	if (run_cd_image(fname) != 0)
		return -1;

	prev_gpu = gpu_plugsel;
	prev_spu = spu_plugsel;
	if (menu_load_config(1) != 0)
		menu_load_config(0);

	// check for plugin changes, have to repeat
	// loading if game config changed plugins to reload them
	if (prev_gpu != gpu_plugsel || prev_spu != spu_plugsel) {
		printf("plugin change detected, reloading plugins..\n");
		if (run_cd_image(fname) != 0)
			return -1;
	}

	strcpy(last_selected_fname, rom_fname_reload);
	return 0;
}

static int swap_cd_image(void)
{
	char *fname;

	fname = menu_loop_romsel(last_selected_fname, sizeof(last_selected_fname));
	if (fname == NULL)
		return -1;

	printf("selected file: %s\n", fname);

	CdromId[0] = '\0';
	CdromLabel[0] = '\0';

	set_cd_image(fname);
	if (ReloadCdromPlugin() < 0) {
		me_update_msg("failed to load cdr plugin");
		return -1;
	}
	if (CDR_open() < 0) {
		me_update_msg("failed to open cdr plugin");
		return -1;
	}

	SetCdOpenCaseTime(time(NULL) + 2);
	LidInterrupt();

	strcpy(last_selected_fname, rom_fname_reload);
	return 0;
}

static int main_menu_handler(int id, int keys)
{
	switch (id)
	{
	case MA_MAIN_RESUME_GAME:
		if (ready_to_go)
			return 1;
		break;
	case MA_MAIN_SAVE_STATE:
		if (ready_to_go)
			return menu_loop_savestate(0);
		break;
	case MA_MAIN_LOAD_STATE:
		if (ready_to_go)
			return menu_loop_savestate(1);
		break;
	case MA_MAIN_RESET_GAME:
		if (ready_to_go && reset_game() == 0)
			return 1;
		break;
	case MA_MAIN_LOAD_ROM:
		if (romsel_run() == 0)
			return 1;
		break;
	case MA_MAIN_SWAP_CD:
		if (swap_cd_image() == 0)
			return 1;
		break;
	case MA_MAIN_RUN_BIOS:
		if (run_bios() == 0)
			return 1;
		break;
	case MA_MAIN_RUN_EXE:
		if (run_exe() == 0)
			return 1;
		break;
	case MA_MAIN_CREDITS:
		draw_menu_message(credits_text, draw_frame_credits);
		in_menu_wait(PBTN_MOK|PBTN_MBACK, 70);
		break;
	case MA_MAIN_EXIT:
		OnFile_Exit();
		break;
	default:
		lprintf("%s: something unknown selected\n", __FUNCTION__);
		break;
	}

	return 0;
}

static menu_entry e_menu_main2[] =
{
	mee_handler_id("Change CD image",    MA_MAIN_SWAP_CD,     main_menu_handler),
	mee_handler_id("Run BIOS",           MA_MAIN_RUN_BIOS,    main_menu_handler),
	mee_handler_id("Run EXE",            MA_MAIN_RUN_EXE,     main_menu_handler),
	mee_handler   ("Memcard manager",    menu_loop_memcards),
	mee_end,
};

static int main_menu2_handler(int id, int keys)
{
	static int sel = 0;

	me_enable(e_menu_main2, MA_MAIN_SWAP_CD,  ready_to_go);
	me_enable(e_menu_main2, MA_MAIN_RUN_BIOS, bios_sel != 0);

	return me_loop_d(e_menu_main2, &sel, NULL, draw_frame_main);
}

static const char h_extra[] = "Change CD, manage memcards..\n";

static menu_entry e_menu_main[] =
{
	mee_label     (""),
	mee_label     (""),
	mee_handler_id("Resume game",        MA_MAIN_RESUME_GAME, main_menu_handler),
	mee_handler_id("Save State",         MA_MAIN_SAVE_STATE,  main_menu_handler),
	mee_handler_id("Load State",         MA_MAIN_LOAD_STATE,  main_menu_handler),
	mee_handler_id("Reset game",         MA_MAIN_RESET_GAME,  main_menu_handler),
	mee_handler_id("Load CD image",      MA_MAIN_LOAD_ROM,    main_menu_handler),
	mee_handler   ("Options",            menu_loop_options),
	mee_handler   ("Controls",           menu_loop_keyconfig),
	mee_handler_h ("Extra stuff",        main_menu2_handler,  h_extra),
	mee_handler_id("Credits",            MA_MAIN_CREDITS,     main_menu_handler),
	mee_handler_id("Exit",               MA_MAIN_EXIT,        main_menu_handler),
	mee_end,
};

// ----------------------------

static void menu_leave_emu(void);

void menu_loop(void)
{
	static int sel = 0;

	menu_leave_emu();

	if (bioses[1] == NULL && !warned_about_bios) {
		menu_bios_warn();
		warned_about_bios = 1;
	}

	me_enable(e_menu_main, MA_MAIN_RESUME_GAME, ready_to_go);
	me_enable(e_menu_main, MA_MAIN_SAVE_STATE,  ready_to_go && CdromId[0]);
	me_enable(e_menu_main, MA_MAIN_LOAD_STATE,  ready_to_go && CdromId[0]);
	me_enable(e_menu_main, MA_MAIN_RESET_GAME,  ready_to_go);

	in_set_config_int(0, IN_CFG_BLOCKING, 1);

	do {
		me_loop_d(e_menu_main, &sel, NULL, draw_frame_main);
	} while (!ready_to_go);

	/* wait until menu, ok, back is released */
	while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	in_set_config_int(0, IN_CFG_BLOCKING, 0);

	menu_prepare_emu();
}

static int qsort_strcmp(const void *p1, const void *p2)
{
	char * const *s1 = (char * const *)p1;
	char * const *s2 = (char * const *)p2;
	return strcasecmp(*s1, *s2);
}

static void scan_bios_plugins(void)
{
	char fname[MAXPATHLEN];
	struct dirent *ent;
	int bios_i, gpu_i, spu_i, mc_i;
	char *p;
	DIR *dir;

	bioses[0] = "HLE";
	gpu_plugins[0] = "builtin_gpu";
	spu_plugins[0] = "builtin_spu";
	memcards[0] = "(none)";
	bios_i = gpu_i = spu_i = mc_i = 1;

	snprintf(fname, sizeof(fname), "%s/", Config.BiosDir);
	dir = opendir(fname);
	if (dir == NULL) {
		perror("scan_bios_plugins bios opendir");
		goto do_plugins;
	}

	while (1) {
		struct stat st;

		errno = 0;
		ent = readdir(dir);
		if (ent == NULL) {
			if (errno != 0)
				perror("readdir");
			break;
		}

		if (ent->d_type != DT_REG && ent->d_type != DT_LNK)
			continue;

		snprintf(fname, sizeof(fname), "%s/%s", Config.BiosDir, ent->d_name);
		if (stat(fname, &st) != 0 || st.st_size != 512*1024) {
			printf("bad BIOS file: %s\n", ent->d_name);
			continue;
		}

		if (bios_i < ARRAY_SIZE(bioses) - 1) {
			bioses[bios_i++] = strdup(ent->d_name);
			continue;
		}

		printf("too many BIOSes, dropping \"%s\"\n", ent->d_name);
	}

	closedir(dir);

do_plugins:
	snprintf(fname, sizeof(fname), "%s/", Config.PluginsDir);
	dir = opendir(fname);
	if (dir == NULL) {
		perror("scan_bios_plugins plugins opendir");
		goto do_memcards;
	}

	while (1) {
		void *h, *tmp;

		errno = 0;
		ent = readdir(dir);
		if (ent == NULL) {
			if (errno != 0)
				perror("readdir");
			break;
		}
		p = strstr(ent->d_name, ".so");
		if (p == NULL)
			continue;

		snprintf(fname, sizeof(fname), "%s/%s", Config.PluginsDir, ent->d_name);
		h = dlopen(fname, RTLD_LAZY | RTLD_LOCAL);
		if (h == NULL) {
			fprintf(stderr, "%s\n", dlerror());
			continue;
		}

		// now what do we have here?
		tmp = dlsym(h, "GPUinit");
		if (tmp) {
			dlclose(h);
			if (gpu_i < ARRAY_SIZE(gpu_plugins) - 1)
				gpu_plugins[gpu_i++] = strdup(ent->d_name);
			continue;
		}

		tmp = dlsym(h, "SPUinit");
		if (tmp) {
			dlclose(h);
			if (spu_i < ARRAY_SIZE(spu_plugins) - 1)
				spu_plugins[spu_i++] = strdup(ent->d_name);
			continue;
		}

		fprintf(stderr, "ignoring unidentified plugin: %s\n", fname);
		dlclose(h);
	}

	closedir(dir);

do_memcards:
	dir = opendir("." MEMCARD_DIR);
	if (dir == NULL) {
		perror("scan_bios_plugins memcards opendir");
		return;
	}

	while (1) {
		struct stat st;

		errno = 0;
		ent = readdir(dir);
		if (ent == NULL) {
			if (errno != 0)
				perror("readdir");
			break;
		}

		if (ent->d_type != DT_REG && ent->d_type != DT_LNK)
			continue;

		snprintf(fname, sizeof(fname), "." MEMCARD_DIR "%s", ent->d_name);
		if (stat(fname, &st) != 0) {
			printf("bad memcard file: %s\n", ent->d_name);
			continue;
		}

		if (mc_i < ARRAY_SIZE(memcards) - 1) {
			memcards[mc_i++] = strdup(ent->d_name);
			continue;
		}

		printf("too many memcards, dropping \"%s\"\n", ent->d_name);
	}

	if (mc_i > 2)
		qsort(memcards + 1, mc_i - 1, sizeof(memcards[0]), qsort_strcmp);

	closedir(dir);
}

void menu_init(void)
{
	char buff[MAXPATHLEN];

	strcpy(last_selected_fname, "/media");

	scan_bios_plugins();
	pnd_menu_init();
	menu_init_common();

	menu_set_defconfig();
	menu_load_config(0);
	last_psx_w = 320;
	last_psx_h = 240;
	last_psx_bpp = 16;

	g_menubg_src_ptr = calloc(g_menuscreen_w * g_menuscreen_h * 2, 1);
	if (g_menubg_src_ptr == NULL)
		exit(1);
	emu_make_path(buff, "skin/background.png", sizeof(buff));
	readpng(g_menubg_src_ptr, buff, READPNG_BG, g_menuscreen_w, g_menuscreen_h);
}

void menu_notify_mode_change(int w, int h, int bpp)
{
	last_psx_w = w;
	last_psx_h = h;
	last_psx_bpp = bpp;

	if (scaling == SCALE_1_1) {
		g_layer_x = 800/2 - w/2;  g_layer_y = 480/2 - h/2;
		g_layer_w = w; g_layer_h = h;
	}
}

static void menu_leave_emu(void)
{
	if (GPU_close != NULL) {
		int ret = GPU_close();
		if (ret)
			fprintf(stderr, "Warning: GPU_close returned %d\n", ret);
	}

	memcpy(g_menubg_ptr, g_menubg_src_ptr, g_menuscreen_w * g_menuscreen_h * 2);
	if (pl_vout_buf != NULL && ready_to_go && last_psx_bpp == 16) {
		int x = max(0, g_menuscreen_w - last_psx_w);
		int y = max(0, g_menuscreen_h / 2 - last_psx_h / 2);
		int w = min(g_menuscreen_w, last_psx_w);
		int h = min(g_menuscreen_h, last_psx_h);
		u16 *d = (u16 *)g_menubg_ptr + g_menuscreen_w * y + x;
		u16 *s = pl_vout_buf;

		for (; h > 0; h--, d += g_menuscreen_w, s += last_psx_w)
			menu_darken_bg(d, s, w, 0);
	}

	if (ready_to_go)
		cpu_clock = get_cpu_clock();

	plat_video_menu_enter(ready_to_go);
}

void menu_prepare_emu(void)
{
	R3000Acpu *prev_cpu = psxCpu;

	plat_video_menu_leave();

	switch (scaling) {
	case SCALE_1_1:
		menu_notify_mode_change(last_psx_w, last_psx_h, last_psx_bpp);
		break;
	case SCALE_4_3:
		g_layer_x = 80;  g_layer_y = 0;
		g_layer_w = 640; g_layer_h = 480;
		break;
	case SCALE_FULLSCREEN:
		g_layer_x = 0;   g_layer_y = 0;
		g_layer_w = 800; g_layer_h = 480;
		break;
	case SCALE_CUSTOM:
		break;
	}

	psxCpu = (Config.Cpu == CPU_INTERPRETER) ? &psxInt : &psxRec;
	if (psxCpu != prev_cpu)
		// note that this does not really reset, just clears drc caches
		psxCpu->Reset();

	// core doesn't care about Config.Cdda changes,
	// so handle them manually here
	if (Config.Cdda)
		CDR_stop();

	menu_sync_config();
	apply_lcdrate(Config.PsxType);
	apply_filter(filter);
	apply_cpu_clock();

	// push config to GPU plugin
	plugin_call_rearmed_cbs();

	if (GPU_open != NULL) {
		int ret = GPU_open(&gpuDisp, "PCSX", NULL);
		if (ret)
			fprintf(stderr, "Warning: GPU_open returned %d\n", ret);
	}

	dfinput_activate();
}

void me_update_msg(const char *msg)
{
	strncpy(menu_error_msg, msg, sizeof(menu_error_msg));
	menu_error_msg[sizeof(menu_error_msg) - 1] = 0;

	menu_error_time = plat_get_ticks_ms();
	lprintf("msg: %s\n", menu_error_msg);
}


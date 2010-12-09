/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "plugin_lib.h"
#include "omap.h"
#include "common/plat.h"
#include "../libpcsxcore/misc.h"

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
} menu_id;

extern int ready_to_go;

static int dummy, state_slot;
static char rom_fname_reload[MAXPATHLEN];
static char last_selected_fname[MAXPATHLEN];

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
	return 0;
}

static int emu_save_load_game(int load, int sram)
{
	return 0;
}

static int emu_write_config(int is_game)
{
	return 0;
}

static void emu_set_defconfig(void)
{
}

#include "common/menu.c"

static void draw_savestate_bg(int slot)
{
}

// -------------- key config --------------

me_bind_action me_ctrl_actions[] =
{
	{ "UP      ", 1 << DKEY_UP},
	{ "DOWN    ", 1 << DKEY_DOWN },
	{ "LEFT    ", 1 << DKEY_LEFT },
	{ "RIGHT   ", 1 << DKEY_RIGHT },
	{ "TRIANGLE", 1 << DKEY_TRIANGLE },
	{ "CIRCLE  ", 1 << DKEY_SQUARE },
	{ "CROSS   ", 1 << DKEY_CROSS },
	{ "SQUARE  ", 1 << DKEY_SQUARE },
	{ "L1      ", 1 << DKEY_L1 },
	{ "R1      ", 1 << DKEY_R1 },
	{ "L2      ", 1 << DKEY_L2 },
	{ "R2      ", 1 << DKEY_R2 },
	{ "START   ", 1 << DKEY_START },
	{ "SELECT  ", 1 << DKEY_SELECT },
	{ NULL,       0 }
};

me_bind_action emuctrl_actions[] =
{
	{ "Load State       ", PEV_STATE_LOAD },
	{ "Save State       ", PEV_STATE_SAVE },
	{ "Prev Save Slot   ", PEV_SSLOT_PREV },
	{ "Next Save Slot   ", PEV_SSLOT_NEXT },
	{ "Enter Menu       ", PEV_MENU },
	{ NULL,                0 }
};

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

static int mh_saveloadcfg(int id, int keys)
{
	switch (id) {
	case MA_OPT_SAVECFG:
	case MA_OPT_SAVECFG_GAME:
		if (emu_write_config(id == MA_OPT_SAVECFG_GAME ? 1 : 0))
			me_update_msg("config saved");
		else
			me_update_msg("failed to write config");
		break;
	default:
		return 0;
	}

	return 1;
}

static menu_entry e_menu_keyconfig[] =
{
	mee_handler_id("Player 1",          MA_CTRL_PLAYER1,    key_config_loop_wrap),
	mee_handler_id("Player 2",          MA_CTRL_PLAYER2,    key_config_loop_wrap),
	mee_handler_id("Emulator controls", MA_CTRL_EMU,        key_config_loop_wrap),
	mee_cust_nosave("Save global config",       MA_OPT_SAVECFG, mh_saveloadcfg, mgn_saveloadcfg),
	mee_cust_nosave("Save cfg for loaded game", MA_OPT_SAVECFG_GAME, mh_saveloadcfg, mgn_saveloadcfg),
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

	me_enable(e_menu_keyconfig, MA_OPT_SAVECFG_GAME, ready_to_go);
	me_loop(e_menu_keyconfig, &sel, NULL);
	return 0;
}

// ------------ adv options menu ------------

static menu_entry e_menu_adv_options[] =
{
	mee_onoff     ("captain placeholder", 0, dummy, 1),
	mee_end,
};

static int menu_loop_adv_options(int id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_adv_options, &sel, NULL);
	return 0;
}

// ------------ gfx options menu ------------

static menu_entry e_menu_gfx_options[] =
{
	mee_end,
};

static int menu_loop_gfx_options(int id, int keys)
{
	static int sel = 0;

	me_loop(e_menu_gfx_options, &sel, NULL);

	return 0;
}

// ------------ options menu ------------

static menu_entry e_menu_options[];

static int mh_restore_defaults(int id, int keys)
{
	emu_set_defconfig();
	me_update_msg("defaults restored");
	return 1;
}

static const char *men_confirm_save[] = { "OFF", "writes", "loads", "both", NULL };
static const char h_confirm_save[]    = "Ask for confirmation when overwriting save,\n"
					"loading state or both";

static menu_entry e_menu_options[] =
{
	mee_range     ("Save slot",                0, state_slot, 0, 9),
	mee_enum_h    ("Confirm savestate",        0, dummy, men_confirm_save, h_confirm_save),
	mee_range     ("",                         MA_OPT_CPU_CLOCKS, dummy, 20, 5000),
	mee_handler   ("[Display]",                menu_loop_gfx_options),
	mee_handler   ("[Advanced]",               menu_loop_adv_options),
	mee_cust_nosave("Save global config",      MA_OPT_SAVECFG,      mh_saveloadcfg, mgn_saveloadcfg),
	mee_cust_nosave("Save cfg for loaded game",MA_OPT_SAVECFG_GAME, mh_saveloadcfg, mgn_saveloadcfg),
	mee_handler   ("Restore defaults",         mh_restore_defaults),
	mee_end,
};

static int menu_loop_options(int id, int keys)
{
	static int sel = 0;
	int i;

	i = me_id2offset(e_menu_options, MA_OPT_CPU_CLOCKS);
	e_menu_options[i].enabled = e_menu_options[i].name[0] ? 1 : 0;
	me_enable(e_menu_options, MA_OPT_SAVECFG_GAME, ready_to_go);

	me_loop(e_menu_options, &sel, NULL);

	return 0;
}

// ------------ debug menu ------------

#ifdef __GNUC__
#define COMPILER "gcc " __VERSION__
#else
#define COMPILER
#endif

static void draw_frame_debug(void)
{
	smalltext_out16(4, 1, "build: "__DATE__ " " __TIME__ " " COMPILER, 0xffff);
}

static void debug_menu_loop(void)
{
	int inp;

	while (1)
	{
		menu_draw_begin(1);
		draw_frame_debug();
		menu_draw_end();

		inp = in_menu_wait(PBTN_MOK|PBTN_MBACK|PBTN_MA2|PBTN_MA3|PBTN_L|PBTN_R |
					PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT, 70);
		if (inp & PBTN_MBACK)
			return;
	}
}

// ------------ main menu ------------

const char *plat_get_credits(void)
{
	return	"(C) 1999-2003 PCSX Team\n"
		"(C) 2005-2009 PCSX-df Team\n"
		"(C) 2009-2010 PCSX-Reloaded Team\n\n"
		"GPU and SPU code by Pete Bernert\n"
		"  and the P.E.Op.S. team\n"
		"ARM recompiler by Ari64\n\n"
		"integration, optimization and\n"
		"  frontend by (C) notaz, 2010\n";
}

static char *romsel_run(void)
{
	char *ret;

	ret = menu_loop_romsel(last_selected_fname, sizeof(last_selected_fname));
	if (ret == NULL)
		return NULL;

	lprintf("selected file: %s\n", ret);
	ready_to_go = 0;

	SetIsoFile(ret);
	LoadPlugins();
	NetOpened = 0;
	if (OpenPlugins() == -1) {
		me_update_msg("failed to open plugins");
		return NULL;
	}

	SysReset();

	if (CheckCdrom() == -1) {
		// Only check the CD if we are starting the console with a CD
		ClosePlugins();
		me_update_msg("unsupported/invalid CD image");
		return NULL;
	}

	// Read main executable directly from CDRom and start it
	if (LoadCdrom() == -1) {
		ClosePlugins();
		me_update_msg("failed to load CD image");
		return NULL;
	}

	ready_to_go = 1;
	return ret;
}

static int main_menu_handler(int id, int keys)
{
	char *ret_name;

	switch (id)
	{
	case MA_MAIN_RESUME_GAME:
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
		break;
	case MA_MAIN_LOAD_ROM:
		ret_name = romsel_run();
		if (ret_name != NULL)
			return 1;
		break;
	case MA_MAIN_CREDITS:
		draw_menu_credits();
		in_menu_wait(PBTN_MOK|PBTN_MBACK, 70);
		break;
	case MA_MAIN_EXIT:
		exit(1); // FIXME
	default:
		lprintf("%s: something unknown selected\n", __FUNCTION__);
		break;
	}

	return 0;
}

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
	mee_handler_id("Credits",            MA_MAIN_CREDITS,     main_menu_handler),
	mee_handler_id("Exit",               MA_MAIN_EXIT,        main_menu_handler),
	mee_end,
};

void menu_loop(void)
{
	static int sel = 0;

	omap_enable_layer(0);

	me_enable(e_menu_main, MA_MAIN_RESUME_GAME, ready_to_go);
	me_enable(e_menu_main, MA_MAIN_SAVE_STATE,  ready_to_go);
	me_enable(e_menu_main, MA_MAIN_LOAD_STATE,  ready_to_go);
	me_enable(e_menu_main, MA_MAIN_RESET_GAME,  ready_to_go);

strcpy(last_selected_fname, "/mnt/ntz/stuff/psx");
	menu_enter(ready_to_go);
	in_set_config_int(0, IN_CFG_BLOCKING, 1);

	do {
		me_loop(e_menu_main, &sel, NULL);
	} while (!ready_to_go);

	/* wait until menu, ok, back is released */
	while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	in_set_config_int(0, IN_CFG_BLOCKING, 0);

	memset(g_menuscreen_ptr, 0, g_menuscreen_w * g_menuscreen_h * 2);
	menu_draw_end();
	omap_enable_layer(1);
}

void me_update_msg(const char *msg)
{
	strncpy(menu_error_msg, msg, sizeof(menu_error_msg));
	menu_error_msg[sizeof(menu_error_msg) - 1] = 0;

	menu_error_time = plat_get_ticks_ms();
	lprintf("msg: %s\n", menu_error_msg);
}


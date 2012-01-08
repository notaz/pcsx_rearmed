/*
 * (C) notaz, 2011
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>

#include "common/input.h"
#include "plugin_lib.h"
#include "plat.h"
#include "main.h"

static const char * const pandora_gpio_keys[KEY_MAX + 1] = {
	[0 ... KEY_MAX] = NULL,
	[KEY_UP]	= "Up",
	[KEY_LEFT]	= "Left",
	[KEY_RIGHT]	= "Right",
	[KEY_DOWN]	= "Down",
	[KEY_HOME]	= "(A)",
	[KEY_PAGEDOWN]	= "(X)",
	[KEY_END]	= "(B)",
	[KEY_PAGEUP]	= "(Y)",
	[KEY_RIGHTSHIFT]= "(L)",
	[KEY_RIGHTCTRL]	= "(R)",
	[KEY_LEFTALT]	= "Start",
	[KEY_LEFTCTRL]	= "Select",
	[KEY_MENU]	= "Pandora",
};

struct in_default_bind in_evdev_defbinds[] = {
	{ KEY_UP,	IN_BINDTYPE_PLAYER12, DKEY_UP },
	{ KEY_DOWN,	IN_BINDTYPE_PLAYER12, DKEY_DOWN },
	{ KEY_LEFT,	IN_BINDTYPE_PLAYER12, DKEY_LEFT },
	{ KEY_RIGHT,	IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
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
	{ KEY_TAB,	IN_BINDTYPE_EMU, SACTION_MINIMIZE },
	{ KEY_SPACE,    IN_BINDTYPE_EMU, SACTION_ENTER_MENU },
	{ KEY_1,        IN_BINDTYPE_EMU, SACTION_SAVE_STATE },
	{ KEY_2,        IN_BINDTYPE_EMU, SACTION_LOAD_STATE },
	{ KEY_3,        IN_BINDTYPE_EMU, SACTION_PREV_SSLOT },
	{ KEY_4,        IN_BINDTYPE_EMU, SACTION_NEXT_SSLOT },
	{ KEY_5,        IN_BINDTYPE_EMU, SACTION_TOGGLE_FSKIP },
	{ KEY_6,        IN_BINDTYPE_EMU, SACTION_SCREENSHOT },
	{ 0, 0, 0 }
};

int plat_pandora_init(void)
{
	in_probe();
	in_set_config(in_name_to_id("evdev:gpio-keys"), IN_CFG_KEY_NAMES,
		      pandora_gpio_keys, sizeof(pandora_gpio_keys));
	in_adev[0] = in_name_to_id("evdev:nub0");
	in_adev[1] = in_name_to_id("evdev:nub1");

	return 0;
}

static const char pnd_script_base[] = "sudo -n /usr/pandora/scripts";

int plat_cpu_clock_get(void)
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

int plat_cpu_clock_apply(int cpu_clock)
{
	char buf[128];

	if (cpu_clock != 0 && cpu_clock != plat_cpu_clock_get()) {
		snprintf(buf, sizeof(buf), "unset DISPLAY; echo y | %s/op_cpuspeed.sh %d",
			 pnd_script_base, cpu_clock);
		system(buf);
	}
	return 0;
}

int plat_get_bat_capacity(void)
{
	FILE *f;
	int ret = 0;
	f = fopen("/sys/class/power_supply/bq27500-0/capacity", "r");
	if (f) {
		fscanf(f, "%d", &ret);
		fclose(f);
	}
	return ret;
}

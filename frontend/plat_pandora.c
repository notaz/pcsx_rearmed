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
#include <dirent.h>
#include <errno.h>

#include "common/input.h"
#include "linux/in_evdev.h"
#include "plugin_lib.h"
#include "plat.h"
#include "plat_omap.h"
#include "main.h"
#include "menu.h"

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

static const struct in_default_bind in_evdev_defbinds[] = {
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

static const char pnd_script_base[] = "sudo -n /usr/pandora/scripts";
static char **pnd_filter_list;

static void scan_for_filters(void)
{
	struct dirent *ent;
	int i, count = 0;
	char **mfilters;
	char buff[64];
	DIR *dir;

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

	pnd_filter_list = mfilters;
	menu_set_filter_list((void *)mfilters);
}

int plat_init(void)
{
	int gpiokeys_id;

	plat_omap_init();

	in_evdev_init(in_evdev_defbinds);
	in_probe();
	gpiokeys_id = in_name_to_id("evdev:gpio-keys");
	in_set_config(gpiokeys_id, IN_CFG_KEY_NAMES,
		      pandora_gpio_keys, sizeof(pandora_gpio_keys));
	in_set_config(gpiokeys_id, IN_CFG_DEFAULT_DEV, NULL, 0);
	in_adev[0] = in_name_to_id("evdev:nub0");
	in_adev[1] = in_name_to_id("evdev:nub1");

	scan_for_filters();

	return 0;
}

void plat_finish(void)
{
	plat_omap_finish();
}

static void apply_lcdrate(int pal)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "%s/op_lcdrate.sh %d",
			pnd_script_base, pal ? 50 : 60);
	system(buf);
}

static void apply_filter(int which)
{
	char buf[128];
	int i;

	if (pnd_filter_list == NULL)
		return;

	for (i = 0; i < which; i++)
		if (pnd_filter_list[i] == NULL)
			return;

	if (pnd_filter_list[i] == NULL)
		return;

	snprintf(buf, sizeof(buf), "%s/op_videofir.sh %s",
		pnd_script_base, pnd_filter_list[i]);
	system(buf);
}

void plat_gvideo_open(int is_pal)
{
	static int old_pal = -1, old_filter = -1;

	if (is_pal != old_pal) {
		apply_lcdrate(is_pal);
		old_pal = is_pal;
	}
	if (filter != old_filter) {
		apply_filter(filter);
		old_filter = filter;
	}

	plat_omap_gvideo_open();
}

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

void plat_step_volume(int is_up)
{
}

void plat_trigger_vibrate(int is_strong)
{
}


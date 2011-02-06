/*
 * (C) notaz, 2011
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
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

static int fdnub[2];
static int analog_init_done;

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

static void analog_init(void)
{
	int i, nub;

	fdnub[0] = fdnub[1] = -1;

	for (i = nub = 0; nub < 2; i++)
	{
		long absbits[(ABS_MAX+1) / sizeof(long) / 8];
		int ret, fd, support = 0;
		char name[64];

		snprintf(name, sizeof(name), "/dev/input/event%d", i);
		fd = open(name, O_RDONLY|O_NONBLOCK);
		if (fd == -1) {
			if (errno == EACCES)
				continue;	/* maybe we can access next one */
			break;
		}

		/* check supported events */
		ret = ioctl(fd, EVIOCGBIT(0, sizeof(support)), &support);
		if (ret == -1) {
			printf("pandora: ioctl failed on %s\n", name);
			goto skip;
		}

		if (!(support & (1 << EV_ABS)))
			goto skip;

		ret = ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		if (ret == -1 || strncmp(name, "nub", 3) != 0)
			goto skip;

		ret = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits);
		if (ret == -1)
			goto skip;
		if ((absbits[0] & ((1 << ABS_X)|(1 << ABS_Y))) != ((1 << ABS_X)|(1 << ABS_Y)))
			goto skip;

		printf("pandora: found analog #%d \"%s\"\n", nub, name);
		fdnub[nub++] = fd;
		continue;

skip:
		close(fd);
	}

	if (nub != 2)
		printf("pandora: warning: not all nubs found: %d\n", nub);

	analog_init_done = 1;
}

void in_update_analogs(void)
{
	int *nubp[2] = { in_a1, in_a2 };
	struct input_absinfo ainfo;
	int i, fd, v, ret;

	if (!analog_init_done)
		analog_init();

	for (i = 0; i < 2; i++) {
		fd = fdnub[i];
		if (fd < 0)
			continue;

		ret = ioctl(fd, EVIOCGABS(ABS_X), &ainfo);
		if (ret == -1) {
			perror("ioctl");
			continue;
		}
		v = ainfo.value / 2 + 127;
		nubp[i][0] = v < 0 ? 0 : v;

		ret = ioctl(fd, EVIOCGABS(ABS_Y), &ainfo);
		if (ret == -1) {
			perror("ioctl");
			continue;
		}
		v = ainfo.value / 2 + 127;
		nubp[i][1] = v < 0 ? 0 : v;
	}
	//printf("%4d %4d %4d %4d\n", in_a1[0], in_a1[1], in_a2[0], in_a2[1]);
}

int pandora_init(void)
{
	in_set_config(in_name_to_id("evdev:gpio-keys"), IN_CFG_KEY_NAMES,
		      pandora_gpio_keys, sizeof(pandora_gpio_keys));

	return 0;
}

/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include "main.h"

unsigned char CurPad, CurByte, CurCmd, CmdLen;

/* since this is not a proper plugin, so we'll hook emu internals in a hackish way like this */
extern void *PAD1_startPoll, *PAD1_poll;
extern void *PAD2_startPoll, *PAD2_poll;
extern unsigned char CALLBACK PAD1__startPoll(int pad);
extern unsigned char CALLBACK PAD2__startPoll(int pad);
extern unsigned char CALLBACK PAD1__poll(unsigned char value);
extern unsigned char CALLBACK PAD2__poll(unsigned char value);

#if 0 //ndef HAVE_LIBRETRO

static int old_controller_type1 = -1, old_controller_type2 = -1;

#define select_pad(n) \
	if (pad.controllerType != old_controller_type##n) \
	{ \
		switch (pad.controllerType) \
		{ \
		case PSE_PAD_TYPE_ANALOGPAD: \
			PAD##n##_startPoll = PADstartPoll_pad; \
			PAD##n##_poll = PADpoll_pad; \
			pad_init(); \
			break; \
		case PSE_PAD_TYPE_GUNCON: \
			PAD##n##_startPoll = PADstartPoll_guncon; \
			PAD##n##_poll = PADpoll_guncon; \
			guncon_init(); \
			break; \
		case PSE_PAD_TYPE_NEGCON: \
		case PSE_PAD_TYPE_GUN: \
		default: \
			PAD##n##_startPoll = PAD##n##__startPoll; \
			PAD##n##_poll = PAD##n##__poll; \
			break; \
		} \
	}

void dfinput_activate(void)
{
	PadDataS pad;

	pad.portMultitap = -1;
	pad.requestPadIndex = 0;
	PAD1_readPort1(&pad);
	select_pad(1);

	pad.requestPadIndex = 1;
	PAD2_readPort2(&pad);
	select_pad(2);
}

#else // use libretro's libpcsxcore/plugins.c code

void dfinput_activate(void)
{
}

#endif

/*
 * Copyright (c) 2009, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 *
 * this is only pure emulation code to handle analogs,
 * extracted from dfinput.
 */

#include <stdint.h>

#include "psemu_plugin_defs.h"
#include "main.h"

enum {
	ANALOG_LEFT = 0,
	ANALOG_RIGHT,

	ANALOG_TOTAL
};

enum {
	CMD_READ_DATA_AND_VIBRATE = 0x42,
	CMD_CONFIG_MODE = 0x43,
	CMD_SET_MODE_AND_LOCK = 0x44,
	CMD_QUERY_MODEL_AND_MODE = 0x45,
	CMD_QUERY_ACT = 0x46, // ??
	CMD_QUERY_COMB = 0x47, // ??
	CMD_QUERY_MODE = 0x4C, // QUERY_MODE ??
	CMD_VIBRATION_TOGGLE = 0x4D,
};

#ifndef HAVE_LIBRETRO
static struct {
	uint8_t PadMode;
	uint8_t PadID;
	uint8_t ConfigMode;
	PadDataS pad;
} padstate[2];

static uint8_t stdpar[2][8] = {
	{0xFF, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80},
	{0xFF, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80}
};

static uint8_t unk46[2][8] = {
	{0xFF, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
	{0xFF, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A}
};

static uint8_t unk47[2][8] = {
	{0xFF, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00}
};

static uint8_t unk4c[2][8] = {
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static uint8_t unk4d[2][8] = { 
	{0xFF, 0x5A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{0xFF, 0x5A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};

static uint8_t stdcfg[2][8]   = { 
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static uint8_t stdmode[2][8]  = { 
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static uint8_t stdmodel[2][8] = { 
	{0xFF,
	 0x5A,
	 0x01, // 03 - dualshock2, 01 - dualshock
	 0x02, // number of modes
	 0x01, // current mode: 01 - analog, 00 - digital
	 0x02,
	 0x01,
	 0x00},
	{0xFF, 
	 0x5A,
	 0x01, // 03 - dualshock2, 01 - dualshock
	 0x02, // number of modes
	 0x01, // current mode: 01 - analog, 00 - digital
	 0x02,
	 0x01,
	 0x00}
};

static uint8_t *buf;

static uint8_t do_cmd(void)
{
	PadDataS *pad = &padstate[CurPad].pad;
	int pad_num = CurPad;

	CmdLen = 8;
	switch (CurCmd) {
		case CMD_SET_MODE_AND_LOCK:
			buf = stdmode[pad_num];
			return 0xF3;

		case CMD_QUERY_MODEL_AND_MODE:
			buf = stdmodel[pad_num];
			buf[4] = padstate[pad_num].PadMode;
			return 0xF3;

		case CMD_QUERY_ACT:
			buf = unk46[pad_num];
			return 0xF3;

		case CMD_QUERY_COMB:
			buf = unk47[pad_num];
			return 0xF3;

		case CMD_QUERY_MODE:
			buf = unk4c[pad_num];
			return 0xF3;

		case CMD_VIBRATION_TOGGLE:
			buf = unk4d[pad_num];
			return 0xF3;

		case CMD_CONFIG_MODE:
			if (padstate[pad_num].ConfigMode) {
				buf = stdcfg[pad_num];
				return 0xF3;
			}
			// else FALLTHROUGH

		case CMD_READ_DATA_AND_VIBRATE:
		default:
			buf = stdpar[pad_num];

			buf[2] = pad->buttonStatus;
			buf[3] = pad->buttonStatus >> 8;

			if (padstate[pad_num].PadMode == 1) {
				buf[4] = pad->rightJoyX;
				buf[5] = pad->rightJoyY;
				buf[6] = pad->leftJoyX;
				buf[7] = pad->leftJoyY;
			} else {
				CmdLen = 4;
			}

			return padstate[pad_num].PadID;
	}
}

static void do_cmd2(unsigned char value)
{
	switch (CurCmd) {
		case CMD_CONFIG_MODE:
			padstate[CurPad].ConfigMode = value;
			break;

		case CMD_SET_MODE_AND_LOCK:
			padstate[CurPad].PadMode = value;
			padstate[CurPad].PadID = value ? 0x73 : 0x41;
			break;

		case CMD_QUERY_ACT:
			switch (value) {
				case 0: // default
					buf[5] = 0x02;
					buf[6] = 0x00;
					buf[7] = 0x0A;
					break;

				case 1: // Param std conf change
					buf[5] = 0x01;
					buf[6] = 0x01;
					buf[7] = 0x14;
					break;
			}
			break;

		case CMD_QUERY_MODE:
			switch (value) {
				case 0: // mode 0 - digital mode
					buf[5] = PSE_PAD_TYPE_STANDARD;
					break;

				case 1: // mode 1 - analog mode
					buf[5] = PSE_PAD_TYPE_ANALOGPAD;
					break;
			}
			break;
	}
}

static void do_vibration(unsigned char value)
{
    int changed = 0;
    int i;

    switch (CurCmd) {
        case CMD_READ_DATA_AND_VIBRATE:
            for (i = 0; i < 2; i++) {
                if (padstate[CurPad].pad.Vib[i] == CurByte
                     && padstate[CurPad].pad.VibF[i] != value) {
                    padstate[CurPad].pad.VibF[i] = value;
                    changed = 1;
                }
            }

            if (!in_enable_vibration || !changed)
                break;

            plat_trigger_vibrate(CurPad,
                                 padstate[CurPad].pad.VibF[0],
                                 padstate[CurPad].pad.VibF[1]);
            break;
        case CMD_VIBRATION_TOGGLE:
            for (i = 0; i < 2; i++) {
                if (padstate[CurPad].pad.Vib[i] == CurByte)
                    buf[CurByte] = 0;
            }
            if (value < 2) {
                padstate[CurPad].pad.Vib[value] = CurByte;
                if((padstate[CurPad].PadID & 0x0f) < (CurByte - 1) / 2) {
                    padstate[CurPad].PadID = (padstate[CurPad].PadID & 0xf0) + (CurByte - 1) / 2;
                }
            }
            break;
    }
}
#endif

#if 0
#include <stdio.h>
unsigned char PADpoll_(unsigned char value);
unsigned char PADpoll(unsigned char value) {
	unsigned char b = CurByte, r = PADpoll_(value);
	printf("poll[%d] %02x %02x\n", b, value, r);
	return r;
}
#define PADpoll PADpoll_
#endif

#ifndef HAVE_LIBRETRO
unsigned char PADpoll_pad(unsigned char value) {
	if (CurByte == 0) {
		CurCmd = value;
		CurByte++;

		// Don't enable Analog/Vibration for a standard pad
		if (padstate[CurPad].pad.controllerType != PSE_PAD_TYPE_ANALOGPAD)
			CurCmd = CMD_READ_DATA_AND_VIBRATE;

		return do_cmd();
	}

	if (CurByte >= CmdLen)
		return 0xff;	// verified

	if (CurByte == 2)
		do_cmd2(value);

	if (padstate[CurPad].pad.controllerType == PSE_PAD_TYPE_ANALOGPAD)
		do_vibration(value);

	return buf[CurByte++];
}

unsigned char PADstartPoll_pad(int pad) {
	CurPad = pad - 1;
	CurByte = 0;

	if (pad == 1)
		PAD1_readPort1(&padstate[0].pad);
	else
		PAD2_readPort2(&padstate[1].pad);

	return 0xFF;
}

void pad_init(void)
{
	int i;

	PAD1_readPort1(&padstate[0].pad);
	PAD2_readPort2(&padstate[1].pad);

	for (i = 0; i < 2; i++) {
		padstate[i].PadID = padstate[i].pad.controllerType == PSE_PAD_TYPE_ANALOGPAD ? 0x73 : 0x41;
		padstate[i].PadMode = padstate[i].pad.controllerType == PSE_PAD_TYPE_ANALOGPAD;
	}
}
#endif

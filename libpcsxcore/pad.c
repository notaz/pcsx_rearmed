/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 ***************************************************************************/

#include <stdio.h> // SEEK_CUR
#include "psxcommon.h"
#include "psxcounters.h"
#include "plugins.h"

#define REPLUG_FRAMES 32u

// Pad information, keystate, mode, config mode, vibration
static struct {
	PadDataS pads[8];
	int reqPos;
	u32 replug_frame;
} g;

// response for request 44, 45, 46, 47, 4C, 4D
static const u8 resp45[8]    = {0xF3, 0x5A, 0x01, 0x02, 0x00, 0x02, 0x01, 0x00};
static const u8 resp46_00[8] = {0xF3, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A};
static const u8 resp46_01[8] = {0xF3, 0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14};
static const u8 resp47[8]    = {0xF3, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
static const u8 resp4C_00[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
static const u8 resp4C_01[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00};

// fixed reponse of request number 41, 48, 49, 4A, 4B, 4E, 4F
static const u8 resp4x[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Resquest of psx core
enum {
	// REQUEST
	// first call of this request for the pad, the pad is configured as an digital pad.
	// 0x0X, 0x42, 0x0Y, 0xZZ, 0xAA, 0x00, 0x00, 0x00, 0x00
	// X pad number (used for the multitap, first request response 0x00, 0x80, 0x5A, (8 bytes pad A), (8 bytes pad B), (8 bytes pad C), (8 bytes pad D)
	// Y if 1 : psx request the full length response for the multitap, 3 bytes header and 4 block of 8 bytes per pad
	// Y if 0 : psx request a pad key state
	// ZZ rumble small motor 00-> OFF, 01 -> ON
	// AA rumble large motor speed 0x00 -> 0xFF
	// RESPONSE
	// header 3 Bytes
	// 0x00
	// PadId -> 0x41 for digital pas, 0x73 for analog pad
	// 0x5A mode has not change (no press on analog button on the center of pad), 0x00 the analog button have been pressed and the mode switch
	// 6 Bytes for keystates
	CMD_READ_DATA_AND_VIBRATE = 0x42,

	// REQUEST
	// Header
	// 0x0N, 0x43, 0x00, XX, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	// XX = 00 -> Normal mode : Seconde bytes of response = padId
	// XX = 01 -> Configuration mode : Seconde bytes of response = 0xF3
	// RESPONSE
	// enter in config mode example :
	// req : 01 43 00 01 00 00 00 00 00 00
	// res : 00 41 5A buttons state, analog states
	// exit config mode :
	// req : 01 43 00 00 00 00 00 00 00 00
	// res : 00 F3 5A buttons state, analog states
	CMD_CONFIG_MODE = 0x43,

	// Set led State
	// REQUEST
	// 0x0N, 0x44, 0x00, VAL, SEL, 0x00, 0x00, 0x00, 0x00
	// If sel = 2 then
	// VAL = 00 -> OFF
	// VAL = 01 -> ON
	// RESPONSE
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	CMD_SET_MODE_AND_LOCK = 0x44,

	// Get Analog Led state
	// REQUEST
	// 0x0N, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// 0x00, 0xF3, 0x5A, 0x01, 0x02, VAL, 0x02, 0x01, 0x00
	// VAL = 00 Led OFF
	// VAL = 01 Led ON
	CMD_QUERY_MODEL_AND_MODE = 0x45,

	//Get Variable A
	// REQUEST
	// 0x0N, 0x46, 0x00, 0xXX, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// XX=00
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A
	// XX=01
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14
	CMD_QUERY_ACT = 0x46,

	// REQUEST
	// 0x0N, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00
	CMD_QUERY_COMB = 0x47,

	// REQUEST
	// 0x0N, 0x4C, 0x00, 0xXX, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// XX = 0
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00
	// XX = 1
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00
	CMD_QUERY_MODE = 0x4C,

	// REQUEST
	// 0x0N, 0x4D, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	// RESPONSE
	// 0x00, 0xF3, 0x5A, old value or
	// AA = 01 unlock large motor (and swap VAL1 and VAL2)
	// BB = 01 unlock large motor (default)
	// CC, DD, EE, FF = all FF -> unlock small motor
	//
	// default repsonse for analog pad with 2 motor : 0x00 0xF3 0x5A 0x00 0x01 0xFF 0xFF 0xFF 0xFF
	//
	CMD_VIBRATION_TOGGLE = 0x4D,
	REQ40 = 0x40,
	REQ41 = 0x41,
	REQ49 = 0x49,
	REQ4A = 0x4A,
	REQ4B = 0x4B,
	REQ4E = 0x4E,
	REQ4F = 0x4F
};

#if 0
#define PAD_LOG_TX(pos, v) buf_tx[pos] = v
#define PAD_LOG_RX(pos, v) buf_rx[pos] = v
static unsigned char buf_tx[35], buf_rx[35];
static void log_pad_print(int port, int last_pos)
{
	if (port == 0) {
		int i;
		for (i = 0; i <= last_pos; i++)
			printf("%02x ", buf_tx[i]);
		printf("|");
		for (i = 0; i <= last_pos; i++)
			printf(" %02x", buf_rx[i]);
		printf("\n");
	}
}
#else
#define PAD_LOG_TX(pos, v)
#define PAD_LOG_RX(pos, v)
#define log_pad_print(port, last_pos)
#endif

static void initBufForRequest(PadDataS *pad, unsigned char value)
{
	if (pad->ds.configMode) {
		pad->rxData[0] = 0xf3;
		pad->rxData[1] = 0x5a;
		pad->respSize = 8;
	}
	else if (value != 0x42 && value != 0x43) {
		pad->respSize = 1;
		return;
	}

	if ((u32)(frame_counter - pad->ds.lastUseFrame) > 2*60u
	    && pad->ds.configModeUsed
	    && !Config.hacks.dualshock_init_analog)
	{
		//SysPrintf("Pad reset\n");
		pad->ds.padMode = 0; // according to nocash
		pad->ds.autoAnalogTried = 0;
	}
	else if (pad->ds.padMode == 0 && value == CMD_READ_DATA_AND_VIBRATE
		 && pad->ds.configModeUsed
		 && !pad->ds.configMode
		 && !pad->ds.userToggled)
	{
		if (pad->ds.autoAnalogTried == 16) {
			// auto-enable for convenience
			SysPrintf("Pad%ld: Auto-enabling dualshock analog mode.\n",
				(long)(pad - g.pads + 1));
			pad->ds.padMode = 1;
			pad->ds.autoAnalogTried = 255;
		}
		else if (pad->ds.autoAnalogTried < 16)
			pad->ds.autoAnalogTried++;
	}
	pad->ds.lastUseFrame = frame_counter;

	switch (value) {
		// keystate already in buffer, set by PADstartPoll_()
		//case CMD_READ_DATA_AND_VIBRATE :
		//case CMD_CONFIG_MODE : // 43
		//	break;
		case CMD_SET_MODE_AND_LOCK :
			break;
		case CMD_QUERY_MODEL_AND_MODE :
			memcpy(pad->rxData, resp45, 8);
			pad->rxData[4] = pad->ds.padMode;
			break;
		case CMD_QUERY_ACT :
			memcpy(pad->rxData, resp46_00, 8);
			break;
		case CMD_QUERY_COMB :
			memcpy(pad->rxData, resp47, 8);
			break;
		case CMD_QUERY_MODE :
			memcpy(pad->rxData, resp4C_00, 8);
			break;
		case CMD_VIBRATION_TOGGLE: // 4d
			memcpy(pad->rxData + 2, pad->ds.cmd4dConfig, 6);
			break;
		case REQ40 :
		case REQ41 :
		case REQ49 :
		case REQ4A :
		case REQ4B :
		case REQ4E :
		case REQ4F :
			memcpy(pad->rxData, resp4x, 8);
			break;
	}
}

static void reqIndex2Treatment(PadDataS *pad, unsigned char value)
{
	switch (pad->txData[0]) {
		case CMD_CONFIG_MODE : // 43
			if (value == 0) {
				pad->ds.configMode = 0;
			} else if (value == 1) {
				pad->ds.configMode = 1;
				pad->ds.configModeUsed = 1;
			}
			break;
		case CMD_SET_MODE_AND_LOCK : // 44
			//0x44 store the led state for change mode if the next value = 0x02
			//0x01 analog ON
			//0x00 analog OFF
			if ((value & ~1) == 0)
				pad->ds.padMode = value;
			break;
		case CMD_QUERY_ACT : // 46
			if (value == 1) {
				memcpy(pad->rxData, resp46_01, 8);
			}
			break;
		case CMD_QUERY_MODE : // 4c
			if (value == 1) {
				memcpy(pad->rxData, resp4C_01, 8);
			}
			break;
	}
}

static void ds_update_vibrate(PadDataS *pad)
{
	if (pad->ds.configModeUsed) {
		pad->Vib[0] = (pad->Vib[0] == 1) ? 1 : 0;
	}
	else {
		// compat mode
		pad->Vib[0] = (pad->Vib[0] & 0xc0) == 0x40 && (pad->Vib[1] & 1);
		pad->Vib[1] = 0;
	}
	if (pad->Vib[0] != pad->VibF[0] || pad->Vib[1] != pad->VibF[1]) {
		size_t padIndex = pad - g.pads;
		//value is different update Value and call libretro for vibration
		pad->VibF[0] = pad->Vib[0];
		pad->VibF[1] = pad->Vib[1];
		plat_trigger_vibrate(padIndex, pad->VibF[0], pad->VibF[1]);
		//printf("vib%zi %02x %02x\n", padIndex, pad->VibF[0], pad->VibF[1]);
	}
}

static void adjust_analog(unsigned char *b)
{
	// ff8 hates 0x80 for whatever reason (broken in 2d area menus),
	// or is this caused by something else we do wrong??
	// Also S.C.A.R.S. treats 0x7f as turning left.
	if (b[6] == 0x7f || b[6] == 0x80)
		b[6] = 0x81;
}

// Build response for 0x42 request Pad in port
static void PADstartPoll_(PadDataS *pad)
{
	pad->respSizeOld = pad->respSize;
	switch (pad->controllerType) {
		case PSE_PAD_TYPE_MOUSE:
			pad->rxData[0] = 0x12;
			pad->rxData[1] = 0x5a;
			pad->rxData[2] = pad->buttonStatus & 0xff;
			pad->rxData[3] = pad->buttonStatus >> 8;
			pad->rxData[4] = pad->moveX;
			pad->rxData[5] = pad->moveY;
			pad->respSize = 6;
			break;
		case PSE_PAD_TYPE_NEGCON: // npc101/npc104(slph00001/slph00069)
			pad->rxData[0] = 0x23;
			pad->rxData[1] = 0x5a;
			pad->rxData[2] = pad->buttonStatus & 0xff;
			pad->rxData[3] = pad->buttonStatus >> 8;
			pad->rxData[4] = pad->rightJoyX;
			pad->rxData[5] = pad->rightJoyY;
			pad->rxData[6] = pad->leftJoyX;
			pad->rxData[7] = pad->leftJoyY;
			pad->respSize = 8;
			break;
		case PSE_PAD_TYPE_GUNCON: // GUNCON - gun controller SLPH-00034 from Namco
			pad->rxData[0] = 0x63;
			pad->rxData[1] = 0x5a;
			pad->rxData[2] = pad->buttonStatus & 0xff;
			pad->rxData[3] = pad->buttonStatus >> 8;

			int absX = pad->absoluteX; // 0-1023
			int absY = pad->absoluteY;

			if (absX == 65536 || absY == 65536) {
				pad->rxData[4] = 0x01;
				pad->rxData[5] = 0x00;
				pad->rxData[6] = 0x0A;
				pad->rxData[7] = 0x00;
			}
			else {
				int y_ofs = 0, yres = 240;
				GPU_getScreenInfo(&y_ofs, &yres);
				int y_top = (Config.PsxType ? 0x30 : 0x19) + y_ofs;
				int w = Config.PsxType ? 385 : 378;
				int x = 0x40 + (w * absX >> 10);
				int y = y_top + (yres * absY >> 10);
				//printf("%3d %3d %4x %4x\n", absX, absY, x, y);

				pad->rxData[4] = x;
				pad->rxData[5] = x >> 8;
				pad->rxData[6] = y;
				pad->rxData[7] = y >> 8;
			}
			pad->respSize = 8;
			break;
		case PSE_PAD_TYPE_GUN: // GUN CONTROLLER - gun controller SLPH-00014 from Konami
			pad->rxData[0] = 0x31;
			pad->rxData[1] = 0x5a;
			pad->rxData[2] = pad->buttonStatus & 0xff;
			pad->rxData[3] = pad->buttonStatus >> 8;
			pad->respSize = 4;
			break;
		case PSE_PAD_TYPE_ANALOGPAD: // scph1150
			if ((pad->ds.padMode | pad->ds.configMode) == 0)
				goto standard;
			pad->rxData[0] = 0x73;
			pad->rxData[1] = 0x5a;
			pad->rxData[2] = pad->buttonStatus & 0xff;
			pad->rxData[3] = pad->buttonStatus >> 8;
			pad->rxData[4] = pad->rightJoyX;
			pad->rxData[5] = pad->rightJoyY;
			pad->rxData[6] = pad->leftJoyX;
			pad->rxData[7] = pad->leftJoyY;
			adjust_analog(pad->rxData);
			pad->respSize = 8;
			break;
		case PSE_PAD_TYPE_ANALOGJOY: // scph1110
			pad->rxData[0] = 0x53;
			pad->rxData[1] = 0x5a;
			pad->rxData[2] = pad->buttonStatus & 0xff;
			pad->rxData[3] = pad->buttonStatus >> 8;
			pad->rxData[4] = pad->rightJoyX;
			pad->rxData[5] = pad->rightJoyY;
			pad->rxData[6] = pad->leftJoyX;
			pad->rxData[7] = pad->leftJoyY;
			adjust_analog(pad->rxData);
			pad->respSize = 8;
			break;
		case PSE_PAD_TYPE_STANDARD:
		standard:
			pad->rxData[0] = 0x41;
			pad->rxData[1] = 0x5a;
			pad->rxData[2] = pad->buttonStatus & 0xff;
			pad->rxData[3] = pad->buttonStatus >> 8;
			pad->respSize = 4;
			break;
		default:
			pad->rxData[0] = 0xff;
			pad->respSize = 1;
			break;
	}
}

static void PADpoll_dualshock(PadDataS *pad, unsigned char value, int pos)
{
	switch (pos) {
		case 0:
			initBufForRequest(pad, value);
			break;
		case 2:
			reqIndex2Treatment(pad, value);
			break;
		case 7:
			if (pad->txData[0] == CMD_VIBRATION_TOGGLE)
				memcpy(pad->ds.cmd4dConfig, pad->txData + 2, 6);
			break;
	}

	if (pad->txData[0] == CMD_READ_DATA_AND_VIBRATE
	    && !pad->ds.configModeUsed && 2 <= pos && pos < 4)
	{
		// "compat" single motor mode
		pad->Vib[pos - 2] = value;
	}
	else if (pad->txData[0] == CMD_READ_DATA_AND_VIBRATE
		 && 2 <= pos && pos < 8)
	{
		// 0 - weak motor, 1 - strong motor
		int dev = pad->ds.cmd4dConfig[pos - 2];
		if (dev < 2)
			pad->Vib[dev] = value;
	}
	if (pos == pad->respSize - 1)
		ds_update_vibrate(pad);
}

static unsigned char PADpoll_(int port, unsigned char value, int pos, int *more_data)
{
	PadDataS *pad = &g.pads[port];

	if (pos < sizeof(pad->txData))
		pad->txData[pos] = value;
	if (pos == 0 && value != 0x42 && pad->controllerType != PSE_PAD_TYPE_ANALOGPAD) {
		pad->respSize = 1;
		pad->rxData[0] = 0xff;
	}

	switch (pad->controllerType) {
		case PSE_PAD_TYPE_ANALOGPAD:
			PADpoll_dualshock(pad, value, pos);
			break;
		case PSE_PAD_TYPE_GUN:
			if (pos == 2)
				pl_gun_byte2(port, value);
			break;
	}

	*more_data = pos < pad->respSize - 1;
	if (pos >= pad->respSize) {
		log_unhandled("pad %zd read %d/%d\n", pad - g.pads, pos, pad->respSize);
		return 0xff; // no response/HiZ
	}

	return pad->rxData[pos];
}

// response: 0x80, 0x5A, 8 bytes each for ports A, B, C, D
static unsigned char PADpollMultitap(int port, unsigned char value, int pos, int *more_data)
{
	unsigned int pos_dev, dev;
	int unused = 0;
	PadDataS *pad;

	if (pos == 0) {
		*more_data = (value == 0x42);
		return 0x80;
	}
	*more_data = pos < 34 - 1;
	if (pos == 1)
		return 0x5a;
	if (pos >= 34) {
		log_unhandled("pad %d read %d/%d\n", port, pos, 34);
		return 0xff;
	}

	pos_dev = pos - 2;
	dev = pos_dev / 8u;
	pad = &g.pads[port + dev];
	if (pos_dev % 8u == 0) {
		memcpy(pad->rxDataOld, pad->rxData, sizeof(pad->rxDataOld));
		PADstartPoll_(pad);
	}
	pos_dev = pos_dev & 7;
	PADpoll_(port + dev, value, pos_dev, &unused);
	return pos_dev < pad->respSizeOld ? pad->rxDataOld[pos_dev] : 0xff;
}

static unsigned char PADpollMain(int port, unsigned char value, int *more_data)
{
	unsigned char ret;
	int pos = g.reqPos++;

	PAD_LOG_TX(pos, value);
	if (pos == 1)
		g.pads[port].txData1 = value;

	if (g.replug_frame) {
		if (frame_counter - g.replug_frame > REPLUG_FRAMES)
			g.replug_frame = 0;
		ret = 0xff;
		*more_data = 0;
	}
	else if (!g.pads[port].portMultitap || !g.pads[port].multitapLongModeEnabled)
		ret = PADpoll_(port, value, pos, more_data);
	else
		ret = PADpollMultitap(port, value, pos, more_data);

	PAD_LOG_RX(pos, ret);
	if (!*more_data)
		log_pad_print(port, pos);
	return ret;

}

static int PADstartPollMain(PadDataS *pad)
{
	g.reqPos = 0;

	pad->multitapLongModeEnabled = 0;
	if (pad->portMultitap)
		pad->multitapLongModeEnabled = pad->txData1 & 1;

	if (!pad->portMultitap || !pad->multitapLongModeEnabled) {
		PADstartPoll_(pad);
		return 0;
	}
	return 1;
}

// refresh the button state on port 1.
// int pad is not needed.
unsigned char PAD1_startPoll(void)
{
	PadDataS *pad = &g.pads[0];
	int i;

	pad->requestPadIndex = 0;
	PAD1_readPort(pad, &pad->portMultitap);

	if (PADstartPollMain(pad)) {
		// a multitap is plugged and enabled: refresh pads 1-3
		for (i = 1; i < 4; i++) {
			g.pads[i].requestPadIndex = i;
			PAD1_readPort(&g.pads[i], NULL);
		}
	}
	return 0xff;
}

unsigned char PAD1_poll(unsigned char value, int *more_data)
{
	return PADpollMain(0, value, more_data);
}

unsigned char PAD2_startPoll(void)
{
	int pad_index = g.pads[0].portMultitap ? 4 : 1;
	PadDataS *pad = &g.pads[pad_index];
	int i;

	pad->requestPadIndex = pad_index;
	PAD2_readPort(pad, &pad->portMultitap);

	if (PADstartPollMain(pad)) {
		for (i = 1; i < 4; i++) {
			g.pads[pad_index + i].requestPadIndex = pad_index + i;
			PAD2_readPort(&g.pads[pad_index + i], NULL);
		}
	}
	return 0xff;
}

unsigned char PAD2_poll(unsigned char value, int *more_data)
{
	return PADpollMain(g.pads[0].portMultitap ? 4 : 1, value, more_data);
}

void padReset(void) {
	size_t p;

	memset(&g, 0, sizeof(g));
	for (p = 0; p < sizeof(g.pads) / sizeof(g.pads[0]); p++) {
		memset(g.pads[p].rxData, 0xff, sizeof(g.pads[p].rxData));
		memset(g.pads[p].ds.cmd4dConfig, 0xff, sizeof(g.pads[p].ds.cmd4dConfig));
	}
}

int padFreeze(void *f, int Mode)
{
	int changed = 0;
	size_t i;

	for (i = 0; i < sizeof(g.pads) / sizeof(g.pads[0]); i++) {
		unsigned char controllerType = g.pads[i].controllerType;
		int portMultitap = g.pads[i].portMultitap;
		g.pads[i].saveSize = sizeof(g.pads[i]);
		gzfreeze(&g.pads[i], sizeof(g.pads[i]));
		if (Mode == 0) { // load
			if (g.pads[i].saveSize != sizeof(g.pads[i]))
				SaveFuncs.seek(f, g.pads[i].saveSize - sizeof(g.pads[i]),
						SEEK_CUR);
			if (controllerType)
				changed |= controllerType != g.pads[i].controllerType;
			changed |= portMultitap != g.pads[i].portMultitap;
		}
	}
	if (changed)
		padChanged();

	return 0;
}

int padToggleAnalog(unsigned int index)
{
	int r = -1;

	if (index < sizeof(g.pads) / sizeof(g.pads[0])) {
		r = (g.pads[index].ds.padMode ^= 1);
		g.pads[index].ds.userToggled = 1;
	}
	return r;
}

void padChanged(void)
{
	padReset();
	g.replug_frame = frame_counter ? frame_counter : -1;
}

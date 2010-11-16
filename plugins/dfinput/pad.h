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
 */

#ifndef PAD_H_
#define PAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MACOSX
#include "config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <SDL.h>
#include <SDL_joystick.h>

#ifdef _MACOSX
#include <Carbon/Carbon.h>
typedef void *Display;
#define ThreadID ThreadID_MACOSX
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#endif

#include "psemu_plugin_defs.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(x)  gettext(x)
#define N_(x) (x)
#else
#define _(x)  (x)
#define N_(x) (x)
#endif

enum {
	DKEY_SELECT = 0,
	DKEY_L3,
	DKEY_R3,
	DKEY_START,
	DKEY_UP,
	DKEY_RIGHT,
	DKEY_DOWN,
	DKEY_LEFT,
	DKEY_L2,
	DKEY_R2,
	DKEY_L1,
	DKEY_R1,
	DKEY_TRIANGLE,
	DKEY_CIRCLE,
	DKEY_CROSS,
	DKEY_SQUARE,

	DKEY_TOTAL
};

enum {
	ANALOG_LEFT = 0,
	ANALOG_RIGHT,

	ANALOG_TOTAL
};

enum { NONE = 0, AXIS, HAT, BUTTON };

typedef struct tagKeyDef {
	uint8_t			JoyEvType;
	union {
		int16_t		d;
		int16_t		Axis;   // positive=axis+, negative=axis-, abs(Axis)-1=axis index
		uint16_t	Hat;	// 8-bit for hat number, 8-bit for direction
		uint16_t	Button; // button number
	} J;
	uint16_t		Key;
} KEYDEF;

enum { ANALOG_XP = 0, ANALOG_XM, ANALOG_YP, ANALOG_YM };

typedef struct tagPadDef {
	int8_t			DevNum;
	uint16_t		Type;
	KEYDEF			KeyDef[DKEY_TOTAL];
	KEYDEF			AnalogDef[ANALOG_TOTAL][4];
} PADDEF;

typedef struct tagConfig {
	uint8_t			Threaded;
	PADDEF			PadDef[2];
} CONFIG;

typedef struct tagPadState {
	SDL_Joystick		*JoyDev;
	uint8_t				PadMode;
	uint8_t				PadID;
	volatile uint16_t	KeyStatus;
	volatile uint16_t	JoyKeyStatus;
	volatile uint8_t	AnalogStatus[ANALOG_TOTAL][2]; // 0-255 where 127 is center position
	volatile uint8_t	AnalogKeyStatus[ANALOG_TOTAL][4];
} PADSTATE;

typedef struct tagGlobalData {
	CONFIG				cfg;

	uint8_t				Opened;
	Display				*Disp;

	PADSTATE			PadState[2];
	volatile long		KeyLeftOver;
} GLOBALDATA;

extern GLOBALDATA		g;

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

// cfg.c functions...
void LoadPADConfig();
void SavePADConfig();

// sdljoy.c functions...
void InitSDLJoy();
void DestroySDLJoy();
void CheckJoy();

// xkb.c functions...
void InitKeyboard();
void DestroyKeyboard();
void CheckKeyboard();

// analog.c functions...
void InitAnalog();
void CheckAnalog();
int AnalogKeyPressed(uint16_t Key);
int AnalogKeyReleased(uint16_t Key);

// pad.c functions...
char *PSEgetLibName(void);
uint32_t PSEgetLibType(void);
uint32_t PSEgetLibVersion(void);
long PADinit(long flags);
long PADshutdown(void);
long PADopen(unsigned long *Disp);
long PADclose(void);
long PADquery(void);
unsigned char PADstartPoll(int pad);
unsigned char PADpoll(unsigned char value);
long PADreadPort1(PadDataS *pad);
long PADreadPort2(PadDataS *pad);
long PADkeypressed(void);
long PADconfigure(void);
void PADabout(void);
long PADtest(void);

#ifdef __cplusplus
}
#endif

#endif

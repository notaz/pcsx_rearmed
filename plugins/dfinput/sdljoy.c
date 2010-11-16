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

#include "pad.h"

void InitSDLJoy() {
	uint8_t				i;

	g.PadState[0].JoyKeyStatus = 0xFFFF;
	g.PadState[1].JoyKeyStatus = 0xFFFF;

	for (i = 0; i < 2; i++) {
		if (g.cfg.PadDef[i].DevNum >= 0) {
			g.PadState[i].JoyDev = SDL_JoystickOpen(g.cfg.PadDef[i].DevNum);
		} else {
			g.PadState[i].JoyDev = NULL;
		}
	}

	SDL_JoystickEventState(SDL_IGNORE);

	InitAnalog();
}

void DestroySDLJoy() {
	uint8_t				i;

	if (SDL_WasInit(SDL_INIT_JOYSTICK)) {
		for (i = 0; i < 2; i++) {
			if (g.PadState[i].JoyDev != NULL) {
				SDL_JoystickClose(g.PadState[i].JoyDev);
			}
		}
	}

	for (i = 0; i < 2; i++) {
		g.PadState[i].JoyDev = NULL;
	}
}

void CheckJoy() {
	uint8_t				i, j, n;

	SDL_JoystickUpdate();

	for (i = 0; i < 2; i++) {
		if (g.PadState[i].JoyDev == NULL) {
			continue;
		}

		for (j = 0; j < DKEY_TOTAL; j++) {
			switch (g.cfg.PadDef[i].KeyDef[j].JoyEvType) {
				case AXIS:
					n = abs(g.cfg.PadDef[i].KeyDef[j].J.Axis) - 1;

					if (g.cfg.PadDef[i].KeyDef[j].J.Axis > 0) {
						if (SDL_JoystickGetAxis(g.PadState[i].JoyDev, n) > 16383) {
							g.PadState[i].JoyKeyStatus &= ~(1 << j);
						} else {
							g.PadState[i].JoyKeyStatus |= (1 << j);
						}
					} else if (g.cfg.PadDef[i].KeyDef[j].J.Axis < 0) {
						if (SDL_JoystickGetAxis(g.PadState[i].JoyDev, n) < -16383) {
							g.PadState[i].JoyKeyStatus &= ~(1 << j);
						} else {
							g.PadState[i].JoyKeyStatus |= (1 << j);
						}
					}
					break;

				case HAT:
					n = (g.cfg.PadDef[i].KeyDef[j].J.Hat >> 8);

					if (SDL_JoystickGetHat(g.PadState[i].JoyDev, n) & (g.cfg.PadDef[i].KeyDef[j].J.Hat & 0xFF)) {
						g.PadState[i].JoyKeyStatus &= ~(1 << j);
					} else {
						g.PadState[i].JoyKeyStatus |= (1 << j);
					}
					break;

				case BUTTON:
					if (SDL_JoystickGetButton(g.PadState[i].JoyDev, g.cfg.PadDef[i].KeyDef[j].J.Button)) {
						g.PadState[i].JoyKeyStatus &= ~(1 << j);
					} else {
						g.PadState[i].JoyKeyStatus |= (1 << j);
					}
					break;

				default:
					break;
			}
		}
	}

	CheckAnalog();
}

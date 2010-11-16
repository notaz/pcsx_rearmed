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

void InitAnalog() {
	g.PadState[0].AnalogStatus[ANALOG_LEFT][0] = 127;
	g.PadState[0].AnalogStatus[ANALOG_LEFT][1] = 127;
	g.PadState[0].AnalogStatus[ANALOG_RIGHT][0] = 127;
	g.PadState[0].AnalogStatus[ANALOG_RIGHT][1] = 127;
	g.PadState[1].AnalogStatus[ANALOG_LEFT][0] = 127;
	g.PadState[1].AnalogStatus[ANALOG_LEFT][1] = 127;
	g.PadState[1].AnalogStatus[ANALOG_RIGHT][0] = 127;
	g.PadState[1].AnalogStatus[ANALOG_RIGHT][1] = 127;

	memset(g.PadState[0].AnalogKeyStatus, 0, sizeof(g.PadState[0].AnalogKeyStatus));
	memset(g.PadState[1].AnalogKeyStatus, 0, sizeof(g.PadState[1].AnalogKeyStatus));
}

void CheckAnalog() {
	int			i, j, k, val;
	uint8_t		n;

	for (i = 0; i < 2; i++) {
		if (g.cfg.PadDef[i].Type != PSE_PAD_TYPE_ANALOGPAD) {
			continue;
		}

		for (j = 0; j < ANALOG_TOTAL; j++) {
			for (k = 0; k < 4; k++) {
				if (g.PadState[i].AnalogKeyStatus[j][k]) {
					switch (k) {
						case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = 255; k++; break;
						case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = 0; break;
						case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = 255; k++; break;
						case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = 0; break;
					}
					continue;
				}

				switch (g.cfg.PadDef[i].AnalogDef[j][k].JoyEvType) {
					case AXIS:
						n = abs(g.cfg.PadDef[i].AnalogDef[j][k].J.Axis) - 1;

						if (g.cfg.PadDef[i].AnalogDef[j][k].J.Axis > 0) {
							val = SDL_JoystickGetAxis(g.PadState[i].JoyDev, n);
							if (val >= 0) {
								val += 32640;
								val /= 256;

								switch (k) {
									case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = val; break;
									case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = 255 - val; break;
									case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = val; break;
									case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = 255 - val; break;
								}
							}
						} else if (g.cfg.PadDef[i].AnalogDef[j][k].J.Axis < 0) {
							val = SDL_JoystickGetAxis(g.PadState[i].JoyDev, n);
							if (val <= 0) {
								val += 32640;
								val /= 256;

								switch (k) {
									case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = 255 - val; break;
									case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = val; break;
									case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = 255 - val; break;
									case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = val; break;
								}
							}
						}
						break;

					case HAT:
						n = (g.cfg.PadDef[i].AnalogDef[j][k].J.Hat >> 8);

						g.PadState[i].AnalogStatus[j][0] = 0;

						if (SDL_JoystickGetHat(g.PadState[i].JoyDev, n) & (g.cfg.PadDef[i].AnalogDef[j][k].J.Hat & 0xFF)) {
							switch (k) {
								case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = 255; k++; break;
								case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = 0; break;
								case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = 255; k++; break;
								case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = 0; break;
							}
						} else {
							switch (k) {
								case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = 127; break;
								case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = 127; break;
								case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = 127; break;
								case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = 127; break;
							}
						}
						break;

					case BUTTON:
						if (SDL_JoystickGetButton(g.PadState[i].JoyDev, g.cfg.PadDef[i].AnalogDef[j][k].J.Button)) {
							switch (k) {
								case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = 255; k++; break;
								case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = 0; break;
								case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = 255; k++; break;
								case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = 0; break;
							}
						} else {
							switch (k) {
								case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = 127; break;
								case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = 127; break;
								case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = 127; break;
								case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = 127; break;
							}
						}
						break;

					default:
						switch (k) {
							case ANALOG_XP: g.PadState[i].AnalogStatus[j][0] = 127; break;
							case ANALOG_XM: g.PadState[i].AnalogStatus[j][0] = 127; break;
							case ANALOG_YP: g.PadState[i].AnalogStatus[j][1] = 127; break;
							case ANALOG_YM: g.PadState[i].AnalogStatus[j][1] = 127; break;
						}
						break;
				}
			}
		}
	}
}

int AnalogKeyPressed(uint16_t Key) {
	int i, j, k;

	for (i = 0; i < 2; i++) {
		if (g.cfg.PadDef[i].Type != PSE_PAD_TYPE_ANALOGPAD) {
			continue;
		}

		for (j = 0; j < ANALOG_TOTAL; j++) {
			for (k = 0; k < 4; k++) {
				if (g.cfg.PadDef[i].AnalogDef[j][k].Key == Key) {
					g.PadState[i].AnalogKeyStatus[j][k] = 1;
					return 1;
				}
			}
		}
	}

	return 0;
}

int AnalogKeyReleased(uint16_t Key) {
	int i, j, k;

	for (i = 0; i < 2; i++) {
		if (g.cfg.PadDef[i].Type != PSE_PAD_TYPE_ANALOGPAD) {
			continue;
		}

		for (j = 0; j < ANALOG_TOTAL; j++) {
			for (k = 0; k < 4; k++) {
				if (g.cfg.PadDef[i].AnalogDef[j][k].Key == Key) {
					g.PadState[i].AnalogKeyStatus[j][k] = 0;
					return 1;
				}
			}
		}
	}

	return 0;
}

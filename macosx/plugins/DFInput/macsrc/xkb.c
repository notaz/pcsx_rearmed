/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
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

void InitKeyboard() {
	g.PadState[0].KeyStatus = 0xFFFF;
	g.PadState[1].KeyStatus = 0xFFFF;
}

void DestroyKeyboard() {
}

void CheckKeyboard() {
	int i, j, k;
	uint16_t key;

	union {
		KeyMap km;
		KeyMapByteArray k;
	} keyState;

	g.PadState[0].KeyStatus = 0xFFFF;
	g.PadState[1].KeyStatus = 0xFFFF;

	GetKeys(keyState.km);

#define KeyDown(X) \
	(keyState.k[((X) - 1) >> 3] & (1 << (((X) - 1) & 7)))

	for (i = 0; i < 2; i++) {
		for (j = 0; j < DKEY_TOTAL; j++) {
			key = g.cfg.PadDef[i].KeyDef[j].Key;
			if (key == 0) continue;

			if (KeyDown(key)) g.PadState[i].KeyStatus &= ~(1 << j);
		}

		if (g.cfg.PadDef[i].Type != PSE_PAD_TYPE_ANALOGPAD) continue;

		for (j = 0; j < ANALOG_TOTAL; j++) {
			for (k = 0; k < 4; k++) {
				key = g.cfg.PadDef[i].AnalogDef[j][k].Key;
				if (key == 0) continue;

				g.PadState[i].AnalogKeyStatus[j][k] = (KeyDown(key) ? 1 : 0);
			}
		}
	}
}

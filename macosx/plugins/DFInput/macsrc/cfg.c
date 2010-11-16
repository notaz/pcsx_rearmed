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

GLOBALDATA			g;

long DoConfiguration();
void DoAbout();

static void SetDefaultConfig() {
	memset(&g.cfg, 0, sizeof(g.cfg));

	g.cfg.Threaded = 1;

	g.cfg.PadDef[0].DevNum = 0;
	g.cfg.PadDef[1].DevNum = 1;

	g.cfg.PadDef[0].Type = PSE_PAD_TYPE_STANDARD;
	g.cfg.PadDef[1].Type = PSE_PAD_TYPE_STANDARD;

	// Pad1 keyboard
	g.cfg.PadDef[0].KeyDef[DKEY_SELECT].Key = 9;
	g.cfg.PadDef[0].KeyDef[DKEY_START].Key = 10;
	g.cfg.PadDef[0].KeyDef[DKEY_UP].Key = 127;
	g.cfg.PadDef[0].KeyDef[DKEY_RIGHT].Key = 125;
	g.cfg.PadDef[0].KeyDef[DKEY_DOWN].Key = 126;
	g.cfg.PadDef[0].KeyDef[DKEY_LEFT].Key = 124;
	g.cfg.PadDef[0].KeyDef[DKEY_L2].Key = 16;
	g.cfg.PadDef[0].KeyDef[DKEY_R2].Key = 18;
	g.cfg.PadDef[0].KeyDef[DKEY_L1].Key = 14;
	g.cfg.PadDef[0].KeyDef[DKEY_R1].Key = 15;
	g.cfg.PadDef[0].KeyDef[DKEY_TRIANGLE].Key = 3;
	g.cfg.PadDef[0].KeyDef[DKEY_CIRCLE].Key = 8;
	g.cfg.PadDef[0].KeyDef[DKEY_CROSS].Key = 7;
	g.cfg.PadDef[0].KeyDef[DKEY_SQUARE].Key = 2;

	// Pad1 joystick
	g.cfg.PadDef[0].KeyDef[DKEY_SELECT].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_SELECT].J.Button = 8;
	g.cfg.PadDef[0].KeyDef[DKEY_START].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_START].J.Button = 9;
	g.cfg.PadDef[0].KeyDef[DKEY_UP].JoyEvType = AXIS;
	g.cfg.PadDef[0].KeyDef[DKEY_UP].J.Axis = -2;
	g.cfg.PadDef[0].KeyDef[DKEY_RIGHT].JoyEvType = AXIS;
	g.cfg.PadDef[0].KeyDef[DKEY_RIGHT].J.Axis = 1;
	g.cfg.PadDef[0].KeyDef[DKEY_DOWN].JoyEvType = AXIS;
	g.cfg.PadDef[0].KeyDef[DKEY_DOWN].J.Axis = 2;
	g.cfg.PadDef[0].KeyDef[DKEY_LEFT].JoyEvType = AXIS;
	g.cfg.PadDef[0].KeyDef[DKEY_LEFT].J.Axis = -1;
	g.cfg.PadDef[0].KeyDef[DKEY_L2].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_L2].J.Button = 4;
	g.cfg.PadDef[0].KeyDef[DKEY_L1].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_L1].J.Button = 6;
	g.cfg.PadDef[0].KeyDef[DKEY_R2].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_R2].J.Button = 5;
	g.cfg.PadDef[0].KeyDef[DKEY_R1].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_R1].J.Button = 7;
	g.cfg.PadDef[0].KeyDef[DKEY_TRIANGLE].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_TRIANGLE].J.Button = 0;
	g.cfg.PadDef[0].KeyDef[DKEY_CIRCLE].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_CIRCLE].J.Button = 1;
	g.cfg.PadDef[0].KeyDef[DKEY_CROSS].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_CROSS].J.Button = 2;
	g.cfg.PadDef[0].KeyDef[DKEY_SQUARE].JoyEvType = BUTTON;
	g.cfg.PadDef[0].KeyDef[DKEY_SQUARE].J.Button = 3;

	// Pad2 joystick
	g.cfg.PadDef[1].KeyDef[DKEY_SELECT].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_SELECT].J.Button = 8;
	g.cfg.PadDef[1].KeyDef[DKEY_START].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_START].J.Button = 9;
	g.cfg.PadDef[1].KeyDef[DKEY_UP].JoyEvType = AXIS;
	g.cfg.PadDef[1].KeyDef[DKEY_UP].J.Axis = -2;
	g.cfg.PadDef[1].KeyDef[DKEY_RIGHT].JoyEvType = AXIS;
	g.cfg.PadDef[1].KeyDef[DKEY_RIGHT].J.Axis = 1;
	g.cfg.PadDef[1].KeyDef[DKEY_DOWN].JoyEvType = AXIS;
	g.cfg.PadDef[1].KeyDef[DKEY_DOWN].J.Axis = 2;
	g.cfg.PadDef[1].KeyDef[DKEY_LEFT].JoyEvType = AXIS;
	g.cfg.PadDef[1].KeyDef[DKEY_LEFT].J.Axis = -1;
	g.cfg.PadDef[1].KeyDef[DKEY_L2].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_L2].J.Button = 4;
	g.cfg.PadDef[1].KeyDef[DKEY_L1].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_L1].J.Button = 6;
	g.cfg.PadDef[1].KeyDef[DKEY_R2].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_R2].J.Button = 5;
	g.cfg.PadDef[1].KeyDef[DKEY_R1].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_R1].J.Button = 7;
	g.cfg.PadDef[1].KeyDef[DKEY_TRIANGLE].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_TRIANGLE].J.Button = 0;
	g.cfg.PadDef[1].KeyDef[DKEY_CIRCLE].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_CIRCLE].J.Button = 1;
	g.cfg.PadDef[1].KeyDef[DKEY_CROSS].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_CROSS].J.Button = 2;
	g.cfg.PadDef[1].KeyDef[DKEY_SQUARE].JoyEvType = BUTTON;
	g.cfg.PadDef[1].KeyDef[DKEY_SQUARE].J.Button = 3;
}

void LoadPADConfig() {
	FILE		*fp;
	char		buf[256];
	int			current, a, b, c;

	SetDefaultConfig();

	sprintf(buf, "%s/Library/Preferences/net.pcsx.DFInput.plist", getenv("HOME"));

	fp = fopen(buf, "r");
	if (fp == NULL) {
		return;
	}

	current = 0;

	while (fgets(buf, 256, fp) != NULL) {
		if (strncmp(buf, "Threaded=", 9) == 0) {
			g.cfg.Threaded = atoi(&buf[9]);
		} else if (strncmp(buf, "[PAD", 4) == 0) {
			current = atoi(&buf[4]) - 1;
			if (current < 0) {
				current = 0;
			} else if (current > 1) {
				current = 1;
			}
		} else if (strncmp(buf, "DevNum=", 7) == 0) {
			g.cfg.PadDef[current].DevNum = atoi(&buf[7]);
		} else if (strncmp(buf, "Type=", 5) == 0) {
			g.cfg.PadDef[current].Type = atoi(&buf[5]);
		} else if (strncmp(buf, "Select=", 7) == 0) {
			sscanf(buf, "Select=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_SELECT].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_SELECT].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_SELECT].J.d = c;
		} else if (strncmp(buf, "L3=", 3) == 0) {
			sscanf(buf, "L3=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_L3].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_L3].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_L3].J.d = c;
		} else if (strncmp(buf, "R3=", 3) == 0) {
			sscanf(buf, "R3=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_R3].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_R3].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_R3].J.d = c;
		} else if (strncmp(buf, "Start=", 6) == 0) {
			sscanf(buf, "Start=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_START].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_START].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_START].J.d = c;
		} else if (strncmp(buf, "Up=", 3) == 0) {
			sscanf(buf, "Up=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_UP].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_UP].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_UP].J.d = c;
		} else if (strncmp(buf, "Right=", 6) == 0) {
			sscanf(buf, "Right=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_RIGHT].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_RIGHT].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_RIGHT].J.d = c;
		} else if (strncmp(buf, "Down=", 5) == 0) {
			sscanf(buf, "Down=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_DOWN].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_DOWN].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_DOWN].J.d = c;
		} else if (strncmp(buf, "Left=", 5) == 0) {
			sscanf(buf, "Left=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_LEFT].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_LEFT].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_LEFT].J.d = c;
		} else if (strncmp(buf, "L2=", 3) == 0) {
			sscanf(buf, "L2=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_L2].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_L2].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_L2].J.d = c;
		} else if (strncmp(buf, "R2=", 3) == 0) {
			sscanf(buf, "R2=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_R2].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_R2].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_R2].J.d = c;
		} else if (strncmp(buf, "L1=", 3) == 0) {
			sscanf(buf, "L1=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_L1].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_L1].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_L1].J.d = c;
		} else if (strncmp(buf, "R1=", 3) == 0) {
			sscanf(buf, "R1=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_R1].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_R1].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_R1].J.d = c;
		} else if (strncmp(buf, "Triangle=", 9) == 0) {
			sscanf(buf, "Triangle=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_TRIANGLE].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_TRIANGLE].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_TRIANGLE].J.d = c;
		} else if (strncmp(buf, "Circle=", 7) == 0) {
			sscanf(buf, "Circle=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_CIRCLE].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_CIRCLE].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_CIRCLE].J.d = c;
		} else if (strncmp(buf, "Cross=", 6) == 0) {
			sscanf(buf, "Cross=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_CROSS].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_CROSS].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_CROSS].J.d = c;
		} else if (strncmp(buf, "Square=", 7) == 0) {
			sscanf(buf, "Square=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].KeyDef[DKEY_SQUARE].Key = a;
			g.cfg.PadDef[current].KeyDef[DKEY_SQUARE].JoyEvType = b;
			g.cfg.PadDef[current].KeyDef[DKEY_SQUARE].J.d = c;
		} else if (strncmp(buf, "LeftAnalogXP=", 13) == 0) {
			sscanf(buf, "LeftAnalogXP=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_XP].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_XP].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_XP].J.d = c;
		} else if (strncmp(buf, "LeftAnalogXM=", 13) == 0) {
			sscanf(buf, "LeftAnalogXM=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_XM].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_XM].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_XM].J.d = c;
		} else if (strncmp(buf, "LeftAnalogYP=", 13) == 0) {
			sscanf(buf, "LeftAnalogYP=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_YP].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_YP].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_YP].J.d = c;
		} else if (strncmp(buf, "LeftAnalogYM=", 13) == 0) {
			sscanf(buf, "LeftAnalogYM=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_YM].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_YM].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_LEFT][ANALOG_YM].J.d = c;
		} else if (strncmp(buf, "RightAnalogXP=", 14) == 0) {
			sscanf(buf, "RightAnalogXP=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_XP].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_XP].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_XP].J.d = c;
		} else if (strncmp(buf, "RightAnalogXM=", 14) == 0) {
			sscanf(buf, "RightAnalogXM=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_XM].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_XM].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_XM].J.d = c;
		} else if (strncmp(buf, "RightAnalogYP=", 14) == 0) {
			sscanf(buf, "RightAnalogYP=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_YP].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_YP].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_YP].J.d = c;
		} else if (strncmp(buf, "RightAnalogYM=", 14) == 0) {
			sscanf(buf, "RightAnalogYM=%d,%d,%d", &a, &b, &c);
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_YM].Key = a;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_YM].JoyEvType = b;
			g.cfg.PadDef[current].AnalogDef[ANALOG_RIGHT][ANALOG_YM].J.d = c;
		}
	}

	fclose(fp);
}

void SavePADConfig() {
	FILE		*fp;
	int			i;
	char		buf[256];

	sprintf(buf, "%s/Library/Preferences/net.pcsx.DFInput.plist", getenv("HOME"));

	fp = fopen(buf, "w");
	if (fp == NULL) {
		return;
	}

	fprintf(fp, "[CONFIG]\n");
	fprintf(fp, "Threaded=%d\n", g.cfg.Threaded);
	fprintf(fp, "\n");

	for (i = 0; i < 2; i++) {
		fprintf(fp, "[PAD%d]\n", i + 1);
		fprintf(fp, "DevNum=%d\n", g.cfg.PadDef[i].DevNum);
		fprintf(fp, "Type=%d\n", g.cfg.PadDef[i].Type);

		fprintf(fp, "Select=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_SELECT].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_SELECT].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_SELECT].J.d);
		fprintf(fp, "L3=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_L3].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_L3].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_L3].J.d);
		fprintf(fp, "R3=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_R3].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_R3].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_R3].J.d);
		fprintf(fp, "Start=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_START].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_START].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_START].J.d);
		fprintf(fp, "Up=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_UP].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_UP].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_UP].J.d);
		fprintf(fp, "Right=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_RIGHT].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_RIGHT].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_RIGHT].J.d);
		fprintf(fp, "Down=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_DOWN].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_DOWN].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_DOWN].J.d);
		fprintf(fp, "Left=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_LEFT].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_LEFT].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_LEFT].J.d);
		fprintf(fp, "L2=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_L2].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_L2].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_L2].J.d);
		fprintf(fp, "R2=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_R2].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_R2].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_R2].J.d);
		fprintf(fp, "L1=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_L1].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_L1].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_L1].J.d);
		fprintf(fp, "R1=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_R1].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_R1].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_R1].J.d);
		fprintf(fp, "Triangle=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_TRIANGLE].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_TRIANGLE].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_TRIANGLE].J.d);
		fprintf(fp, "Circle=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_CIRCLE].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_CIRCLE].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_CIRCLE].J.d);
		fprintf(fp, "Cross=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_CROSS].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_CROSS].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_CROSS].J.d);
		fprintf(fp, "Square=%d,%d,%d\n", g.cfg.PadDef[i].KeyDef[DKEY_SQUARE].Key,
			g.cfg.PadDef[i].KeyDef[DKEY_SQUARE].JoyEvType, g.cfg.PadDef[i].KeyDef[DKEY_SQUARE].J.d);
		fprintf(fp, "LeftAnalogXP=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_XP].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_XP].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_XP].J.d);
		fprintf(fp, "LeftAnalogXM=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_XM].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_XM].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_XM].J.d);
		fprintf(fp, "LeftAnalogYP=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_YP].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_YP].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_YP].J.d);
		fprintf(fp, "LeftAnalogYM=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_YM].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_YM].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_LEFT][ANALOG_YM].J.d);
		fprintf(fp, "RightAnalogXP=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_XP].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_XP].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_XP].J.d);
		fprintf(fp, "RightAnalogXM=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_XM].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_XM].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_XM].J.d);
		fprintf(fp, "RightAnalogYP=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_YP].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_YP].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_YP].J.d);
		fprintf(fp, "RightAnalogYM=%d,%d,%d\n", g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_YM].Key,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_YM].JoyEvType,
			g.cfg.PadDef[i].AnalogDef[ANALOG_RIGHT][ANALOG_YM].J.d);

		fprintf(fp, "\n");
	}

	fclose(fp);
}

long PADconfigure(void) {
	if (SDL_WasInit(SDL_INIT_JOYSTICK)) return -1; // cannot change settings on the fly

	DoConfiguration();
	LoadPADConfig();
	return 0;
}

void PADabout(void) {
	DoAbout();
}

struct {
	uint16_t code;
	const char *desc;
} KeyString[] = {
	{ 0x01, "A" },
	{ 0x0C, "B" },
	{ 0x09, "C" },
	{ 0x03, "D" },
	{ 0x0F, "E" },
	{ 0x04, "F" },
	{ 0x06, "G" },
	{ 0x05, "H" },
	{ 0x23, "I" },
	{ 0x27, "J" },
	{ 0x29, "K" },
	{ 0x26, "L" },
	{ 0x2F, "M" },
	{ 0x2E, "N" },
	{ 0x20, "O" },
	{ 0x24, "P" },
	{ 0x0D, "Q" },
	{ 0x10, "R" },
	{ 0x02, "S" },
	{ 0x12, "T" },
	{ 0x21, "U" },
	{ 0x0A, "V" },
	{ 0x0E, "W" },
	{ 0x08, "X" },
	{ 0x11, "Y" },
	{ 0x07, "Z" },
	{ 0x22, "[" },
	{ 0x1F, "]" },
	{ 0x2A, ";" },
	{ 0x28, "'" },
	{ 0x2C, "," },
	{ 0x30, "." },
	{ 0x2D, "/" },
	{ 0x33, "`" },
	{ 0x13, "1" },
	{ 0x14, "2" },
	{ 0x15, "3" },
	{ 0x16, "4" },
	{ 0x18, "5" },
	{ 0x17, "6" },
	{ 0x1B, "7" },
	{ 0x1D, "8" },
	{ 0x1A, "9" },
	{ 0x1E, "0" },
	{ 0x1C, "-" },
	{ 0x19, "=" },
	{ 0x2B, "\\" },
	{ 0x31, "Tab" },
	{ 0x39, "Shift" },
	{ 0x3C, "Control" },
	{ 0x38, "Command" },
	{ 0x32, "Spacebar" },
	{ 0x34, "Backspace" },
	{ 0x25, "Enter" },
	{ 0x7F, "Up" },
	{ 0x7E, "Down" },
	{ 0x7C, "Left" },
	{ 0x7D, "Right" },
	{ 0x73, "Insert" },
	{ 0x76, "Delete" },
	{ 0x74, "Home" },
	{ 0x78, "End" },
	{ 0x75, "Page Up" },
	{ 0x7A, "Page Down" },
	{ 0x48, "Num Lock" },
	{ 0x4C, "Keypad /" },
	{ 0x44, "Keypad *" },
	{ 0x4F, "Keypad -" },
	{ 0x46, "Keypad +" },
	{ 0x4D, "Keypad Enter" },
	{ 0x53, "Keypad 0" },
	{ 0x54, "Keypad 1" },
	{ 0x55, "Keypad 2" },
	{ 0x56, "Keypad 3" },
	{ 0x57, "Keypad 4" },
	{ 0x58, "Keypad 5" },
	{ 0x59, "Keypad 6" },
	{ 0x5A, "Keypad 7" },
	{ 0x5C, "Keypad 8" },
	{ 0x5D, "Keypad 9" },
	{ 0x42, "Keypad ." },
	{ 0x00, NULL }
};

static const char *XKeysymToString(uint16_t key) {
	static char buf[64];
	int i = 0;

	while (KeyString[i].code != 0) {
		if (KeyString[i].code == key) {
			strcpy(buf, KeyString[i].desc);
			return buf;
		}
		i++;
	}

	sprintf(buf, "0x%.2X", key);
	return buf;
}

void GetKeyDescription(char *buf, int joynum, int key) {
	const char *hatname[16] = {"Centered", "Up", "Right", "Rightup",
		"Down", "", "Rightdown", "", "Left", "Leftup", "", "",
		"Leftdown", "", "", ""};

	switch (g.cfg.PadDef[joynum].KeyDef[key].JoyEvType) {
		case BUTTON:
			sprintf(buf, "Joystick: Button %d", g.cfg.PadDef[joynum].KeyDef[key].J.Button);
			break;

		case AXIS:
			sprintf(buf, "Joystick: Axis %d%c", abs(g.cfg.PadDef[joynum].KeyDef[key].J.Axis) - 1,
				g.cfg.PadDef[joynum].KeyDef[key].J.Axis > 0 ? '+' : '-');
			break;

		case HAT:
			sprintf(buf, "Joystick: Hat %d %s", (g.cfg.PadDef[joynum].KeyDef[key].J.Hat >> 8),
				hatname[g.cfg.PadDef[joynum].KeyDef[key].J.Hat & 0x0F]);
			break;

		case NONE:
		default:
			buf[0] = '\0';
			break;
	}

	if (g.cfg.PadDef[joynum].KeyDef[key].Key != 0) {
		if (buf[0] != '\0') {
			strcat(buf, " / ");
		}

		strcat(buf, "Keyboard:");
		strcat(buf, " ");
		strcat(buf, XKeysymToString(g.cfg.PadDef[joynum].KeyDef[key].Key));
	}
}

void GetAnalogDescription(char *buf, int joynum, int analognum, int dir) {
	const char *hatname[16] = {"Centered", "Up", "Right", "Rightup",
		"Down", "", "Rightdown", "", "Left", "Leftup", "", "",
		"Leftdown", "", "", ""};

	switch (g.cfg.PadDef[joynum].AnalogDef[analognum][dir].JoyEvType) {
		case BUTTON:
			sprintf(buf, "Joystick: Button %d", g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Button);
			break;

		case AXIS:
			sprintf(buf, "Joystick: Axis %d%c", abs(g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Axis) - 1,
				g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Axis > 0 ? '+' : '-');
			break;

		case HAT:
			sprintf(buf, "Joystick: Hat %d %s", (g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Hat >> 8),
				hatname[g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Hat & 0x0F]);
			break;

		case NONE:
		default:
			buf[0] = '\0';
			break;
	}

	if (g.cfg.PadDef[joynum].AnalogDef[analognum][dir].Key != 0) {
		if (buf[0] != '\0') {
			strcat(buf, " / ");
		}

		strcat(buf, "Keyboard:");
		strcat(buf, " ");
		strcat(buf, XKeysymToString(g.cfg.PadDef[joynum].AnalogDef[analognum][dir].Key));
	}
}

int CheckKeyDown() {
	KeyMap theKeys;
	unsigned char *keybytes;
	int i;

	GetKeys(theKeys);
	keybytes = (unsigned char *) theKeys;

	for (i = 0; i < 128; i++) {
		if (i == 0x3A) continue; // Ignore capslock

		if (keybytes[i >> 3] & (1 << (i & 7)))
			return i + 1;
	}

	return 0;
}

static Sint16 InitialAxisPos[256], PrevAxisPos[256];

#define NUM_AXES(js) (SDL_JoystickNumAxes(js) > 256 ? 256 : SDL_JoystickNumAxes(js))

void InitAxisPos(int padnum) {
	int i;
	SDL_Joystick *js;

	if (g.cfg.PadDef[padnum].DevNum >= 0) {
		js = SDL_JoystickOpen(g.cfg.PadDef[padnum].DevNum);
		SDL_JoystickEventState(SDL_IGNORE);
	} else return;

	SDL_JoystickUpdate();

	for (i = 0; i < NUM_AXES(js); i++) {
		InitialAxisPos[i] = PrevAxisPos[i] = SDL_JoystickGetAxis(js, i);
	}

	SDL_JoystickClose(js);
}

int ReadDKeyEvent(int padnum, int key) {
	SDL_Joystick *js;
	int i, changed = 0, t;
	Sint16 axis;

	if (g.cfg.PadDef[padnum].DevNum >= 0) {
		js = SDL_JoystickOpen(g.cfg.PadDef[padnum].DevNum);
		SDL_JoystickEventState(SDL_IGNORE);
	} else {
		js = NULL;
	}

	for (t = 0; t < 1000000 / 1000; t++) {
		// check joystick events
		if (js != NULL) {
			SDL_JoystickUpdate();

			for (i = 0; i < SDL_JoystickNumButtons(js); i++) {
				if (SDL_JoystickGetButton(js, i)) {
					g.cfg.PadDef[padnum].KeyDef[key].JoyEvType = BUTTON;
					g.cfg.PadDef[padnum].KeyDef[key].J.Button = i;
					changed = 1;
					goto end;
				}
			}

			for (i = 0; i < NUM_AXES(js); i++) {
				axis = SDL_JoystickGetAxis(js, i);
				if (abs(axis) > 16383 && (abs(axis - PrevAxisPos[i]) > 4096 || abs(axis - InitialAxisPos[i]) > 4096)) {
					g.cfg.PadDef[padnum].KeyDef[key].JoyEvType = AXIS;
					g.cfg.PadDef[padnum].KeyDef[key].J.Axis = (i + 1) * (axis > 0 ? 1 : -1);
					changed = 1;
					goto end;
				}
				PrevAxisPos[i] = axis;
			}

			for (i = 0; i < SDL_JoystickNumHats(js); i++) {
				axis = SDL_JoystickGetHat(js, i);
				if (axis != SDL_HAT_CENTERED) {
					g.cfg.PadDef[padnum].KeyDef[key].JoyEvType = HAT;

					if (axis & SDL_HAT_UP) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_UP);
					} else if (axis & SDL_HAT_DOWN) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_DOWN);
					} else if (axis & SDL_HAT_LEFT) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_LEFT);
					} else if (axis & SDL_HAT_RIGHT) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_RIGHT);
					}

					changed = 1;
					goto end;
				}
			}
		}

		// check keyboard events
		i = CheckKeyDown();
		if (i != 0) {
			if (i != 0x36) g.cfg.PadDef[padnum].KeyDef[key].Key = i;
			changed = 1;
			goto end;
		}

		// check mouse events
		if (Button()) {
			changed = 2;
			goto end;
		}

		usleep(1000);
	}

end:
	if (js != NULL) {
		SDL_JoystickClose(js);
	}

	return changed;
}

int ReadAnalogEvent(int padnum, int analognum, int analogdir) {
	SDL_Joystick *js;
	int i, changed = 0, t;
	Sint16 axis;

	if (g.cfg.PadDef[padnum].DevNum >= 0) {
		js = SDL_JoystickOpen(g.cfg.PadDef[padnum].DevNum);
		SDL_JoystickEventState(SDL_IGNORE);
	} else {
		js = NULL;
	}

	for (t = 0; t < 1000000 / 1000; t++) {
		// check joystick events
		if (js != NULL) {
			SDL_JoystickUpdate();

			for (i = 0; i < SDL_JoystickNumButtons(js); i++) {
				if (SDL_JoystickGetButton(js, i)) {
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].JoyEvType = BUTTON;
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Button = i;
					changed = 1;
					goto end;
				}
			}

			for (i = 0; i < NUM_AXES(js); i++) {
				axis = SDL_JoystickGetAxis(js, i);
				if (abs(axis) > 16383 && (abs(axis - PrevAxisPos[i]) > 4096 || abs(axis - InitialAxisPos[i]) > 4096)) {
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].JoyEvType = AXIS;
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Axis = (i + 1) * (axis > 0 ? 1 : -1);
					changed = 1;
					goto end;
				}
				PrevAxisPos[i] = axis;
			}

			for (i = 0; i < SDL_JoystickNumHats(js); i++) {
				axis = SDL_JoystickGetHat(js, i);
				if (axis != SDL_HAT_CENTERED) {
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].JoyEvType = HAT;

					if (axis & SDL_HAT_UP) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_UP);
					} else if (axis & SDL_HAT_DOWN) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_DOWN);
					} else if (axis & SDL_HAT_LEFT) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_LEFT);
					} else if (axis & SDL_HAT_RIGHT) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_RIGHT);
					}

					changed = 1;
					goto end;
				}
			}
		}

		// check keyboard events
		i = CheckKeyDown();
		if (i != 0) {
			if (i != 0x36) g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].Key = i;
			changed = 1;
			goto end;
		}

		// check mouse events
		if (Button()) {
			changed = 2;
			goto end;
		}

		usleep(1000);
	}

end:
	if (js != NULL) {
		SDL_JoystickClose(js);
	}

	return changed;
}

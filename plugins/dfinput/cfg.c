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

#define CONFIG_FILE		"dfinput.cfg"

GLOBALDATA			g;

static void SetDefaultConfig() {
	memset(&g.cfg, 0, sizeof(g.cfg));

	g.cfg.Threaded = 1;

	g.cfg.PadDef[0].DevNum = 0;
	g.cfg.PadDef[1].DevNum = 1;

	g.cfg.PadDef[0].Type = PSE_PAD_TYPE_STANDARD;
	g.cfg.PadDef[1].Type = PSE_PAD_TYPE_STANDARD;

	// Pad1 keyboard
	g.cfg.PadDef[0].KeyDef[DKEY_SELECT].Key = XK_c;
	g.cfg.PadDef[0].KeyDef[DKEY_START].Key = XK_v;
	g.cfg.PadDef[0].KeyDef[DKEY_UP].Key = XK_Up;
	g.cfg.PadDef[0].KeyDef[DKEY_RIGHT].Key = XK_Right;
	g.cfg.PadDef[0].KeyDef[DKEY_DOWN].Key = XK_Down;
	g.cfg.PadDef[0].KeyDef[DKEY_LEFT].Key = XK_Left;
	g.cfg.PadDef[0].KeyDef[DKEY_L2].Key = XK_e;
	g.cfg.PadDef[0].KeyDef[DKEY_R2].Key = XK_t;
	g.cfg.PadDef[0].KeyDef[DKEY_L1].Key = XK_w;
	g.cfg.PadDef[0].KeyDef[DKEY_R1].Key = XK_r;
	g.cfg.PadDef[0].KeyDef[DKEY_TRIANGLE].Key = XK_d;
	g.cfg.PadDef[0].KeyDef[DKEY_CIRCLE].Key = XK_x;
	g.cfg.PadDef[0].KeyDef[DKEY_CROSS].Key = XK_z;
	g.cfg.PadDef[0].KeyDef[DKEY_SQUARE].Key = XK_s;

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

	fp = fopen(CONFIG_FILE, "r");
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

	fp = fopen(CONFIG_FILE, "w");
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

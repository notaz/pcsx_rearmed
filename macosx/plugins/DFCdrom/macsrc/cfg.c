/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * Based on: Cdrom for Psemu Pro like Emulators
 * By: linuzappz <linuzappz@hotmail.com>
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

#include "cdr.h"

void AboutDlgProc();
void ConfDlgProc();
void ReadConfig();

char CdromDev[256];
long ReadMode;
long UseSubQ;
long CacheSize;
long CdrSpeed;
long SpinDown;

void LoadConf() {
	strcpy(CdromDev, "");
	ReadMode = THREADED;
	UseSubQ = 0;
	CacheSize = 64;
	CdrSpeed = 0;
	SpinDown = SPINDOWN_VENDOR_SPECIFIC;

	ReadConfig();
}

long CDRconfigure() {
	ConfDlgProc();
	return 0;
}

void CDRabout() {
	AboutDlgProc();
}
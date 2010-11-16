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

#ifdef USE_NULL

int OpenCdHandle(const char *dev) {
	return -1;
}

void CloseCdHandle() {
}

int IsCdHandleOpen() {
	return 0;
}

long GetTN(unsigned char *buffer) {
	buffer[0] = 0;
	buffer[1] = 0;
	return 0;
}

long GetTD(unsigned char track, unsigned char *buffer) {
	memset(buffer + 1, 0, 3);
	return 0;
}

long GetTE(unsigned char track, unsigned char *m, unsigned char *s, unsigned char *f) {
	return -1;
}

long ReadSector(crdata *cr) {
	return -1;
}

long PlayCDDA(unsigned char *sector) {
	return 0;
}

long StopCDDA() {
	return 0;
}

long GetStatus(int playing, struct CdrStat *stat) {
	return -1;
}

unsigned char *ReadSub(const unsigned char *time) {
	return NULL;
}

#endif

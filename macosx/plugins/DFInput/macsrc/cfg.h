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

#ifndef CFG_H
#define CFG_H

#include "pad.h"

void GetKeyDescription(char *buf, int joynum, int key);
void GetAnalogDescription(char *buf, int joynum, int analognum, int dir);
void InitAxisPos(int padnum);
int ReadDKeyEvent(int padnum, int key);
int ReadAnalogEvent(int padnum, int analognum, int analogdir);

#endif

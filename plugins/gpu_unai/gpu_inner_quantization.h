/***************************************************************************
*   Copyright (C) 2016 PCSX4ALL Team                                      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
***************************************************************************/

#ifndef _OP_DITHER_H_
#define _OP_DITHER_H_

static void SetupDitheringConstants()
{
	static const s8 DitherMatrix[4][4] = {
		{ -4,  0, -3,  1 },
		{  2, -2,  3, -1 },
		{ -3,  1, -4,  0 },
		{  3, -1,  2, -2 }
	};

	int i, j;
	for (i = 0; i < 4; i++) {
		u32 packed = 0;
		for (j = 0; j < 4; j++) {
			u32 val = (u32)(s32)DitherMatrix[i][j] << 4;
			gpu_unai.DitherLut16[i][j] = (s16)val;
			packed |= (val & 0xffu) << j*8u;
		}
		gpu_unai.DitherLut32[i] = packed;
	}
}

#endif //_OP_DITHER_H_

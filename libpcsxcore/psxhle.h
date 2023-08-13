/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
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

#ifndef __PSXHLE_H__
#define __PSXHLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"
#include "plugins.h"

void hleExc0_0_1(); void hleExc0_0_2();
void hleExc0_1_1(); void hleExc0_1_2();
void hleExc0_2_2_syscall();
void hleExc1_0_1(); void hleExc1_0_2();
void hleExc1_1_1(); void hleExc1_1_2();
void hleExc1_2_1(); void hleExc1_2_2();
void hleExc1_3_1(); void hleExc1_3_2();
void hleExc3_0_2_defint();
void hleExcPadCard1(); void hleExcPadCard2();

enum hle_op {
	hleop_dummy = 0, hleop_a0, hleop_b0, hleop_c0,
	hleop_bootstrap, hleop_execret, hleop_exception, hleop_unused,
	hleop_exc0_0_1, hleop_exc0_0_2,
	hleop_exc0_1_1, hleop_exc0_1_2, hleop_exc0_2_2,
	hleop_exc1_0_1, hleop_exc1_0_2,
	hleop_exc1_1_1, hleop_exc1_1_2,
	hleop_exc1_2_1, hleop_exc1_2_2,
	hleop_exc1_3_1, hleop_exc1_3_2,
	hleop_exc3_0_2,
	hleop_exc_padcard1, hleop_exc_padcard2,
};

extern void (* const psxHLEt[24])();

#ifdef __cplusplus
}
#endif
#endif

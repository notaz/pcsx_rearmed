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

/*
* Sound (SPU) functions.
*/

#include "spu.h"

void CALLBACK SPUirq(void) {
	psxHu32ref(0x1070) |= SWAPu32(0x200);
}

// spuUpdate
void CALLBACK SPUschedule(unsigned int cycles_after) {
	psxRegs.interrupt |= (1 << PSXINT_SPU_UPDATE);
	psxRegs.intCycle[PSXINT_SPU_UPDATE].cycle = cycles_after;
	psxRegs.intCycle[PSXINT_SPU_UPDATE].sCycle = psxRegs.cycle;
	new_dyna_set_event(PSXINT_SPU_UPDATE, cycles_after);
}

void spuUpdate() {
	SPU_async(psxRegs.cycle, 0);
}

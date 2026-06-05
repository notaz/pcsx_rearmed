/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2016  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses>.
 */

#ifndef __GTE_ARM_H__
#define __GTE_ARM_H__

#include "psxcommon.h"

u32 gteMAC123f_arm(psxCP2Regs *regs, s32 vx, s32 vy, s32 vz,
	const s16 *mx, const s32 *cv, int shift);

// note: doesn't use opcode, just maintaining common signature
void gteRTPS_sf1lm0_arm(psxCP2Regs *cp2_regs, u32 opcode);
void gteRTPT_sf1lm0_arm(psxCP2Regs *cp2_regs, u32 opcode);
void gteRTPS_sf1lm0_nf_arm(psxCP2Regs *cp2_regs, u32 opcode);
void gteRTPT_sf1lm0_nf_arm(psxCP2Regs *cp2_regs, u32 opcode);
void gteNCLIP_arm(psxCP2Regs *cp2_regs, u32 opcode);

// decomposed ops, nonstd calling convention (see asm)
void gteMVMVA_part_sf0_arm(void *cp2_regs, u32 opcode);
void gteMVMVA_part_sf1_arm(void *cp2_regs, u32 opcode);
void gteMVMVA_part_sf0cv3_arm(void *cp2_regs, u32 opcode);
void gteMVMVA_part_sf1cv3_arm(void *cp2_regs, u32 opcode);

void gteMACtoIR_lm0_arm(void *cp2_regs, u32 gte_flags);
void gteMACtoIR_lm1_arm(void *cp2_regs, u32 gte_flags);
void gteMACtoIR_lm0_nf_arm(void *cp2_regs);
void gteMACtoIR_lm1_nf_arm(void *cp2_regs);

#endif /* __GTE_ARM_H__ */

/***************************************************************************
 *   PCSX-Revolution - PlayStation Emulator for Nintendo Wii               *
 *   Copyright (C) 2009-2010  PCSX-Revolution Dev Team                     *
 *   <http://code.google.com/p/pcsx-revolution/>                           *
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

#ifndef __GTE_H__
#define __GTE_H__

enum gteop_opcodes {
	GTEOP_RTPS  = 0x01,
	GTEOP_NCLIP = 0x06,
	GTEOP_OP    = 0x0c,
	GTEOP_DPCS  = 0x10,
	GTEOP_INTPL = 0x11,
	GTEOP_MVMVA = 0x12,
	GTEOP_NCDS  = 0x13,
	GTEOP_CDP   = 0x14,
	GTEOP_NCDT  = 0x16,
	GTEOP_NCCS  = 0x1b,
	GTEOP_CC    = 0x1c,
	GTEOP_NCS   = 0x1e,
	GTEOP_NCT   = 0x20,
	GTEOP_SQR   = 0x28,
	GTEOP_DCPL  = 0x29,
	GTEOP_DPCT  = 0x2a,
	GTEOP_AVSZ3 = 0x2d,
	GTEOP_AVSZ4 = 0x2e,
	GTEOP_RTPT  = 0x30,
	GTEOP_GPF   = 0x3d,
	GTEOP_GPL   = 0x3e,
	GTEOP_NCCT  = 0x3f,
};

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"

struct psxCP2Regs;

extern const unsigned char gte_cycletab[64];

void gteCheckStall(u32 op);
void gteDispatch(psxCP2Regs *regs, u32 code);

u32  MFC2(struct psxCP2Regs *regs, int reg);
void MTC2(struct psxCP2Regs *regs, u32 value, int reg);
void CTC2(struct psxCP2Regs *regs, u32 value, int reg);

typedef void (gte_handler)(psxCP2Regs *regs, u32 code);
gte_handler *gteGetHandler(u32 code);
gte_handler *gteGetHandler_nf(u32 code);

void gteSQR_part_noshift(struct psxCP2Regs *regs);
void gteSQR_part_shift(struct psxCP2Regs *regs);
void gteOP_part_noshift(struct psxCP2Regs *regs);
void gteOP_part_shift(struct psxCP2Regs *regs);
void gteGPF_part_noshift(struct psxCP2Regs *regs);
void gteGPF_part_shift(struct psxCP2Regs *regs);
void gteGPL_part_noshift(struct psxCP2Regs *regs);

void gteGPL_part_shift(struct psxCP2Regs *regs);
void gteGPL_part_shift_nf(struct psxCP2Regs *regs);
void gteDPCS_part_noshift(struct psxCP2Regs *regs);
void gteDPCS_part_noshift_nf(struct psxCP2Regs *regs);
void gteDPCS_part_shift(struct psxCP2Regs *regs);
void gteDPCS_part_shift_nf(struct psxCP2Regs *regs);
void gteINTPL_part_noshift(struct psxCP2Regs *regs);
void gteINTPL_part_noshift_nf(struct psxCP2Regs *regs);
void gteINTPL_part_shift(struct psxCP2Regs *regs);
void gteINTPL_part_shift_nf(struct psxCP2Regs *regs);
void gteMACtoRGB(struct psxCP2Regs *regs);
void gteMACtoRGB_nf(struct psxCP2Regs *regs);

#ifdef __cplusplus
}
#endif
#endif

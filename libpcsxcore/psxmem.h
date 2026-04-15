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

#ifndef __PSXMEMORY_H__
#define __PSXMEMORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define SWAP16(v) __builtin_bswap16(v)
#define SWAP32(v) __builtin_bswap32(v)
#define SWAPu32(v) SWAP32((u32)(v))
#define SWAPs32(v) SWAP32((s32)(v))

#define SWAPu16(v) SWAP16((u16)(v))
#define SWAPs16(v) SWAP16((s16)(v))

#else

#define SWAP16(b) (b)
#define SWAP32(b) (b)

#define SWAPu16(b) (b)
#define SWAPu32(b) (b)

#endif

// this is needed because:
// - lighrec sometimes maps ram at address 0
// - memRLUT[] may contain 0 as a valid entry
#define INVALID_PTR_VAL -1l
#ifdef LIGHTREC
#define INVALID_PTR ((void *)INVALID_PTR_VAL)
#else
#define INVALID_PTR NULL
#endif

#define PSXM_SHIFT 19

#define psxMu8(mem)		(*(u8 *)&psxRegs.ptrs.psxM[(mem) & 0x1fffff])
#define psxMu16(mem)	(SWAP16(*(u16 *)&psxRegs.ptrs.psxM[(mem) & 0x1fffff]))
#define psxMu32(mem)	(SWAP32(*(u32 *)&psxRegs.ptrs.psxM[(mem) & 0x1fffff]))

#define psxMu8ref(mem)	(*(u8  *)&psxRegs.ptrs.psxM[(mem) & 0x1fffff])
#define psxMu16ref(mem)	(*(u16 *)&psxRegs.ptrs.psxM[(mem) & 0x1fffff])
#define psxMu32ref(mem)	(*(u32 *)&psxRegs.ptrs.psxM[(mem) & 0x1fffff])

#define psxHu8(mem)		(*(u8 *)&psxRegs.ptrs.psxH[(mem) & 0xffff])
#define psxHu16(mem)	(SWAP16(*(u16 *)&psxRegs.ptrs.psxH[(mem) & 0xffff]))
#define psxHu32(mem)	(SWAP32(*(u32 *)&psxRegs.ptrs.psxH[(mem) & 0xffff]))

#define psxHu8ref(mem)	(*(u8  *)&psxRegs.ptrs.psxH[(mem) & 0xffff])
#define psxHu16ref(mem)	(*(u16 *)&psxRegs.ptrs.psxH[(mem) & 0xffff])
#define psxHu32ref(mem)	(*(u32 *)&psxRegs.ptrs.psxH[(mem) & 0xffff])

extern int cache_isolated;

#ifndef DISABLE_MEM_LUTS
#define DISABLE_MEM_LUTS 0
#endif

// it may seem returning the pointer would be convenient here,
// but that causes double compare (after lut and after ptr arithmetic),
// and this is a hot path for the interpreter and non-dynarec-allowing systems, like apple
static inline int psxm_lut(u8 **ret, const psxRegisters *regs, u32 mem, int write,
	const uintptr_t *lut)
{
	if (!DISABLE_MEM_LUTS) {
		uintptr_t ptr = lut[mem >> PSXM_SHIFT];
		if (ptr != INVALID_PTR_VAL) {
			*ret = (u8 *)(ptr + mem);
			return 1;
		}
		return 0;
	}

	if (mem >= 0xa0000000)
		mem -= 0xa0000000;
	else
		mem &= ~0x80000000;

	if (mem < 0x800000u) {
		if (write && cache_isolated)
			return 0;

		*ret = regs->ptrs.psxM + (mem & 0x1fffff);
		return 1;
	}

	if (!write && mem - 0x1fc00000u < 0x80000u) {
		*ret = regs->ptrs.psxR + (mem - 0x1fc00000u);
		return 1;
	}

	return 0;
}

static inline int psxm_(u8 **ret, const psxRegisters *regs, u32 mem, int write)
{
	return psxm_lut(ret, regs, mem, write,
		write ? regs->ptrs.memWLUT : regs->ptrs.memRLUT);
}

static inline void * psxm(u32 mem, int write)
{
	u8 *ret;
	if (psxm_(&ret, &psxRegs, mem, write))
		return ret;
	if ((mem & 0x7ffffc00) == 0x1f800000)
		return psxRegs.ptrs.psxR + (mem & 0x3ff);
	return INVALID_PTR;
}

#define PSXM(mem) psxm(mem, 0)
#define PSXMu8(mem)		(*(u8 *)PSXM(mem))
#define PSXMu16(mem)	(SWAP16(*(u16 *)PSXM(mem)))
#define PSXMu32(mem)	(SWAP32(*(u32 *)PSXM(mem)))

#define PSXMu32ref(mem)	(*(u32 *)PSXM(mem))

struct psxRegisters;

int psxMemInit();
void psxMemReset();
void psxMemOnIsolate(int enable);
void psxMemShutdown();

u8  psxMemRead8 (struct psxRegisters *regs, u32 mem);
u16 psxMemRead16(struct psxRegisters *regs, u32 mem);
u32 psxMemRead32(struct psxRegisters *regs, u32 mem);
void psxMemWrite8 (struct psxRegisters *regs, u32 mem, u32 value);
void psxMemWrite16(struct psxRegisters *regs, u32 mem, u32 value);
void psxMemWrite32(struct psxRegisters *regs, u32 mem, u32 value);

#ifdef __cplusplus
}
#endif
#endif

/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2003  Pcsx Team
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA 02111-1307 USA
 */

#ifdef _MSC_VER_
#pragma warning(disable:4244)
#pragma warning(disable:4761)
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#include "../psxcommon.h"
#include "ppc.h"
#include "reguse.h"
#include "../r3000a.h"
#include "../psxhle.h"

//#define NO_CONSTANT

u32 *psxRecLUT;

#undef _Op_
#define _Op_     _fOp_(psxRegs.code)
#undef _Funct_
#define _Funct_  _fFunct_(psxRegs.code)
#undef _Rd_
#define _Rd_     _fRd_(psxRegs.code)
#undef _Rt_
#define _Rt_     _fRt_(psxRegs.code)
#undef _Rs_
#define _Rs_     _fRs_(psxRegs.code)
#undef _Sa_
#define _Sa_     _fSa_(psxRegs.code)
#undef _Im_
#define _Im_     _fIm_(psxRegs.code)
#undef _Target_
#define _Target_ _fTarget_(psxRegs.code)

#undef _Imm_
#define _Imm_	 _fImm_(psxRegs.code)
#undef _ImmU_
#define _ImmU_	 _fImmU_(psxRegs.code)

#undef PC_REC
#undef PC_REC8
#undef PC_REC16
#undef PC_REC32
#define PC_REC(x)	(psxRecLUT[x >> 16] + (x & 0xffff))
#define PC_REC8(x)	(*(u8 *)PC_REC(x))
#define PC_REC16(x) (*(u16*)PC_REC(x))
#define PC_REC32(x) (*(u32*)PC_REC(x))

#define OFFSET(X,Y) ((u32)(Y)-(u32)(X))

#define RECMEM_SIZE		(12*1024*1024)

static char *recMem;	/* the recompiled blocks will be here */
static char *recRAM;	/* and the ptr to the blocks here */
static char *recROM;	/* and here */

static u32 pc;			/* recompiler pc */
static u32 pcold;		/* recompiler oldpc */
static int count;		/* recompiler intruction count */
static int branch;		/* set for branch */
static u32 target;		/* branch target */
static u32 resp;

u32 cop2readypc = 0;
u32 idlecyclecount = 0;

#define NUM_REGISTERS	34
typedef struct {
	int state;
	u32 k;
	int reg;
} iRegisters;

static iRegisters iRegs[34];

#define ST_UNK      0x00
#define ST_CONST    0x01
#define ST_MAPPED   0x02

#ifdef NO_CONSTANT
#define IsConst(reg) 0
#else
#define IsConst(reg)  (iRegs[reg].state & ST_CONST)
#endif
#define IsMapped(reg) (iRegs[reg].state & ST_MAPPED)

static void (*recBSC[64])();
static void (*recSPC[64])();
static void (*recREG[32])();
static void (*recCP0[32])();
static void (*recCP2[64])();
static void (*recCP2BSC[32])();

#define REG_LO			32
#define REG_HI			33

// Hardware register usage
#define HWUSAGE_NONE     0x00

#define HWUSAGE_READ     0x01
#define HWUSAGE_WRITE    0x02
#define HWUSAGE_CONST    0x04
#define HWUSAGE_ARG      0x08	/* used as an argument for a function call */

#define HWUSAGE_RESERVED 0x10	/* won't get flushed when flushing all regs */
#define HWUSAGE_SPECIAL  0x20	/* special purpose register */
#define HWUSAGE_HARDWIRED 0x40	/* specific hardware register mapping that is never disposed */
#define HWUSAGE_INITED    0x80
#define HWUSAGE_PSXREG    0x100

// Remember to invalidate the special registers if they are modified by compiler
enum {
    ARG1 = 3,
    ARG2 = 4,
    ARG3 = 5,
    PSXREGS,	// ptr
	 PSXMEM,		// ptr
    CYCLECOUNT,	// ptr
    PSXPC,	// ptr
    TARGETPTR,	// ptr
    TARGET,	// ptr
    RETVAL,
    REG_RZERO,
    REG_WZERO
};

typedef struct {
    int code;
    u32 k;
    int usage;
    int lastUsed;
    
    void (*flush)(int hwreg);
    int private;
} HWRegister;
static HWRegister HWRegisters[NUM_HW_REGISTERS];
static int HWRegUseCount;
static int DstCPUReg;
static int UniqueRegAlloc;

static int GetFreeHWReg();
static void InvalidateCPURegs();
static void DisposeHWReg(int index);
static void FlushHWReg(int index);
static void FlushAllHWReg();
static void MapPsxReg32(int reg);
static void FlushPsxReg32(int hwreg);
static int UpdateHWRegUsage(int hwreg, int usage);
static int GetHWReg32(int reg);
static int PutHWReg32(int reg);
static int GetSpecialIndexFromHWRegs(int which);
static int GetHWRegFromCPUReg(int cpureg);
static int MapRegSpecial(int which);
static void FlushRegSpecial(int hwreg);
static int GetHWRegSpecial(int which);
static int PutHWRegSpecial(int which);
static void recRecompile();
static void recError();

#pragma mark --- Generic register mapping ---

static int GetFreeHWReg()
{
	int i, least, index;
	
	if (DstCPUReg != -1) {
		index = GetHWRegFromCPUReg(DstCPUReg);
		DstCPUReg = -1;
	} else {
	    // LRU algorith with a twist ;)
	    for (i=0; i<NUM_HW_REGISTERS; i++) {
		    if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
			    break;
		    }
	    }
    
	    least = HWRegisters[i].lastUsed; index = i;
	    for (; i<NUM_HW_REGISTERS; i++) {
		    if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
			    if (HWRegisters[i].usage == HWUSAGE_NONE && HWRegisters[i].code >= 13) {
				    index = i;
				    break;
			    }
			    else if (HWRegisters[i].lastUsed < least) {
				    least = HWRegisters[i].lastUsed;
				    index = i;
			    }
		    }
	    }
		 
		 // Cycle the registers
		 if (HWRegisters[index].usage == HWUSAGE_NONE) {
			for (; i<NUM_HW_REGISTERS; i++) {
				if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
					if (HWRegisters[i].usage == HWUSAGE_NONE && 
						 HWRegisters[i].code >= 13 && 
						 HWRegisters[i].lastUsed < least) {
						least = HWRegisters[i].lastUsed;
						index = i;
						break;
					}
				}
			}
		 }
	}
	
/*	if (HWRegisters[index].code < 13 && HWRegisters[index].code > 3) {
		SysPrintf("Allocating volatile register %i\n", HWRegisters[index].code);
	}
	if (HWRegisters[index].usage != HWUSAGE_NONE) {
		SysPrintf("RegUse too big. Flushing %i\n", HWRegisters[index].code);
	}*/
	if (HWRegisters[index].usage & (HWUSAGE_RESERVED | HWUSAGE_HARDWIRED)) {
		if (HWRegisters[index].usage & HWUSAGE_RESERVED) {
			SysPrintf("Error! Trying to map a new register to a reserved register (r%i)", 
						HWRegisters[index].code);
		}
		if (HWRegisters[index].usage & HWUSAGE_HARDWIRED) {
			SysPrintf("Error! Trying to map a new register to a hardwired register (r%i)", 
						HWRegisters[index].code);
		}
	}
	
	if (HWRegisters[index].lastUsed != 0) {
		UniqueRegAlloc = 0;
	}
	
	// Make sure the register is really flushed!
	FlushHWReg(index);
	HWRegisters[index].usage = HWUSAGE_NONE;
	HWRegisters[index].flush = NULL;
	
	return index;
}

static void FlushHWReg(int index)
{
	if (index < 0) return;
	if (HWRegisters[index].usage == HWUSAGE_NONE) return;
	
	if (HWRegisters[index].flush) {
		HWRegisters[index].usage |= HWUSAGE_RESERVED;
		HWRegisters[index].flush(index);
		HWRegisters[index].flush = NULL;
	}
	
	if (HWRegisters[index].usage & HWUSAGE_HARDWIRED) {
		HWRegisters[index].usage &= ~(HWUSAGE_READ | HWUSAGE_WRITE);
	} else {
		HWRegisters[index].usage = HWUSAGE_NONE;
	}
}

// get rid of a mapped register without flushing the contents to the memory
static void DisposeHWReg(int index)
{
	if (index < 0) return;
	if (HWRegisters[index].usage == HWUSAGE_NONE) return;
	
	HWRegisters[index].usage &= ~(HWUSAGE_READ | HWUSAGE_WRITE);
	if (HWRegisters[index].usage == HWUSAGE_NONE) {
		SysPrintf("Error! not correctly disposing register (r%i)", HWRegisters[index].code);
	}
	
	FlushHWReg(index);
}

// operated on cpu registers
__inline static void FlushCPURegRange(int start, int end)
{
	int i;
	
	if (end <= 0) end = 31;
	if (start <= 0) start = 0;
	
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
			if (HWRegisters[i].flush)
				FlushHWReg(i);
	}

	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
			FlushHWReg(i);
	}
}

static void FlushAllHWReg()
{
	FlushCPURegRange(0,31);
}

static void InvalidateCPURegs()
{
	FlushCPURegRange(0,12);
}

#pragma mark --- Mapping utility functions ---

static void MoveHWRegToCPUReg(int cpureg, int hwreg)
{
	int dstreg;
	
	if (HWRegisters[hwreg].code == cpureg)
		return;
	
	dstreg = GetHWRegFromCPUReg(cpureg);
	
	HWRegisters[dstreg].usage &= ~(HWUSAGE_HARDWIRED | HWUSAGE_ARG);
	if (HWRegisters[hwreg].usage & (HWUSAGE_READ | HWUSAGE_WRITE)) {
		FlushHWReg(dstreg);
		MR(HWRegisters[dstreg].code, HWRegisters[hwreg].code);
	} else {
		if (HWRegisters[dstreg].usage & (HWUSAGE_READ | HWUSAGE_WRITE)) {
			MR(HWRegisters[hwreg].code, HWRegisters[dstreg].code);
		}
		else if (HWRegisters[dstreg].usage != HWUSAGE_NONE) {
			FlushHWReg(dstreg);
		}
	}
	
	HWRegisters[dstreg].code = HWRegisters[hwreg].code;
	HWRegisters[hwreg].code = cpureg;
}

static int UpdateHWRegUsage(int hwreg, int usage)
{
	HWRegisters[hwreg].lastUsed = ++HWRegUseCount;    
	if (usage & HWUSAGE_WRITE) {
		HWRegisters[hwreg].usage &= ~HWUSAGE_CONST;
	}
	if (!(usage & HWUSAGE_INITED)) {
		HWRegisters[hwreg].usage &= ~HWUSAGE_INITED;
	}
	HWRegisters[hwreg].usage |= usage;
	
	return HWRegisters[hwreg].code;
}

static int GetHWRegFromCPUReg(int cpureg)
{
	int i;
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code == cpureg) {
			return i;
		}
	}
	
	SysPrintf("Error! Register location failure (r%i)", cpureg);
	return 0;
}

// this function operates on cpu registers
void SetDstCPUReg(int cpureg)
{
	DstCPUReg = cpureg;
}

static void ReserveArgs(int args)
{
	int index, i;
	
	for (i=0; i<args; i++) {
		index = GetHWRegFromCPUReg(3+i);
		HWRegisters[index].usage |= HWUSAGE_RESERVED | HWUSAGE_HARDWIRED | HWUSAGE_ARG;
	}
}

static void ReleaseArgs()
{
	int i;
	
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].usage & HWUSAGE_ARG) {
			//HWRegisters[i].usage = HWUSAGE_NONE;
			//HWRegisters[i].flush = NULL;
			HWRegisters[i].usage &= ~(HWUSAGE_RESERVED | HWUSAGE_HARDWIRED | HWUSAGE_ARG);
			FlushHWReg(i);
		}
	}
}

#pragma mark --- Psx register mapping ---

static void MapPsxReg32(int reg)
{
    int hwreg = GetFreeHWReg();
    HWRegisters[hwreg].flush = FlushPsxReg32;
    HWRegisters[hwreg].private = reg;
    
    if (iRegs[reg].reg != -1) {
        SysPrintf("error: double mapped psx register");
    }
    
    iRegs[reg].reg = hwreg;
    iRegs[reg].state |= ST_MAPPED;
}

static void FlushPsxReg32(int hwreg)
{
	int reg = HWRegisters[hwreg].private;
	
	if (iRegs[reg].reg == -1) {
		SysPrintf("error: flushing unmapped psx register");
	}
	
	if (HWRegisters[hwreg].usage & HWUSAGE_WRITE) {
		if (branch) {
			/*int reguse = nextPsxRegUse(pc-8, reg);
			if (reguse == REGUSE_NONE || (reguse & REGUSE_READ))*/ {
				STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
			}
		} else {
			int reguse = nextPsxRegUse(pc-4, reg);
			if (reguse == REGUSE_NONE || (reguse & REGUSE_READ)) {
				STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
			}
		}
	}
	
	iRegs[reg].reg = -1;
	iRegs[reg].state = ST_UNK;
}

static int GetHWReg32(int reg)
{
	int usage = HWUSAGE_PSXREG | HWUSAGE_READ;
	
	if (reg == 0) {
		return GetHWRegSpecial(REG_RZERO);
	}
	if (!IsMapped(reg)) {
		usage |= HWUSAGE_INITED;
		MapPsxReg32(reg);
		
		HWRegisters[iRegs[reg].reg].usage |= HWUSAGE_RESERVED;
		if (IsConst(reg)) {
			LIW(HWRegisters[iRegs[reg].reg].code, iRegs[reg].k);
			usage |= HWUSAGE_WRITE | HWUSAGE_CONST;
			//iRegs[reg].state &= ~ST_CONST;
		}
		else {
			LWZ(HWRegisters[iRegs[reg].reg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
		}
		HWRegisters[iRegs[reg].reg].usage &= ~HWUSAGE_RESERVED;
	}
	else if (DstCPUReg != -1) {
		int dst = DstCPUReg;
		DstCPUReg = -1;
		
		if (HWRegisters[iRegs[reg].reg].code < 13) {
			MoveHWRegToCPUReg(dst, iRegs[reg].reg);
		} else {
			MR(DstCPUReg, HWRegisters[iRegs[reg].reg].code);
		}
	}
	
	DstCPUReg = -1;
	
	return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

static int PutHWReg32(int reg)
{
	int usage = HWUSAGE_PSXREG | HWUSAGE_WRITE;
	if (reg == 0) {
		return PutHWRegSpecial(REG_WZERO);
	}
	
	if (DstCPUReg != -1 && IsMapped(reg)) {
		if (HWRegisters[iRegs[reg].reg].code != DstCPUReg) {
			int tmp = DstCPUReg;
			DstCPUReg = -1;
			DisposeHWReg(iRegs[reg].reg);
			DstCPUReg = tmp;
		}
	}
	if (!IsMapped(reg)) {
		usage |= HWUSAGE_INITED;
		MapPsxReg32(reg);
	}
	
	DstCPUReg = -1;
	iRegs[reg].state &= ~ST_CONST;

	return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

#pragma mark --- Special register mapping ---

static int GetSpecialIndexFromHWRegs(int which)
{
	int i;
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].usage & HWUSAGE_SPECIAL) {
			if (HWRegisters[i].private == which) {
				return i;
			}
		}
	}
	return -1;
}

static int MapRegSpecial(int which)
{
	int hwreg = GetFreeHWReg();
	HWRegisters[hwreg].flush = FlushRegSpecial;
	HWRegisters[hwreg].private = which;
	
	return hwreg;
}

static void FlushRegSpecial(int hwreg)
{
	int which = HWRegisters[hwreg].private;
	
	if (!(HWRegisters[hwreg].usage & HWUSAGE_WRITE))
		return;
	
	switch (which) {
		case CYCLECOUNT:
			STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.cycle), GetHWRegSpecial(PSXREGS));
			break;
		case PSXPC:
			STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.pc), GetHWRegSpecial(PSXREGS));
			break;
		case TARGET:
			STW(HWRegisters[hwreg].code, 0, GetHWRegSpecial(TARGETPTR));
			break;
	}
}

static int GetHWRegSpecial(int which)
{
	int index = GetSpecialIndexFromHWRegs(which);
	int usage = HWUSAGE_READ | HWUSAGE_SPECIAL;
	
	if (index == -1) {
		usage |= HWUSAGE_INITED;
		index = MapRegSpecial(which);
		
		HWRegisters[index].usage |= HWUSAGE_RESERVED;
		switch (which) {
			case PSXREGS:
			case PSXMEM:
				SysPrintf("error! shouldn't be here!\n");
				//HWRegisters[index].flush = NULL;
				//LIW(HWRegisters[index].code, (u32)&psxRegs);
				break;
			case TARGETPTR:
				HWRegisters[index].flush = NULL;
				LIW(HWRegisters[index].code, (u32)&target);
				break;
			case REG_RZERO:
				HWRegisters[index].flush = NULL;
				LIW(HWRegisters[index].code, 0);
				break;
			case RETVAL:
				MoveHWRegToCPUReg(3, index);
				/*reg = GetHWRegFromCPUReg(3);
				HWRegisters[reg].code = HWRegisters[index].code;
				HWRegisters[index].code = 3;*/
				HWRegisters[index].flush = NULL;
				
				usage |= HWUSAGE_RESERVED;
				break;

			case CYCLECOUNT:
				LWZ(HWRegisters[index].code, OFFSET(&psxRegs, &psxRegs.cycle), GetHWRegSpecial(PSXREGS));
				break;
			case PSXPC:
				LWZ(HWRegisters[index].code, OFFSET(&psxRegs, &psxRegs.pc), GetHWRegSpecial(PSXREGS));
				break;
			case TARGET:
				LWZ(HWRegisters[index].code, 0, GetHWRegSpecial(TARGETPTR));
				break;
			default:
				SysPrintf("Error: Unknown special register in GetHWRegSpecial()\n");
				break;
		}
		HWRegisters[index].usage &= ~HWUSAGE_RESERVED;
	}
	else if (DstCPUReg != -1) {
		int dst = DstCPUReg;
		DstCPUReg = -1;
		
		MoveHWRegToCPUReg(dst, index);
	}

	return UpdateHWRegUsage(index, usage);
}

static int PutHWRegSpecial(int which)
{
	int index = GetSpecialIndexFromHWRegs(which);
	int usage = HWUSAGE_WRITE | HWUSAGE_SPECIAL;
	
	if (DstCPUReg != -1 && index != -1) {
		if (HWRegisters[index].code != DstCPUReg) {
			int tmp = DstCPUReg;
			DstCPUReg = -1;
			DisposeHWReg(index);
			DstCPUReg = tmp;
		}
	}
	switch (which) {
		case PSXREGS:
		case TARGETPTR:
			SysPrintf("Error: Read-only special register in PutHWRegSpecial()\n");
		case REG_WZERO:
			if (index >= 0) {
					if (HWRegisters[index].usage & HWUSAGE_WRITE)
						break;
			}
			index = MapRegSpecial(which);
			HWRegisters[index].flush = NULL;
			break;
		default:
			if (index == -1) {
				usage |= HWUSAGE_INITED;
				index = MapRegSpecial(which);
				
				HWRegisters[index].usage |= HWUSAGE_RESERVED;
				switch (which) {
					case ARG1:
					case ARG2:
					case ARG3:
						MoveHWRegToCPUReg(3+(which-ARG1), index);
						/*reg = GetHWRegFromCPUReg(3+(which-ARG1));
						
						if (HWRegisters[reg].usage != HWUSAGE_NONE) {
							HWRegisters[reg].usage &= ~(HWUSAGE_HARDWIRED | HWUSAGE_ARG);
							if (HWRegisters[reg].flush != NULL && HWRegisters[reg].usage & (HWUSAGE_WRITE | HWUSAGE_READ)) {
								MR(HWRegisters[index].code, HWRegisters[reg].code);
							} else {
								FlushHWReg(reg);
							}
						}
						HWRegisters[reg].code = HWRegisters[index].code;
						if (!(HWRegisters[index].code >= 3 && HWRegisters[index].code <=31))
							SysPrintf("Error! Register allocation");
						HWRegisters[index].code = 3+(which-ARG1);*/
						HWRegisters[index].flush = NULL;
						
						usage |= HWUSAGE_RESERVED | HWUSAGE_HARDWIRED | HWUSAGE_ARG;
						break;
				}
			}
			HWRegisters[index].usage &= ~HWUSAGE_RESERVED;
			break;
	}
	
	DstCPUReg = -1;

	return UpdateHWRegUsage(index, usage);
}

#pragma mark --- ---

static void MapConst(int reg, u32 _const) {
	if (reg == 0)
		return;
	if (IsConst(reg) && iRegs[reg].k == _const)
		return;
	
	DisposeHWReg(iRegs[reg].reg);
	iRegs[reg].k = _const;
	iRegs[reg].state = ST_CONST;
}

static void MapCopy(int dst, int src)
{
    // do it the lazy way for now
    MR(PutHWReg32(dst), GetHWReg32(src));
}

static void iFlushReg(u32 nextpc, int reg) {
	if (!IsMapped(reg) && IsConst(reg)) {
		GetHWReg32(reg);
	}
	if (IsMapped(reg)) {
		if (nextpc) {
			int use = nextPsxRegUse(nextpc, reg);
			if ((use & REGUSE_RW) == REGUSE_WRITE) {
				DisposeHWReg(iRegs[reg].reg);
			} else {
				FlushHWReg(iRegs[reg].reg);
			}
		} else {
			FlushHWReg(iRegs[reg].reg);
		}
	}
}

static void iFlushRegs(u32 nextpc) {
	int i;

	for (i=1; i<NUM_REGISTERS; i++) {
		iFlushReg(nextpc, i);
	}
}

static void Return()
{
	iFlushRegs(0);
	FlushAllHWReg();
	if (((u32)returnPC & 0x1fffffc) == (u32)returnPC) {
		BA((u32)returnPC);
	}
	else {
		LIW(0, (u32)returnPC);
		MTLR(0);
		BLR();
	}
}

static void iRet() {
    /* store cycle */
    count = (idlecyclecount + (pc - pcold) / 4) * BIAS;
    ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
    Return();
}

static int iLoadTest() {
	u32 tmp;

	// check for load delay
	tmp = psxRegs.code >> 26;
	switch (tmp) {
		case 0x10: // COP0
			switch (_Rs_) {
				case 0x00: // MFC0
				case 0x02: // CFC0
					return 1;
			}
			break;
		case 0x12: // COP2
			switch (_Funct_) {
				case 0x00:
					switch (_Rs_) {
						case 0x00: // MFC2
						case 0x02: // CFC2
							return 1;
					}
					break;
			}
			break;
		case 0x32: // LWC2
			return 1;
		default:
			if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
				return 1;
			}
			break;
	}
	return 0;
}

/* set a pending branch */
static void SetBranch() {
	int treg;
	branch = 1;
	psxRegs.code = PSXMu32(pc);
	pc+=4;

	if (iLoadTest() == 1) {
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));
		/* store cycle */
		count = (idlecyclecount + (pc - pcold) / 4) * BIAS;
		ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
		
		treg = GetHWRegSpecial(TARGET);
		MR(PutHWRegSpecial(ARG2), treg);
		DisposeHWReg(GetHWRegFromCPUReg(treg));
		LIW(PutHWRegSpecial(ARG1), _Rt_);
                LIW(GetHWRegSpecial(PSXPC), pc);
		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);

		Return();
		return;
	}

	recBSC[psxRegs.code>>26]();

	iFlushRegs(0);
	treg = GetHWRegSpecial(TARGET);
	MR(PutHWRegSpecial(PSXPC), GetHWRegSpecial(TARGET)); // FIXME: this line should not be needed
	DisposeHWReg(GetHWRegFromCPUReg(treg));
	FlushAllHWReg();

	count = (idlecyclecount + (pc - pcold) / 4) * BIAS;
        ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);
	
	// TODO: don't return if target is compiled
	Return();
}

static void iJump(u32 branchPC) {
	u32 *b1, *b2;
	branch = 1;
	psxRegs.code = PSXMu32(pc);
	pc+=4;

	if (iLoadTest() == 1) {
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));
		/* store cycle */
		count = (idlecyclecount + (pc - pcold) / 4) * BIAS;
		ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);

		LIW(PutHWRegSpecial(ARG2), branchPC);
		LIW(PutHWRegSpecial(ARG1), _Rt_);
		LIW(GetHWRegSpecial(PSXPC), pc);
		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);
                
		Return();
		return;
	}

	recBSC[psxRegs.code>>26]();

	iFlushRegs(branchPC);
	LIW(PutHWRegSpecial(PSXPC), branchPC);
	FlushAllHWReg();

	count = (idlecyclecount + (pc - pcold) / 4) * BIAS;
        //if (/*psxRegs.code == 0 &&*/ count == 2 && branchPC == pcold) {
        //    LIW(PutHWRegSpecial(CYCLECOUNT), 0);
        //} else {
            ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
        //}
	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);

	if (!Config.HLE && Config.PsxOut &&
	    ((branchPC & 0x1fffff) == 0xa0 ||
	     (branchPC & 0x1fffff) == 0xb0 ||
	     (branchPC & 0x1fffff) == 0xc0))
	  CALLFunc((u32)psxJumpTest);

	// always return for now...
	//Return();
		
	// maybe just happened an interruption, check so
	LIW(0, branchPC);
	CMPLW(GetHWRegSpecial(PSXPC), 0);
	BNE_L(b1);
	
	LIW(3, PC_REC(branchPC));
	LWZ(3, 0, 3);
	CMPLWI(3, 0);
	BNE_L(b2);
	
	B_DST(b1);
	Return();
	
	// next bit is already compiled - jump right to it
	B_DST(b2);
	MTCTR(3);
	BCTR();
}

static void iBranch(u32 branchPC, int savectx) {
	HWRegister HWRegistersS[NUM_HW_REGISTERS];
	iRegisters iRegsS[NUM_REGISTERS];
	int HWRegUseCountS = 0;
	u32 respold=0;
	u32 *b1, *b2;

	if (savectx) {
		respold = resp;
		memcpy(iRegsS, iRegs, sizeof(iRegs));
		memcpy(HWRegistersS, HWRegisters, sizeof(HWRegisters));
		HWRegUseCountS = HWRegUseCount;
	}
	
	branch = 1;
	psxRegs.code = PSXMu32(pc);

	// the delay test is only made when the branch is taken
	// savectx == 0 will mean that :)
	if (savectx == 0 && iLoadTest() == 1) {
		iFlushRegs(0);
		LIW(0, psxRegs.code);
		STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));
		/* store cycle */
		count = (idlecyclecount + ((pc+4) - pcold) / 4) * BIAS;
		ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);

		LIW(PutHWRegSpecial(ARG2), branchPC);
		LIW(PutHWRegSpecial(ARG1), _Rt_);
                LIW(GetHWRegSpecial(PSXPC), pc);
		FlushAllHWReg();
		CALLFunc((u32)psxDelayTest);

		Return();
		return;
	}

	pc+= 4;
	recBSC[psxRegs.code>>26]();
	
	iFlushRegs(branchPC);
	LIW(PutHWRegSpecial(PSXPC), branchPC);
	FlushAllHWReg();

	/* store cycle */
	count = (idlecyclecount + (pc - pcold) / 4) * BIAS;
        //if (/*psxRegs.code == 0 &&*/ count == 2 && branchPC == pcold) {
        //    LIW(PutHWRegSpecial(CYCLECOUNT), 0);
        //} else {
            ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
        //}
	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);
	
	// always return for now...
	//Return();

	LIW(0, branchPC);
	CMPLW(GetHWRegSpecial(PSXPC), 0);
	BNE_L(b1);
	
	LIW(3, PC_REC(branchPC));
	LWZ(3, 0, 3);
	CMPLWI(3, 0);
	BNE_L(b2);
	
	B_DST(b1);
	Return();
	
	B_DST(b2);
	MTCTR(3);
	BCTR();

	// maybe just happened an interruption, check so
/*	CMP32ItoM((u32)&psxRegs.pc, branchPC);
	j8Ptr[1] = JE8(0);
	RET();

	x86SetJ8(j8Ptr[1]);
	MOV32MtoR(EAX, PC_REC(branchPC));
	TEST32RtoR(EAX, EAX);
	j8Ptr[2] = JNE8(0);
	RET();

	x86SetJ8(j8Ptr[2]);
	JMP32R(EAX);*/

	pc-= 4;
	if (savectx) {
		resp = respold;
		memcpy(iRegs, iRegsS, sizeof(iRegs));
		memcpy(HWRegisters, HWRegistersS, sizeof(HWRegisters));
		HWRegUseCount = HWRegUseCountS;
	}
}


static void iDumpRegs() {
	int i, j;

	printf("%lx %lx\n", psxRegs.pc, psxRegs.cycle);
	for (i=0; i<4; i++) {
		for (j=0; j<8; j++)
			printf("%lx ", psxRegs.GPR.r[j*i]);
		printf("\n");
	}
}

void iDumpBlock(char *ptr) {
/*	FILE *f;
	u32 i;

	SysPrintf("dump1 %x:%x, %x\n", psxRegs.pc, pc, psxCurrentCycle);

	for (i = psxRegs.pc; i < pc; i+=4)
		SysPrintf("%s\n", disR3000AF(PSXMu32(i), i));

	fflush(stdout);
	f = fopen("dump1", "w");
	fwrite(ptr, 1, (u32)x86Ptr - (u32)ptr, f);
	fclose(f);
	system("ndisasmw -u dump1");
	fflush(stdout);*/
}

#define REC_FUNC(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(PutHWRegSpecial(ARG1), (u32)psxRegs.code); \
	STW(GetHWRegSpecial(ARG1), OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
	FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
/*	branch = 2; */\
}

#define REC_SYS(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(PutHWRegSpecial(ARG1), (u32)psxRegs.code); \
        STW(GetHWRegSpecial(ARG1), OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
        FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
	branch = 2; \
	iRet(); \
}

#define REC_BRANCH(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(PutHWRegSpecial(ARG1), (u32)psxRegs.code); \
        STW(GetHWRegSpecial(ARG1), OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
        FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
	branch = 2; \
	iRet(); \
}

static void freeMem(int all)
{
    if (recMem) free(recMem);
    if (recRAM) free(recRAM);
    if (recROM) free(recROM);
    recMem = recRAM = recROM = 0;
    
    if (all && psxRecLUT) {
        free(psxRecLUT); psxRecLUT = NULL;
    }
}

static int allocMem() {
	int i;

	freeMem(0);
        
	if (psxRecLUT==NULL)
		psxRecLUT = (u32*) malloc(0x010000 * 4);

	recMem = (char*) malloc(RECMEM_SIZE);
        //recMem = mmap(NULL, RECMEM_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1,  0);
	recRAM = (char*) malloc(0x200000);
	recROM = (char*) malloc(0x080000);
	if (recRAM == NULL || recROM == NULL || recMem == NULL/*(void *)-1*/ || psxRecLUT == NULL) {
                freeMem(1);
		SysMessage("Error allocating memory"); return -1;
	}

	for (i=0; i<0x80; i++) psxRecLUT[i + 0x0000] = (u32)&recRAM[(i & 0x1f) << 16];
	memcpy(psxRecLUT + 0x8000, psxRecLUT, 0x80 * 4);
	memcpy(psxRecLUT + 0xa000, psxRecLUT, 0x80 * 4);

	for (i=0; i<0x08; i++) psxRecLUT[i + 0xbfc0] = (u32)&recROM[i << 16];
	
	return 0;
}

static int recInit() {
	return allocMem();
}

static void recReset() {
	memset(recRAM, 0, 0x200000);
	memset(recROM, 0, 0x080000);

	ppcInit();
	ppcSetPtr((u32 *)recMem);

	branch = 0;
	memset(iRegs, 0, sizeof(iRegs));
	iRegs[0].state = ST_CONST;
	iRegs[0].k     = 0;
}

static void recShutdown() {
        freeMem(1);
	ppcShutdown();
}

static void recError() {
	SysReset();
	ClosePlugins();
	SysMessage("Unrecoverable error while running recompiler\n");
	SysRunGui();
}

__inline static void execute() {
	void (**recFunc)();
	char *p;

	p =	(char*)PC_REC(psxRegs.pc);
	/*if (p != NULL)*/ recFunc = (void (**)()) (u32)p;
	/*else { recError(); return; }*/

	if (*recFunc == 0) {
		recRecompile();
	}
	recRun(*recFunc, (u32)&psxRegs, (u32)&psxM);
}

static void recExecute() {
	for (;;) execute();
}

static void recExecuteBlock() {
	execute();
}

static void recClear(u32 Addr, u32 Size) {
	memset((void*)PC_REC(Addr), 0, Size * 4);
}

static void recNULL() {
//	SysMessage("recUNK: %8.8x\n", psxRegs.code);
}

/*********************************************************
* goes to opcodes tables...                              *
* Format:  table[something....]                          *
*********************************************************/

//REC_SYS(SPECIAL);
static void recSPECIAL() {
	recSPC[_Funct_]();
}

static void recREGIMM() {
	recREG[_Rt_]();
}

static void recCOP0() {
	recCP0[_Rs_]();
}

//REC_SYS(COP2);
static void recCOP2() {
	recCP2[_Funct_]();
}

static void recBASIC() {
	recCP2BSC[_Rs_]();
}

//end of Tables opcodes...

#pragma mark - Arithmetic with immediate operand -
/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

#if 0
/*REC_FUNC(ADDI);
REC_FUNC(ADDIU);
REC_FUNC(ANDI);
REC_FUNC(ORI);
REC_FUNC(XORI);
REC_FUNC(SLTI);
REC_FUNC(SLTIU);*/
#else
static void recADDIU()  {
// Rt = Rs + Im
	if (!_Rt_) return;

	if (IsConst(_Rs_)) {
		MapConst(_Rt_, iRegs[_Rs_].k + _Imm_);
	} else {
		if (_Imm_ == 0) {
			MapCopy(_Rt_, _Rs_);
		} else {
			ADDI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _Imm_);
		}
	}
}

static void recADDI()  {
// Rt = Rs + Im
	recADDIU();
}

//REC_FUNC(SLTI);
//REC_FUNC(SLTIU);
//CR0:	SIGN      | POSITIVE | ZERO  | SOVERFLOW | SOVERFLOW | OVERFLOW | CARRY
static void recSLTI() {
// Rt = Rs < Im (signed)
	if (!_Rt_) return;

	if (IsConst(_Rs_)) {
		MapConst(_Rt_, (s32)iRegs[_Rs_].k < _Imm_);
	} else {
		if (_Imm_ == 0) {
			SRWI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), 31);
		} else {
			int reg;
			CMPWI(GetHWReg32(_Rs_), _Imm_);
			reg = PutHWReg32(_Rt_);
			LI(reg, 1);
			BLT(1);
			LI(reg, 0);
		}
	}
}

static void recSLTIU() {
// Rt = Rs < Im (unsigned)
	if (!_Rt_) return;

	if (IsConst(_Rs_)) {
		MapConst(_Rt_, iRegs[_Rs_].k < _ImmU_);
	} else {
		int reg;
		CMPLWI(GetHWReg32(_Rs_), _Imm_);
		reg = PutHWReg32(_Rt_);
		LI(reg, 1);
		BLT(1);
		LI(reg, 0);
	}
}

static void recANDI() {
// Rt = Rs And Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k & _ImmU_);
    } else {
        ANDI_(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
    }
}

static void recORI() {
// Rt = Rs Or Im
	if (!_Rt_) return;

	if (IsConst(_Rs_)) {
		MapConst(_Rt_, iRegs[_Rs_].k | _ImmU_);
	} else {
		if (_Imm_ == 0) {
			MapCopy(_Rt_, _Rs_);
		} else {
			ORI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
		}
	}
}

static void recXORI() {
// Rt = Rs Xor Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k ^ _ImmU_);
    } else {
        XORI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
    }
}
#endif
//end of * Arithmetic with immediate operand  

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
//REC_FUNC(LUI);
//#if 0*/
static void recLUI()  {
// Rt = Imm << 16
	if (!_Rt_) return;

	MapConst(_Rt_, psxRegs.code << 16);
}
//#endif
//End of Load Higher .....

#pragma mark - Register arithmetic -
/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

#if 0
/*REC_FUNC(ADD);
REC_FUNC(ADDU);
REC_FUNC(SUB);
REC_FUNC(SUBU);
REC_FUNC(AND);
REC_FUNC(OR);
REC_FUNC(XOR);
REC_FUNC(NOR);
REC_FUNC(SLT);
REC_FUNC(SLTU);*/
#else
static void recADDU() {
// Rd = Rs + Rt 
	if (!_Rd_) return;

	if (IsConst(_Rs_) && IsConst(_Rt_)) {
		MapConst(_Rd_, iRegs[_Rs_].k + iRegs[_Rt_].k);
	} else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
		if ((s32)(s16)iRegs[_Rs_].k == (s32)iRegs[_Rs_].k) {
			ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), (s16)iRegs[_Rs_].k);
		} else if ((iRegs[_Rs_].k & 0xffff) == 0) {
			ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k>>16);
		} else {
			ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
	} else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
		if ((s32)(s16)iRegs[_Rt_].k == (s32)iRegs[_Rt_].k) {
			ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), (s16)iRegs[_Rt_].k);
		} else if ((iRegs[_Rt_].k & 0xffff) == 0) {
			ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k>>16);
		} else {
			ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
	} else {
		ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
	}
}

static void recADD() {
// Rd = Rs + Rt
	recADDU();
}

static void recSUBU() {
// Rd = Rs - Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k - iRegs[_Rt_].k);
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((s32)(s16)(-iRegs[_Rt_].k) == (s32)(-iRegs[_Rt_].k)) {
            ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), -iRegs[_Rt_].k);
        } else if (((-iRegs[_Rt_].k) & 0xffff) == 0) {
            ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rs_), (-iRegs[_Rt_].k)>>16);
        } else {
            SUB(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        SUB(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}   

static void recSUB() {
// Rd = Rs - Rt
	recSUBU();
}

static void recAND() {
// Rd = Rs And Rt
    if (!_Rd_) return;
    
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k & iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        // TODO: implement shifted (ANDIS) versions of these
        if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
            ANDI_(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
        } else {
            AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
            ANDI_(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
        } else {
            AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}   

static void recOR() {
// Rd = Rs Or Rt
    if (!_Rd_) return;
    
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k | iRegs[_Rt_].k);
    }
    else {
        if (_Rs_ == _Rt_) {
            MapCopy(_Rd_, _Rs_);
        }
        else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
            if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
                ORI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else {
                OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        }
        else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
            if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
                ORI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else {
                OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else {
            OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    }
}

static void recXOR() {
// Rd = Rs Xor Rt
    if (!_Rd_) return;
    
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k ^ iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
            XORI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
        } else {
            XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
            XORI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
        } else {
            XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recNOR() {
// Rd = Rs Nor Rt
    if (!_Rd_) return;
    
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, ~(iRegs[_Rs_].k | iRegs[_Rt_].k));
    } /*else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
            NORI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
        } else {
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
            NORI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
        } else {
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } */else {
        NOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recSLT() {
// Rd = Rs < Rt (signed)
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, (s32)iRegs[_Rs_].k < (s32)iRegs[_Rt_].k);
    } else { // TODO: add immidiate cases
        int reg;
        CMPW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        reg = PutHWReg32(_Rd_);
        LI(reg, 1);
        BLT(1);
        LI(reg, 0);
    }
}

static void recSLTU() { 
// Rd = Rs < Rt (unsigned)
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k < iRegs[_Rt_].k);
    } else { // TODO: add immidiate cases
        SUBFC(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
        SUBFE(PutHWReg32(_Rd_), GetHWReg32(_Rd_), GetHWReg32(_Rd_));
        NEG(PutHWReg32(_Rd_), GetHWReg32(_Rd_));
    }
}
#endif
//End of * Register arithmetic

#pragma mark - mult/div & Register trap logic -
/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/

#if 0
REC_FUNC(MULT);
REC_FUNC(MULTU);
REC_FUNC(DIV);
REC_FUNC(DIVU);
#else

int DoShift(u32 k)
{
	u32 i;
	for (i=0; i<30; i++) {
		if (k == (1ul << i))
			return i;
	}
	return -1;
}

//REC_FUNC(MULT);

// FIXME: doesn't work in GT - wrong way marker
static void recMULT() {
// Lo/Hi = Rs * Rt (signed)
	s32 k; int r;
	int usehi, uselo;
	
	if ((IsConst(_Rs_) && iRegs[_Rs_].k == 0) ||
		(IsConst(_Rt_) && iRegs[_Rt_].k == 0)) {
		MapConst(REG_LO, 0);
		MapConst(REG_HI, 0);
		return;
	}
	
	if (IsConst(_Rs_) && IsConst(_Rt_)) {
		u64 res = (s64)((s64)(s32)iRegs[_Rs_].k * (s64)(s32)iRegs[_Rt_].k);
		MapConst(REG_LO, (res & 0xffffffff));
		MapConst(REG_HI, ((res >> 32) & 0xffffffff));
		return;
	}
	
	if (IsConst(_Rs_)) {
		k = (s32)iRegs[_Rs_].k;
		r = _Rt_;
	} else if (IsConst(_Rt_)) {
		k = (s32)iRegs[_Rt_].k;
		r = _Rs_;
	} else {
		r = -1;
		k = 0;
	}
	
	// FIXME: this should not be needed!!!
//	uselo = isPsxRegUsed(pc, REG_LO);
//	usehi = isPsxRegUsed(pc, REG_HI);
	uselo = 1; //isPsxRegUsed(pc, REG_LO);
	usehi = 1; //isPsxRegUsed(pc, REG_HI);


	if (r != -1) {
		int shift = DoShift(k);
		if (shift != -1) {
			if (uselo) {
				SLWI(PutHWReg32(REG_LO), GetHWReg32(r), shift)
			}
			if (usehi) {
				SRAWI(PutHWReg32(REG_HI), GetHWReg32(r), 31-shift);
			}
		} else {
			//if ((s32)(s16)k == k) {
			//	MULLWI(PutHWReg32(REG_LO), GetHWReg32(r), k);
			//	MULHWI(PutHWReg32(REG_HI), GetHWReg32(r), k);
			//} else
			{
				if (uselo) {
					MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
				}
				if (usehi) {
					MULHW(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
				}
			}
		}
	} else {
		if (uselo) {
			MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
		if (usehi) {
			MULHW(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
	}
}

static void recMULTU() {
// Lo/Hi = Rs * Rt (unsigned)
	u32 k; int r;
	int usehi, uselo;
	
	if ((IsConst(_Rs_) && iRegs[_Rs_].k == 0) ||
		(IsConst(_Rt_) && iRegs[_Rt_].k == 0)) {
		MapConst(REG_LO, 0);
		MapConst(REG_HI, 0);
		return;
	}
	
	if (IsConst(_Rs_) && IsConst(_Rt_)) {
		u64 res = (u64)((u64)(u32)iRegs[_Rs_].k * (u64)(u32)iRegs[_Rt_].k);
		MapConst(REG_LO, (res & 0xffffffff));
		MapConst(REG_HI, ((res >> 32) & 0xffffffff));
		return;
	}
	
	if (IsConst(_Rs_)) {
		k = (s32)iRegs[_Rs_].k;
		r = _Rt_;
	} else if (IsConst(_Rt_)) {
		k = (s32)iRegs[_Rt_].k;
		r = _Rs_;
	} else {
		r = -1;
		k = 0;
	}
	
	uselo = isPsxRegUsed(pc, REG_LO);
	usehi = isPsxRegUsed(pc, REG_HI);

	if (r != -1) {
		int shift = DoShift(k);
		if (shift != -1) {
			if (uselo) {
				SLWI(PutHWReg32(REG_LO), GetHWReg32(r), shift);
			}
			if (usehi) {
				SRWI(PutHWReg32(REG_HI), GetHWReg32(r), 31-shift);
			}
		} else {
			{
				if (uselo) {
					MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
				}
				if (usehi) {
					MULHWU(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
				}
			}
		}
	} else {
		if (uselo) {
			MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
		if (usehi) {
			MULHWU(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
	}
}

static void recDIV() {
// Lo/Hi = Rs / Rt (signed)
	int usehi;

	if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
		MapConst(REG_LO, 0);
		MapConst(REG_HI, 0);
		return;
	}
	if (IsConst(_Rt_) && IsConst(_Rs_)) {
		MapConst(REG_LO, (s32)iRegs[_Rs_].k / (s32)iRegs[_Rt_].k);
		MapConst(REG_HI, (s32)iRegs[_Rs_].k % (s32)iRegs[_Rt_].k);
		return;
	}
	
	usehi = isPsxRegUsed(pc, REG_HI);
	
	if (IsConst(_Rt_)) {
		int shift = DoShift(iRegs[_Rt_].k);
		if (shift != -1) {
			SRAWI(PutHWReg32(REG_LO), GetHWReg32(_Rs_), shift);
			ADDZE(PutHWReg32(REG_LO), GetHWReg32(REG_LO));
			if (usehi) {
				RLWINM(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 0, 31-shift, 31);
			}
		} else if (iRegs[_Rt_].k == 3) {
			// http://the.wall.riscom.net/books/proc/ppc/cwg/code2.html
			LIS(PutHWReg32(REG_HI), 0x5555);
			ADDI(PutHWReg32(REG_HI), GetHWReg32(REG_HI), 0x5556);
			MULHW(PutHWReg32(REG_LO), GetHWReg32(REG_HI), GetHWReg32(_Rs_));
			SRWI(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 31);
			ADD(PutHWReg32(REG_LO), GetHWReg32(REG_LO), GetHWReg32(REG_HI));
			if (usehi) {
				MULLI(PutHWReg32(REG_HI), GetHWReg32(REG_LO), 3);
				SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
			}
		} else {
			DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
			if (usehi) {
				if ((iRegs[_Rt_].k & 0x7fff) == iRegs[_Rt_].k) {
					MULLI(PutHWReg32(REG_HI), GetHWReg32(REG_LO), iRegs[_Rt_].k);
				} else {
					MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
				}
				SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
			}
		}
	} else {
		DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		if (usehi) {
			MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
			SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
		}
	}
}

static void recDIVU() {
// Lo/Hi = Rs / Rt (unsigned)
	int usehi;

	if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
		MapConst(REG_LO, 0);
		MapConst(REG_HI, 0);
		return;
	}
	if (IsConst(_Rt_) && IsConst(_Rs_)) {
		MapConst(REG_LO, (u32)iRegs[_Rs_].k / (u32)iRegs[_Rt_].k);
		MapConst(REG_HI, (u32)iRegs[_Rs_].k % (u32)iRegs[_Rt_].k);
		return;
	}
	
	usehi = isPsxRegUsed(pc, REG_HI);

	if (IsConst(_Rt_)) {
		int shift = DoShift(iRegs[_Rt_].k);
		if (shift != -1) {
			SRWI(PutHWReg32(REG_LO), GetHWReg32(_Rs_), shift);
			if (usehi) {
				RLWINM(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 0, 31-shift, 31);
			}
		} else {
			DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
			if (usehi) {
				MULLW(PutHWReg32(REG_HI), GetHWReg32(_Rt_), GetHWReg32(REG_LO));
				SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
			}
		}
	} else {
		DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		if (usehi) {
			MULLW(PutHWReg32(REG_HI), GetHWReg32(_Rt_), GetHWReg32(REG_LO));
			SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
		}
	}
}
#endif
//End of * Register mult/div & Register trap logic  

#pragma mark - memory access -

#if 0
REC_FUNC(LB);
REC_FUNC(LBU);
REC_FUNC(LH);
REC_FUNC(LHU);
REC_FUNC(LW);

REC_FUNC(SB);
REC_FUNC(SH);
REC_FUNC(SW);

REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(SWL);
REC_FUNC(SWR);
#else
static void preMemRead()
{
	int rs;
	
	ReserveArgs(1);
	if (_Rs_ != _Rt_) {
		DisposeHWReg(iRegs[_Rt_].reg);
	}
	rs = GetHWReg32(_Rs_);
	if (rs != 3 || _Imm_ != 0) {
		ADDI(PutHWRegSpecial(ARG1), rs, _Imm_);
	}
	if (_Rs_ == _Rt_) {
		DisposeHWReg(iRegs[_Rt_].reg);
	}
	InvalidateCPURegs();
	//FlushAllHWReg();
}

static void preMemWrite(int size)
{
	int rs;
	
	ReserveArgs(2);
	rs = GetHWReg32(_Rs_);
	if (rs != 3 || _Imm_ != 0) {
		ADDI(PutHWRegSpecial(ARG1), rs, _Imm_);
	}
	if (size == 1) {
		RLWINM(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_), 0, 24, 31);
		//ANDI_(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_), 0xff);
	} else if (size == 2) {
		RLWINM(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_), 0, 16, 31);
		//ANDI_(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_), 0xffff);
	} else {
		MR(PutHWRegSpecial(ARG2), GetHWReg32(_Rt_));
	}
	
	InvalidateCPURegs();
	//FlushAllHWReg();
}

static void recLB() {
// Rt = mem[Rs + Im] (signed)
	
    /*if (IsConst(_Rs_)) {
        u32 addr = iRegs[_Rs_].k + _Imm_;
        int t = addr >> 16;
    
        if ((t & 0xfff0)  == 0xbfc0) {
            if (!_Rt_) return;
            // since bios is readonly it won't change
            MapConst(_Rt_, psxRs8(addr));
            return;
        }
        if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
            if (!_Rt_) return;
                
            addr = (u32)&psxM[addr & 0x1fffff];
            LIW(PutHWReg32(_Rt_), ((addr>>16)<<16)+(addr&0x8000<<1)); // FIXME: is this correct?
            LBZ(PutHWReg32(_Rt_), addr&0xffff, GetHWReg32(_Rt_));
            EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80 && addr < 0x1f801000) {
            if (!_Rt_) return;
    
            addr = (u32)&psxH[addr & 0xfff];
            LIW(PutHWReg32(_Rt_), ((addr>>16)<<16)+(addr&0x8000<<1)); // FIXME: is this correct?
            LBZ(PutHWReg32(_Rt_), addr&0xffff, GetHWReg32(_Rt_));
            EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
            return;
        }
    //	SysPrintf("unhandled r8 %x\n", addr);
    }*/
	
	preMemRead();
	CALLFunc((u32)psxMemRead8);
	if (_Rt_) {
		EXTSB(PutHWReg32(_Rt_), GetHWRegSpecial(RETVAL));
		DisposeHWReg(GetSpecialIndexFromHWRegs(RETVAL));
	}
}

static void recLBU() {
// Rt = mem[Rs + Im] (unsigned)

    /*if (IsConst(_Rs_)) {
        u32 addr = iRegs[_Rs_].k + _Imm_;
        int t = addr >> 16;
    
        if ((t & 0xfff0)  == 0xbfc0) {
            if (!_Rt_) return;
            // since bios is readonly it won't change
            MapConst(_Rt_, psxRu8(addr));
            return;
        }
        if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
            if (!_Rt_) return;
                
            addr = (u32)&psxM[addr & 0x1fffff];
            LIW(PutHWReg32(_Rt_), ((addr>>16)<<16)+(addr&0x8000<<1)); // FIXME: is this correct?
            LBZ(PutHWReg32(_Rt_), addr&0xffff, GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80 && addr < 0x1f801000) {
            if (!_Rt_) return;
    
            addr = (u32)&psxH[addr & 0xfff];
            LIW(PutHWReg32(_Rt_), ((addr>>16)<<16)+(addr&0x8000<<1)); // FIXME: is this correct?
            LBZ(PutHWReg32(_Rt_), addr&0xffff, GetHWReg32(_Rt_));
            return;
        }
    //	SysPrintf("unhandled r8 %x\n", addr);
    }*/
        
	preMemRead();
	CALLFunc((u32)psxMemRead8);
	
	if (_Rt_) {
		SetDstCPUReg(3);
		PutHWReg32(_Rt_);
	}
}

static void recLH() {
// Rt = mem[Rs + Im] (signed)

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;
	
		if ((t & 0xfff0)  == 0xbfc0) {
			if (!_Rt_) return;
			// since bios is readonly it won't change
			MapConst(_Rt_, psxRs16(addr));
			return;
		}
		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxM[addr & 0x1fffff]);
			LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxH[addr & 0xfff]);
			LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
			return;
		}
	//	SysPrintf("unhandled r16 %x\n", addr);
	}
    
	preMemRead();
	CALLFunc((u32)psxMemRead16);
	if (_Rt_) {
		EXTSH(PutHWReg32(_Rt_), GetHWRegSpecial(RETVAL));
		DisposeHWReg(GetSpecialIndexFromHWRegs(RETVAL));
	}
}

static void recLHU() {
// Rt = mem[Rs + Im] (unsigned)

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;
	
		if ((t & 0xfff0) == 0xbfc0) {
			if (!_Rt_) return;
			// since bios is readonly it won't change
			MapConst(_Rt_, psxRu16(addr));
			return;
		}
		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxM[addr & 0x1fffff]);
			LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxH[addr & 0xfff]);
			LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
		if (t == 0x1f80) {
			if (addr >= 0x1f801c00 && addr < 0x1f801e00) {
					if (!_Rt_) return;
					
					ReserveArgs(1);
					LIW(PutHWRegSpecial(ARG1), addr);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32)SPU_readRegister);
					
					SetDstCPUReg(3);
					PutHWReg32(_Rt_);
					return;
			}
			switch (addr) {
					case 0x1f801100: case 0x1f801110: case 0x1f801120:
						if (!_Rt_) return;
						
						ReserveArgs(1);
						LIW(PutHWRegSpecial(ARG1), (addr >> 4) & 0x3);
						DisposeHWReg(iRegs[_Rt_].reg);
						InvalidateCPURegs();
						CALLFunc((u32)psxRcntRcount);
						
						SetDstCPUReg(3);
						PutHWReg32(_Rt_);
						return;

					case 0x1f801104: case 0x1f801114: case 0x1f801124:
						if (!_Rt_) return;
						
                        ReserveArgs(1);
                        LIW(PutHWRegSpecial(ARG1), (addr >> 4) & 0x3);
                        DisposeHWReg(iRegs[_Rt_].reg);
                        InvalidateCPURegs();
                        CALLFunc((u32)psxRcntRmode);
                        
                        SetDstCPUReg(3);
                        PutHWReg32(_Rt_);
						return;
	
					case 0x1f801108: case 0x1f801118: case 0x1f801128:
						if (!_Rt_) return;

                        ReserveArgs(1);
                        LIW(PutHWRegSpecial(ARG1), (addr >> 4) & 0x3);
                        DisposeHWReg(iRegs[_Rt_].reg);
                        InvalidateCPURegs();
                        CALLFunc((u32)psxRcntRtarget);
                        
                        SetDstCPUReg(3);
                        PutHWReg32(_Rt_);
						return;
					}
		}
	//	SysPrintf("unhandled r16u %x\n", addr);
	}
	
	preMemRead();
	CALLFunc((u32)psxMemRead16);
	if (_Rt_) {
		SetDstCPUReg(3);
		PutHWReg32(_Rt_);
	}
}

static void recLW() {
// Rt = mem[Rs + Im] (unsigned)

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0xfff0) == 0xbfc0) {
			if (!_Rt_) return;
			// since bios is readonly it won't change
			MapConst(_Rt_, psxRu32(addr));
			return;
		}
		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxM[addr & 0x1fffff]);
			LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			if (!_Rt_) return;

			LIW(PutHWReg32(_Rt_), (u32)&psxH[addr & 0xfff]);
			LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
			return;
		}
		if (t == 0x1f80) {
			switch (addr) {
				case 0x1f801080: case 0x1f801084: case 0x1f801088: 
				case 0x1f801090: case 0x1f801094: case 0x1f801098: 
				case 0x1f8010a0: case 0x1f8010a4: case 0x1f8010a8: 
				case 0x1f8010b0: case 0x1f8010b4: case 0x1f8010b8: 
				case 0x1f8010c0: case 0x1f8010c4: case 0x1f8010c8: 
				case 0x1f8010d0: case 0x1f8010d4: case 0x1f8010d8: 
				case 0x1f8010e0: case 0x1f8010e4: case 0x1f8010e8: 
				case 0x1f801070: case 0x1f801074:
				case 0x1f8010f0: case 0x1f8010f4:
					if (!_Rt_) return;
					
					LIW(PutHWReg32(_Rt_), (u32)&psxH[addr & 0xffff]);
					LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
					return;

				case 0x1f801810:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32)GPU_readData);
					
					SetDstCPUReg(3);
					PutHWReg32(_Rt_);
					return;

				case 0x1f801814:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32)GPU_readStatus);
					
					SetDstCPUReg(3);
					PutHWReg32(_Rt_);
					return;
			}
		}
//		SysPrintf("unhandled r32 %x\n", addr);
	}

	preMemRead();
	CALLFunc((u32)psxMemRead32);
	if (_Rt_) {
		SetDstCPUReg(3);
		PutHWReg32(_Rt_);
	}
}

REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(SWL);
REC_FUNC(SWR);
/*extern u32 LWL_MASK[4];
extern u32 LWL_SHIFT[4];

void iLWLk(u32 shift) {
	if (IsConst(_Rt_)) {
		MOV32ItoR(ECX, iRegs[_Rt_].k);
	} else {
		MOV32MtoR(ECX, (u32)&psxRegs.GPR.r[_Rt_]);
	}
	AND32ItoR(ECX, LWL_MASK[shift]);
	SHL32ItoR(EAX, LWL_SHIFT[shift]);
	OR32RtoR (EAX, ECX);
}

void recLWL() {
// Rt = Rt Merge mem[Rs + Im]

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			MOV32MtoR(EAX, (u32)&psxM[addr & 0x1ffffc]);
			iLWLk(addr & 3);

			iRegs[_Rt_].state = ST_UNK;
			MOV32RtoM((u32)&psxRegs.GPR.r[_Rt_], EAX);
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			MOV32MtoR(EAX, (u32)&psxH[addr & 0xffc]);
			iLWLk(addr & 3);

			iRegs[_Rt_].state = ST_UNK;
			MOV32RtoM((u32)&psxRegs.GPR.r[_Rt_], EAX);
			return;
		}
	}

	if (IsConst(_Rs_)) MOV32ItoR(EAX, iRegs[_Rs_].k + _Imm_);
	else {
		MOV32MtoR(EAX, (u32)&psxRegs.GPR.r[_Rs_]);
		if (_Imm_) ADD32ItoR(EAX, _Imm_);
	}
	PUSH32R  (EAX);
	AND32ItoR(EAX, ~3);
	PUSH32R  (EAX);
	CALLFunc((u32)psxMemRead32);

	if (_Rt_) {
		ADD32ItoR(ESP, 4);
		POP32R   (EDX);
		AND32ItoR(EDX, 0x3); // shift = addr & 3;

		MOV32ItoR(ECX, (u32)LWL_SHIFT);
		MOV32RmStoR(ECX, ECX, EDX, 2);
		SHL32CLtoR(EAX); // mem(EAX) << LWL_SHIFT[shift]

		MOV32ItoR(ECX, (u32)LWL_MASK);
		MOV32RmStoR(ECX, ECX, EDX, 2);
		if (IsConst(_Rt_)) {
			MOV32ItoR(EDX, iRegs[_Rt_].k);
		} else {
			MOV32MtoR(EDX, (u32)&psxRegs.GPR.r[_Rt_]);
		}
		AND32RtoR(EDX, ECX); // _rRt_ & LWL_MASK[shift]

		OR32RtoR(EAX, EDX);

		iRegs[_Rt_].state = ST_UNK;
		MOV32RtoM((u32)&psxRegs.GPR.r[_Rt_], EAX);
	} else {
//		ADD32ItoR(ESP, 8);
		resp+= 8;
	}
}

static void recLWBlock(int count) {
	u32 *code = PSXM(pc);
	int i, respsave;
// Rt = mem[Rs + Im] (unsigned)

//	iFlushRegs(0);

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0xfff0) == 0xbfc0) {
			// since bios is readonly it won't change
			for (i=0; i<count; i++, code++, addr+=4) {
				if (_fRt_(*code)) {
					MapConst(_fRt_(*code), psxRu32(addr));
				}
			}
			return;
		}
		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			for (i=0; i<count; i++, code++, addr+=4) {
				if (!_fRt_(*code)) return;
				iRegs[_fRt_(*code)].state = ST_UNK;

				MOV32MtoR(EAX, (u32)&psxM[addr & 0x1fffff]);
				MOV32RtoM((u32)&psxRegs.GPR.r[_fRt_(*code)], EAX);
			}
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			for (i=0; i<count; i++, code++, addr+=4) {
				if (!_fRt_(*code)) return;
				iRegs[_fRt_(*code)].state = ST_UNK;

				MOV32MtoR(EAX, (u32)&psxH[addr & 0xfff]);
				MOV32RtoM((u32)&psxRegs.GPR.r[_fRt_(*code)], EAX);
			}
			return;
		}
	}

	SysPrintf("recLWBlock %d: %d\n", count, IsConst(_Rs_));
	iPushOfB();
	CALLFunc((u32)psxMemPointer);
//	ADD32ItoR(ESP, 4);
	resp+= 4;

	respsave = resp; resp = 0;
	TEST32RtoR(EAX, EAX);
	j32Ptr[4] = JZ32(0);
	XOR32RtoR(ECX, ECX);
	for (i=0; i<count; i++, code++) {
		if (_fRt_(*code)) {
			iRegs[_fRt_(*code)].state = ST_UNK;
			
			MOV32RmStoR(EDX, EAX, ECX, 2);
			MOV32RtoM((u32)&psxRegs.GPR.r[_fRt_(*code)], EDX);
		}
		if (i != (count-1)) INC32R(ECX);
	}
	j32Ptr[5] = JMP32(0);
	x86SetJ32(j32Ptr[4]);
	for (i=0, code = PSXM(pc); i<count; i++, code++) {
		psxRegs.code = *code;
		recLW();
	}
	ADD32ItoR(ESP, resp);
	x86SetJ32(j32Ptr[5]);
	resp = respsave;
}

extern u32 LWR_MASK[4];
extern u32 LWR_SHIFT[4];

void iLWRk(u32 shift) {
	if (IsConst(_Rt_)) {
		MOV32ItoR(ECX, iRegs[_Rt_].k);
	} else {
		MOV32MtoR(ECX, (u32)&psxRegs.GPR.r[_Rt_]);
	}
	AND32ItoR(ECX, LWR_MASK[shift]);
	SHR32ItoR(EAX, LWR_SHIFT[shift]);
	OR32RtoR (EAX, ECX);
}

void recLWR() {
// Rt = Rt Merge mem[Rs + Im]

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			MOV32MtoR(EAX, (u32)&psxM[addr & 0x1ffffc]);
			iLWRk(addr & 3);

			iRegs[_Rt_].state = ST_UNK;
			MOV32RtoM((u32)&psxRegs.GPR.r[_Rt_], EAX);
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			MOV32MtoR(EAX, (u32)&psxH[addr & 0xffc]);
			iLWRk(addr & 3);

			iRegs[_Rt_].state = ST_UNK;
			MOV32RtoM((u32)&psxRegs.GPR.r[_Rt_], EAX);
			return;
		}
	}

	if (IsConst(_Rs_)) MOV32ItoR(EAX, iRegs[_Rs_].k + _Imm_);
	else {
		MOV32MtoR(EAX, (u32)&psxRegs.GPR.r[_Rs_]);
		if (_Imm_) ADD32ItoR(EAX, _Imm_);
	}
	PUSH32R  (EAX);
	AND32ItoR(EAX, ~3);
	PUSH32R  (EAX);
	CALLFunc((u32)psxMemRead32);

	if (_Rt_) {
		ADD32ItoR(ESP, 4);
		POP32R   (EDX);
		AND32ItoR(EDX, 0x3); // shift = addr & 3;

		MOV32ItoR(ECX, (u32)LWR_SHIFT);
		MOV32RmStoR(ECX, ECX, EDX, 2);
		SHR32CLtoR(EAX); // mem(EAX) >> LWR_SHIFT[shift]

		MOV32ItoR(ECX, (u32)LWR_MASK);
		MOV32RmStoR(ECX, ECX, EDX, 2);

		if (IsConst(_Rt_)) {
			MOV32ItoR(EDX, iRegs[_Rt_].k);
		} else {
			MOV32MtoR(EDX, (u32)&psxRegs.GPR.r[_Rt_]);
		}
		AND32RtoR(EDX, ECX); // _rRt_ & LWR_MASK[shift]

		OR32RtoR(EAX, EDX);

		iRegs[_Rt_].state = ST_UNK;
		MOV32RtoM((u32)&psxRegs.GPR.r[_Rt_], EAX);
	} else {
//		ADD32ItoR(ESP, 8);
		resp+= 8;
	}
}*/

static void recSB() {
// mem[Rs + Im] = Rt

	/*if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			if (IsConst(_Rt_)) {
				MOV8ItoM((u32)&psxM[addr & 0x1fffff], (u8)iRegs[_Rt_].k);
			} else {
				MOV8MtoR(EAX, (u32)&psxRegs.GPR.r[_Rt_]);
				MOV8RtoM((u32)&psxM[addr & 0x1fffff], EAX);
			}
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			if (IsConst(_Rt_)) {
				MOV8ItoM((u32)&psxH[addr & 0xfff], (u8)iRegs[_Rt_].k);
			} else {
				MOV8MtoR(EAX, (u32)&psxRegs.GPR.r[_Rt_]);
				MOV8RtoM((u32)&psxH[addr & 0xfff], EAX);
			}
			return;
		}
//		SysPrintf("unhandled w8 %x\n", addr);
	}*/

	preMemWrite(1);
	CALLFunc((u32)psxMemWrite8);
}

static void recSH() {
// mem[Rs + Im] = Rt

	/*if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			if (IsConst(_Rt_)) {
				MOV16ItoM((u32)&psxM[addr & 0x1fffff], (u16)iRegs[_Rt_].k);
			} else {
				MOV16MtoR(EAX, (u32)&psxRegs.GPR.r[_Rt_]);
				MOV16RtoM((u32)&psxM[addr & 0x1fffff], EAX);
			}
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			if (IsConst(_Rt_)) {
				MOV16ItoM((u32)&psxH[addr & 0xfff], (u16)iRegs[_Rt_].k);
			} else {
				MOV16MtoR(EAX, (u32)&psxRegs.GPR.r[_Rt_]);
				MOV16RtoM((u32)&psxH[addr & 0xfff], EAX);
			}
			return;
		}
		if (t == 0x1f80) {
			if (addr >= 0x1f801c00 && addr < 0x1f801e00) {
				if (IsConst(_Rt_)) {
					PUSH32I(iRegs[_Rt_].k);
				} else {
					PUSH32M((u32)&psxRegs.GPR.r[_Rt_]);
				}
				PUSH32I  (addr);
				CALL32M  ((u32)&SPU_writeRegister);
#ifndef __WIN32__
				resp+= 8;
#endif
				return;
			}
		}
//		SysPrintf("unhandled w16 %x\n", addr);
	}*/

	preMemWrite(2);
	CALLFunc((u32)psxMemWrite16);
}

static void recSW() {
// mem[Rs + Im] = Rt
	u32 *b1, *b2;
#if 0
	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			LIW(0, addr & 0x1fffff);
			STWBRX(GetHWReg32(_Rt_), GetHWRegSpecial(PSXMEM), 0);
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			LIW(0, (u32)&psxH[addr & 0xfff]);
			STWBRX(GetHWReg32(_Rt_), 0, 0);
			return;
		}
		if (t == 0x1f80) {
			switch (addr) {
				case 0x1f801080: case 0x1f801084: 
				case 0x1f801090: case 0x1f801094: 
				case 0x1f8010a0: case 0x1f8010a4: 
				case 0x1f8010b0: case 0x1f8010b4: 
				case 0x1f8010c0: case 0x1f8010c4: 
				case 0x1f8010d0: case 0x1f8010d4: 
				case 0x1f8010e0: case 0x1f8010e4: 
				case 0x1f801074:
				case 0x1f8010f0:
					LIW(0, (u32)&psxH[addr & 0xffff]);
					STWBRX(GetHWReg32(_Rt_), 0, 0);
					return;

/*				case 0x1f801810:
					if (IsConst(_Rt_)) {
						PUSH32I(iRegs[_Rt_].k);
					} else {
						PUSH32M((u32)&psxRegs.GPR.r[_Rt_]);
					}
					CALL32M((u32)&GPU_writeData);
#ifndef __WIN32__
					resp+= 4;
#endif
					return;

				case 0x1f801814:
					if (IsConst(_Rt_)) {
						PUSH32I(iRegs[_Rt_].k);
					} else {
						PUSH32M((u32)&psxRegs.GPR.r[_Rt_]);
					}
					CALL32M((u32)&GPU_writeStatus);
#ifndef __WIN32__
					resp+= 4;
#endif*/
			}
		}
//		SysPrintf("unhandled w32 %x\n", addr);
	}
	
/*	LIS(0, 0x0079 + ((_Imm_ <= 0) ? 1 : 0));
	CMPLW(GetHWReg32(_Rs_), 0);
	BGE_L(b1);
	
	//SaveContext();
	ADDI(0, GetHWReg32(_Rs_), _Imm_);
	RLWINM(0, GetHWReg32(_Rs_), 0, 11, 31);
	STWBRX(GetHWReg32(_Rt_), GetHWRegSpecial(PSXMEM), 0);
	B_L(b2);
	
	B_DST(b1);*/
#endif
	preMemWrite(4);
	CALLFunc((u32)psxMemWrite32);
	
	//B_DST(b2);
}

/*
static void recSWBlock(int count) {
	u32 *code;
	int i, respsave;
// mem[Rs + Im] = Rt

//	iFlushRegs();

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;
		code = PSXM(pc);

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			for (i=0; i<count; i++, code++, addr+=4) {
				if (IsConst(_fRt_(*code))) {
					MOV32ItoM((u32)&psxM[addr & 0x1fffff], iRegs[_fRt_(*code)].k);
				} else {
					MOV32MtoR(EAX, (u32)&psxRegs.GPR.r[_fRt_(*code)]);
					MOV32RtoM((u32)&psxM[addr & 0x1fffff], EAX);
				}
			}
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			for (i=0; i<count; i++, code++, addr+=4) {
				if (!_fRt_(*code)) return;
				iRegs[_fRt_(*code)].state = ST_UNK;

				MOV32MtoR(EAX, (u32)&psxH[addr & 0xfff]);
				MOV32RtoM((u32)&psxRegs.GPR.r[_fRt_(*code)], EAX);
			}
			return;
		}
	}

	SysPrintf("recSWBlock %d: %d\n", count, IsConst(_Rs_));
	iPushOfB();
	CALLFunc((u32)psxMemPointer);
//	ADD32ItoR(ESP, 4);
	resp+= 4;

	respsave = resp; resp = 0;
	TEST32RtoR(EAX, EAX);
	j32Ptr[4] = JZ32(0);
	XOR32RtoR(ECX, ECX);
	for (i=0, code = PSXM(pc); i<count; i++, code++) {
		if (IsConst(_fRt_(*code))) {
			MOV32ItoR(EDX, iRegs[_fRt_(*code)].k);
		} else {
			MOV32MtoR(EDX, (u32)&psxRegs.GPR.r[_fRt_(*code)]);
		}
		MOV32RtoRmS(EAX, ECX, 2, EDX);
		if (i != (count-1)) INC32R(ECX);
	}
	j32Ptr[5] = JMP32(0);
	x86SetJ32(j32Ptr[4]);
	for (i=0, code = PSXM(pc); i<count; i++, code++) {
		psxRegs.code = *code;
		recSW();
	}
	ADD32ItoR(ESP, resp);
	x86SetJ32(j32Ptr[5]);
	resp = respsave;
}

extern u32 SWL_MASK[4];
extern u32 SWL_SHIFT[4];

void iSWLk(u32 shift) {
	if (IsConst(_Rt_)) {
		MOV32ItoR(ECX, iRegs[_Rt_].k);
	} else {
		MOV32MtoR(ECX, (u32)&psxRegs.GPR.r[_Rt_]);
	}
	SHR32ItoR(ECX, SWL_SHIFT[shift]);
	AND32ItoR(EAX, SWL_MASK[shift]);
	OR32RtoR (EAX, ECX);
}

void recSWL() {
// mem[Rs + Im] = Rt Merge mem[Rs + Im]

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			MOV32MtoR(EAX, (u32)&psxM[addr & 0x1ffffc]);
			iSWLk(addr & 3);
			MOV32RtoM((u32)&psxM[addr & 0x1ffffc], EAX);
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			MOV32MtoR(EAX, (u32)&psxH[addr & 0xffc]);
			iSWLk(addr & 3);
			MOV32RtoM((u32)&psxH[addr & 0xffc], EAX);
			return;
		}
	}

	if (IsConst(_Rs_)) MOV32ItoR(EAX, iRegs[_Rs_].k + _Imm_);
	else {
		MOV32MtoR(EAX, (u32)&psxRegs.GPR.r[_Rs_]);
		if (_Imm_) ADD32ItoR(EAX, _Imm_);
	}
	PUSH32R  (EAX);
	AND32ItoR(EAX, ~3);
	PUSH32R  (EAX);

	CALLFunc((u32)psxMemRead32);

	ADD32ItoR(ESP, 4);
	POP32R   (EDX);
	AND32ItoR(EDX, 0x3); // shift = addr & 3;

	MOV32ItoR(ECX, (u32)SWL_MASK);
	MOV32RmStoR(ECX, ECX, EDX, 2);
	AND32RtoR(EAX, ECX); // mem & SWL_MASK[shift]

	MOV32ItoR(ECX, (u32)SWL_SHIFT);
	MOV32RmStoR(ECX, ECX, EDX, 2);
	if (IsConst(_Rt_)) {
		MOV32ItoR(EDX, iRegs[_Rt_].k);
	} else {
		MOV32MtoR(EDX, (u32)&psxRegs.GPR.r[_Rt_]);
	}
	SHR32CLtoR(EDX); // _rRt_ >> SWL_SHIFT[shift]

	OR32RtoR (EAX, EDX);
	PUSH32R  (EAX);

	if (IsConst(_Rs_)) MOV32ItoR(EAX, iRegs[_Rs_].k + _Imm_);
	else {
		MOV32MtoR(EAX, (u32)&psxRegs.GPR.r[_Rs_]);
		if (_Imm_) ADD32ItoR(EAX, _Imm_);
	}
	AND32ItoR(EAX, ~3);
	PUSH32R  (EAX);

	CALLFunc((u32)psxMemWrite32);
//	ADD32ItoR(ESP, 8);
	resp+= 8;
}

extern u32 SWR_MASK[4];
extern u32 SWR_SHIFT[4];

void iSWRk(u32 shift) {
	if (IsConst(_Rt_)) {
		MOV32ItoR(ECX, iRegs[_Rt_].k);
	} else {
		MOV32MtoR(ECX, (u32)&psxRegs.GPR.r[_Rt_]);
	}
	SHL32ItoR(ECX, SWR_SHIFT[shift]);
	AND32ItoR(EAX, SWR_MASK[shift]);
	OR32RtoR (EAX, ECX);
}

void recSWR() {
// mem[Rs + Im] = Rt Merge mem[Rs + Im]

	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].k + _Imm_;
		int t = addr >> 16;

		if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
			MOV32MtoR(EAX, (u32)&psxM[addr & 0x1ffffc]);
			iSWRk(addr & 3);
			MOV32RtoM((u32)&psxM[addr & 0x1ffffc], EAX);
			return;
		}
		if (t == 0x1f80 && addr < 0x1f801000) {
			MOV32MtoR(EAX, (u32)&psxH[addr & 0xffc]);
			iSWRk(addr & 3);
			MOV32RtoM((u32)&psxH[addr & 0xffc], EAX);
			return;
		}
	}

	if (IsConst(_Rs_)) MOV32ItoR(EAX, iRegs[_Rs_].k + _Imm_);
	else {
		MOV32MtoR(EAX, (u32)&psxRegs.GPR.r[_Rs_]);
		if (_Imm_) ADD32ItoR(EAX, _Imm_);
	}
	PUSH32R  (EAX);
	AND32ItoR(EAX, ~3);
	PUSH32R  (EAX);

	CALLFunc((u32)psxMemRead32);

	ADD32ItoR(ESP, 4);
	POP32R   (EDX);
	AND32ItoR(EDX, 0x3); // shift = addr & 3;

	MOV32ItoR(ECX, (u32)SWR_MASK);
	MOV32RmStoR(ECX, ECX, EDX, 2);
	AND32RtoR(EAX, ECX); // mem & SWR_MASK[shift]

	MOV32ItoR(ECX, (u32)SWR_SHIFT);
	MOV32RmStoR(ECX, ECX, EDX, 2);
	if (IsConst(_Rt_)) {
		MOV32ItoR(EDX, iRegs[_Rt_].k);
	} else {
		MOV32MtoR(EDX, (u32)&psxRegs.GPR.r[_Rt_]);
	}
	SHL32CLtoR(EDX); // _rRt_ << SWR_SHIFT[shift]

	OR32RtoR (EAX, EDX);
	PUSH32R  (EAX);

	if (IsConst(_Rs_)) MOV32ItoR(EAX, iRegs[_Rs_].k + _Imm_);
	else {
		MOV32MtoR(EAX, (u32)&psxRegs.GPR.r[_Rs_]);
		if (_Imm_) ADD32ItoR(EAX, _Imm_);
	}
	AND32ItoR(EAX, ~3);
	PUSH32R  (EAX);

	CALLFunc((u32)psxMemWrite32);
//	ADD32ItoR(ESP, 8);
	resp+= 8;
}*/
#endif

#if 0
/*REC_FUNC(SLL);
REC_FUNC(SRL);
REC_FUNC(SRA);*/
#else
static void recSLL() {
// Rd = Rt << Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rt_].k << _Sa_);
    } else {
        SLWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

static void recSRL() {
// Rd = Rt >> Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rt_].k >> _Sa_);
    } else {
        SRWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

static void recSRA() {
// Rd = Rt >> Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, (s32)iRegs[_Rt_].k >> _Sa_);
    } else {
        SRAWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}
#endif

#pragma mark - shift ops -
#if 0
/*REC_FUNC(SLLV);
REC_FUNC(SRLV);
REC_FUNC(SRAV);*/
#else
static void recSLLV() {
// Rd = Rt << Rs
	if (!_Rd_) return;

	if (IsConst(_Rt_) && IsConst(_Rs_)) {
		MapConst(_Rd_, iRegs[_Rt_].k << iRegs[_Rs_].k);
	} else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
		SLWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
	} else {
		SLW(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
	}
}

static void recSRLV() {
// Rd = Rt >> Rs
	if (!_Rd_) return;

	if (IsConst(_Rt_) && IsConst(_Rs_)) {
		MapConst(_Rd_, iRegs[_Rt_].k >> iRegs[_Rs_].k);
	} else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
		SRWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
	} else {
		SRW(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
	}
}

static void recSRAV() {
// Rd = Rt >> Rs
	if (!_Rd_) return;

	if (IsConst(_Rt_) && IsConst(_Rs_)) {
		MapConst(_Rd_, (s32)iRegs[_Rt_].k >> iRegs[_Rs_].k);
	} else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
		SRAWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
	} else {
		SRAW(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
	}
}
#endif

//REC_SYS(SYSCALL);
//REC_SYS(BREAK);

//#if 0*/
/*int dump;*/
static void recSYSCALL() {
//	dump=1;
	iFlushRegs(0);
	
	ReserveArgs(2);
	LIW(PutHWRegSpecial(PSXPC), pc - 4);
	LIW(PutHWRegSpecial(ARG1), 0x20);
	LIW(PutHWRegSpecial(ARG2), (branch == 1 ? 1 : 0));
	FlushAllHWReg();
	CALLFunc ((u32)psxException);

	branch = 2;
	iRet();
}

static void recBREAK() {
}
//#endif

#if 0
/*REC_FUNC(MFHI);
REC_FUNC(MTHI);
REC_FUNC(MFLO);
REC_FUNC(MTLO);*/
#else
static void recMFHI() {
// Rd = Hi
	if (!_Rd_) return;
	
	if (IsConst(REG_HI)) {
		MapConst(_Rd_, iRegs[REG_HI].k);
	} else {
		MapCopy(_Rd_, REG_HI);
	}
}

static void recMTHI() {
// Hi = Rs

	if (IsConst(_Rs_)) {
		MapConst(REG_HI, iRegs[_Rs_].k);
	} else {
		MapCopy(REG_HI, _Rs_);
	}
}

static void recMFLO() {
// Rd = Lo
	if (!_Rd_) return;

	if (IsConst(REG_LO)) {
		MapConst(_Rd_, iRegs[REG_LO].k);
	} else {
		MapCopy(_Rd_, REG_LO);
	}
}

static void recMTLO() {
// Lo = Rs

	if (IsConst(_Rs_)) {
		MapConst(REG_LO, iRegs[_Rs_].k);
	} else {
		MapCopy(REG_LO, _Rs_);
	}
}
#endif

#pragma mark - branch ops -
#if 0
/*REC_BRANCH(J);
REC_BRANCH(JR);
REC_BRANCH(JAL);
REC_BRANCH(JALR);
REC_BRANCH(BLTZ);
REC_BRANCH(BGTZ);
REC_BRANCH(BLTZAL);
REC_BRANCH(BGEZAL);
REC_BRANCH(BNE);
REC_BRANCH(BEQ);
REC_BRANCH(BLEZ);
REC_BRANCH(BGEZ);*/
#else
static void recBLTZ() {
// Branch if Rs < 0
	u32 bpc = _Imm_ * 4 + pc;
	u32 *b;

	if (IsConst(_Rs_)) {
		if ((s32)iRegs[_Rs_].k < 0) {
			iJump(bpc); return;
		} else {
			iJump(pc+4); return;
		}
	}

	CMPWI(GetHWReg32(_Rs_), 0);
	BLT_L(b);
	
	iBranch(pc+4, 1);

	B_DST(b);
	
	iBranch(bpc, 0);
	pc+=4;
}

static void recBGTZ() {
// Branch if Rs > 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32)iRegs[_Rs_].k > 0) {
            iJump(bpc); return;
        } else {
            iJump(pc+4); return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGT_L(b);
    
    iBranch(pc+4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc+=4;
}

static void recBLTZAL() {
// Branch if Rs < 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32)iRegs[_Rs_].k < 0) {
            MapConst(31, pc + 4);

            iJump(bpc); return;
        } else {
            iJump(pc+4); return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLT_L(b);
    
    iBranch(pc+4, 1);

    B_DST(b);
    
    MapConst(31, pc + 4);

    iBranch(bpc, 0);
    pc+=4;
}

static void recBGEZAL() {
// Branch if Rs >= 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (IsConst(_Rs_)) {
        if ((s32)iRegs[_Rs_].k >= 0) {
            MapConst(31, pc + 4);

            iJump(bpc); return;
        } else {
            iJump(pc+4); return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGE_L(b);
    
    iBranch(pc+4, 1);

    B_DST(b);
    
    MapConst(31, pc + 4);

    iBranch(bpc, 0);
    pc+=4;
}

static void recJ() {
// j target

	iJump(_Target_ * 4 + (pc & 0xf0000000));
}

static void recJAL() {
// jal target
	MapConst(31, pc + 4);

	iJump(_Target_ * 4 + (pc & 0xf0000000));
}

static void recJR() {
// jr Rs

	if (IsConst(_Rs_)) {
		iJump(iRegs[_Rs_].k);
		//LIW(PutHWRegSpecial(TARGET), iRegs[_Rs_].k);
	} else {
		MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
		SetBranch();
	}
}

static void recJALR() {
// jalr Rs

	if (_Rd_) {
		MapConst(_Rd_, pc + 4);
	}
	
	if (IsConst(_Rs_)) {
		iJump(iRegs[_Rs_].k);
		//LIW(PutHWRegSpecial(TARGET), iRegs[_Rs_].k);
	} else {
		MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
		SetBranch();
	}
}

static void recBEQ() {
// Branch if Rs == Rt
	u32 bpc = _Imm_ * 4 + pc;
	u32 *b;

	if (_Rs_ == _Rt_) {
		iJump(bpc);
	}
	else {
		if (IsConst(_Rs_) && IsConst(_Rt_)) {
			if (iRegs[_Rs_].k == iRegs[_Rt_].k) {
				iJump(bpc); return;
			} else {
				iJump(pc+4); return;
			}
		}
		else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
			if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
				CMPLWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
			}
			else if ((s32)(s16)iRegs[_Rs_].k == (s32)iRegs[_Rs_].k) {
				CMPWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
			}
			else {
				CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
			}
		}
		else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
			if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
				CMPLWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
			}
			else if ((s32)(s16)iRegs[_Rt_].k == (s32)iRegs[_Rt_].k) {
				CMPWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
			}
			else {
				CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
			}
		}
		else {
			CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
		
		BEQ_L(b);
		
		iBranch(pc+4, 1);
	
		B_DST(b);
		
		iBranch(bpc, 0);
		pc+=4;
	}
}

static void recBNE() {
// Branch if Rs != Rt
	u32 bpc = _Imm_ * 4 + pc;
	u32 *b;

	if (_Rs_ == _Rt_) {
		iJump(pc+4);
	}
	else {
		if (IsConst(_Rs_) && IsConst(_Rt_)) {
			if (iRegs[_Rs_].k != iRegs[_Rt_].k) {
				iJump(bpc); return;
			} else {
				iJump(pc+4); return;
			}
		}
		else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
			if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
				CMPLWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
			}
			else if ((s32)(s16)iRegs[_Rs_].k == (s32)iRegs[_Rs_].k) {
				CMPWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
			}
			else {
				CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
			}
		}
		else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
			if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
				CMPLWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
			}
			else if ((s32)(s16)iRegs[_Rt_].k == (s32)iRegs[_Rt_].k) {
				CMPWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
			}
			else {
				CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
			}
		}
		else {
			CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
		}
		
		BNE_L(b);
		
		iBranch(pc+4, 1);
	
		B_DST(b);
		
		iBranch(bpc, 0);
		pc+=4;
	}
}

static void recBLEZ() {
// Branch if Rs <= 0
	u32 bpc = _Imm_ * 4 + pc;
	u32 *b;

	if (IsConst(_Rs_)) {
		if ((s32)iRegs[_Rs_].k <= 0) {
			iJump(bpc); return;
		} else {
			iJump(pc+4); return;
		}
	}

	CMPWI(GetHWReg32(_Rs_), 0);
	BLE_L(b);
	
	iBranch(pc+4, 1);

	B_DST(b);
	
	iBranch(bpc, 0);
	pc+=4;
}

static void recBGEZ() {
// Branch if Rs >= 0
	u32 bpc = _Imm_ * 4 + pc;
	u32 *b;

	if (IsConst(_Rs_)) {
		if ((s32)iRegs[_Rs_].k >= 0) {
			iJump(bpc); return;
		} else {
			iJump(pc+4); return;
		}
	}

	CMPWI(GetHWReg32(_Rs_), 0);
	BGE_L(b);
	
	iBranch(pc+4, 1);

	B_DST(b);
	
	iBranch(bpc, 0);
	pc+=4;
}
#endif

#if 1
//REC_FUNC(MFC0);
//REC_SYS(MTC0);
//REC_FUNC(CFC0);
//REC_SYS(CTC0);
REC_FUNC(RFE);
//#else
static void recMFC0() {
// Rt = Cop0->Rd
	if (!_Rt_) return;
	
	LWZ(PutHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
}

static void recCFC0() {
// Rt = Cop0->Rd

	recMFC0();
}

static void recMTC0() {
// Cop0->Rd = Rt

	/*if (IsConst(_Rt_)) {
		switch (_Rd_) {
			case 12:
				MOV32ItoM((u32)&psxRegs.CP0.r[_Rd_], iRegs[_Rt_].k);
				break;
			case 13:
				MOV32ItoM((u32)&psxRegs.CP0.r[_Rd_], iRegs[_Rt_].k & ~(0xfc00));
				break;
			default:
				MOV32ItoM((u32)&psxRegs.CP0.r[_Rd_], iRegs[_Rt_].k);
				break;
		}
	} else*/ {
		switch (_Rd_) {
			case 13:
				RLWINM(0,GetHWReg32(_Rt_),0,22,15); // & ~(0xfc00)
				STW(0, OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
				break;
			default:
				STW(GetHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
				break;
		}
	}

	if (_Rd_ == 12 || _Rd_ == 13) {
		iFlushRegs(0);
		LIW(PutHWRegSpecial(PSXPC), (u32)pc);
		FlushAllHWReg();
		CALLFunc((u32)psxTestSWInts);
		if(_Rd_ == 12) {
		  LWZ(0, OFFSET(&psxRegs, &psxRegs.interrupt), GetHWRegSpecial(PSXREGS));
		  ORIS(0, 0, 0x8000);
		  STW(0, OFFSET(&psxRegs, &psxRegs.interrupt), GetHWRegSpecial(PSXREGS));
		}
		branch = 2;
		iRet();
	}
}

static void recCTC0() {
// Cop0->Rd = Rt

	recMTC0();
}
#else
static void recRFE() {
	// TODO: implement multiple temp registers or cop0 registers
	RLWINM(t1,Status,0,0,27);
	RLWINM(Status,Status,30,28,31);
	OR(Status,t1,Status);
	
	MOV32MtoR(EAX, (u32)&psxRegs.CP0.n.Status);
	MOV32RtoR(ECX, EAX);
	AND32ItoR(EAX, 0xfffffff0);
	AND32ItoR(ECX, 0x3c);
	SHR32ItoR(ECX, 2);
	OR32RtoR (EAX, ECX);
	MOV32RtoM((u32)&psxRegs.CP0.n.Status, EAX);
	CALLFunc((u32)psxExceptionTest);
}
#endif

#if 0
#define CP2_FUNC(f) \
void gte##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(0, (u32)psxRegs.code); \
	STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	FlushAllHWReg(); \
	CALLFunc ((u32)gte##f); \
}
CP2_FUNC(LWC2);
CP2_FUNC(SWC2);

#else
#include "pGte.h"
#endif
//

static void recHLE() {
	iFlushRegs(0);
	FlushAllHWReg();
	
	if ((psxRegs.code & 0x3ffffff) == (psxRegs.code & 0x7)) {
		CALLFunc((u32)psxHLEt[psxRegs.code & 0x7]);
	} else {
		// somebody else must have written to current opcode for this to happen!!!!
		CALLFunc((u32)psxHLEt[0]); // call dummy function
	}
	
	count = (idlecyclecount + (pc - pcold) / 4 + 20) * BIAS;
	ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
	FlushAllHWReg();
	CALLFunc((u32)psxBranchTest);
	Return();
	
	branch = 2;
}

//

static void (*recBSC[64])() = {
	recSPECIAL, recREGIMM, recJ   , recJAL  , recBEQ , recBNE , recBLEZ, recBGTZ,
	recADDI   , recADDIU , recSLTI, recSLTIU, recANDI, recORI , recXORI, recLUI ,
	recCOP0   , recNULL  , recCOP2, recNULL , recNULL, recNULL, recNULL, recNULL,
	recNULL   , recNULL  , recNULL, recNULL , recNULL, recNULL, recNULL, recNULL,
	recLB     , recLH    , recLWL , recLW   , recLBU , recLHU , recLWR , recNULL,
	recSB     , recSH    , recSWL , recSW   , recNULL, recNULL, recSWR , recNULL,
	recNULL   , recNULL  , recLWC2, recNULL , recNULL, recNULL, recNULL, recNULL,
	recNULL   , recNULL  , recSWC2, recHLE  , recNULL, recNULL, recNULL, recNULL
};

static void (*recSPC[64])() = {
	recSLL , recNULL, recSRL , recSRA , recSLLV   , recNULL , recSRLV, recSRAV,
	recJR  , recJALR, recNULL, recNULL, recSYSCALL, recBREAK, recNULL, recNULL,
	recMFHI, recMTHI, recMFLO, recMTLO, recNULL   , recNULL , recNULL, recNULL,
	recMULT, recMULTU, recDIV, recDIVU, recNULL   , recNULL , recNULL, recNULL,
	recADD , recADDU, recSUB , recSUBU, recAND    , recOR   , recXOR , recNOR ,
	recNULL, recNULL, recSLT , recSLTU, recNULL   , recNULL , recNULL, recNULL,
	recNULL, recNULL, recNULL, recNULL, recNULL   , recNULL , recNULL, recNULL,
	recNULL, recNULL, recNULL, recNULL, recNULL   , recNULL , recNULL, recNULL
};

static void (*recREG[32])() = {
	recBLTZ  , recBGEZ  , recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
	recNULL  , recNULL  , recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
	recBLTZAL, recBGEZAL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
	recNULL  , recNULL  , recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recCP0[32])() = {
	recMFC0, recNULL, recCFC0, recNULL, recMTC0, recNULL, recCTC0, recNULL,
	recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
	recRFE , recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
	recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recCP2[64])() = {
	recBASIC, recRTPS , recNULL , recNULL, recNULL, recNULL , recNCLIP, recNULL, // 00
	recNULL , recNULL , recNULL , recNULL, recOP  , recNULL , recNULL , recNULL, // 08
	recDPCS , recINTPL, recMVMVA, recNCDS, recCDP , recNULL , recNCDT , recNULL, // 10
	recNULL , recNULL , recNULL , recNCCS, recCC  , recNULL , recNCS  , recNULL, // 18
	recNCT  , recNULL , recNULL , recNULL, recNULL, recNULL , recNULL , recNULL, // 20
	recSQR  , recDCPL , recDPCT , recNULL, recNULL, recAVSZ3, recAVSZ4, recNULL, // 28 
	recRTPT , recNULL , recNULL , recNULL, recNULL, recNULL , recNULL , recNULL, // 30
	recNULL , recNULL , recNULL , recNULL, recNULL, recGPF  , recGPL  , recNCCT  // 38
};

static void (*recCP2BSC[32])() = {
	recMFC2, recNULL, recCFC2, recNULL, recMTC2, recNULL, recCTC2, recNULL,
	recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
	recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
	recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void recRecompile() {
	//static int recCount = 0;
	char *p;
	u32 *ptr;
	int i;
	
	cop2readypc = 0;
	idlecyclecount = 0;

	// initialize state variables
	UniqueRegAlloc = 1;
	HWRegUseCount = 0;
	DstCPUReg = -1;
	memset(HWRegisters, 0, sizeof(HWRegisters));
	for (i=0; i<NUM_HW_REGISTERS; i++)
		HWRegisters[i].code = cpuHWRegisters[NUM_HW_REGISTERS-i-1];
	
	// reserve the special psxReg register
	HWRegisters[0].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	HWRegisters[0].private = PSXREGS;
	HWRegisters[0].k = (u32)&psxRegs;

	HWRegisters[1].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	HWRegisters[1].private = PSXMEM;
	HWRegisters[1].k = (u32)&psxM;

	// reserve the special psxRegs.cycle register
	//HWRegisters[1].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
	//HWRegisters[1].private = CYCLECOUNT;
	
	//memset(iRegs, 0, sizeof(iRegs));
	for (i=0; i<NUM_REGISTERS; i++) {
		iRegs[i].state = ST_UNK;
		iRegs[i].reg = -1;
	}
	iRegs[0].k = 0;
	iRegs[0].state = ST_CONST;
	
	/* if ppcPtr reached the mem limit reset whole mem */
	if (((u32)ppcPtr - (u32)recMem) >= (RECMEM_SIZE - 0x10000))
		recReset();

	ppcAlign(/*32*/4);
	ptr = ppcPtr;
	
	// give us write access
	//mprotect(recMem, RECMEM_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE);
	
	// tell the LUT where to find us
	PC_REC32(psxRegs.pc) = (u32)ppcPtr;

	pcold = pc = psxRegs.pc;
	
	//SysPrintf("RecCount: %i\n", recCount++);
	
	for (count=0; count<500;) {
		p = (char *)PSXM(pc);
		if (p == NULL) recError();
		psxRegs.code = SWAP32(*(u32 *)p);
/*
		if ((psxRegs.code >> 26) == 0x23) { // LW
			int i;
			u32 code;

			for (i=1;; i++) {
				p = (char *)PSXM(pc+i*4);
				if (p == NULL) recError();
				code = *(u32 *)p;

				if ((code >> 26) != 0x23 ||
					_fRs_(code)  != _Rs_ ||
					_fImm_(code) != (_Imm_+i*4))
					break;
			}
			if (i > 1) {
				recLWBlock(i);
				pc = pc + i*4; continue;
			}
		}

		if ((psxRegs.code >> 26) == 0x2b) { // SW
			int i;
			u32 code;

			for (i=1;; i++) {
				p = (char *)PSXM(pc+i*4);
				if (p == NULL) recError();
				code = *(u32 *)p;

				if ((code >> 26) != 0x2b ||
					_fRs_(code)  != _Rs_ ||
					_fImm_(code) != (_Imm_+i*4))
					break;
			}
			if (i > 1) {
				recSWBlock(i);
				pc = pc + i*4; continue;
			}
		}*/

		pc+=4; count++;
//		iFlushRegs(0); // test
		recBSC[psxRegs.code>>26]();

		if (branch) {
			branch = 0;
			//if (dump) iDumpBlock(ptr);
			goto done;
		}
	}

	iFlushRegs(pc);
	
	LIW(PutHWRegSpecial(PSXPC), pc);

	iRet();

done:;
#if 0
	MakeDataExecutable(ptr, ((u8*)ppcPtr)-((u8*)ptr));
#else
	u32 a = (u32)(u8*)ptr;
	while(a < (u32)(u8*)ppcPtr) {
	  __asm__ __volatile__("icbi 0,%0" : : "r" (a));
	  __asm__ __volatile__("dcbst 0,%0" : : "r" (a));
	  a += 4;
	}
	__asm__ __volatile__("sync");
	__asm__ __volatile__("isync");
#endif
	
#if 1
	sprintf((char *)ppcPtr, "PC=%08x", pcold);
	ppcPtr += strlen((char *)ppcPtr);
#endif

	//mprotect(recMem, RECMEM_SIZE, PROT_EXEC|PROT_READ/*|PROT_WRITE*/);
}


R3000Acpu psxRec = {
	recInit,
	recReset,
	recExecute,
	recExecuteBlock,
	recClear,
	recShutdown
};


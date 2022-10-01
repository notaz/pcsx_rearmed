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
 * PSX assembly interpreter.
 */

#include "psxcommon.h"
#include "r3000a.h"
#include "gte.h"
#include "psxhle.h"
#include "psxinterpreter.h"
#include <stddef.h>
#include <assert.h>
//#include "debug.h"
#define ProcessDebug()

static int branch = 0;
static int branch2 = 0;
static u32 branchPC;

// These macros are used to assemble the repassembler functions

#ifdef PSXCPU_LOG
#define debugI() PSXCPU_LOG("%s\n", disR3000AF(psxRegs.code, psxRegs.pc)); 
#else
#define debugI()
#endif

#ifdef __i386__
#define INT_ATTR __attribute__((regparm(2)))
#else
#define INT_ATTR
#endif
#ifndef INVALID_PTR
#define INVALID_PTR NULL
#endif

// Subsets
static void (INT_ATTR *psxBSC[64])(psxRegisters *regs_, u32 code);
static void (INT_ATTR *psxSPC[64])(psxRegisters *regs_, u32 code);

static u32 INT_ATTR fetchNoCache(u8 **memRLUT, u32 pc)
{
	u8 *base = memRLUT[pc >> 16];
	if (base == INVALID_PTR)
		return 0;
	u32 *code = (u32 *)(base + (pc & 0xfffc));
	return SWAP32(*code);
}

/*
Formula One 2001 :
Use old CPU cache code when the RAM location is updated with new code (affects in-game racing)
*/
static struct cache_entry {
	u32 tag;
	u32 data[4];
} ICache[256];

static u32 INT_ATTR fetchICache(u8 **memRLUT, u32 pc)
{
	// cached?
	if (pc < 0xa0000000)
	{
		// this is not how the hardware works but whatever
		struct cache_entry *entry = &ICache[(pc & 0xff0) >> 4];

		if (((entry->tag ^ pc) & 0xfffffff0) != 0 || pc < entry->tag)
		{
			const u8 *base = memRLUT[pc >> 16];
			const u32 *code;
			if (base == INVALID_PTR)
				return 0;
			code = (u32 *)(base + (pc & 0xfff0));

			entry->tag = pc;
			// treat as 4 words, although other configurations are said to be possible
			switch (pc & 0x0c)
			{
				case 0x00: entry->data[0] = SWAP32(code[0]);
				case 0x04: entry->data[1] = SWAP32(code[1]);
				case 0x08: entry->data[2] = SWAP32(code[2]);
				case 0x0c: entry->data[3] = SWAP32(code[3]);
			}
		}
		return entry->data[(pc & 0x0f) >> 2];
	}

	return fetchNoCache(memRLUT, pc);
}

static u32 (INT_ATTR *fetch)(u8 **memRLUT, u32 pc) = fetchNoCache;

// Make the timing events trigger faster as we are currently assuming everything
// takes one cycle, which is not the case on real hardware.
// FIXME: count cache misses, memory latencies, stalls to get rid of this
static inline void addCycle(void)
{
	assert(psxRegs.subCycleStep >= 0x10000);
	psxRegs.subCycle += psxRegs.subCycleStep;
	psxRegs.cycle += psxRegs.subCycle >> 16;
	psxRegs.subCycle &= 0xffff;
}

static void delayRead(int reg, u32 bpc) {
	u32 rold, rnew;

//	SysPrintf("delayRead at %x!\n", psxRegs.pc);

	rold = psxRegs.GPR.r[reg];
	psxBSC[psxRegs.code >> 26](&psxRegs, psxRegs.code); // branch delay load
	rnew = psxRegs.GPR.r[reg];

	psxRegs.pc = bpc;

	branch = 0;

	psxRegs.GPR.r[reg] = rold;
	execI(); // first branch opcode
	psxRegs.GPR.r[reg] = rnew;

	psxBranchTest();
}

static void delayWrite(int reg, u32 bpc) {

/*	SysPrintf("delayWrite at %x!\n", psxRegs.pc);

	SysPrintf("%s\n", disR3000AF(psxRegs.code, psxRegs.pc-4));
	SysPrintf("%s\n", disR3000AF(PSXMu32(bpc), bpc));*/

	// no changes from normal behavior

	psxBSC[psxRegs.code >> 26](&psxRegs, psxRegs.code);

	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

static void delayReadWrite(int reg, u32 bpc) {

//	SysPrintf("delayReadWrite at %x!\n", psxRegs.pc);

	// the branch delay load is skipped

	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

/**** R3000A Instruction Macros ****/
#define _PC_            regs_->pc       // The next PC to be executed

#define _fOp_(code)     ((code >> 26)       )  // The opcode part of the instruction register
#define _fFunct_(code)  ((code      ) & 0x3F)  // The funct part of the instruction register
#define _fRd_(code)     ((code >> 11) & 0x1F)  // The rd part of the instruction register
#define _fRt_(code)     ((code >> 16) & 0x1F)  // The rt part of the instruction register
#define _fRs_(code)     ((code >> 21) & 0x1F)  // The rs part of the instruction register
#define _fSa_(code)     ((code >>  6) & 0x1F)  // The sa part of the instruction register
#define _fIm_(code)     ((u16)code)            // The immediate part of the instruction register
#define _fTarget_(code) (code & 0x03ffffff)    // The target part of the instruction register

#define _fImm_(code)    ((s16)code)            // sign-extended immediate
#define _fImmU_(code)   (code&0xffff)          // zero-extended immediate

#define _Op_     _fOp_(code)
#define _Funct_  _fFunct_(code)
#define _Rd_     _fRd_(code)
#define _Rt_     _fRt_(code)
#define _Rs_     _fRs_(code)
#define _Sa_     _fSa_(code)
#define _Im_     _fIm_(code)
#define _Target_ _fTarget_(code)

#define _Imm_    _fImm_(code)
#define _ImmU_   _fImmU_(code)

#define _rRs_   regs_->GPR.r[_Rs_]   // Rs register
#define _rRt_   regs_->GPR.r[_Rt_]   // Rt register
#define _rRd_   regs_->GPR.r[_Rd_]   // Rd register
#define _rSa_   regs_->GPR.r[_Sa_]   // Sa register
#define _rFs_   regs_->CP0.r[_Rd_]   // Fs register

#define _rHi_   regs_->GPR.n.hi   // The HI register
#define _rLo_   regs_->GPR.n.lo   // The LO register

#define _JumpTarget_    ((_Target_ * 4) + (_PC_ & 0xf0000000))   // Calculates the target during a jump instruction
#define _BranchTarget_  ((s16)_Im_ * 4 + _PC_)                 // Calculates the target during a branch instruction

#define _SetLink(x)     regs_->GPR.r[x] = _PC_ + 4;       // Sets the return address in the link register

#define OP(name) \
	static inline INT_ATTR void name(psxRegisters *regs_, u32 code)

// this defines shall be used with the tmp 
// of the next func (instead of _Funct_...)
#define _tFunct_  ((tmp      ) & 0x3F)  // The funct part of the instruction register 
#define _tRd_     ((tmp >> 11) & 0x1F)  // The rd part of the instruction register 
#define _tRt_     ((tmp >> 16) & 0x1F)  // The rt part of the instruction register 
#define _tRs_     ((tmp >> 21) & 0x1F)  // The rs part of the instruction register 
#define _tSa_     ((tmp >>  6) & 0x1F)  // The sa part of the instruction register

#define _i32(x) (s32)(x)
#define _u32(x) (u32)(x)

static int psxTestLoadDelay(int reg, u32 tmp) {
	if (tmp == 0) return 0; // NOP
	switch (tmp >> 26) {
		case 0x00: // SPECIAL
			switch (_tFunct_) {
				case 0x00: // SLL
				case 0x02: case 0x03: // SRL/SRA
					if (_tRd_ == reg && _tRt_ == reg) return 1; else
					if (_tRt_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x08: // JR
					if (_tRs_ == reg) return 2;
					break;
				case 0x09: // JALR
					if (_tRd_ == reg && _tRs_ == reg) return 1; else
					if (_tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				// SYSCALL/BREAK just a break;

				case 0x20: case 0x21: case 0x22: case 0x23:
				case 0x24: case 0x25: case 0x26: case 0x27: 
				case 0x2a: case 0x2b: // ADD/ADDU...
				case 0x04: case 0x06: case 0x07: // SLLV...
					if (_tRd_ == reg && (_tRt_ == reg || _tRs_ == reg)) return 1; else
					if (_tRt_ == reg || _tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x10: case 0x12: // MFHI/MFLO
					if (_tRd_ == reg) return 3;
					break;
				case 0x11: case 0x13: // MTHI/MTLO
					if (_tRs_ == reg) return 2;
					break;

				case 0x18: case 0x19:
				case 0x1a: case 0x1b: // MULT/DIV...
					if (_tRt_ == reg || _tRs_ == reg) return 2;
					break;
			}
			break;

		case 0x01: // REGIMM
			switch (_tRt_) {
				case 0x00: case 0x01:
				case 0x10: case 0x11: // BLTZ/BGEZ...
					// Xenogears - lbu v0 / beq v0
					// - no load delay (fixes battle loading)
					break;

					if (_tRs_ == reg) return 2;
					break;
			}
			break;

		// J would be just a break;
		case 0x03: // JAL
			if (31 == reg) return 3;
			break;

		case 0x04: case 0x05: // BEQ/BNE
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg || _tRt_ == reg) return 2;
			break;

		case 0x06: case 0x07: // BLEZ/BGTZ
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg) return 2;
			break;

		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: // ADDI/ADDIU...
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x0f: // LUI
			if (_tRt_ == reg) return 3;
			break;

		case 0x10: // COP0
			switch (_tFunct_) {
				case 0x00: // MFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x02: // CFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x04: // MTC0
					if (_tRt_ == reg) return 2;
					break;
				case 0x06: // CTC0
					if (_tRt_ == reg) return 2;
					break;
				// RFE just a break;
			}
			break;

		case 0x12: // COP2
			switch (_tFunct_) {
				case 0x00: 
					switch (_tRs_) {
						case 0x00: // MFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x02: // CFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x04: // MTC2
							if (_tRt_ == reg) return 2;
							break;
						case 0x06: // CTC2
							if (_tRt_ == reg) return 2;
							break;
					}
					break;
				// RTPS... break;
			}
			break;

		case 0x22: case 0x26: // LWL/LWR
			if (_tRt_ == reg) return 3; else
			if (_tRs_ == reg) return 2;
			break;

		case 0x20: case 0x21: case 0x23:
		case 0x24: case 0x25: // LB/LH/LW/LBU/LHU
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x28: case 0x29: case 0x2a:
		case 0x2b: case 0x2e: // SB/SH/SWL/SW/SWR
			if (_tRt_ == reg || _tRs_ == reg) return 2;
			break;

		case 0x32: case 0x3a: // LWC2/SWC2
			if (_tRs_ == reg) return 2;
			break;
	}

	return 0;
}

static void psxDelayTest(int reg, u32 bpc) {
	u32 tmp = fetch(psxMemRLUT, bpc);
	branch = 1;

	switch (psxTestLoadDelay(reg, tmp)) {
		case 1:
			delayReadWrite(reg, bpc); return;
		case 2:
			delayRead(reg, bpc); return;
		case 3:
			delayWrite(reg, bpc); return;
	}
	psxBSC[psxRegs.code >> 26](&psxRegs, psxRegs.code);

	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

static u32 psxBranchNoDelay(psxRegisters *regs_) {
	u32 temp, code;

	regs_->code = code = fetch(psxMemRLUT, regs_->pc);
	switch (_Op_) {
		case 0x00: // SPECIAL
			switch (_Funct_) {
				case 0x08: // JR
					return _u32(_rRs_);
				case 0x09: // JALR
					temp = _u32(_rRs_);
					if (_Rd_) { _SetLink(_Rd_); }
					return temp;
			}
			break;
		case 0x01: // REGIMM
			switch (_Rt_) {
				case 0x00: // BLTZ
					if (_i32(_rRs_) < 0)
						return _BranchTarget_;
					break;
				case 0x01: // BGEZ
					if (_i32(_rRs_) >= 0)
						return _BranchTarget_;
					break;
				case 0x08: // BLTZAL
					if (_i32(_rRs_) < 0) {
						_SetLink(31);
						return _BranchTarget_;
					}
					break;
				case 0x09: // BGEZAL
					if (_i32(_rRs_) >= 0) {
						_SetLink(31);
						return _BranchTarget_;
					}
					break;
			}
			break;
		case 0x02: // J
			return _JumpTarget_;
		case 0x03: // JAL
			_SetLink(31);
			return _JumpTarget_;
		case 0x04: // BEQ
			if (_i32(_rRs_) == _i32(_rRt_))
				return _BranchTarget_;
			break;
		case 0x05: // BNE
			if (_i32(_rRs_) != _i32(_rRt_))
				return _BranchTarget_;
			break;
		case 0x06: // BLEZ
			if (_i32(_rRs_) <= 0)
				return _BranchTarget_;
			break;
		case 0x07: // BGTZ
			if (_i32(_rRs_) > 0)
				return _BranchTarget_;
			break;
	}

	return (u32)-1;
}

static int psxDelayBranchExec(u32 tar) {
	execI();

	branch = 0;
	psxRegs.pc = tar;
	addCycle();
	psxBranchTest();
	return 1;
}

static int psxDelayBranchTest(u32 tar1) {
	u32 tar2, tmp1, tmp2;

	tar2 = psxBranchNoDelay(&psxRegs);
	if (tar2 == (u32)-1)
		return 0;

	debugI();

	/*
	 * Branch in delay slot:
	 * - execute 1 instruction at tar1
	 * - jump to tar2 (target of branch in delay slot; this branch
	 *   has no normal delay slot, instruction at tar1 was fetched instead)
	 */
	psxRegs.pc = tar1;
	tmp1 = psxBranchNoDelay(&psxRegs);
	if (tmp1 == (u32)-1) {
		return psxDelayBranchExec(tar2);
	}
	debugI();
	addCycle();

	/*
	 * Got a branch at tar1:
	 * - execute 1 instruction at tar2
	 * - jump to target of that branch (tmp1)
	 */
	psxRegs.pc = tar2;
	tmp2 = psxBranchNoDelay(&psxRegs);
	if (tmp2 == (u32)-1) {
		return psxDelayBranchExec(tmp1);
	}
	debugI();
	addCycle();

	/*
	 * Got a branch at tar2:
	 * - execute 1 instruction at tmp1
	 * - jump to target of that branch (tmp2)
	 */
	psxRegs.pc = tmp1;
	return psxDelayBranchExec(tmp2);
}

static void doBranch(u32 tar) {
	u32 tmp, code;

	branch2 = branch = 1;
	branchPC = tar;

	// check for branch in delay slot
	if (psxDelayBranchTest(tar))
		return;

	psxRegs.code = code = fetch(psxMemRLUT, psxRegs.pc);

	debugI();

	psxRegs.pc += 4;
	addCycle();

	// check for load delay
	tmp = psxRegs.code >> 26;
	switch (tmp) {
		case 0x10: // COP0
			switch (_Rs_) {
				case 0x00: // MFC0
				case 0x02: // CFC0
					psxDelayTest(_Rt_, branchPC);
					return;
			}
			break;
		case 0x12: // COP2
			switch (_Funct_) {
				case 0x00:
					switch (_Rs_) {
						case 0x00: // MFC2
						case 0x02: // CFC2
							psxDelayTest(_Rt_, branchPC);
							return;
					}
					break;
			}
			break;
		case 0x32: // LWC2
			psxDelayTest(_Rt_, branchPC);
			return;
		default:
			if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
				psxDelayTest(_Rt_, branchPC);
				return;
			}
			break;
	}

	psxBSC[psxRegs.code >> 26](&psxRegs, psxRegs.code);

	branch = 0;
	psxRegs.pc = branchPC;

	psxBranchTest();
}

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
OP(psxADDI)  { if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }  // Rt = Rs + Im 	(Exception on Integer Overflow)
OP(psxADDIU) { if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }  // Rt = Rs + Im
OP(psxANDI)  { if (!_Rt_) return; _rRt_ = _u32(_rRs_) & _ImmU_; }  // Rt = Rs And Im
OP(psxORI)   { if (!_Rt_) return; _rRt_ = _u32(_rRs_) | _ImmU_; }  // Rt = Rs Or  Im
OP(psxXORI)  { if (!_Rt_) return; _rRt_ = _u32(_rRs_) ^ _ImmU_; }  // Rt = Rs Xor Im
OP(psxSLTI)  { if (!_Rt_) return; _rRt_ = _i32(_rRs_) < _Imm_ ; }  // Rt = Rs < Im		(Signed)
OP(psxSLTIU) { if (!_Rt_) return; _rRt_ = _u32(_rRs_) < ((u32)_Imm_); } // Rt = Rs < Im		(Unsigned)

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
OP(psxADD)   { if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt		(Exception on Integer Overflow)
OP(psxADDU)  { if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt
OP(psxSUB)   { if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt		(Exception on Integer Overflow)
OP(psxSUBU)  { if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt
OP(psxAND)   { if (!_Rd_) return; _rRd_ = _u32(_rRs_) & _u32(_rRt_); }	// Rd = Rs And Rt
OP(psxOR)    { if (!_Rd_) return; _rRd_ = _u32(_rRs_) | _u32(_rRt_); }	// Rd = Rs Or  Rt
OP(psxXOR)   { if (!_Rd_) return; _rRd_ = _u32(_rRs_) ^ _u32(_rRt_); }	// Rd = Rs Xor Rt
OP(psxNOR)   { if (!_Rd_) return; _rRd_ =~(_u32(_rRs_) | _u32(_rRt_)); }// Rd = Rs Nor Rt
OP(psxSLT)   { if (!_Rd_) return; _rRd_ = _i32(_rRs_) < _i32(_rRt_); }	// Rd = Rs < Rt		(Signed)
OP(psxSLTU)  { if (!_Rd_) return; _rRd_ = _u32(_rRs_) < _u32(_rRt_); }	// Rd = Rs < Rt		(Unsigned)

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
OP(psxDIV) {
	if (!_rRt_) {
		_rHi_ = _rRs_;
		if (_rRs_ & 0x80000000) {
			_rLo_ = 1;
		} else {
			_rLo_ = 0xFFFFFFFF;
		}
	}
#if !defined(__arm__) && !defined(__aarch64__)
	else if (_rRs_ == 0x80000000 && _rRt_ == 0xFFFFFFFF) {
		_rLo_ = 0x80000000;
		_rHi_ = 0;
	}
#endif
	else {
		_rLo_ = _i32(_rRs_) / _i32(_rRt_);
		_rHi_ = _i32(_rRs_) % _i32(_rRt_);
	}
}

OP(psxDIV_stall) {
	regs_->muldivBusyCycle = regs_->cycle + 37;
	psxDIV(regs_, code);
}

OP(psxDIVU) {
	if (_rRt_ != 0) {
		_rLo_ = _rRs_ / _rRt_;
		_rHi_ = _rRs_ % _rRt_;
	}
	else {
		_rLo_ = 0xffffffff;
		_rHi_ = _rRs_;
	}
}

OP(psxDIVU_stall) {
	regs_->muldivBusyCycle = regs_->cycle + 37;
	psxDIVU(regs_, code);
}

OP(psxMULT) {
	u64 res = (s64)_i32(_rRs_) * _i32(_rRt_);

	regs_->GPR.n.lo = (u32)res;
	regs_->GPR.n.hi = (u32)(res >> 32);
}

OP(psxMULT_stall) {
	// approximate, but maybe good enough
	u32 rs = _rRs_;
	u32 lz = __builtin_clz(((rs ^ ((s32)rs >> 21)) | 1));
	u32 c = 7 + (2 - (lz / 11)) * 4;
	regs_->muldivBusyCycle = regs_->cycle + c;
	psxMULT(regs_, code);
}

OP(psxMULTU) {
	u64 res = (u64)_u32(_rRs_) * _u32(_rRt_);

	regs_->GPR.n.lo = (u32)(res & 0xffffffff);
	regs_->GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

OP(psxMULTU_stall) {
	// approximate, but maybe good enough
	u32 lz = __builtin_clz(_rRs_ | 1);
	u32 c = 7 + (2 - (lz / 11)) * 4;
	regs_->muldivBusyCycle = regs_->cycle + c;
	psxMULTU(regs_, code);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32(op)      if(_i32(_rRs_) op 0) doBranch(_BranchTarget_);
#define RepZBranchLinki32(op)  { _SetLink(31); if(_i32(_rRs_) op 0) { doBranch(_BranchTarget_); } }

OP(psxBGEZ)   { RepZBranchi32(>=) }      // Branch if Rs >= 0
OP(psxBGEZAL) { RepZBranchLinki32(>=) }  // Branch if Rs >= 0 and link
OP(psxBGTZ)   { RepZBranchi32(>) }       // Branch if Rs >  0
OP(psxBLEZ)   { RepZBranchi32(<=) }      // Branch if Rs <= 0
OP(psxBLTZ)   { RepZBranchi32(<) }       // Branch if Rs <  0
OP(psxBLTZAL) { RepZBranchLinki32(<) }   // Branch if Rs <  0 and link

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
OP(psxSLL) { if (!_Rd_) return; _rRd_ = _u32(_rRt_) << _Sa_; } // Rd = Rt << sa
OP(psxSRA) { if (!_Rd_) return; _rRd_ = _i32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (arithmetic)
OP(psxSRL) { if (!_Rd_) return; _rRd_ = _u32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (logical)

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/
OP(psxSLLV) { if (!_Rd_) return; _rRd_ = _u32(_rRt_) << (_u32(_rRs_) & 0x1F); } // Rd = Rt << rs
OP(psxSRAV) { if (!_Rd_) return; _rRd_ = _i32(_rRt_) >> (_u32(_rRs_) & 0x1F); } // Rd = Rt >> rs (arithmetic)
OP(psxSRLV) { if (!_Rd_) return; _rRd_ = _u32(_rRt_) >> (_u32(_rRs_) & 0x1F); } // Rd = Rt >> rs (logical)

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
OP(psxLUI) { if (!_Rt_) return; _rRt_ = code << 16; } // Upper halfword of Rt = Im

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
OP(psxMFHI) { if (!_Rd_) return; _rRd_ = _rHi_; } // Rd = Hi
OP(psxMFLO) { if (!_Rd_) return; _rRd_ = _rLo_; } // Rd = Lo

static void mflohiCheckStall(psxRegisters *regs_)
{
	u32 left = regs_->muldivBusyCycle - regs_->cycle;
	if (left <= 37) {
		//printf("muldiv stall %u\n", left);
		regs_->cycle = regs_->muldivBusyCycle;
	}
}

OP(psxMFHI_stall) { mflohiCheckStall(regs_); psxMFHI(regs_, code); }
OP(psxMFLO_stall) { mflohiCheckStall(regs_); psxMFLO(regs_, code); }

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
OP(psxMTHI) { _rHi_ = _rRs_; } // Hi = Rs
OP(psxMTLO) { _rLo_ = _rRs_; } // Lo = Rs

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
OP(psxBREAK) {
	regs_->pc -= 4;
	psxException(0x24, branch);
}

OP(psxSYSCALL) {
	regs_->pc -= 4;
	psxException(0x20, branch);
}

static inline void psxTestSWInts(psxRegisters *regs_) {
	if (regs_->CP0.n.Cause & regs_->CP0.n.Status & 0x0300 &&
	   regs_->CP0.n.Status & 0x1) {
		regs_->CP0.n.Cause &= ~0x7c;
		psxException(regs_->CP0.n.Cause, branch);
	}
}

OP(psxRFE) {
//	SysPrintf("psxRFE\n");
	regs_->CP0.n.Status = (regs_->CP0.n.Status & 0xfffffff0) |
	                      ((regs_->CP0.n.Status & 0x3c) >> 2);
	psxTestSWInts(regs_);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op)      if(_i32(_rRs_) op _i32(_rRt_)) doBranch(_BranchTarget_);

OP(psxBEQ) { RepBranchi32(==) }  // Branch if Rs == Rt
OP(psxBNE) { RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
OP(psxJ)   {               doBranch(_JumpTarget_); }
OP(psxJAL) { _SetLink(31); doBranch(_JumpTarget_); }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
OP(psxJR) {
	doBranch(_rRs_ & ~3);
	psxJumpTest();
}

OP(psxJALR) {
	u32 temp = _u32(_rRs_);
	if (_Rd_) { _SetLink(_Rd_); }
	doBranch(temp & ~3);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define _oB_ (regs_->GPR.r[_Rs_] + _Imm_)

OP(psxLB)  { u32 v =  (s8)psxMemRead8(_oB_);  if (_Rt_) _rRt_ = v; }
OP(psxLBU) { u32 v =      psxMemRead8(_oB_);  if (_Rt_) _rRt_ = v; }
OP(psxLH)  { u32 v = (s16)psxMemRead16(_oB_); if (_Rt_) _rRt_ = v; }
OP(psxLHU) { u32 v =      psxMemRead16(_oB_); if (_Rt_) _rRt_ = v; }
OP(psxLW)  { u32 v =      psxMemRead32(_oB_); if (_Rt_) _rRt_ = v; }

OP(psxLWL) {
	static const u32 LWL_MASK[4] = { 0xffffff, 0xffff, 0xff, 0 };
	static const u32 LWL_SHIFT[4] = { 24, 16, 8, 0 };
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);

	if (!_Rt_) return;
	_rRt_ = (_u32(_rRt_) & LWL_MASK[shift]) | (mem << LWL_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   4bcd   (mem << 24) | (reg & 0x00ffffff)
	1   34cd   (mem << 16) | (reg & 0x0000ffff)
	2   234d   (mem <<  8) | (reg & 0x000000ff)
	3   1234   (mem      ) | (reg & 0x00000000)
	*/
}

OP(psxLWR) {
	static const u32 LWR_MASK[4] = { 0, 0xff000000, 0xffff0000, 0xffffff00 };
	static const u32 LWR_SHIFT[4] = { 0, 8, 16, 24 };
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);

	if (!_Rt_) return;
	_rRt_ = (_u32(_rRt_) & LWR_MASK[shift]) | (mem >> LWR_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)
	*/
}

OP(psxSB) { psxMemWrite8 (_oB_, _rRt_ &   0xff); }
OP(psxSH) { psxMemWrite16(_oB_, _rRt_ & 0xffff); }
OP(psxSW) { psxMemWrite32(_oB_, _rRt_); }

OP(psxSWL) {
	static const u32 SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0 };
	static const u32 SWL_SHIFT[4] = { 24, 16, 8, 0 };
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);

	psxMemWrite32(addr & ~3,  (_u32(_rRt_) >> SWL_SHIFT[shift]) |
			     (  mem & SWL_MASK[shift]) );
	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)
	*/
}

OP(psxSWR) {
	static const u32 SWR_MASK[4] = { 0, 0xff, 0xffff, 0xffffff };
	static const u32 SWR_SHIFT[4] = { 0, 8, 16, 24 };
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);

	psxMemWrite32(addr & ~3,  (_u32(_rRt_) << SWR_SHIFT[shift]) |
			     (  mem & SWR_MASK[shift]) );

	/*
	Mem = 1234.  Reg = abcd

	0   abcd   (reg      ) | (mem & 0x00000000)
	1   bcd4   (reg <<  8) | (mem & 0x000000ff)
	2   cd34   (reg << 16) | (mem & 0x0000ffff)
	3   d234   (reg << 24) | (mem & 0x00ffffff)
	*/
}

/*********************************************************
* Moves between GPR and COPx                             *
* Format:  OP rt, fs                                     *
*********************************************************/
OP(psxMFC0) { if (!_Rt_) return; _rRt_ = _rFs_; }
OP(psxCFC0) { if (!_Rt_) return; _rRt_ = _rFs_; }

void MTC0(psxRegisters *regs_, int reg, u32 val) {
//	SysPrintf("MTC0 %d: %x\n", reg, val);
	switch (reg) {
		case 12: // Status
			regs_->CP0.r[12] = val;
			psxTestSWInts(regs_);
			break;

		case 13: // Cause
			regs_->CP0.n.Cause &= ~0x0300;
			regs_->CP0.n.Cause |= val & 0x0300;
			psxTestSWInts(regs_);
			break;

		default:
			regs_->CP0.r[reg] = val;
			break;
	}
}

OP(psxMTC0) { MTC0(regs_, _Rd_, _u32(_rRt_)); }
OP(psxCTC0) { MTC0(regs_, _Rd_, _u32(_rRt_)); }

/*********************************************************
* Unknow instruction (would generate an exception)       *
* Format:  ?                                             *
*********************************************************/
static inline void psxNULL_(void) {
#ifdef PSXCPU_LOG
	PSXCPU_LOG("psx: Unimplemented op %x\n", psxRegs.code);
#endif
}

OP(psxNULL) { psxNULL_(); }
void gteNULL(struct psxCP2Regs *regs) { psxNULL_(); }

OP(psxSPECIAL) {
	psxSPC[_Funct_](regs_, code);
}

OP(psxCOP0) {
	switch (_Rs_) {
		case 0x00: psxMFC0(regs_, code); break;
		case 0x02: psxCFC0(regs_, code); break;
		case 0x04: psxMTC0(regs_, code); break;
		case 0x06: psxCTC0(regs_, code); break;
		case 0x10: psxRFE(regs_, code);  break;
		default:   psxNULL_();           break;
	}
}

OP(psxCOP2) {
	psxCP2[_Funct_](&regs_->CP2);
}

OP(psxCOP2_stall) {
	u32 f = _Funct_;
	gteCheckStall(f);
	psxCP2[f](&regs_->CP2);
}

OP(gteMFC2) {
	if (!_Rt_) return;
	regs_->GPR.r[_Rt_] = MFC2(&regs_->CP2, _Rd_);
}

OP(gteCFC2) {
	if (!_Rt_) return;
	regs_->GPR.r[_Rt_] = regs_->CP2C.r[_Rd_];
}

OP(gteMTC2) {
	MTC2(&regs_->CP2, regs_->GPR.r[_Rt_], _Rd_);
}

OP(gteCTC2) {
	CTC2(&regs_->CP2, regs_->GPR.r[_Rt_], _Rd_);
}

OP(gteLWC2) {
	MTC2(&regs_->CP2, psxMemRead32(_oB_), _Rt_);
}

OP(gteSWC2) {
	psxMemWrite32(_oB_, MFC2(&regs_->CP2, _Rt_));
}

OP(gteLWC2_stall) {
	gteCheckStall(0);
	gteLWC2(regs_, code);
}

OP(gteSWC2_stall) {
	gteCheckStall(0);
	gteSWC2(regs_, code);
}

static void psxBASIC(struct psxCP2Regs *cp2regs) {
	psxRegisters *regs_ = (void *)((char *)cp2regs - offsetof(psxRegisters, CP2));
	u32 code = regs_->code;
	assert(regs_ == &psxRegs);
	switch (_Rs_) {
		case 0x00: gteMFC2(regs_, code); break;
		case 0x02: gteCFC2(regs_, code); break;
		case 0x04: gteMTC2(regs_, code); break;
		case 0x06: gteCTC2(regs_, code); break;
		default:   psxNULL_();           break;
	}
}

OP(psxREGIMM) {
	switch (_Rt_) {
		case 0x00: psxBLTZ(regs_, code);   break;
		case 0x01: psxBGEZ(regs_, code);   break;
		case 0x10: psxBLTZAL(regs_, code); break;
		case 0x11: psxBGEZAL(regs_, code); break;
		default:   psxNULL_();             break;
	}
}

OP(psxHLE) {
    uint32_t hleCode = code & 0x03ffffff;
    if (hleCode >= (sizeof(psxHLEt) / sizeof(psxHLEt[0]))) {
        psxNULL_();
    } else {
        psxHLEt[hleCode]();
    }
}

static void (INT_ATTR *psxBSC[64])(psxRegisters *regs_, u32 code) = {
	psxSPECIAL, psxREGIMM, psxJ   , psxJAL  , psxBEQ , psxBNE , psxBLEZ, psxBGTZ,
	psxADDI   , psxADDIU , psxSLTI, psxSLTIU, psxANDI, psxORI , psxXORI, psxLUI ,
	psxCOP0   , psxNULL  , psxCOP2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL   , psxNULL  , psxNULL, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxLB     , psxLH    , psxLWL , psxLW   , psxLBU , psxLHU , psxLWR , psxNULL,
	psxSB     , psxSH    , psxSWL , psxSW   , psxNULL, psxNULL, psxSWR , psxNULL, 
	psxNULL   , psxNULL  , gteLWC2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL   , psxNULL  , gteSWC2, psxHLE  , psxNULL, psxNULL, psxNULL, psxNULL 
};

static void (INT_ATTR *psxSPC[64])(psxRegisters *regs_, u32 code) = {
	psxSLL , psxNULL , psxSRL , psxSRA , psxSLLV   , psxNULL , psxSRLV, psxSRAV,
	psxJR  , psxJALR , psxNULL, psxNULL, psxSYSCALL, psxBREAK, psxNULL, psxNULL,
	psxMFHI, psxMTHI , psxMFLO, psxMTLO, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxMULT, psxMULTU, psxDIV , psxDIVU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxADD , psxADDU , psxSUB , psxSUBU, psxAND    , psxOR   , psxXOR , psxNOR ,
	psxNULL, psxNULL , psxSLT , psxSLTU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL
};

void (*psxCP2[64])(struct psxCP2Regs *regs) = {
	psxBASIC, gteRTPS , gteNULL , gteNULL, gteNULL, gteNULL , gteNCLIP, gteNULL, // 00
	gteNULL , gteNULL , gteNULL , gteNULL, gteOP  , gteNULL , gteNULL , gteNULL, // 08
	gteDPCS , gteINTPL, gteMVMVA, gteNCDS, gteCDP , gteNULL , gteNCDT , gteNULL, // 10
	gteNULL , gteNULL , gteNULL , gteNCCS, gteCC  , gteNULL , gteNCS  , gteNULL, // 18
	gteNCT  , gteNULL , gteNULL , gteNULL, gteNULL, gteNULL , gteNULL , gteNULL, // 20
	gteSQR  , gteDCPL , gteDPCT , gteNULL, gteNULL, gteAVSZ3, gteAVSZ4, gteNULL, // 28
	gteRTPT , gteNULL , gteNULL , gteNULL, gteNULL, gteNULL , gteNULL , gteNULL, // 30
	gteNULL , gteNULL , gteNULL , gteNULL, gteNULL, gteGPF  , gteGPL  , gteNCCT  // 38
};

///////////////////////////////////////////

static int intInit() {
	return 0;
}

static void intReset() {
	memset(&ICache, 0xff, sizeof(ICache));
}

static inline void execI_(u8 **memRLUT, psxRegisters *regs_) {
	regs_->code = fetch(memRLUT, regs_->pc);

	debugI();

	if (Config.Debug) ProcessDebug();

	regs_->pc += 4;
	addCycle();

	psxBSC[regs_->code >> 26](regs_, regs_->code);
}

static void intExecute() {
	psxRegisters *regs_ = &psxRegs;
	u8 **memRLUT = psxMemRLUT;
	extern int stop;

	while (!stop)
		execI_(memRLUT, regs_);
}

static void intExecuteBlock() {
	psxRegisters *regs_ = &psxRegs;
	u8 **memRLUT = psxMemRLUT;

	branch2 = 0;
	while (!branch2)
		execI_(memRLUT, regs_);
}

static void intClear(u32 Addr, u32 Size) {
}

void intNotify (int note, void *data) {
	/* Gameblabla - Only clear the icache if it's isolated */
	if (note == R3000ACPU_NOTIFY_CACHE_ISOLATED)
	{
		memset(&ICache, 0xff, sizeof(ICache));
	}
}

void intApplyConfig() {
	int cycle_mult;

	assert(psxBSC[18] == psxCOP2  || psxBSC[18] == psxCOP2_stall);
	assert(psxBSC[50] == gteLWC2  || psxBSC[50] == gteLWC2_stall);
	assert(psxBSC[58] == gteSWC2  || psxBSC[58] == gteSWC2_stall);
	assert(psxSPC[16] == psxMFHI  || psxSPC[16] == psxMFHI_stall);
	assert(psxSPC[18] == psxMFLO  || psxSPC[18] == psxMFLO_stall);
	assert(psxSPC[24] == psxMULT  || psxSPC[24] == psxMULT_stall);
	assert(psxSPC[25] == psxMULTU || psxSPC[25] == psxMULTU_stall);
	assert(psxSPC[26] == psxDIV   || psxSPC[26] == psxDIV_stall);
	assert(psxSPC[27] == psxDIVU  || psxSPC[27] == psxDIVU_stall);

	if (Config.DisableStalls) {
		psxBSC[18] = psxCOP2;
		psxBSC[50] = gteLWC2;
		psxBSC[58] = gteSWC2;
		psxSPC[16] = psxMFHI;
		psxSPC[18] = psxMFLO;
		psxSPC[24] = psxMULT;
		psxSPC[25] = psxMULTU;
		psxSPC[26] = psxDIV;
		psxSPC[27] = psxDIVU;
	} else {
		psxBSC[18] = psxCOP2_stall;
		psxBSC[50] = gteLWC2_stall;
		psxBSC[58] = gteSWC2_stall;
		psxSPC[16] = psxMFHI_stall;
		psxSPC[18] = psxMFLO_stall;
		psxSPC[24] = psxMULT_stall;
		psxSPC[25] = psxMULTU_stall;
		psxSPC[26] = psxDIV_stall;
		psxSPC[27] = psxDIVU_stall;
	}

	// dynarec may occasionally call the interpreter, in such a case the
	// cache won't work (cache only works right if all fetches go through it)
	if (!Config.icache_emulation || psxCpu != &psxInt)
		fetch = fetchNoCache;
	else
		fetch = fetchICache;

	cycle_mult = Config.cycle_multiplier_override && Config.cycle_multiplier == CYCLE_MULT_DEFAULT
		? Config.cycle_multiplier_override : Config.cycle_multiplier;
	psxRegs.subCycleStep = 0x10000 * cycle_mult / 100;
}

static void intShutdown() {
}

// single step (may do several ops in case of a branch)
void execI() {
	execI_(psxMemRLUT, &psxRegs);
}

R3000Acpu psxInt = {
	intInit,
	intReset,
	intExecute,
	intExecuteBlock,
	intClear,
	intNotify,
	intApplyConfig,
	intShutdown
};

/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *   Copyright (C) 2023 notaz                                              *
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
#include "../include/compiler_features.h"

// these may cause issues: because of poor timing we may step
// on instructions that real hardware would never reach
#define DO_EXCEPTION_RESERVEDI
#define DO_EXCEPTION_ALIGNMENT_BRANCH
//#define DO_EXCEPTION_ALIGNMENT_DATA
#define HANDLE_LOAD_DELAY

static int branch = 0;
static int branch2 = 0;

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

// load delay
static void doLoad(psxRegisters *regs, u32 r, u32 val)
{
#ifdef HANDLE_LOAD_DELAY
	int sel = regs->dloadSel ^ 1;
	assert(regs->dloadReg[sel] == 0);
	regs->dloadReg[sel] = r;
	regs->dloadVal[sel] = r ? val : 0;
	if (regs->dloadReg[sel ^ 1] == r)
		regs->dloadVal[sel ^ 1] = regs->dloadReg[sel ^ 1] = 0;
#else
	regs->GPR.r[r] = r ? val : 0;
#endif
}

static void dloadRt(psxRegisters *regs, u32 r, u32 val)
{
#ifdef HANDLE_LOAD_DELAY
	int sel = regs->dloadSel;
	if (unlikely(regs->dloadReg[sel] == r))
		regs->dloadVal[sel] = regs->dloadReg[sel] = 0;
#endif
	regs->GPR.r[r] = r ? val : 0;
}

static void dloadStep(psxRegisters *regs)
{
#ifdef HANDLE_LOAD_DELAY
	int sel = regs->dloadSel;
	regs->GPR.r[regs->dloadReg[sel]] = regs->dloadVal[sel];
	regs->dloadVal[sel] = regs->dloadReg[sel] = 0;
	regs->dloadSel ^= 1;
	assert(regs->GPR.r[0] == 0);
#endif
}

static void dloadFlush(psxRegisters *regs)
{
#ifdef HANDLE_LOAD_DELAY
	regs->GPR.r[regs->dloadReg[0]] = regs->dloadVal[0];
	regs->GPR.r[regs->dloadReg[1]] = regs->dloadVal[1];
	regs->dloadVal[0] = regs->dloadVal[1] = 0;
	regs->dloadReg[0] = regs->dloadReg[1] = 0;
	assert(regs->GPR.r[0] == 0);
#endif
}

static void dloadClear(psxRegisters *regs)
{
#ifdef HANDLE_LOAD_DELAY
	regs->dloadVal[0] = regs->dloadVal[1] = 0;
	regs->dloadReg[0] = regs->dloadReg[1] = 0;
	regs->dloadSel = 0;
#endif
}

static void intException(psxRegisters *regs, u32 pc, u32 cause)
{
	dloadFlush(regs);
	regs->pc = pc;
	psxException(cause, branch, &regs->CP0);
}

// get an opcode without triggering exceptions or affecting cache
u32 intFakeFetch(u32 pc)
{
	u8 *base = psxMemRLUT[pc >> 16];
	u32 *code;
	if (unlikely(base == INVALID_PTR))
		return 0; // nop
	code = (u32 *)(base + (pc & 0xfffc));
	return SWAP32(*code);

}

static u32 INT_ATTR fetchNoCache(psxRegisters *regs, u8 **memRLUT, u32 pc)
{
	u8 *base = memRLUT[pc >> 16];
	u32 *code;
	if (unlikely(base == INVALID_PTR)) {
		SysPrintf("game crash @%08x, ra=%08x\n", pc, regs->GPR.n.ra);
		intException(regs, pc, R3000E_IBE << 2);
		return 0; // execute as nop
	}
	code = (u32 *)(base + (pc & 0xfffc));
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

static u32 INT_ATTR fetchICache(psxRegisters *regs, u8 **memRLUT, u32 pc)
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
			if (unlikely(base == INVALID_PTR)) {
				SysPrintf("game crash @%08x, ra=%08x\n", pc, regs->GPR.n.ra);
				intException(regs, pc, R3000E_IBE << 2);
				return 0; // execute as nop
			}
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

	return fetchNoCache(regs, memRLUT, pc);
}

static u32 (INT_ATTR *fetch)(psxRegisters *regs_, u8 **memRLUT, u32 pc) = fetchNoCache;

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
#define _rSa_   regs_->GPR.r[_Sa_]   // Sa register

#define _rHi_   regs_->GPR.n.hi   // The HI register
#define _rLo_   regs_->GPR.n.lo   // The LO register

#define _JumpTarget_    ((_Target_ * 4) + (_PC_ & 0xf0000000))   // Calculates the target during a jump instruction
#define _BranchTarget_  ((s16)_Im_ * 4 + _PC_)                 // Calculates the target during a branch instruction

#define _SetLink(x)     dloadRt(regs_, x, _PC_ + 4);       // Sets the return address in the link register

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

#define isBranch(c_) \
	((1 <= ((c_) >> 26) && ((c_) >> 26) <= 7) || ((c_) & 0xfc00003e) == 8)
#define swap_(a_, b_) { u32 t_ = a_; a_ = b_; b_ = t_; }

// tar1 is main branch target, 'code' is opcode in DS
static u32 psxBranchNoDelay(psxRegisters *regs_, u32 tar1, u32 code, int *taken) {
	u32 temp, rt;

	assert(isBranch(code));
	*taken = 1;
	switch (code >> 26) {
		case 0x00: // SPECIAL
			switch (_Funct_) {
				case 0x08: // JR
					return _u32(_rRs_);
				case 0x09: // JALR
					temp = _u32(_rRs_);
					if (_Rd_)
						regs_->GPR.r[_Rd_] = tar1 + 4;
					return temp;
			}
			break;
		case 0x01: // REGIMM
			rt = _Rt_;
			switch (rt) {
				case 0x10: // BLTZAL
					regs_->GPR.n.ra = tar1 + 4;
					if (_i32(_rRs_) < 0)
						return tar1 + (s16)_Im_ * 4;
					break;
				case 0x11: // BGEZAL
					regs_->GPR.n.ra = tar1 + 4;
					if (_i32(_rRs_) >= 0)
						return tar1 + (s16)_Im_ * 4;
					break;
				default:
					if (rt & 1) { // BGEZ
						if (_i32(_rRs_) >= 0)
							return tar1 + (s16)_Im_ * 4;
					}
					else {        // BLTZ
						if (_i32(_rRs_) < 0)
							return tar1 + (s16)_Im_ * 4;
					}
					break;
			}
			break;
		case 0x02: // J
			return (tar1 & 0xf0000000u) + _Target_ * 4;
		case 0x03: // JAL
			regs_->GPR.n.ra = tar1 + 4;
			return (tar1 & 0xf0000000u) + _Target_ * 4;
		case 0x04: // BEQ
			if (_i32(_rRs_) == _i32(_rRt_))
				return tar1 + (s16)_Im_ * 4;
			break;
		case 0x05: // BNE
			if (_i32(_rRs_) != _i32(_rRt_))
				return tar1 + (s16)_Im_ * 4;
			break;
		case 0x06: // BLEZ
			if (_i32(_rRs_) <= 0)
				return tar1 + (s16)_Im_ * 4;
			break;
		case 0x07: // BGTZ
			if (_i32(_rRs_) > 0)
				return tar1 + (s16)_Im_ * 4;
			break;
	}

	*taken = 0;
	return tar1;
}

static void psxDoDelayBranch(psxRegisters *regs, u32 tar1, u32 code1) {
	u32 tar2, code;
	int taken, lim;

	tar2 = psxBranchNoDelay(regs, tar1, code1, &taken);
	regs->pc = tar1;
	if (!taken)
		return;

	/*
	 * taken branch in delay slot:
	 * - execute 1 instruction at tar1
	 * - jump to tar2 (target of branch in delay slot; this branch
	 *   has no normal delay slot, instruction at tar1 was fetched instead)
	 */
	for (lim = 0; lim < 8; lim++) {
		regs->code = code = fetch(regs, psxMemRLUT, tar1);
		addCycle();
		if (likely(!isBranch(code))) {
			dloadStep(regs);
			psxBSC[code >> 26](regs, code);
			regs->pc = tar2;
			return;
		}
		tar1 = psxBranchNoDelay(regs, tar2, code, &taken);
		regs->pc = tar2;
		if (!taken)
			return;
		swap_(tar1, tar2);
	}
	SysPrintf("Evil chained DS branches @ %08x %08x %08x\n", regs->pc, tar1, tar2);
}

static void doBranch(psxRegisters *regs, u32 tar) {
	u32 code, pc;

	branch2 = branch = 1;

	// fetch the delay slot
	pc = regs->pc;
	regs->pc = pc + 4;
	regs->code = code = fetch(regs, psxMemRLUT, pc);

	addCycle();

	// check for branch in delay slot
	if (unlikely(isBranch(code))) {
		psxDoDelayBranch(regs, tar, code);
		log_unhandled("branch in DS: %08x->%08x\n", pc, regs->pc);
		branch = 0;
		psxBranchTest();
		return;
	}

	dloadStep(regs);
	psxBSC[code >> 26](regs, code);

	branch = 0;
	regs->pc = tar;

	psxBranchTest();
}

static void doBranchReg(psxRegisters *regs, u32 tar) {
#ifdef DO_EXCEPTION_ALIGNMENT_BRANCH
	if (unlikely(tar & 3)) {
		SysPrintf("game crash @%08x, ra=%08x\n", tar, regs->GPR.n.ra);
		psxRegs.CP0.n.BadVAddr = tar;
		intException(regs, tar, R3000E_AdEL << 2);
		return;
	}
#else
	tar &= ~3;
#endif
	doBranch(regs, tar);
}

#if __has_builtin(__builtin_add_overflow) || (defined(__GNUC__) && __GNUC__ >= 5)
#define add_overflow(a, b, r) __builtin_add_overflow(a, b, &(r))
#define sub_overflow(a, b, r) __builtin_sub_overflow(a, b, &(r))
#else
#define add_overflow(a, b, r) ({r = (u32)a + (u32)b; (a ^ ~b) & (a ^ r) & (1u<<31);})
#define sub_overflow(a, b, r) ({r = (u32)a - (u32)b; (a ^  b) & (a ^ r) & (1u<<31);})
#endif

static void addExc(psxRegisters *regs, u32 rt, s32 a1, s32 a2) {
	s32 val;
	if (add_overflow(a1, a2, val)) {
		//printf("ov %08x + %08x = %08x\n", a1, a2, val);
		intException(regs, regs->pc - 4, R3000E_Ov << 2);
		return;
	}
	dloadRt(regs, rt, val);
}

static void subExc(psxRegisters *regs, u32 rt, s32 a1, s32 a2) {
	s32 val;
	if (sub_overflow(a1, a2, val)) {
		intException(regs, regs->pc - 4, R3000E_Ov << 2);
		return;
	}
	dloadRt(regs, rt, val);
}

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
OP(psxADDI)  { addExc (regs_, _Rt_, _i32(_rRs_), _Imm_); } // Rt = Rs + Im (Exception on Integer Overflow)
OP(psxADDIU) { dloadRt(regs_, _Rt_, _u32(_rRs_) + _Imm_ ); }  // Rt = Rs + Im
OP(psxANDI)  { dloadRt(regs_, _Rt_, _u32(_rRs_) & _ImmU_); }  // Rt = Rs And Im
OP(psxORI)   { dloadRt(regs_, _Rt_, _u32(_rRs_) | _ImmU_); }  // Rt = Rs Or  Im
OP(psxXORI)  { dloadRt(regs_, _Rt_, _u32(_rRs_) ^ _ImmU_); }  // Rt = Rs Xor Im
OP(psxSLTI)  { dloadRt(regs_, _Rt_, _i32(_rRs_) < _Imm_ ); }  // Rt = Rs < Im (Signed)
OP(psxSLTIU) { dloadRt(regs_, _Rt_, _u32(_rRs_) < ((u32)_Imm_)); } // Rt = Rs < Im (Unsigned)

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
OP(psxADD)   { addExc (regs_, _Rd_, _i32(_rRs_), _i32(_rRt_)); } // Rd = Rs + Rt (Exception on Integer Overflow)
OP(psxSUB)   { subExc (regs_, _Rd_, _i32(_rRs_), _i32(_rRt_)); } // Rd = Rs - Rt (Exception on Integer Overflow)
OP(psxADDU)  { dloadRt(regs_, _Rd_, _u32(_rRs_) + _u32(_rRt_)); }  // Rd = Rs + Rt
OP(psxSUBU)  { dloadRt(regs_, _Rd_, _u32(_rRs_) - _u32(_rRt_)); }  // Rd = Rs - Rt
OP(psxAND)   { dloadRt(regs_, _Rd_, _u32(_rRs_) & _u32(_rRt_)); }  // Rd = Rs And Rt
OP(psxOR)    { dloadRt(regs_, _Rd_, _u32(_rRs_) | _u32(_rRt_)); }  // Rd = Rs Or  Rt
OP(psxXOR)   { dloadRt(regs_, _Rd_, _u32(_rRs_) ^ _u32(_rRt_)); }  // Rd = Rs Xor Rt
OP(psxNOR)   { dloadRt(regs_, _Rd_, ~_u32(_rRs_ | _u32(_rRt_))); } // Rd = Rs Nor Rt
OP(psxSLT)   { dloadRt(regs_, _Rd_, _i32(_rRs_) < _i32(_rRt_)); }  // Rd = Rs < Rt (Signed)
OP(psxSLTU)  { dloadRt(regs_, _Rd_, _u32(_rRs_) < _u32(_rRt_)); }  // Rd = Rs < Rt (Unsigned)

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
#define RepZBranchi32(op) \
	if(_i32(_rRs_) op 0) \
		doBranch(regs_, _BranchTarget_);
#define RepZBranchLinki32(op)  { \
	s32 temp = _i32(_rRs_); \
	_SetLink(31); \
	if(temp op 0) \
		doBranch(regs_, _BranchTarget_); \
}

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
OP(psxSLL) { dloadRt(regs_, _Rd_, _u32(_rRt_) << _Sa_); } // Rd = Rt << sa
OP(psxSRA) { dloadRt(regs_, _Rd_, _i32(_rRt_) >> _Sa_); } // Rd = Rt >> sa (arithmetic)
OP(psxSRL) { dloadRt(regs_, _Rd_, _u32(_rRt_) >> _Sa_); } // Rd = Rt >> sa (logical)

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/
OP(psxSLLV) { dloadRt(regs_, _Rd_, _u32(_rRt_) << (_u32(_rRs_) & 0x1F)); } // Rd = Rt << rs
OP(psxSRAV) { dloadRt(regs_, _Rd_, _i32(_rRt_) >> (_u32(_rRs_) & 0x1F)); } // Rd = Rt >> rs (arithmetic)
OP(psxSRLV) { dloadRt(regs_, _Rd_, _u32(_rRt_) >> (_u32(_rRs_) & 0x1F)); } // Rd = Rt >> rs (logical)

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
OP(psxLUI) { dloadRt(regs_, _Rt_, code << 16); } // Upper halfword of Rt = Im

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
OP(psxMFHI) { dloadRt(regs_, _Rd_, _rHi_); } // Rd = Hi
OP(psxMFLO) { dloadRt(regs_, _Rd_, _rLo_); } // Rd = Lo

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
	intException(regs_, regs_->pc - 4, R3000E_Bp << 2);
}

OP(psxSYSCALL) {
	intException(regs_, regs_->pc - 4, R3000E_Syscall << 2);
}

static inline void execI_(u8 **memRLUT, psxRegisters *regs_);

static inline void psxTestSWInts(psxRegisters *regs_, int step) {
	if (regs_->CP0.n.Cause & regs_->CP0.n.Status & 0x0300 &&
	   regs_->CP0.n.Status & 0x1) {
		if (step)
			execI_(psxMemRLUT, regs_);
		regs_->CP0.n.Cause &= ~0x7c;
		intException(regs_, regs_->pc, regs_->CP0.n.Cause);
	}
}

OP(psxRFE) {
//	SysPrintf("psxRFE\n");
	regs_->CP0.n.Status = (regs_->CP0.n.Status & 0xfffffff0) |
	                      ((regs_->CP0.n.Status & 0x3c) >> 2);
	psxTestSWInts(regs_, 0);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op) { \
	if (_i32(_rRs_) op _i32(_rRt_)) \
		doBranch(regs_, _BranchTarget_); \
}

OP(psxBEQ) { RepBranchi32(==) }  // Branch if Rs == Rt
OP(psxBNE) { RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
OP(psxJ)   {               doBranch(regs_, _JumpTarget_); }
OP(psxJAL) { _SetLink(31); doBranch(regs_, _JumpTarget_); }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
OP(psxJR) {
	doBranchReg(regs_, _rRs_);
	psxJumpTest();
}

OP(psxJALR) {
	u32 temp = _u32(_rRs_);
	if (_Rd_) { _SetLink(_Rd_); }
	doBranchReg(regs_, temp);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

static int algnChkL(psxRegisters *regs, u32 addr, u32 m) {
	if (unlikely(addr & m)) {
		log_unhandled("unaligned load %08x @%08x\n", addr, regs->pc - 4);
#ifdef DO_EXCEPTION_ALIGNMENT_DATA
		psxRegs.CP0.n.BadVAddr = addr;
		intException(regs, regs->pc - 4, R3000E_AdEL << 2);
		return 0;
#endif
	}
	return 1;
}

static int algnChkS(psxRegisters *regs, u32 addr, u32 m) {
	if (unlikely(addr & m)) {
		log_unhandled("unaligned store %08x @%08x\n", addr, regs->pc - 4);
#ifdef DO_EXCEPTION_ALIGNMENT_DATA
		psxRegs.CP0.n.BadVAddr = addr;
		intException(regs, regs->pc - 4, R3000E_AdES << 2);
		return 0;
#endif
	}
	return 1;
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define _oB_ (regs_->GPR.r[_Rs_] + _Imm_)

OP(psxLB)  {                               doLoad(regs_, _Rt_,  (s8)psxMemRead8(_oB_));  }
OP(psxLBU) {                               doLoad(regs_, _Rt_,      psxMemRead8(_oB_));  }
OP(psxLH)  { if (algnChkL(regs_, _oB_, 1)) doLoad(regs_, _Rt_, (s16)psxMemRead16(_oB_)); }
OP(psxLHU) { if (algnChkL(regs_, _oB_, 1)) doLoad(regs_, _Rt_,      psxMemRead16(_oB_)); }
OP(psxLW)  { if (algnChkL(regs_, _oB_, 3)) doLoad(regs_, _Rt_,      psxMemRead32(_oB_)); }

OP(psxLWL) {
	static const u32 LWL_MASK[4] = { 0xffffff, 0xffff, 0xff, 0 };
	static const u32 LWL_SHIFT[4] = { 24, 16, 8, 0 };
	u32 addr = _oB_, val;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);
	u32 rt = _Rt_;
	u32 oldval = regs_->GPR.r[rt];

#ifdef HANDLE_LOAD_DELAY
	int sel = regs_->dloadSel;
	if (regs_->dloadReg[sel] == rt)
		oldval = regs_->dloadVal[sel];
#endif
	val = (oldval & LWL_MASK[shift]) | (mem << LWL_SHIFT[shift]);
	doLoad(regs_, rt, val);

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
	u32 addr = _oB_, val;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);
	u32 rt = _Rt_;
	u32 oldval = regs_->GPR.r[rt];

#ifdef HANDLE_LOAD_DELAY
	int sel = regs_->dloadSel;
	if (regs_->dloadReg[sel] == rt)
		oldval = regs_->dloadVal[sel];
#endif
	val = (oldval & LWR_MASK[shift]) | (mem >> LWR_SHIFT[shift]);
	doLoad(regs_, rt, val);

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)
	*/
}

OP(psxSB) {                               psxMemWrite8 (_oB_, _rRt_ &   0xff); }
OP(psxSH) { if (algnChkS(regs_, _oB_, 1)) psxMemWrite16(_oB_, _rRt_ & 0xffff); }
OP(psxSW) { if (algnChkS(regs_, _oB_, 3)) psxMemWrite32(_oB_, _rRt_); }

// FIXME: this rmw implementation is wrong and would break on io like fifos
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
OP(psxMFC0) {
	u32 r = _Rd_;
#ifdef DO_EXCEPTION_RESERVEDI
	if (unlikely(r == 0))
		intException(regs_, regs_->pc - 4, R3000E_RI << 2);
#endif
	doLoad(regs_, _Rt_, regs_->CP0.r[r]);
}

OP(psxCFC0) { doLoad(regs_, _Rt_, regs_->CP0.r[_Rd_]); }

static void setupCop(u32 sr);

void MTC0(psxRegisters *regs_, int reg, u32 val) {
//	SysPrintf("MTC0 %d: %x\n", reg, val);
	switch (reg) {
		case 12: // Status
			if (unlikely((regs_->CP0.n.Status ^ val) & (1 << 16)))
				psxMemOnIsolate((val >> 16) & 1);
			if (unlikely((regs_->CP0.n.Status ^ val) & (7 << 29)))
				setupCop(val);
			regs_->CP0.n.Status = val;
			psxTestSWInts(regs_, 1);
			break;

		case 13: // Cause
			regs_->CP0.n.Cause &= ~0x0300;
			regs_->CP0.n.Cause |= val & 0x0300;
			psxTestSWInts(regs_, 0);
			break;

		default:
			regs_->CP0.r[reg] = val;
			break;
	}
}

OP(psxMTC0) { MTC0(regs_, _Rd_, _u32(_rRt_)); }
OP(psxCTC0) { MTC0(regs_, _Rd_, _u32(_rRt_)); }

/*********************************************************
* Unknown instruction (would generate an exception)      *
* Format:  ?                                             *
*********************************************************/
static inline void psxNULL_(void) {
	//printf("op %08x @%08x\n", psxRegs.code, psxRegs.pc);
}

OP(psxNULL) {
	psxNULL_();
#ifdef DO_EXCEPTION_RESERVEDI
	intException(regs_, regs_->pc - 4, R3000E_RI << 2);
#endif
}

void gteNULL(struct psxCP2Regs *regs) {
	psxNULL_();
}

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

OP(psxLWC0) {
	// MTC0(regs_, _Rt_, psxMemRead32(_oB_)); // ?
	log_unhandled("LWC0 %08x\n", code);
}

OP(psxCOP1) {
	// ??? what actually happens here?
}

OP(psxCOP1d) {
#ifdef DO_EXCEPTION_RESERVEDI
	intException(regs_, regs_->pc - 4, (1<<28) | (R3000E_RI << 2));
#endif
}

OP(psxCOP2) {
	psxCP2[_Funct_](&regs_->CP2);
}

OP(psxCOP2_stall) {
	u32 f = _Funct_;
	gteCheckStall(f);
	psxCP2[f](&regs_->CP2);
}

OP(psxCOP2d) {
#ifdef DO_EXCEPTION_RESERVEDI
	intException(regs_, regs_->pc - 4, (2<<28) | (R3000E_RI << 2));
#endif
}

OP(gteMFC2) {
	doLoad(regs_, _Rt_, MFC2(&regs_->CP2, _Rd_));
}

OP(gteCFC2) {
	doLoad(regs_, _Rt_, regs_->CP2C.r[_Rd_]);
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

OP(psxCOP3) {
	// ??? what actually happens here?
}

OP(psxCOP3d) {
#ifdef DO_EXCEPTION_RESERVEDI
	intException(regs_, regs_->pc - 4, (3<<28) | (R3000E_RI << 2));
#endif
}

OP(psxLWCx) {
	// does this read memory?
	log_unhandled("LWCx %08x\n", code);
}

OP(psxSWCx) {
	// does this write something to memory?
	log_unhandled("SWCx %08x\n", code);
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
	u32 rt = _Rt_;
	switch (rt) {
		case 0x10: psxBLTZAL(regs_, code); break;
		case 0x11: psxBGEZAL(regs_, code); break;
		default:
			if (rt & 1)
				psxBGEZ(regs_, code);
			else
				psxBLTZ(regs_, code);
	}
}

OP(psxHLE) {
	u32 hleCode;
	if (unlikely(!Config.HLE)) {
		psxSWCx(regs_, code);
		return;
	}
	hleCode = code & 0x03ffffff;
	if (hleCode >= (sizeof(psxHLEt) / sizeof(psxHLEt[0]))) {
		psxSWCx(regs_, code);
		return;
	}
	psxHLEt[hleCode]();
}

static void (INT_ATTR *psxBSC[64])(psxRegisters *regs_, u32 code) = {
	psxSPECIAL, psxREGIMM, psxJ   , psxJAL  , psxBEQ , psxBNE , psxBLEZ, psxBGTZ,
	psxADDI   , psxADDIU , psxSLTI, psxSLTIU, psxANDI, psxORI , psxXORI, psxLUI ,
	psxCOP0   , psxCOP1d , psxCOP2, psxCOP3d, psxNULL, psxCOP1d,psxCOP2d,psxCOP3d,
	psxNULL   , psxCOP1d , psxCOP2d,psxCOP3d, psxNULL, psxCOP1d,psxCOP2d,psxCOP3d,
	psxLB     , psxLH    , psxLWL , psxLW   , psxLBU , psxLHU , psxLWR , psxCOP3d,
	psxSB     , psxSH    , psxSWL , psxSW   , psxNULL, psxCOP1d,psxSWR , psxCOP3d,
	psxLWC0   , psxLWCx  , gteLWC2, psxLWCx , psxNULL, psxCOP1d,psxCOP2d,psxCOP3d,
	psxSWCx   , psxSWCx  , gteSWC2, psxHLE  , psxNULL, psxCOP1d,psxCOP2d,psxCOP3d,
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
	dloadClear(&psxRegs);
}

static inline void execI_(u8 **memRLUT, psxRegisters *regs_) {
	u32 pc = regs_->pc;
	regs_->pc += 4;
	regs_->code = fetch(regs_, memRLUT, pc);

	addCycle();

	dloadStep(regs_);
	psxBSC[regs_->code >> 26](regs_, regs_->code);
}

static void intExecute() {
	psxRegisters *regs_ = &psxRegs;
	u8 **memRLUT = psxMemRLUT;
	extern int stop;

	while (!stop)
		execI_(memRLUT, regs_);
}

void intExecuteBlock(enum blockExecCaller caller) {
	psxRegisters *regs_ = &psxRegs;
	u8 **memRLUT = psxMemRLUT;

	branch2 = 0;
	while (!branch2)
		execI_(memRLUT, regs_);
}

static void intClear(u32 Addr, u32 Size) {
}

static void intNotify(enum R3000Anote note, void *data) {
	switch (note) {
	case R3000ACPU_NOTIFY_BEFORE_SAVE:
		dloadFlush(&psxRegs);
		break;
	case R3000ACPU_NOTIFY_AFTER_LOAD:
		dloadClear(&psxRegs);
		setupCop(psxRegs.CP0.n.Status);
		// fallthrough
	case R3000ACPU_NOTIFY_CACHE_ISOLATED: // Armored Core?
		memset(&ICache, 0xff, sizeof(ICache));
		break;
	case R3000ACPU_NOTIFY_CACHE_UNISOLATED:
		break;
	}
}

static void setupCop(u32 sr)
{
	if (sr & (1u << 29))
		psxBSC[17] = psxCOP1;
	else
		psxBSC[17] = psxCOP1d;
	if (sr & (1u << 30))
		psxBSC[18] = Config.DisableStalls ? psxCOP2 : psxCOP2_stall;
	else
		psxBSC[18] = psxCOP2d;
	if (sr & (1u << 31))
		psxBSC[19] = psxCOP3;
	else
		psxBSC[19] = psxCOP3d;
}

void intApplyConfig() {
	int cycle_mult;

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
	setupCop(psxRegs.CP0.n.Status);

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

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

/*
* GTE functions.
*/

#include "gte.h"
#include "gte_arm.h"
#include "psxmem.h"
#include "../include/compiler_features.h"
#include "../include/arm_features.h"

#ifndef GTE_LOG
#define GTE_LOG(...)
#endif

#ifdef FLAGLESS
#define NF _nf
#define NM(n) n##_nf
#else
#define NF
#define NM(n) n
#endif

#define VX(n) (n < 3 ? regs->CP2D.p[n << 1].sw.l : regs->CP2D.p[9].sw.l)
#define VY(n) (n < 3 ? regs->CP2D.p[n << 1].sw.h : regs->CP2D.p[10].sw.l)
#define VZ(n) (n < 3 ? regs->CP2D.p[(n << 1) + 1].sw.l : regs->CP2D.p[11].sw.l)

#define fSX(n) ((regs->CP2D.p)[((n) + 12)].sw.l)
#define fSY(n) ((regs->CP2D.p)[((n) + 12)].sw.h)
#define fSZ(n) ((regs->CP2D.p)[((n) + 17)].w.l) /* (n == 0) => SZ1; */

#define fR(x)    (regs->CP2D.p[(x) + 20].b.l)
#define fG(x)    (regs->CP2D.p[(x) + 20].b.h)
#define fB(x)    (regs->CP2D.p[(x) + 20].b.h2)
#define fCODE(x) (regs->CP2D.p[(x) + 20].b.h3)

#define gteVXY0 (regs->CP2D.r[0])
#define gteVX0  (regs->CP2D.p[0].sw.l)
#define gteVY0  (regs->CP2D.p[0].sw.h)
#define gteVZ0  (regs->CP2D.p[1].sw.l)
#define gteVXY1 (regs->CP2D.r[2])
#define gteVX1  (regs->CP2D.p[2].sw.l)
#define gteVY1  (regs->CP2D.p[2].sw.h)
#define gteVZ1  (regs->CP2D.p[3].sw.l)
#define gteVXY2 (regs->CP2D.r[4])
#define gteVX2  (regs->CP2D.p[4].sw.l)
#define gteVY2  (regs->CP2D.p[4].sw.h)
#define gteVZ2  (regs->CP2D.p[5].sw.l)
#define gteRGB  (regs->CP2D.r[6])
#define gteR    (regs->CP2D.p[6].b.l)
#define gteG    (regs->CP2D.p[6].b.h)
#define gteB    (regs->CP2D.p[6].b.h2)
#define gteCODE (regs->CP2D.p[6].b.h3)
#define gteOTZ  (regs->CP2D.p[7].w.l)
#define gteIR0  (regs->CP2D.p[8].sw.l)
#define gteIR1  (regs->CP2D.p[9].sw.l)
#define gteIR2  (regs->CP2D.p[10].sw.l)
#define gteIR3  (regs->CP2D.p[11].sw.l)
#define gteSXY0 (regs->CP2D.r[12])
#define gteSX0  (regs->CP2D.p[12].sw.l)
#define gteSY0  (regs->CP2D.p[12].sw.h)
#define gteSXY1 (regs->CP2D.r[13])
#define gteSX1  (regs->CP2D.p[13].sw.l)
#define gteSY1  (regs->CP2D.p[13].sw.h)
#define gteSXY2 (regs->CP2D.r[14])
#define gteSX2  (regs->CP2D.p[14].sw.l)
#define gteSY2  (regs->CP2D.p[14].sw.h)
#define gteSXYP (regs->CP2D.r[15])
#define gteSXP  (regs->CP2D.p[15].sw.l)
#define gteSYP  (regs->CP2D.p[15].sw.h)
#define gteSZ0  (regs->CP2D.p[16].w.l)
#define gteSZ1  (regs->CP2D.p[17].w.l)
#define gteSZ2  (regs->CP2D.p[18].w.l)
#define gteSZ3  (regs->CP2D.p[19].w.l)
#define gteRGB0  (regs->CP2D.r[20])
#define gteR0    (regs->CP2D.p[20].b.l)
#define gteG0    (regs->CP2D.p[20].b.h)
#define gteB0    (regs->CP2D.p[20].b.h2)
#define gteCODE0 (regs->CP2D.p[20].b.h3)
#define gteRGB1  (regs->CP2D.r[21])
#define gteR1    (regs->CP2D.p[21].b.l)
#define gteG1    (regs->CP2D.p[21].b.h)
#define gteB1    (regs->CP2D.p[21].b.h2)
#define gteCODE1 (regs->CP2D.p[21].b.h3)
#define gteRGB2  (regs->CP2D.r[22])
#define gteR2    (regs->CP2D.p[22].b.l)
#define gteG2    (regs->CP2D.p[22].b.h)
#define gteB2    (regs->CP2D.p[22].b.h2)
#define gteCODE2 (regs->CP2D.p[22].b.h3)
#define gteRES1  (regs->CP2D.r[23])
#define gteMAC0  (((s32 *)regs->CP2D.r)[24])
#define gteMAC1  (((s32 *)regs->CP2D.r)[25])
#define gteMAC2  (((s32 *)regs->CP2D.r)[26])
#define gteMAC3  (((s32 *)regs->CP2D.r)[27])
#define gteIRGB  (regs->CP2D.r[28])
#define gteORGB  (regs->CP2D.r[29])
#define gteLZCS  (regs->CP2D.r[30])
#define gteLZCR  (regs->CP2D.r[31])

#define gteR11R12 (((s32 *)regs->CP2C.r)[0])
#define gteR22R23 (((s32 *)regs->CP2C.r)[2])
#define gteR11 (regs->CP2C.p[0].sw.l)
#define gteR12 (regs->CP2C.p[0].sw.h)
#define gteR13 (regs->CP2C.p[1].sw.l)
#define gteR21 (regs->CP2C.p[1].sw.h)
#define gteR22 (regs->CP2C.p[2].sw.l)
#define gteR23 (regs->CP2C.p[2].sw.h)
#define gteR31 (regs->CP2C.p[3].sw.l)
#define gteR32 (regs->CP2C.p[3].sw.h)
#define gteR33 (regs->CP2C.p[4].sw.l)
#define gteTRX (((s32 *)regs->CP2C.r)[5])
#define gteTRY (((s32 *)regs->CP2C.r)[6])
#define gteTRZ (((s32 *)regs->CP2C.r)[7])
#define gteL11 (regs->CP2C.p[8].sw.l)
#define gteL12 (regs->CP2C.p[8].sw.h)
#define gteL13 (regs->CP2C.p[9].sw.l)
#define gteL21 (regs->CP2C.p[9].sw.h)
#define gteL22 (regs->CP2C.p[10].sw.l)
#define gteL23 (regs->CP2C.p[10].sw.h)
#define gteL31 (regs->CP2C.p[11].sw.l)
#define gteL32 (regs->CP2C.p[11].sw.h)
#define gteL33 (regs->CP2C.p[12].sw.l)
#define gteRBK (((s32 *)regs->CP2C.r)[13])
#define gteGBK (((s32 *)regs->CP2C.r)[14])
#define gteBBK (((s32 *)regs->CP2C.r)[15])
#define gteLR1 (regs->CP2C.p[16].sw.l)
#define gteLR2 (regs->CP2C.p[16].sw.h)
#define gteLR3 (regs->CP2C.p[17].sw.l)
#define gteLG1 (regs->CP2C.p[17].sw.h)
#define gteLG2 (regs->CP2C.p[18].sw.l)
#define gteLG3 (regs->CP2C.p[18].sw.h)
#define gteLB1 (regs->CP2C.p[19].sw.l)
#define gteLB2 (regs->CP2C.p[19].sw.h)
#define gteLB3 (regs->CP2C.p[20].sw.l)
#define gteRFC (((s32 *)regs->CP2C.r)[21])
#define gteGFC (((s32 *)regs->CP2C.r)[22])
#define gteBFC (((s32 *)regs->CP2C.r)[23])
#define gteOFX (((s32 *)regs->CP2C.r)[24])
#define gteOFY (((s32 *)regs->CP2C.r)[25])
// senquack - gteH register is u16, not s16, and used in GTE that way.
//  HOWEVER when read back by CPU using CFC2, it will be incorrectly
//  sign-extended by bug in original hardware, according to Nocash docs
//  GTE section 'Screen Offset and Distance'. The emulator does this
//  sign extension when it is loaded to GTE by CTC2.
//#define gteH   (regs->CP2C.p[26].sw.l)
#define gteH   (regs->CP2C.p[26].w.l)
#define gteDQA (regs->CP2C.p[27].sw.l)
#define gteDQB (((s32 *)regs->CP2C.r)[28])
#define gteZSF3 (regs->CP2C.p[29].sw.l)
#define gteZSF4 (regs->CP2C.p[30].sw.l)
#define gteFLAG (regs->CP2C.r[31])

#define GTE_SF(op) ((op >> 19) & 1)
#define GTE_MX(op) ((op >> 17) & 3)
#define GTE_V(op) ((op >> 15) & 3)
#define GTE_CV(op) ((op >> 13) & 3)
#define GTE_CD(op) ((op >> 11) & 3) /* not used */
#define GTE_LM(op) ((op >> 10) & 1)
#define GTE_CT(op) ((op >> 6) & 15) /* not used */

// shift the gte 44bit accumulator to 64bit
#define MAC123_SHIFT (32-12)

// mac 123 without flags (expensive to calculate, rarely used)
// if your platform is slow, consider adding it here
#if defined(FLAGLESS) || (defined(__arm__) && !defined(HAVE_ARMV5))

static inline s64 mac123add4(u32 id, u32 *flags, s32 a1, s32 a2, s32 a3, s32 a4, int shift) {
	return (((s64)a1 << 12) + a2 + a3 + a4) >> shift;
}

static inline s32 mac123add_s12(u32 id, u32 *flags, s32 in12, s32 addend, int shift) {
	return (((s64)in12 << 12) + addend) >> shift;
}

static inline s32 mac123sub_s12(u32 id, u32 *flags, s32 in12, s32 subtrahend, int shift) {
	return (((s64)in12 << 12) - subtrahend) >> shift;
}

#else

static inline s64 mac123add(u32 id, u32 *flags, s64 in, s32 addend) {
	s64 a;
#if defined(__arm__)
	u32 flag = 1u << (31 - id);
	asm("adds %Q[a], %Q[in], %[add], lsl #20\n"
	    "adcs %R[a], %R[in], %[add], asr #12\n"
	    "movpl %[flag], %[flag], lsr #3\n"
	    "orrvs %[flags], %[flags], %[flag]"
	    : [a]"=&r"(a), [flags]"+&r"(*flags), [flag]"+&r"(flag)
	    : [in]"r"(in), [add]"r"(addend)
	    : "cc");
#elif 0 // defined(__aarch64__) // slower
	u32 flag = 1u << (31 - id);
	u32 flagpl = 1u << (31 - id - 3);
	s64 add_ = addend;
	asm("adds %[a], %[in], %[add], lsl %[shift]\n"
	    "csel %w[flag], %w[flagpl], %w[flag], pl\n"
	    "csel %w[flag], %w[flag], wzr, vs\n"
	    : [a]"=&r"(a), [flag]"+&r"(flag)
	    : [in]"r"(in), [add]"r"(add_), [flagpl]"r"(flagpl), [shift]"i"(MAC123_SHIFT)
	    : "cc");
	*flags |= flag;
#else
	int o = __builtin_add_overflow(in, (s64)addend << MAC123_SHIFT, &a);
	*flags |= (o && a <  0) << (31 - id);
	*flags |= (o && a >= 0) << (28 - id);
#endif
	return a;
}

static inline s64 mac123add4(u32 id, u32 *flags, s32 a1, s32 a2, s32 a3, s32 a4, int shift) {
	s64 a = (s64)a1 << (12 + MAC123_SHIFT);
	a = mac123add(id, flags, a, a2);
	a = mac123add(id, flags, a, a3);
	a = mac123add(id, flags, a, a4);
	return a >> (shift + MAC123_SHIFT);
}

static inline s32 mac123add_s12(u32 id, u32 *flags, s32 in12, s32 addend, int shift) {
	return mac123add(id, flags, (s64)in12 << (12+MAC123_SHIFT), addend) >> (shift+MAC123_SHIFT);
}

static inline s64 mac123sub_s12(u32 id, u32 *flags, s32 in12, s32 subtrahend, int shift) {
	s64 a;
#if defined(__arm__)
	u32 flag = 1u << (31 - id);
	s64 in = (s64)in12 << (12+MAC123_SHIFT);
	asm("subs %Q[a], %Q[in], %[sub], lsl #20\n"
	    "sbcs %R[a], %R[in], %[sub], asr #12\n"
	    "movpl %[flag], %[flag], lsr #3\n"
	    "orrvs %[flags], %[flags], %[flag]"
	    : [a]"=&r"(a), [flags]"+&r"(*flags), [flag]"+&r"(flag)
	    : [in]"r"(in), [sub]"r"(subtrahend)
	    : "cc");
#elif 0 // defined(__aarch64__)
	u32 flag = 1u << (31 - id);
	u32 flagpl = 1u << (31 - id - 3);
	s64 in = (s64)in12 << (12+MAC123_SHIFT);
	s64 sub_ = subtrahend;
	asm("subs %[a], %[in], %[sub], lsl %[shift]\n"
	    "csel %w[flag], %w[flagpl], %w[flag], pl\n"
	    "csel %w[flag], %w[flag], wzr, vs\n"
	    : [a]"=&r"(a), [flag]"+&r"(flag)
	    : [in]"r"(in), [sub]"r"(sub_), [flagpl]"r"(flagpl), [shift]"i"(MAC123_SHIFT)
	    : "cc");
	*flags |= flag;
#else
	int o = __builtin_sub_overflow((s64)in12 << (12+MAC123_SHIFT), (s64)subtrahend << MAC123_SHIFT, &a);
	*flags |= (o && subtrahend <  0) << (31 - id);
	*flags |= (o && subtrahend >= 0) << (28 - id);
#endif
	return a >> (shift+MAC123_SHIFT);
}

#endif // !FLAGLESS for mac 123

#ifndef FLAGLESS

static inline s64 mac0flags(u32 *flags, s64 a) {
#if 1
	if (a != (s32)a)
		*flags |= 1u << (16 + (a >> 63));
#else
	if (a > 0x7fffffff)
		*flags |= 1u << 16;
	if (a < -(s64)0x80000000)
		*flags |= 1u << 15;
#endif
	return a;
}

#if defined(__arm__)

#define LIM(flags_, value_, max_, min_, flag_) \
({s32 r_ = value_; \
  asm("cmp   %[val], %[max]\n" \
      "movgt %[val], %[max]\n" \
      "orrgt %[flags], %[flag]\n" \
      "cmp   %[val], %[min]\n" \
      "movlt %[val], %[min]\n" \
      "orrlt %[flags], %[flag]\n" \
      : [val]"+&r"(r_), [flags]"+&r"(*(flags_)) \
      : [max]"r"(max_), [min]"r"(min_), [flag]"i"(flag_) \
      : "cc"); \
  r_;})

#elif defined(__aarch64__)

#define LIM(flags_, value_, max_, min_, flagc_) \
({s32 r_ = value_; \
  u32 flag_o_, flag_ = flagc_; \
  asm("cmp  %w[val], %w[max]\n" \
      "csel %w[val], %w[max], %w[val], gt\n" \
      "csel %w[flag_o], %w[flag], wzr, gt\n" \
      "cmp  %w[val], %w[min]\n" \
      "csel %w[val], %w[min], %w[val], lt\n" \
      "csel %w[flag_o], %w[flag], %w[flag_o], lt\n" \
      : [val]"+&r"(r_), [flag_o]"=&r"(flag_o_) \
      : [max]"r"(max_), [min]"r"(min_), [flag]"r"(flag_) \
      : "cc"); \
  *(flags_) |= flag_o_; \
  r_;})

#else

static inline s32 LIM(u32 *flags, s32 value, s32 max, s32 min, u32 flag) {
	s32 ret = value;
	if (ret > max)
		ret = max;
	if (ret < min)
		ret = min;
	if (ret != value)
		*flags |= flag;
	return ret;
}

#endif

static inline void LIMF(u32 *flags, s32 value, s32 max, s32 min, u32 flag) {
	if (value > max || value < min)
		*flags |= flag;
}

static inline u32 getFinalFlag(u32 flags) {
	flags |= ~((flags & 0x7f87e000u) - 1) & (1u << 31);
	return flags;
}

#else

static inline s64 mac0flags(u32 *flags, s64 a) {
	return a;
}

static inline s32 LIM(u32 *flags, s32 value, s32 max, s32 min, u32 flag) {
	s32 ret = value;
	if (ret > max)
		ret = max;
	if (ret < min)
		ret = min;
	return ret;
}

#define LIMF(flags, a, ...) (void)(a)

static inline u32 getFinalFlag(u32 flags) {
	return 0;
}

#endif

#define limB1(flags, a, l)   LIM(flags, a, 0x7fff, -0x8000 * !l, (1u << 24))
#define limB2(flags, a, l)   LIM(flags, a, 0x7fff, -0x8000 * !l, (1u << 23))
#define limB3(flags, a, l)   LIM(flags, a, 0x7fff, -0x8000 * !l, (1u << 22))
#define limBF1(flags, a, l) LIMF(flags, a, 0x7fff, -0x8000 * !l, (1u << 24))
#define limBF2(flags, a, l) LIMF(flags, a, 0x7fff, -0x8000 * !l, (1u << 23))
#define limBF3(flags, a, l) LIMF(flags, a, 0x7fff, -0x8000 * !l, (1u << 22))
#define limC1(flags, a) LIM(flags, a, 0x00ff, 0x0000, (1u << 21))
#define limC2(flags, a) LIM(flags, a, 0x00ff, 0x0000, (1u << 20))
#define limC3(flags, a) LIM(flags, a, 0x00ff, 0x0000, (1u << 19))
#define limD(flags, a)  LIM(flags, a, 0xffff, 0x0000, (1u << 18))
#define limG1(flags, a) LIM(flags, a,  0x3ff, -0x400, (1u << 14))
#define limG2(flags, a) LIM(flags, a,  0x3ff, -0x400, (1u << 13))
#define limH(flags, a)  LIM(flags, a, 0x1000, 0x0000, (1u << 12))

//senquack - n param should be unsigned (will be 'gteH' reg which is u16)
#ifdef GTE_USE_NATIVE_DIVIDE
INLINE u32 DIVIDE(u16 n, u16 d) {
	return ((u32)n << 16) / d;
}
#else
#include "gte_divider.h"
#endif // GTE_USE_NATIVE_DIVIDE

#ifndef FLAGLESS

const unsigned char gte_cycletab[64] = {
	/*   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
	 0, 15,  0,  0,  0,  0,  8,  0,  0,  0,  0,  0,  6,  0,  0,  0,
	 8,  8,  8, 19, 13,  0, 44,  0,  0,  0,  0, 17, 11,  0, 14,  0,
	30,  0,  0,  0,  0,  0,  0,  0,  5,  8, 17,  0,  0,  5,  6,  0,
	23,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5,  5, 39,
};

// warning: ari64 drc stores its negative cycles in gteBusyCycle
static int gteCheckStallRaw(u32 op_cycles, psxRegisters *regs) {
	u32 left = regs->gteBusyCycle - regs->cycle;
	int stall = 0;

	if (left <= 44) {
		//printf("c %2u stall %2u %u\n", op_cycles, left, regs->cycle);
		regs->cycle = regs->gteBusyCycle;
		stall = left;
	}
	regs->gteBusyCycle = regs->cycle + op_cycles;
	return stall;
}

void gteCheckStall(u32 op) {
	gteCheckStallRaw(gte_cycletab[op], &psxRegs);
}

static inline u32 ir2rgb(s32 ir)
{
	ir >>= 7;
	if (ir < 0)
		ir = 0;
	else if (ir > 0x1f)
		ir = 0x1f;
	return ir;
}

u32 MFC2(struct psxCP2Regs *regs, int reg) {
	switch (reg) {
		case 1:
		case 3:
		case 5:
		case 8:
		case 9:
		case 10:
		case 11:
			regs->CP2D.r[reg] = (s32)regs->CP2D.p[reg].sw.l;
			break;

		case 7:
		case 16:
		case 17:
		case 18:
		case 19:
			regs->CP2D.r[reg] = (u32)regs->CP2D.p[reg].w.l;
			break;

		case 15:
			regs->CP2D.r[reg] = gteSXY2;
			break;

		case 28:
		case 29:
			regs->CP2D.r[reg] = ir2rgb(gteIR1) | (ir2rgb(gteIR2) << 5) | (ir2rgb(gteIR3) << 10);
			break;
	}
	return regs->CP2D.r[reg];
}

static u32 lzc(s32 val)
{
#if __has_builtin(__builtin_clrsb)
	return 1 + __builtin_clrsb(val);
#else
	val ^= val >> 31;
	return val ? __builtin_clz(val) : 32;
#endif
}

void MTC2(struct psxCP2Regs *regs, u32 value, int reg) {
	switch (reg) {
		case 15:
			gteSXY0 = gteSXY1;
			gteSXY1 = gteSXY2;
			gteSXY2 = value;
			gteSXYP = value;
			break;

		case 28:
			gteIRGB = value;
			// not gteIR1 etc. just to be consistent with dynarec
			regs->CP2D.n.ir1 = (value & 0x1f) << 7;
			regs->CP2D.n.ir2 = (value & 0x3e0) << 2;
			regs->CP2D.n.ir3 = (value & 0x7c00) >> 3;
			break;

		case 30:
			gteLZCS = value;
			gteLZCR = lzc(value);
			break;

		case 31:
			return;

		default:
			regs->CP2D.r[reg] = value;
	}
}

void CTC2(struct psxCP2Regs *regs, u32 value, int reg) {
	switch (reg) {
		case 4:
		case 12:
		case 20:
		case 26:
		case 27:
		case 29:
		case 30:
			value = (s32)(s16)value;
			break;

		case 31:
			value = getFinalFlag(value & 0x7ffff000);
			break;
	}

	regs->CP2C.r[reg] = value;
}

#endif // FLAGLESS

#if 0
#define DIVIDE DIVIDE_
static u32 DIVIDE_(s16 n, u16 d) {
	s32 n_ = n;
	return ((n_ << 16) + d / 2) / d;
	//return (u32)((float)(n_ << 16) / (float)d + (float)0.5);
}
#endif

static inline s32 divide(u32 *flags, u16 h, u16 sz3)
{
	if (likely(h < sz3 * 2u)) {
		s32 r = DIVIDE(h, sz3);
		return r >= 0x1ffff ? 0x1ffff : r;
	}
#ifndef FLAGLESS
	*flags |= 1u << 17;
#endif
	return 0x1ffff;
}

#ifdef HAVE_ARMV5

#define gteMAC123f gteMAC123f_arm

#else

static u32 gteMAC123f(psxCP2Regs *regs, s32 vx, s32 vy, s32 vz,
	const s16 *mx, const s32 *cv, int shift)
{
	u32 flags = 0;

	gteMAC1 = mac123add4(1, &flags, cv[0], mx[0] * vx, mx[1] * vy, mx[2] * vz, shift);
	gteMAC2 = mac123add4(2, &flags, cv[1], mx[3] * vx, mx[4] * vy, mx[5] * vz, shift);
	gteMAC3 = mac123add4(3, &flags, cv[2], mx[6] * vx, mx[7] * vy, mx[8] * vz, shift);

	return flags;
}

#endif

static inline force_inline void gteRTPS(psxCP2Regs *regs, int shift, int lm)
{
	s32 vx = gteVX0, vy = gteVY0, vz = gteVZ0;
	s32 sz3, quotient;
	s32 mac1, mac2;
	s64 mac3, mac0;
	u32 flags = 0;

	GTE_LOG("GTE RTPS\n");

	gteMAC1 = mac1 = mac123add4(1, &flags, gteTRX, gteR11 * vx, gteR12 * vy, gteR13 * vz, shift);
	gteMAC2 = mac2 = mac123add4(2, &flags, gteTRY, gteR21 * vx, gteR22 * vy, gteR23 * vz, shift);
	gteMAC3 = mac3 = mac123add4(3, &flags, gteTRZ, gteR31 * vx, gteR32 * vy, gteR33 * vz, shift);
	gteIR1 = limB1(&flags, mac1, lm);
	gteIR2 = limB2(&flags, mac2, lm);
	gteIR3 = LIM(&flags, mac3, 0x7fff, -0x8000 * !lm, 0);
	sz3 = mac3 >> (12-shift);
	limBF3(&flags, sz3, 0);
	sz3 = limD(&flags, sz3);
	quotient = divide(&flags, gteH, sz3);
	gteSZ0 = gteSZ1;
	gteSZ1 = gteSZ2;
	gteSZ2 = gteSZ3;
	gteSZ3 = sz3;
	gteSXY0 = gteSXY1;
	gteSXY1 = gteSXY2;

	gteSX2 = limG1(&flags, mac0flags(&flags, gteOFX + (s64)gteIR1 * quotient) >> 16);
	gteSY2 = limG2(&flags, mac0flags(&flags, gteOFY + (s64)gteIR2 * quotient) >> 16);

	gteMAC0 = mac0 = mac0flags(&flags, gteDQB + (s64)gteDQA * quotient);
	gteIR0 = limH(&flags, mac0 >> 12);
	gteFLAG = getFinalFlag(flags);
}

static inline force_inline void gteRTPT(psxCP2Regs *regs, int shift, int lm)
{
	s32 sz3, quotient;
	s32 mac1, mac2;
	s64 mac3, mac0;
	s32 vx, vy, vz;
	s32 ir1, ir2;
	u32 h = gteH;
	u32 flags = 0;
	int v;

	GTE_LOG("GTE RTPT\n");

	gteSZ0 = gteSZ3;
	for (v = 0; v < 3; v++) {
		vx = VX(v);
		vy = VY(v);
		vz = VZ(v);
		mac1 = mac123add4(1, &flags, gteTRX, gteR11 * vx, gteR12 * vy, gteR13 * vz, shift);
		mac2 = mac123add4(2, &flags, gteTRY, gteR21 * vx, gteR22 * vy, gteR23 * vz, shift);
		mac3 = mac123add4(3, &flags, gteTRZ, gteR31 * vx, gteR32 * vy, gteR33 * vz, shift);
		ir1 = limB1(&flags, mac1, lm);
		ir2 = limB2(&flags, mac2, lm);
		sz3 = mac3 >> (12-shift);
		limBF3(&flags, sz3, 0);
		sz3 = limD(&flags, sz3);
		quotient = divide(&flags, h, sz3);
		fSZ(v) = sz3;
		fSX(v) = limG1(&flags, mac0flags(&flags, gteOFX + (s64)ir1 * quotient) >> 16);
		fSY(v) = limG2(&flags, mac0flags(&flags, gteOFY + (s64)ir2 * quotient) >> 16);
	}

	gteMAC1 = mac1;
	gteMAC2 = mac2;
	gteMAC3 = mac3;
	gteIR1 = ir1;
	gteIR2 = ir2;
	gteIR3 = LIM(&flags, mac3, 0x7fff, -0x8000 * !lm, 0);
	gteMAC0 = mac0 = mac0flags(&flags, gteDQB + (s64)gteDQA * quotient);
	gteIR0 = limH(&flags, mac0 >> 12);
	gteFLAG = getFinalFlag(flags);
}

static inline force_inline void NM(gteMVMVAn)(psxCP2Regs *regs,
	const s16 *mx, const s16 *v, const s32 *cv, int shift, int lm)
{
	u32 flags = 0;

	flags = gteMAC123f(regs, v[0], v[1], v[2], mx, cv, shift);
	gteIR1 = limB1(&flags, gteMAC1, lm);
	gteIR2 = limB2(&flags, gteMAC2, lm);
	gteIR3 = limB3(&flags, gteMAC3, lm);
	gteFLAG = getFinalFlag(flags);
}

static noinline void NM(gteMVMVAbugged)(psxCP2Regs *regs,
	const s16 *mx, const s16 *v, const s32 *cv, int shift, int lm)
{
	s32 vx = v[0], vy = v[1], vz = v[2];
	u32 flags = 0;
	s32 mac1 = mac123add_s12(1, &flags, cv[0], mx[0] * vx, shift);
	s32 mac2 = mac123add_s12(2, &flags, cv[1], mx[3] * vx, shift);
	s32 mac3 = mac123add_s12(3, &flags, cv[2], mx[6] * vx, shift);
	limBF1(&flags, mac1, 0);
	limBF2(&flags, mac2, 0);
	limBF3(&flags, mac3, 0);
	gteMAC1 = ((s64)(mx[1] * vy) + (mx[2] * vz)) >> shift;
	gteMAC2 = ((s64)(mx[4] * vy) + (mx[5] * vz)) >> shift;
	gteMAC3 = ((s64)(mx[7] * vy) + (mx[8] * vz)) >> shift;
	gteIR1 = limB1(&flags, gteMAC1, lm);
	gteIR2 = limB2(&flags, gteMAC2, lm);
	gteIR3 = limB3(&flags, gteMAC3, lm);
	gteFLAG = getFinalFlag(flags);
}

static noinline void NM(gteMVMVAn_sf0lm0)(psxCP2Regs *regs,
			const s16 *mx, const s16 *v, const s32 *cv) {
	NM(gteMVMVAn)(regs, mx, v, cv,  0, 0);
}
static noinline void NM(gteMVMVAn_sf0lm1)(psxCP2Regs *regs,
			const s16 *mx, const s16 *v, const s32 *cv) {
	NM(gteMVMVAn)(regs, mx, v, cv,  0, 1);
}
static noinline void NM(gteMVMVAn_sf1lm0)(psxCP2Regs *regs,
			const s16 *mx, const s16 *v, const s32 *cv) {
	NM(gteMVMVAn)(regs, mx, v, cv, 12, 0);
}
static noinline void NM(gteMVMVAn_sf1lm1)(psxCP2Regs *regs,
			const s16 *mx, const s16 *v, const s32 *cv) {
	NM(gteMVMVAn)(regs, mx, v, cv, 12, 1);
}

static inline force_inline void gteMVMVA(psxCP2Regs *regs,
	int mx, int v, int cv, int shift, int lm)
{
	const s16 v3[3] = { regs->CP2D.p[9].sw.l, regs->CP2D.p[10].sw.l, regs->CP2D.p[11].sw.l };
	const s16 *vp = &regs->CP2D.p[v << 1].sw.l;
	const s16 *mxp = &regs->CP2C.p[mx << 3].sw.l;
	const s32 *cvp = (s32 *)&regs->CP2C.r[(cv << 3) + 5];
	const s32 cv3[3] = { 0, 0, 0 };
	s16 mx3[9];

	GTE_LOG("GTE MVMVA\n");

	if (v == 3)
		vp = v3;
	if (unlikely(mx == 3)) {
		mxp = mx3;
		mx3[0] = -regs->CP2D.p[6].b.l << 4;
		mx3[1] =  regs->CP2D.p[6].b.l << 4;
		mx3[2] = regs->CP2D.p[8].sw.l;
		mx3[3] = regs->CP2C.p[1].sw.l;
		mx3[4] = regs->CP2C.p[1].sw.l;
		mx3[5] = regs->CP2C.p[1].sw.l;
		mx3[6] = regs->CP2C.p[2].sw.l;
		mx3[7] = regs->CP2C.p[2].sw.l;
		mx3[8] = regs->CP2C.p[2].sw.l;
	}
	if (unlikely(cv == 3))
		cvp = cv3;
	if (unlikely(cv == 2))
		NM(gteMVMVAbugged)(regs, mxp, vp, cvp, shift, lm);
	else {
		if (shift && lm)
			NM(gteMVMVAn_sf1lm1)(regs, mxp, vp, cvp);
		else if (shift && !lm)
			NM(gteMVMVAn_sf1lm0)(regs, mxp, vp, cvp);
		else if (lm)
			NM(gteMVMVAn_sf0lm1)(regs, mxp, vp, cvp);
		else
			NM(gteMVMVAn_sf0lm0)(regs, mxp, vp, cvp);
	}
}

void NM(gteMVMVA_generic)(psxCP2Regs *regs, u32 code)
{
	gteMVMVA(regs, GTE_MX(code), GTE_V(code), GTE_CV(code),
	         12 * GTE_SF(code), GTE_LM(code));
}

static inline void gteNCLIP_(psxCP2Regs *regs)
{
	u32 flags = 0;

	GTE_LOG("GTE NCLIP\n");

	gteMAC0 = mac0flags(&flags, (s64)(gteSX0 * (gteSY1 - gteSY2)) +
				gteSX1 * (gteSY2 - gteSY0) +
				gteSX2 * (gteSY0 - gteSY1));
	gteFLAG = getFinalFlag(flags);
}

static inline void gteAVSZ3_(psxCP2Regs *regs)
{
	u32 flags = 0;
	s64 r;

	GTE_LOG("GTE AVSZ3\n");

	r = (s64)gteZSF3 * (gteSZ1 + gteSZ2 + gteSZ3);
	gteMAC0 = mac0flags(&flags, r);
	gteOTZ = limD(&flags, r >> 12);
	gteFLAG = getFinalFlag(flags);
}

static inline void gteAVSZ4_(psxCP2Regs *regs)
{
	u32 flags = 0;
	s64 r;

	GTE_LOG("GTE AVSZ4\n");

	r = (s64)gteZSF4 * (gteSZ0 + gteSZ1 + gteSZ2 + gteSZ3);
	gteMAC0 = mac0flags(&flags, r);
	gteOTZ = limD(&flags, r >> 12);
	gteFLAG = getFinalFlag(flags);
}

static inline void gteSQR(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE SQR\n");

	gteMAC1 = (gteIR1 * gteIR1) >> shift;
	gteMAC2 = (gteIR2 * gteIR2) >> shift;
	gteMAC3 = (gteIR3 * gteIR3) >> shift;
	gteIR1 = limB1(&flags, gteMAC1, lm);
	gteIR2 = limB2(&flags, gteMAC2, lm);
	gteIR3 = limB3(&flags, gteMAC3, lm);
	gteFLAG = getFinalFlag(flags);
}

static inline force_inline void runColor1(psxCP2Regs *regs, u32 *flags, int shift, int lm,
	int is_ncc, int is_ncd, int v, int o, int writeback)
{
	s32 mac1, mac2, mac3;
	s32 ir1, ir2, ir3;
	s32 vx, vy, vz;

	vx = VX(v);
	vy = VY(v);
	vz = VZ(v);
	mac1 = ((s64)(gteL11 * vx) + (gteL12 * vy) + (gteL13 * vz)) >> shift;
	mac2 = ((s64)(gteL21 * vx) + (gteL22 * vy) + (gteL23 * vz)) >> shift;
	mac3 = ((s64)(gteL31 * vx) + (gteL32 * vy) + (gteL33 * vz)) >> shift;
	ir1 = limB1(flags, mac1, lm);
	ir2 = limB2(flags, mac2, lm);
	ir3 = limB3(flags, mac3, lm);
	*flags |= gteMAC123f(regs, ir1, ir2, ir3, &gteLR1, &gteRBK, shift);
	mac1 = gteMAC1;
	mac2 = gteMAC2;
	mac3 = gteMAC3;
	if (is_ncc || is_ncd) {
		ir1 = limB1(flags, mac1, lm);
		ir2 = limB2(flags, mac2, lm);
		ir3 = limB3(flags, mac3, lm);
		mac1 = ((s32)gteR * ir1) << 4;
		mac2 = ((s32)gteG * ir2) << 4;
		mac3 = ((s32)gteB * ir3) << 4;
		if (is_ncd) {
			s32 ir0 = gteIR0;
			ir1 = limB1(flags, mac123sub_s12(1, flags, gteRFC, mac1, shift), 0);
			ir2 = limB2(flags, mac123sub_s12(2, flags, gteGFC, mac2, shift), 0);
			ir3 = limB3(flags, mac123sub_s12(3, flags, gteBFC, mac3, shift), 0);
			mac1 = (ir0 * ir1 + (s64)mac1) >> shift;
			mac2 = (ir0 * ir2 + (s64)mac2) >> shift;
			mac3 = (ir0 * ir3 + (s64)mac3) >> shift;
		}
		else {
			mac1 >>= shift;
			mac2 >>= shift;
			mac3 >>= shift;
		}
	}
	fR(o) = limC1(flags, mac1 >> 4);
	fG(o) = limC2(flags, mac2 >> 4);
	fB(o) = limC3(flags, mac3 >> 4);
	fCODE(o) = gteCODE;
	if (writeback) {
		gteMAC1 = mac1;
		gteMAC2 = mac2;
		gteMAC3 = mac3;
		gteIR1 = limB1(flags, mac1, lm);
		gteIR2 = limB2(flags, mac2, lm);
		gteIR3 = limB3(flags, mac3, lm);
		gteFLAG = getFinalFlag(*flags);
	}
	else {
		limBF1(flags, mac1, lm);
		limBF2(flags, mac2, lm);
		limBF3(flags, mac3, lm);
	}
}

static inline void shiftRGB(psxCP2Regs *regs)
{
	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
}

static inline void gteNCCS(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE NCCS\n");

	shiftRGB(regs);
	runColor1(regs, &flags, shift, lm, 1, 0, 0, 2, 1);
}

static inline void gteNCCT(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE NCCT\n");

	runColor1(regs, &flags, shift, lm, 1, 0, 0, 0, 0);
	runColor1(regs, &flags, shift, lm, 1, 0, 1, 1, 0);
	runColor1(regs, &flags, shift, lm, 1, 0, 2, 2, 1);
}

static inline void gteNCDS(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE NCDS\n");

	shiftRGB(regs);
	runColor1(regs, &flags, shift, lm, 0, 1, 0, 2, 1);
}

static inline void gteNCDT(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE NCDT\n");

	runColor1(regs, &flags, shift, lm, 0, 1, 0, 0, 0);
	runColor1(regs, &flags, shift, lm, 0, 1, 1, 1, 0);
	runColor1(regs, &flags, shift, lm, 0, 1, 2, 2, 1);
}

static inline void gteNCS(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE NCS\n");

	shiftRGB(regs);
	runColor1(regs, &flags, shift, lm, 0, 0, 0, 2, 1);
}

static inline void gteNCT(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE NCT\n");

	runColor1(regs, &flags, shift, lm, 0, 0, 0, 0, 0);
	runColor1(regs, &flags, shift, lm, 0, 0, 1, 1, 0);
	runColor1(regs, &flags, shift, lm, 0, 0, 2, 2, 1);
}

static inline void gteOP(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE OP\n");

	gteMAC1 = ((gteR22 * gteIR3) - (gteR33 * gteIR2)) >> shift;
	gteMAC2 = ((gteR33 * gteIR1) - (gteR11 * gteIR3)) >> shift;
	gteMAC3 = ((gteR11 * gteIR2) - (gteR22 * gteIR1)) >> shift;
	gteIR1 = limB1(&flags, gteMAC1, lm);
	gteIR2 = limB2(&flags, gteMAC2, lm);
	gteIR3 = limB3(&flags, gteMAC3, lm);
	gteFLAG = getFinalFlag(flags);
}

static inline void gteGPF(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE GPF\n");

	shiftRGB(regs);
	gteMAC1 = (gteIR0 * gteIR1) >> shift;
	gteMAC2 = (gteIR0 * gteIR2) >> shift;
	gteMAC3 = (gteIR0 * gteIR3) >> shift;
	gteIR1 = limB1(&flags, gteMAC1, lm);
	gteIR2 = limB2(&flags, gteMAC2, lm);
	gteIR3 = limB3(&flags, gteMAC3, lm);
	gteR2 = limC1(&flags, gteMAC1 >> 4);
	gteG2 = limC2(&flags, gteMAC2 >> 4);
	gteB2 = limC3(&flags, gteMAC3 >> 4);
	gteFLAG = getFinalFlag(flags);
}

static inline void gteGPL(psxCP2Regs *regs, int shift, int lm)
{
	s16 ir0 = gteIR0;
	u32 flags = 0;

	GTE_LOG("GTE GPL\n");

	shiftRGB(regs);
	if (shift) {
		gteMAC1 = mac123add_s12(1, &flags, gteMAC1, ir0 * gteIR1, 12);
		gteMAC2 = mac123add_s12(2, &flags, gteMAC2, ir0 * gteIR2, 12);
		gteMAC3 = mac123add_s12(3, &flags, gteMAC3, ir0 * gteIR3, 12);
	}
	else {
		gteMAC1 += ir0 * gteIR1;
		gteMAC2 += ir0 * gteIR2;
		gteMAC3 += ir0 * gteIR3;
	}
	gteIR1 = limB1(&flags, gteMAC1, lm);
	gteIR2 = limB2(&flags, gteMAC2, lm);
	gteIR3 = limB3(&flags, gteMAC3, lm);
	gteR2 = limC1(&flags, gteMAC1 >> 4);
	gteG2 = limC2(&flags, gteMAC2 >> 4);
	gteB2 = limC3(&flags, gteMAC3 >> 4);
	gteFLAG = getFinalFlag(flags);
}

static inline void runColor2(psxCP2Regs *regs, u32 *flags, int shift, int lm,
	s32 mac1, s32 mac2, s32 mac3, int o, int writeback)
{
	s32 ir1, ir2, ir3;
	s32 ir0 = gteIR0;

	ir1 = limB1(flags, mac123sub_s12(1, flags, gteRFC, mac1, shift), 0);
	ir2 = limB2(flags, mac123sub_s12(2, flags, gteGFC, mac2, shift), 0);
	ir3 = limB3(flags, mac123sub_s12(3, flags, gteBFC, mac3, shift), 0);
	mac1 = gteMAC1 = (ir0 * ir1 + (s64)mac1) >> shift;
	mac2 = gteMAC2 = (ir0 * ir2 + (s64)mac2) >> shift;
	mac3 = gteMAC3 = (ir0 * ir3 + (s64)mac3) >> shift;
	ir1 = limB1(flags, mac1, lm);
	ir2 = limB2(flags, mac2, lm);
	ir3 = limB3(flags, mac3, lm);
	fR(o) = limC1(flags, mac1 >> 4);
	fG(o) = limC2(flags, mac2 >> 4);
	fB(o) = limC3(flags, mac3 >> 4);
	if (writeback) {
		gteMAC1 = mac1;
		gteMAC2 = mac2;
		gteMAC3 = mac3;
		gteIR1 = ir1;
		gteIR2 = ir2;
		gteIR3 = ir3;
		gteFLAG = getFinalFlag(*flags);
	}
}

static inline void gteDCPL(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE DCPL\n");

	s32 mac1 = ((s32)gteR * gteIR1) << 4;
	s32 mac2 = ((s32)gteG * gteIR2) << 4;
	s32 mac3 = ((s32)gteB * gteIR3) << 4;
	shiftRGB(regs);
	runColor2(regs, &flags, shift, lm, mac1, mac2, mac3, 2, 1);
}

static inline void gteDPCS(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE DPCS\n");

	shiftRGB(regs);
	runColor2(regs, &flags, shift, lm, gteR << 16, gteG << 16, gteB << 16, 2, 1);
}

static inline void gteDPCT(psxCP2Regs *regs, int shift, int lm)
{
	s32 mac1, mac2, mac3;
	u8 code0 = gteCODE;
	u32 flags = 0;
	int v;

	GTE_LOG("GTE DPCT\n");

	for (v = 0; v < 3; v++) {
		mac1 = fR(v) << 16;
		mac2 = fG(v) << 16;
		mac3 = fB(v) << 16;
		runColor2(regs, &flags, shift, lm, mac1, mac2, mac3, v, v == 2);
		fCODE(v) = code0;
	}
}

static inline void gteINTPL(psxCP2Regs *regs, int shift, int lm)
{
	u32 flags = 0;

	GTE_LOG("GTE INTPL\n");

	shiftRGB(regs);
	runColor2(regs, &flags, shift, lm, gteIR1 << 12, gteIR2 << 12, gteIR3 << 12, 2, 1);
}

static inline void runColor3(psxCP2Regs *regs, int shift, int lm, int is_cdp)
{
	s32 mac1, mac2, mac3;
	s32 ir1, ir2, ir3;
	u32 flags;

	flags = gteMAC123f(regs, gteIR1, gteIR2, gteIR3, &gteLR1, &gteRBK, shift);
	ir1 = limB1(&flags, gteMAC1, lm);
	ir2 = limB2(&flags, gteMAC2, lm);
	ir3 = limB3(&flags, gteMAC3, lm);
	mac1 = ((s32)gteR * ir1) << 4;
	mac2 = ((s32)gteG * ir2) << 4;
	mac3 = ((s32)gteB * ir3) << 4;
	if (is_cdp) {
		s32 ir0 = gteIR0;
		ir1 = limB1(&flags, mac123sub_s12(1, &flags, gteRFC, mac1, shift), 0);
		ir2 = limB2(&flags, mac123sub_s12(2, &flags, gteGFC, mac2, shift), 0);
		ir3 = limB3(&flags, mac123sub_s12(3, &flags, gteBFC, mac3, shift), 0);
		mac1 = (ir0 * ir1 + (s64)mac1) >> shift;
		mac2 = (ir0 * ir2 + (s64)mac2) >> shift;
		mac3 = (ir0 * ir3 + (s64)mac3) >> shift;
	}
	else {
		mac1 >>= shift;
		mac2 >>= shift;
		mac3 >>= shift;
	}
	gteMAC1 = mac1;
	gteMAC2 = mac2;
	gteMAC3 = mac3;
	gteIR1 = limB1(&flags, mac1, lm);
	gteIR2 = limB2(&flags, mac2, lm);
	gteIR3 = limB3(&flags, mac3, lm);
	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(&flags, mac1 >> 4);
	gteG2 = limC2(&flags, mac2 >> 4);
	gteB2 = limC3(&flags, mac3 >> 4);
	gteFLAG = getFinalFlag(flags);
}

static inline void gteCC(psxCP2Regs *regs, int shift, int lm)
{
	GTE_LOG("GTE CC\n");
	runColor3(regs, shift, lm, 0);
}

static inline void gteCDP(psxCP2Regs *regs, int shift, int lm)
{
	GTE_LOG("GTE CDP\n");
	runColor3(regs, shift, lm, 1);
}

#define DECL_OP4__(name, nf, decl_) \
decl_ void gte##name##_sf0lm0##nf(psxCP2Regs *regs, u32 code) { gte##name(regs,  0, 0); } \
decl_ void gte##name##_sf0lm1##nf(psxCP2Regs *regs, u32 code) { gte##name(regs,  0, 1); } \
decl_ void gte##name##_sf1lm0##nf(psxCP2Regs *regs, u32 code) { gte##name(regs, 12, 0); } \
decl_ void gte##name##_sf1lm1##nf(psxCP2Regs *regs, u32 code) { gte##name(regs, 12, 1); }
#define DECL_OP1_(name, nf) \
static void gte##name##nf(psxCP2Regs *regs, u32 code) { gte##name##_(regs); }

#define DECL_OP4_(name, nf, decl_) DECL_OP4__(name, nf, decl_)
#define DECL_OP4(name, nf) DECL_OP4_(name, nf, static)
#define DECL_OP1(name, nf) DECL_OP1_(name, nf)

#define DECL_MVMVA1(nf, sf_, mx_, v_, cv_, lm_) \
static void no_stackprotector gteMVMVA_mx##mx_##v##v_##cv##cv_##sf##sf_##lm##lm_##nf( \
		psxCP2Regs *regs, u32 code) { \
	gteMVMVA(regs, mx_, v_, cv_, sf_ * 12, lm_); \
}
#define DECL_MVMVA2( nf, sf, mx, v, cv) \
	DECL_MVMVA1( nf, sf, mx, v, cv, 0) \
	DECL_MVMVA1( nf, sf, mx, v, cv, 1)
#define DECL_MVMVA4( nf, sf, mx, v) \
	DECL_MVMVA2( nf, sf, mx, v, 0) \
	DECL_MVMVA2( nf, sf, mx, v, 1)
#define DECL_MVMVA16(nf, sf, mx) \
	DECL_MVMVA4( nf, sf, mx, 0) \
	DECL_MVMVA4( nf, sf, mx, 1) \
	DECL_MVMVA4( nf, sf, mx, 2) \
	DECL_MVMVA4( nf, sf, mx, 3)
#define DECL_MVMVA32(nf, sf) \
	DECL_MVMVA16(nf, sf, 0) \
	DECL_MVMVA16(nf, sf, 1)
#define DECL_MVMVA64(nf) \
	DECL_MVMVA32(nf, 0) \
	DECL_MVMVA32(nf, 1)

#define TABLE_ENTRY4(name) \
	[GTEOP_##name * 4 + 0] = NM(gte##name##_sf0lm0), \
	[GTEOP_##name * 4 + 1] = NM(gte##name##_sf0lm1), \
	[GTEOP_##name * 4 + 2] = NM(gte##name##_sf1lm0), \
	[GTEOP_##name * 4 + 3] = NM(gte##name##_sf1lm1)
#define TABLE_ENTRY1(name) \
	[GTEOP_##name * 4 + 0] = NM(gte##name), \
	[GTEOP_##name * 4 + 1] = NM(gte##name), \
	[GTEOP_##name * 4 + 2] = NM(gte##name), \
	[GTEOP_##name * 4 + 3] = NM(gte##name)
#define TABLE_ENTRY1_MVMVA1(nf, sf_, mx_, v_, cv_, lm_) \
	[0x40*4 + sf_*32 + mx_*16 + v_*4 + cv_*2 + lm_] = \
		gteMVMVA_mx##mx_##v##v_##cv##cv_##sf##sf_##lm##lm_##nf,
#define TABLE_ENTRY1_MVMVA2( nf, sf, mx, v, cv) \
	TABLE_ENTRY1_MVMVA1( nf, sf, mx, v, cv, 0) \
	TABLE_ENTRY1_MVMVA1( nf, sf, mx, v, cv, 1)
#define TABLE_ENTRY1_MVMVA4( nf, sf, mx, v) \
	TABLE_ENTRY1_MVMVA2( nf, sf, mx, v, 0) \
	TABLE_ENTRY1_MVMVA2( nf, sf, mx, v, 1)
#define TABLE_ENTRY1_MVMVA16(nf, sf, mx) \
	TABLE_ENTRY1_MVMVA4( nf, sf, mx, 0) \
	TABLE_ENTRY1_MVMVA4( nf, sf, mx, 1) \
	TABLE_ENTRY1_MVMVA4( nf, sf, mx, 2) \
	TABLE_ENTRY1_MVMVA4( nf, sf, mx, 3)
#define TABLE_ENTRY1_MVMVA32(nf, sf) \
	TABLE_ENTRY1_MVMVA16(nf, sf, 0) \
	TABLE_ENTRY1_MVMVA16(nf, sf, 1)
#define TABLE_ENTRY1_MVMVA64(nf) \
	TABLE_ENTRY1_MVMVA32(nf, 0) \
	TABLE_ENTRY1_MVMVA32(nf, 1)

DECL_OP4_(RTPS, NF, )
DECL_OP1(NCLIP, NF)
DECL_OP4(OP,    NF)
DECL_OP4(DPCS,  NF)
DECL_OP4(INTPL, NF)
DECL_OP4(NCDS,  NF)
DECL_OP4(CDP,   NF)
DECL_OP4(NCDT,  NF)
DECL_OP4(NCCS,  NF)
DECL_OP4(CC,    NF)
DECL_OP4(NCS,   NF)
DECL_OP4(NCT,   NF)
DECL_OP4(SQR,   NF)
DECL_OP4(DCPL,  NF)
DECL_OP4(DPCT,  NF)
DECL_OP1(AVSZ3, NF)
DECL_OP1(AVSZ4, NF)
DECL_OP4_(RTPT, NF, )
DECL_OP4(GPF,   NF)
DECL_OP4(GPL,   NF)
DECL_OP4(NCCT,  NF)
DECL_MVMVA64(NF)

static void (*gteOPS[0x40*(4+1)])(psxCP2Regs *regs, u32 code) =
{
	TABLE_ENTRY4(RTPS),
	TABLE_ENTRY1(NCLIP),
	TABLE_ENTRY4(OP),
	TABLE_ENTRY4(DPCS),
	TABLE_ENTRY4(INTPL),
	//TABLE_ENTRY1(MVMVA),
	TABLE_ENTRY4(NCDS),
	TABLE_ENTRY4(CDP),
	TABLE_ENTRY4(NCDT),
	TABLE_ENTRY4(NCCS),
	TABLE_ENTRY4(CC),
	TABLE_ENTRY4(NCS),
	TABLE_ENTRY4(NCT),
	TABLE_ENTRY4(SQR),
	TABLE_ENTRY4(DCPL),
	TABLE_ENTRY4(DPCT),
	TABLE_ENTRY1(AVSZ3),
	TABLE_ENTRY1(AVSZ4),
	TABLE_ENTRY4(RTPT),
	TABLE_ENTRY4(GPF),
	TABLE_ENTRY4(GPL),
	TABLE_ENTRY4(NCCT),
	TABLE_ENTRY1_MVMVA64(NF)
};

gte_handler * NM(gteGetHandler)(u32 code)
{
	u32 l, op = code & 0x3f;
	u32 sflm = ((code >> (19-1)) & 2) | ((code >> 10) & 1);
	gte_handler *h = gteOPS[(op << 2) | sflm];
	if (h)
		return h;
	if (op == GTEOP_MVMVA) {
		if (code & ((1u << 18) | (1u << 14))) // "bugged" mx/cv
			h = NM(gteMVMVA_generic);
		else {
			l = ((code >> (19-5)) & 0x20) | // sf
			    ((code >> (17-4)) & 0x10) | // mx
			    ((code >> (15-2)) & 0x0c) | // v
			    ((code >> (13-1)) & 0x02) | // cv
			    ((code >>  10)    & 0x01);  // lm
			h = gteOPS[0x40*4 + l];
		}
	}
	return h;
}

#ifndef FLAGLESS

void gteDispatch(psxCP2Regs *regs, u32 code)
{
	gte_handler *h = gteGetHandler(code);
	if (h)
		h(regs, code);
	else
		log_unhandled("unhandled gte op %08x\n", code);
}

/* decomposed/parametrized versions for the recompiler */

void gteSQR_part_noshift(psxCP2Regs *regs) {
	gteMAC1 = gteIR1 * gteIR1;
	gteMAC2 = gteIR2 * gteIR2;
	gteMAC3 = gteIR3 * gteIR3;
}

void gteSQR_part_shift(psxCP2Regs *regs) {
	gteMAC1 = (gteIR1 * gteIR1) >> 12;
	gteMAC2 = (gteIR2 * gteIR2) >> 12;
	gteMAC3 = (gteIR3 * gteIR3) >> 12;
}

void gteOP_part_noshift(psxCP2Regs *regs) {
	gteMAC1 = (gteR22 * gteIR3) - (gteR33 * gteIR2);
	gteMAC2 = (gteR33 * gteIR1) - (gteR11 * gteIR3);
	gteMAC3 = (gteR11 * gteIR2) - (gteR22 * gteIR1);
}

void gteOP_part_shift(psxCP2Regs *regs) {
	gteMAC1 = ((gteR22 * gteIR3) - (gteR33 * gteIR2)) >> 12;
	gteMAC2 = ((gteR33 * gteIR1) - (gteR11 * gteIR3)) >> 12;
	gteMAC3 = ((gteR11 * gteIR2) - (gteR22 * gteIR1)) >> 12;
}

void gteGPF_part_noshift(psxCP2Regs *regs) {
	gteFLAG = 0;

	gteMAC1 = gteIR0 * gteIR1;
	gteMAC2 = gteIR0 * gteIR2;
	gteMAC3 = gteIR0 * gteIR3;
}

void gteGPF_part_shift(psxCP2Regs *regs) {
	gteFLAG = 0;

	gteMAC1 = (gteIR0 * gteIR1) >> 12;
	gteMAC2 = (gteIR0 * gteIR2) >> 12;
	gteMAC3 = (gteIR0 * gteIR3) >> 12;
}

void gteGPL_part_noshift(psxCP2Regs *regs) {
	s16 ir0 = gteIR0;
	gteFLAG = 0;

	gteMAC1 += ir0 * gteIR1;
	gteMAC2 += ir0 * gteIR2;
	gteMAC3 += ir0 * gteIR3;
}

#endif // !FLAGLESS

void NM(gteGPL_part_shift)(psxCP2Regs *regs) {
	s16 ir0 = gteIR0;
	u32 flags = 0;
	gteMAC1 = mac123add_s12(1, &flags, gteMAC1, ir0 * gteIR1, 12);
	gteMAC2 = mac123add_s12(2, &flags, gteMAC2, ir0 * gteIR2, 12);
	gteMAC3 = mac123add_s12(3, &flags, gteMAC3, ir0 * gteIR3, 12);
	gteFLAG = getFinalFlag(flags);
}

static inline void runColor2part(psxCP2Regs *regs, int shift, s32 mac1, s32 mac2, s32 mac3)
{
	s32 ir1, ir2, ir3;
	s32 ir0 = gteIR0;
	u32 flags = 0;

	ir1 = limB1(&flags, mac123sub_s12(1, &flags, gteRFC, mac1, shift), 0);
	ir2 = limB2(&flags, mac123sub_s12(2, &flags, gteGFC, mac2, shift), 0);
	ir3 = limB3(&flags, mac123sub_s12(3, &flags, gteBFC, mac3, shift), 0);
	gteMAC1 = (ir0 * ir1 + (s64)mac1) >> shift;
	gteMAC2 = (ir0 * ir2 + (s64)mac2) >> shift;
	gteMAC3 = (ir0 * ir3 + (s64)mac3) >> shift;
	gteFLAG = flags;
}

void NM(gteDPCS_part_noshift)(psxCP2Regs *regs) {
	runColor2part(regs, 0, gteR << 16, gteG << 16, gteB << 16);
}

void NM(gteDPCS_part_shift)(psxCP2Regs *regs) {
	runColor2part(regs, 12, gteR << 16, gteG << 16, gteB << 16);
}

void NM(gteINTPL_part_noshift)(psxCP2Regs *regs) {
	runColor2part(regs, 0, gteIR1 << 12, gteIR2 << 12, gteIR3 << 12);
}

void NM(gteINTPL_part_shift)(psxCP2Regs *regs) {
	runColor2part(regs, 12, gteIR1 << 12, gteIR2 << 12, gteIR3 << 12);
}

void NM(gteMACtoRGB)(psxCP2Regs *regs) {
	u32 flags = gteFLAG;
	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(&flags, gteMAC1 >> 4);
	gteG2 = limC2(&flags, gteMAC2 >> 4);
	gteB2 = limC3(&flags, gteMAC3 >> 4);
	gteFLAG = getFinalFlag(flags);
}

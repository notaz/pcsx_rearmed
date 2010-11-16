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
#include "psxmem.h"

#define VX(n) (n < 3 ? psxRegs.CP2D.p[n << 1].sw.l : psxRegs.CP2D.p[9].sw.l)
#define VY(n) (n < 3 ? psxRegs.CP2D.p[n << 1].sw.h : psxRegs.CP2D.p[10].sw.l)
#define VZ(n) (n < 3 ? psxRegs.CP2D.p[(n << 1) + 1].sw.l : psxRegs.CP2D.p[11].sw.l)
#define MX11(n) (n < 3 ? psxRegs.CP2C.p[(n << 3)].sw.l : 0)
#define MX12(n) (n < 3 ? psxRegs.CP2C.p[(n << 3)].sw.h : 0)
#define MX13(n) (n < 3 ? psxRegs.CP2C.p[(n << 3) + 1].sw.l : 0)
#define MX21(n) (n < 3 ? psxRegs.CP2C.p[(n << 3) + 1].sw.h : 0)
#define MX22(n) (n < 3 ? psxRegs.CP2C.p[(n << 3) + 2].sw.l : 0)
#define MX23(n) (n < 3 ? psxRegs.CP2C.p[(n << 3) + 2].sw.h : 0)
#define MX31(n) (n < 3 ? psxRegs.CP2C.p[(n << 3) + 3].sw.l : 0)
#define MX32(n) (n < 3 ? psxRegs.CP2C.p[(n << 3) + 3].sw.h : 0)
#define MX33(n) (n < 3 ? psxRegs.CP2C.p[(n << 3) + 4].sw.l : 0)
#define CV1(n) (n < 3 ? (s32)psxRegs.CP2C.r[(n << 3) + 5] : 0)
#define CV2(n) (n < 3 ? (s32)psxRegs.CP2C.r[(n << 3) + 6] : 0)
#define CV3(n) (n < 3 ? (s32)psxRegs.CP2C.r[(n << 3) + 7] : 0)

#define fSX(n) ((psxRegs.CP2D.p)[((n) + 12)].sw.l)
#define fSY(n) ((psxRegs.CP2D.p)[((n) + 12)].sw.h)
#define fSZ(n) ((psxRegs.CP2D.p)[((n) + 17)].w.l) /* (n == 0) => SZ1; */

#define gteVXY0 (psxRegs.CP2D.r[0])
#define gteVX0  (psxRegs.CP2D.p[0].sw.l)
#define gteVY0  (psxRegs.CP2D.p[0].sw.h)
#define gteVZ0  (psxRegs.CP2D.p[1].sw.l)
#define gteVXY1 (psxRegs.CP2D.r[2])
#define gteVX1  (psxRegs.CP2D.p[2].sw.l)
#define gteVY1  (psxRegs.CP2D.p[2].sw.h)
#define gteVZ1  (psxRegs.CP2D.p[3].sw.l)
#define gteVXY2 (psxRegs.CP2D.r[4])
#define gteVX2  (psxRegs.CP2D.p[4].sw.l)
#define gteVY2  (psxRegs.CP2D.p[4].sw.h)
#define gteVZ2  (psxRegs.CP2D.p[5].sw.l)
#define gteRGB  (psxRegs.CP2D.r[6])
#define gteR    (psxRegs.CP2D.p[6].b.l)
#define gteG    (psxRegs.CP2D.p[6].b.h)
#define gteB    (psxRegs.CP2D.p[6].b.h2)
#define gteCODE (psxRegs.CP2D.p[6].b.h3)
#define gteOTZ  (psxRegs.CP2D.p[7].w.l)
#define gteIR0  (psxRegs.CP2D.p[8].sw.l)
#define gteIR1  (psxRegs.CP2D.p[9].sw.l)
#define gteIR2  (psxRegs.CP2D.p[10].sw.l)
#define gteIR3  (psxRegs.CP2D.p[11].sw.l)
#define gteSXY0 (psxRegs.CP2D.r[12])
#define gteSX0  (psxRegs.CP2D.p[12].sw.l)
#define gteSY0  (psxRegs.CP2D.p[12].sw.h)
#define gteSXY1 (psxRegs.CP2D.r[13])
#define gteSX1  (psxRegs.CP2D.p[13].sw.l)
#define gteSY1  (psxRegs.CP2D.p[13].sw.h)
#define gteSXY2 (psxRegs.CP2D.r[14])
#define gteSX2  (psxRegs.CP2D.p[14].sw.l)
#define gteSY2  (psxRegs.CP2D.p[14].sw.h)
#define gteSXYP (psxRegs.CP2D.r[15])
#define gteSXP  (psxRegs.CP2D.p[15].sw.l)
#define gteSYP  (psxRegs.CP2D.p[15].sw.h)
#define gteSZ0  (psxRegs.CP2D.p[16].w.l)
#define gteSZ1  (psxRegs.CP2D.p[17].w.l)
#define gteSZ2  (psxRegs.CP2D.p[18].w.l)
#define gteSZ3  (psxRegs.CP2D.p[19].w.l)
#define gteRGB0  (psxRegs.CP2D.r[20])
#define gteR0    (psxRegs.CP2D.p[20].b.l)
#define gteG0    (psxRegs.CP2D.p[20].b.h)
#define gteB0    (psxRegs.CP2D.p[20].b.h2)
#define gteCODE0 (psxRegs.CP2D.p[20].b.h3)
#define gteRGB1  (psxRegs.CP2D.r[21])
#define gteR1    (psxRegs.CP2D.p[21].b.l)
#define gteG1    (psxRegs.CP2D.p[21].b.h)
#define gteB1    (psxRegs.CP2D.p[21].b.h2)
#define gteCODE1 (psxRegs.CP2D.p[21].b.h3)
#define gteRGB2  (psxRegs.CP2D.r[22])
#define gteR2    (psxRegs.CP2D.p[22].b.l)
#define gteG2    (psxRegs.CP2D.p[22].b.h)
#define gteB2    (psxRegs.CP2D.p[22].b.h2)
#define gteCODE2 (psxRegs.CP2D.p[22].b.h3)
#define gteRES1  (psxRegs.CP2D.r[23])
#define gteMAC0  (((s32 *)psxRegs.CP2D.r)[24])
#define gteMAC1  (((s32 *)psxRegs.CP2D.r)[25])
#define gteMAC2  (((s32 *)psxRegs.CP2D.r)[26])
#define gteMAC3  (((s32 *)psxRegs.CP2D.r)[27])
#define gteIRGB  (psxRegs.CP2D.r[28])
#define gteORGB  (psxRegs.CP2D.r[29])
#define gteLZCS  (psxRegs.CP2D.r[30])
#define gteLZCR  (psxRegs.CP2D.r[31])

#define gteR11R12 (((s32 *)psxRegs.CP2C.r)[0])
#define gteR22R23 (((s32 *)psxRegs.CP2C.r)[2])
#define gteR11 (psxRegs.CP2C.p[0].sw.l)
#define gteR12 (psxRegs.CP2C.p[0].sw.h)
#define gteR13 (psxRegs.CP2C.p[1].sw.l)
#define gteR21 (psxRegs.CP2C.p[1].sw.h)
#define gteR22 (psxRegs.CP2C.p[2].sw.l)
#define gteR23 (psxRegs.CP2C.p[2].sw.h)
#define gteR31 (psxRegs.CP2C.p[3].sw.l)
#define gteR32 (psxRegs.CP2C.p[3].sw.h)
#define gteR33 (psxRegs.CP2C.p[4].sw.l)
#define gteTRX (((s32 *)psxRegs.CP2C.r)[5])
#define gteTRY (((s32 *)psxRegs.CP2C.r)[6])
#define gteTRZ (((s32 *)psxRegs.CP2C.r)[7])
#define gteL11 (psxRegs.CP2C.p[8].sw.l)
#define gteL12 (psxRegs.CP2C.p[8].sw.h)
#define gteL13 (psxRegs.CP2C.p[9].sw.l)
#define gteL21 (psxRegs.CP2C.p[9].sw.h)
#define gteL22 (psxRegs.CP2C.p[10].sw.l)
#define gteL23 (psxRegs.CP2C.p[10].sw.h)
#define gteL31 (psxRegs.CP2C.p[11].sw.l)
#define gteL32 (psxRegs.CP2C.p[11].sw.h)
#define gteL33 (psxRegs.CP2C.p[12].sw.l)
#define gteRBK (((s32 *)psxRegs.CP2C.r)[13])
#define gteGBK (((s32 *)psxRegs.CP2C.r)[14])
#define gteBBK (((s32 *)psxRegs.CP2C.r)[15])
#define gteLR1 (psxRegs.CP2C.p[16].sw.l)
#define gteLR2 (psxRegs.CP2C.p[16].sw.h)
#define gteLR3 (psxRegs.CP2C.p[17].sw.l)
#define gteLG1 (psxRegs.CP2C.p[17].sw.h)
#define gteLG2 (psxRegs.CP2C.p[18].sw.l)
#define gteLG3 (psxRegs.CP2C.p[18].sw.h)
#define gteLB1 (psxRegs.CP2C.p[19].sw.l)
#define gteLB2 (psxRegs.CP2C.p[19].sw.h)
#define gteLB3 (psxRegs.CP2C.p[20].sw.l)
#define gteRFC (((s32 *)psxRegs.CP2C.r)[21])
#define gteGFC (((s32 *)psxRegs.CP2C.r)[22])
#define gteBFC (((s32 *)psxRegs.CP2C.r)[23])
#define gteOFX (((s32 *)psxRegs.CP2C.r)[24])
#define gteOFY (((s32 *)psxRegs.CP2C.r)[25])
#define gteH   (psxRegs.CP2C.p[26].sw.l)
#define gteDQA (psxRegs.CP2C.p[27].sw.l)
#define gteDQB (((s32 *)psxRegs.CP2C.r)[28])
#define gteZSF3 (psxRegs.CP2C.p[29].sw.l)
#define gteZSF4 (psxRegs.CP2C.p[30].sw.l)
#define gteFLAG (psxRegs.CP2C.r[31])

#define GTE_OP(op) ((op >> 20) & 31)
#define GTE_SF(op) ((op >> 19) & 1)
#define GTE_MX(op) ((op >> 17) & 3)
#define GTE_V(op) ((op >> 15) & 3)
#define GTE_CV(op) ((op >> 13) & 3)
#define GTE_CD(op) ((op >> 11) & 3) /* not used */
#define GTE_LM(op) ((op >> 10) & 1)
#define GTE_CT(op) ((op >> 6) & 15) /* not used */
#define GTE_FUNCT(op) (op & 63)

#define gteop (psxRegs.code & 0x1ffffff)

static inline s64 BOUNDS(s64 n_value, s64 n_max, int n_maxflag, s64 n_min, int n_minflag) {
	if (n_value > n_max) {
		gteFLAG |= n_maxflag;
	} else if (n_value < n_min) {
		gteFLAG |= n_minflag;
	}
	return n_value;
}

static inline s32 LIM(s32 value, s32 max, s32 min, u32 flag) {
	s32 ret = value;
	if (value > max) {
		gteFLAG |= flag;
		ret = max;
	} else if (value < min) {
		gteFLAG |= flag;
		ret = min;
	}
	return ret;
}

#define A1(a) BOUNDS((a), 0x7fffffff, (1 << 30), -(s64)0x80000000, (1 << 31) | (1 << 27))
#define A2(a) BOUNDS((a), 0x7fffffff, (1 << 29), -(s64)0x80000000, (1 << 31) | (1 << 26))
#define A3(a) BOUNDS((a), 0x7fffffff, (1 << 28), -(s64)0x80000000, (1 << 31) | (1 << 25))
#define limB1(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1 << 31) | (1 << 24))
#define limB2(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1 << 31) | (1 << 23))
#define limB3(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1 << 22))
#define limC1(a) LIM((a), 0x00ff, 0x0000, (1 << 21))
#define limC2(a) LIM((a), 0x00ff, 0x0000, (1 << 20))
#define limC3(a) LIM((a), 0x00ff, 0x0000, (1 << 19))
#define limD(a) LIM((a), 0xffff, 0x0000, (1 << 31) | (1 << 18))

static inline u32 limE(u32 result) {
	if (result > 0x1ffff) {
		gteFLAG |= (1 << 31) | (1 << 17);
		return 0x1ffff;
	}
	return result;
}

#define F(a) BOUNDS((a), 0x7fffffff, (1 << 31) | (1 << 16), -(s64)0x80000000, (1 << 31) | (1 << 15))
#define limG1(a) LIM((a), 0x3ff, -0x400, (1 << 31) | (1 << 14))
#define limG2(a) LIM((a), 0x3ff, -0x400, (1 << 31) | (1 << 13))
#define limH(a) LIM((a), 0xfff, 0x000, (1 << 12))

#include "gte_divider.h"

static inline u32 MFC2(int reg) {
	switch (reg) {
		case 1:
		case 3:
		case 5:
		case 8:
		case 9:
		case 10:
		case 11:
			psxRegs.CP2D.r[reg] = (s32)psxRegs.CP2D.p[reg].sw.l;
			break;

		case 7:
		case 16:
		case 17:
		case 18:
		case 19:
			psxRegs.CP2D.r[reg] = (u32)psxRegs.CP2D.p[reg].w.l;
			break;

		case 15:
			psxRegs.CP2D.r[reg] = gteSXY2;
			break;

		case 28:
		case 30:
			return 0;

		case 29:
			psxRegs.CP2D.r[reg] = LIM(gteIR1 >> 7, 0x1f, 0, 0) |
									(LIM(gteIR2 >> 7, 0x1f, 0, 0) << 5) |
									(LIM(gteIR3 >> 7, 0x1f, 0, 0) << 10);
			break;
	}
	return psxRegs.CP2D.r[reg];
}

static inline void MTC2(u32 value, int reg) {
	switch (reg) {
		case 15:
			gteSXY0 = gteSXY1;
			gteSXY1 = gteSXY2;
			gteSXY2 = value;
			gteSXYP = value;
			break;

		case 28:
			gteIRGB = value;
			gteIR1 = (value & 0x1f) << 7;
			gteIR2 = (value & 0x3e0) << 2;
			gteIR3 = (value & 0x7c00) >> 3;
			break;

		case 30:
			{
				int a;
				gteLZCS = value;

				a = gteLZCS;
				if (a > 0) {
					int i;
					for (i = 31; (a & (1 << i)) == 0 && i >= 0; i--);
					gteLZCR = 31 - i;
				} else if (a < 0) {
					int i;
					a ^= 0xffffffff;
					for (i = 31; (a & (1 << i)) == 0 && i >= 0; i--);
					gteLZCR = 31 - i;
				} else {
					gteLZCR = 32;
				}
			}
			break;

		case 7:
		case 29:
		case 31:
			return;

		default:
			psxRegs.CP2D.r[reg] = value;
	}
}

static inline void CTC2(u32 value, int reg) {
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
			value = value & 0x7ffff000;
			if (value & 0x7f87e000) value |= 0x80000000;
			break;
	}

	psxRegs.CP2C.r[reg] = value;
}

void gteMFC2() {
	if (!_Rt_) return;
	psxRegs.GPR.r[_Rt_] = MFC2(_Rd_);
}

void gteCFC2() {
	if (!_Rt_) return;
	psxRegs.GPR.r[_Rt_] = psxRegs.CP2C.r[_Rd_];
}

void gteMTC2() {
	MTC2(psxRegs.GPR.r[_Rt_], _Rd_);
}

void gteCTC2() {
	CTC2(psxRegs.GPR.r[_Rt_], _Rd_);
}

#define _oB_ (psxRegs.GPR.r[_Rs_] + _Imm_)

void gteLWC2() {
	MTC2(psxMemRead32(_oB_), _Rt_);
}

void gteSWC2() {
	psxMemWrite32(_oB_, MFC2(_Rt_));
}

void gteRTPS() {
	int quotient;

#ifdef GTE_LOG
	GTE_LOG("GTE RTPS\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)gteTRX << 12) + (gteR11 * gteVX0) + (gteR12 * gteVY0) + (gteR13 * gteVZ0)) >> 12);
	gteMAC2 = A2((((s64)gteTRY << 12) + (gteR21 * gteVX0) + (gteR22 * gteVY0) + (gteR23 * gteVZ0)) >> 12);
	gteMAC3 = A3((((s64)gteTRZ << 12) + (gteR31 * gteVX0) + (gteR32 * gteVY0) + (gteR33 * gteVZ0)) >> 12);
	gteIR1 = limB1(gteMAC1, 0);
	gteIR2 = limB2(gteMAC2, 0);
	gteIR3 = limB3(gteMAC3, 0);
	gteSZ0 = gteSZ1;
	gteSZ1 = gteSZ2;
	gteSZ2 = gteSZ3;
	gteSZ3 = limD(gteMAC3);
	quotient = limE(DIVIDE(gteH, gteSZ3));
	gteSXY0 = gteSXY1;
	gteSXY1 = gteSXY2;
	gteSX2 = limG1(F((s64)gteOFX + ((s64)gteIR1 * quotient)) >> 16);
	gteSY2 = limG2(F((s64)gteOFY + ((s64)gteIR2 * quotient)) >> 16);

	gteMAC0 = F((s64)(gteDQB + ((s64)gteDQA * quotient)) >> 12);
	gteIR0 = limH(gteMAC0);
}

void gteRTPT() {
	int quotient;
	int v;
	s32 vx, vy, vz;

#ifdef GTE_LOG
	GTE_LOG("GTE RTPT\n");
#endif
	gteFLAG = 0;

	gteSZ0 = gteSZ3;
	for (v = 0; v < 3; v++) {
		vx = VX(v);
		vy = VY(v);
		vz = VZ(v);
		gteMAC1 = A1((((s64)gteTRX << 12) + (gteR11 * vx) + (gteR12 * vy) + (gteR13 * vz)) >> 12);
		gteMAC2 = A2((((s64)gteTRY << 12) + (gteR21 * vx) + (gteR22 * vy) + (gteR23 * vz)) >> 12);
		gteMAC3 = A3((((s64)gteTRZ << 12) + (gteR31 * vx) + (gteR32 * vy) + (gteR33 * vz)) >> 12);
		gteIR1 = limB1(gteMAC1, 0);
		gteIR2 = limB2(gteMAC2, 0);
		gteIR3 = limB3(gteMAC3, 0);
		fSZ(v) = limD(gteMAC3);
		quotient = limE(DIVIDE(gteH, fSZ(v)));
		fSX(v) = limG1(F((s64)gteOFX + ((s64)gteIR1 * quotient)) >> 16);
		fSY(v) = limG2(F((s64)gteOFY + ((s64)gteIR2 * quotient)) >> 16);
	}
	gteMAC0 = F((s64)(gteDQB + ((s64)gteDQA * quotient)) >> 12);
	gteIR0 = limH(gteMAC0);
}

void gteMVMVA() {
	int shift = 12 * GTE_SF(gteop);
	int mx = GTE_MX(gteop);
	int v = GTE_V(gteop);
	int cv = GTE_CV(gteop);
	int lm = GTE_LM(gteop);
	s32 vx = VX(v);
	s32 vy = VY(v);
	s32 vz = VZ(v);

#ifdef GTE_LOG
	GTE_LOG("GTE MVMVA\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)CV1(cv) << 12) + (MX11(mx) * vx) + (MX12(mx) * vy) + (MX13(mx) * vz)) >> shift);
	gteMAC2 = A2((((s64)CV2(cv) << 12) + (MX21(mx) * vx) + (MX22(mx) * vy) + (MX23(mx) * vz)) >> shift);
	gteMAC3 = A3((((s64)CV3(cv) << 12) + (MX31(mx) * vx) + (MX32(mx) * vy) + (MX33(mx) * vz)) >> shift);

	gteIR1 = limB1(gteMAC1, lm);
	gteIR2 = limB2(gteMAC2, lm);
	gteIR3 = limB3(gteMAC3, lm);
}

void gteNCLIP() {
#ifdef GTE_LOG
	GTE_LOG("GTE NCLIP\n");
#endif
	gteFLAG = 0;

	gteMAC0 = F((s64)gteSX0 * (gteSY1 - gteSY2) +
				gteSX1 * (gteSY2 - gteSY0) +
				gteSX2 * (gteSY0 - gteSY1));
}

void gteAVSZ3() {
#ifdef GTE_LOG
	GTE_LOG("GTE AVSZ3\n");
#endif
	gteFLAG = 0;

	gteMAC0 = F((s64)(gteZSF3 * gteSZ1) + (gteZSF3 * gteSZ2) + (gteZSF3 * gteSZ3));
	gteOTZ = limD(gteMAC0 >> 12);
}

void gteAVSZ4() {
#ifdef GTE_LOG
	GTE_LOG("GTE AVSZ4\n");
#endif
	gteFLAG = 0;

	gteMAC0 = F((s64)(gteZSF4 * (gteSZ0 + gteSZ1 + gteSZ2 + gteSZ3)));
	gteOTZ = limD(gteMAC0 >> 12);
}

void gteSQR() {
	int shift = 12 * GTE_SF(gteop);
	int lm = GTE_LM(gteop);

#ifdef GTE_LOG
	GTE_LOG("GTE SQR\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((gteIR1 * gteIR1) >> shift);
	gteMAC2 = A2((gteIR2 * gteIR2) >> shift);
	gteMAC3 = A3((gteIR3 * gteIR3) >> shift);
	gteIR1 = limB1(gteMAC1 >> shift, lm);
	gteIR2 = limB2(gteMAC2 >> shift, lm);
	gteIR3 = limB3(gteMAC3 >> shift, lm);
}

void gteNCCS() {
#ifdef GTE_LOG
	GTE_LOG("GTE NCCS\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)gteL11 * gteVX0) + (gteL12 * gteVY0) + (gteL13 * gteVZ0)) >> 12);
	gteMAC2 = A2((((s64)gteL21 * gteVX0) + (gteL22 * gteVY0) + (gteL23 * gteVZ0)) >> 12);
	gteMAC3 = A3((((s64)gteL31 * gteVX0) + (gteL32 * gteVY0) + (gteL33 * gteVZ0)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
	gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
	gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
	gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
	gteMAC1 = A1(((s64)gteR * gteIR1) >> 8);
	gteMAC2 = A2(((s64)gteG * gteIR2) >> 8);
	gteMAC3 = A3(((s64)gteB * gteIR3) >> 8);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteNCCT() {
	int v;
	s32 vx, vy, vz;

#ifdef GTE_LOG
	GTE_LOG("GTE NCCT\n");
#endif
	gteFLAG = 0;

	for (v = 0; v < 3; v++) {
		vx = VX(v);
		vy = VY(v);
		vz = VZ(v);
		gteMAC1 = A1((((s64)gteL11 * vx) + (gteL12 * vy) + (gteL13 * vz)) >> 12);
		gteMAC2 = A2((((s64)gteL21 * vx) + (gteL22 * vy) + (gteL23 * vz)) >> 12);
		gteMAC3 = A3((((s64)gteL31 * vx) + (gteL32 * vy) + (gteL33 * vz)) >> 12);
		gteIR1 = limB1(gteMAC1, 1);
		gteIR2 = limB2(gteMAC2, 1);
		gteIR3 = limB3(gteMAC3, 1);
		gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
		gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
		gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
		gteIR1 = limB1(gteMAC1, 1);
		gteIR2 = limB2(gteMAC2, 1);
		gteIR3 = limB3(gteMAC3, 1);
		gteMAC1 = A1(((s64)gteR * gteIR1) >> 8);
		gteMAC2 = A2(((s64)gteG * gteIR2) >> 8);
		gteMAC3 = A3(((s64)gteB * gteIR3) >> 8);

		gteRGB0 = gteRGB1;
		gteRGB1 = gteRGB2;
		gteCODE2 = gteCODE;
		gteR2 = limC1(gteMAC1 >> 4);
		gteG2 = limC2(gteMAC2 >> 4);
		gteB2 = limC3(gteMAC3 >> 4);
	}
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
}

void gteNCDS() {
#ifdef GTE_LOG
	GTE_LOG("GTE NCDS\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)gteL11 * gteVX0) + (gteL12 * gteVY0) + (gteL13 * gteVZ0)) >> 12);
	gteMAC2 = A2((((s64)gteL21 * gteVX0) + (gteL22 * gteVY0) + (gteL23 * gteVZ0)) >> 12);
	gteMAC3 = A3((((s64)gteL31 * gteVX0) + (gteL32 * gteVY0) + (gteL33 * gteVZ0)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
	gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
	gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
	gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
	gteMAC1 = A1(((((s64)gteR << 4) * gteIR1) + (gteIR0 * limB1(gteRFC - ((gteR * gteIR1) >> 8), 0))) >> 12);
	gteMAC2 = A2(((((s64)gteG << 4) * gteIR2) + (gteIR0 * limB2(gteGFC - ((gteG * gteIR2) >> 8), 0))) >> 12);
	gteMAC3 = A3(((((s64)gteB << 4) * gteIR3) + (gteIR0 * limB3(gteBFC - ((gteB * gteIR3) >> 8), 0))) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteNCDT() {
	int v;
	s32 vx, vy, vz;

#ifdef GTE_LOG
	GTE_LOG("GTE NCDT\n");
#endif
	gteFLAG = 0;

	for (v = 0; v < 3; v++) {
		vx = VX(v);
		vy = VY(v);
		vz = VZ(v);
		gteMAC1 = A1((((s64)gteL11 * vx) + (gteL12 * vy) + (gteL13 * vz)) >> 12);
		gteMAC2 = A2((((s64)gteL21 * vx) + (gteL22 * vy) + (gteL23 * vz)) >> 12);
		gteMAC3 = A3((((s64)gteL31 * vx) + (gteL32 * vy) + (gteL33 * vz)) >> 12);
		gteIR1 = limB1(gteMAC1, 1);
		gteIR2 = limB2(gteMAC2, 1);
		gteIR3 = limB3(gteMAC3, 1);
		gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
		gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
		gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
		gteIR1 = limB1(gteMAC1, 1);
		gteIR2 = limB2(gteMAC2, 1);
		gteIR3 = limB3(gteMAC3, 1);
		gteMAC1 = A1(((((s64)gteR << 4) * gteIR1) + (gteIR0 * limB1(gteRFC - ((gteR * gteIR1) >> 8), 0))) >> 12);
		gteMAC2 = A2(((((s64)gteG << 4) * gteIR2) + (gteIR0 * limB2(gteGFC - ((gteG * gteIR2) >> 8), 0))) >> 12);
		gteMAC3 = A3(((((s64)gteB << 4) * gteIR3) + (gteIR0 * limB3(gteBFC - ((gteB * gteIR3) >> 8), 0))) >> 12);

		gteRGB0 = gteRGB1;
		gteRGB1 = gteRGB2;
		gteCODE2 = gteCODE;
		gteR2 = limC1(gteMAC1 >> 4);
		gteG2 = limC2(gteMAC2 >> 4);
		gteB2 = limC3(gteMAC3 >> 4);
	}
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
}

void gteOP() {
	int shift = 12 * GTE_SF(gteop);
	int lm = GTE_LM(gteop);

#ifdef GTE_LOG
	GTE_LOG("GTE OP\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1(((s64)(gteR22 * gteIR3) - (gteR33 * gteIR2)) >> shift);
	gteMAC2 = A2(((s64)(gteR33 * gteIR1) - (gteR11 * gteIR3)) >> shift);
	gteMAC3 = A3(((s64)(gteR11 * gteIR2) - (gteR22 * gteIR1)) >> shift);
	gteIR1 = limB1(gteMAC1, lm);
	gteIR2 = limB2(gteMAC2, lm);
	gteIR3 = limB3(gteMAC3, lm);
}

void gteDCPL() {
	int lm = GTE_LM(gteop);

	s64 RIR1 = ((s64)gteR * gteIR1) >> 8;
	s64 GIR2 = ((s64)gteG * gteIR2) >> 8;
	s64 BIR3 = ((s64)gteB * gteIR3) >> 8;

#ifdef GTE_LOG
	GTE_LOG("GTE DCPL\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1(RIR1 + ((gteIR0 * limB1(gteRFC - RIR1, 0)) >> 12));
	gteMAC2 = A2(GIR2 + ((gteIR0 * limB1(gteGFC - GIR2, 0)) >> 12));
	gteMAC3 = A3(BIR3 + ((gteIR0 * limB1(gteBFC - BIR3, 0)) >> 12));

	gteIR1 = limB1(gteMAC1, lm);
	gteIR2 = limB2(gteMAC2, lm);
	gteIR3 = limB3(gteMAC3, lm);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteGPF() {
	int shift = 12 * GTE_SF(gteop);

#ifdef GTE_LOG
	GTE_LOG("GTE GPF\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1(((s64)gteIR0 * gteIR1) >> shift);
	gteMAC2 = A2(((s64)gteIR0 * gteIR2) >> shift);
	gteMAC3 = A3(((s64)gteIR0 * gteIR3) >> shift);
	gteIR1 = limB1(gteMAC1, 0);
	gteIR2 = limB2(gteMAC2, 0);
	gteIR3 = limB3(gteMAC3, 0);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteGPL() {
	int shift = 12 * GTE_SF(gteop);

#ifdef GTE_LOG
	GTE_LOG("GTE GPL\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)gteMAC1 << shift) + (gteIR0 * gteIR1)) >> shift);
	gteMAC2 = A2((((s64)gteMAC2 << shift) + (gteIR0 * gteIR2)) >> shift);
	gteMAC3 = A3((((s64)gteMAC3 << shift) + (gteIR0 * gteIR3)) >> shift);
	gteIR1 = limB1(gteMAC1, 0);
	gteIR2 = limB2(gteMAC2, 0);
	gteIR3 = limB3(gteMAC3, 0);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteDPCS() {
	int shift = 12 * GTE_SF(gteop);

#ifdef GTE_LOG
	GTE_LOG("GTE DPCS\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1(((gteR << 16) + (gteIR0 * limB1(A1((s64)gteRFC - (gteR << 4)) << (12 - shift), 0))) >> 12);
	gteMAC2 = A2(((gteG << 16) + (gteIR0 * limB2(A2((s64)gteGFC - (gteG << 4)) << (12 - shift), 0))) >> 12);
	gteMAC3 = A3(((gteB << 16) + (gteIR0 * limB3(A3((s64)gteBFC - (gteB << 4)) << (12 - shift), 0))) >> 12);

	gteIR1 = limB1(gteMAC1, 0);
	gteIR2 = limB2(gteMAC2, 0);
	gteIR3 = limB3(gteMAC3, 0);
	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteDPCT() {
	int v;

#ifdef GTE_LOG
	GTE_LOG("GTE DPCT\n");
#endif
	gteFLAG = 0;

	for (v = 0; v < 3; v++) {
		gteMAC1 = A1((((s64)gteR0 << 16) + ((s64)gteIR0 * (limB1(gteRFC - (gteR0 << 4), 0)))) >> 12);
		gteMAC2 = A2((((s64)gteG0 << 16) + ((s64)gteIR0 * (limB1(gteGFC - (gteG0 << 4), 0)))) >> 12);
		gteMAC3 = A3((((s64)gteB0 << 16) + ((s64)gteIR0 * (limB1(gteBFC - (gteB0 << 4), 0)))) >> 12);

		gteRGB0 = gteRGB1;
		gteRGB1 = gteRGB2;
		gteCODE2 = gteCODE;
		gteR2 = limC1(gteMAC1 >> 4);
		gteG2 = limC2(gteMAC2 >> 4);
		gteB2 = limC3(gteMAC3 >> 4);
	}
	gteIR1 = limB1(gteMAC1, 0);
	gteIR2 = limB2(gteMAC2, 0);
	gteIR3 = limB3(gteMAC3, 0);
}

void gteNCS() {
#ifdef GTE_LOG
	GTE_LOG("GTE NCS\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)gteL11 * gteVX0) + (gteL12 * gteVY0) + (gteL13 * gteVZ0)) >> 12);
	gteMAC2 = A2((((s64)gteL21 * gteVX0) + (gteL22 * gteVY0) + (gteL23 * gteVZ0)) >> 12);
	gteMAC3 = A3((((s64)gteL31 * gteVX0) + (gteL32 * gteVY0) + (gteL33 * gteVZ0)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
	gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
	gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
	gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteNCT() {
	int v;
	s32 vx, vy, vz;

#ifdef GTE_LOG
	GTE_LOG("GTE NCT\n");
#endif
	gteFLAG = 0;

	for (v = 0; v < 3; v++) {
		vx = VX(v);
		vy = VY(v);
		vz = VZ(v);
		gteMAC1 = A1((((s64)gteL11 * vx) + (gteL12 * vy) + (gteL13 * vz)) >> 12);
		gteMAC2 = A2((((s64)gteL21 * vx) + (gteL22 * vy) + (gteL23 * vz)) >> 12);
		gteMAC3 = A3((((s64)gteL31 * vx) + (gteL32 * vy) + (gteL33 * vz)) >> 12);
		gteIR1 = limB1(gteMAC1, 1);
		gteIR2 = limB2(gteMAC2, 1);
		gteIR3 = limB3(gteMAC3, 1);
		gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
		gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
		gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
		gteRGB0 = gteRGB1;
		gteRGB1 = gteRGB2;
		gteCODE2 = gteCODE;
		gteR2 = limC1(gteMAC1 >> 4);
		gteG2 = limC2(gteMAC2 >> 4);
		gteB2 = limC3(gteMAC3 >> 4);
	}
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
}

void gteCC() {
#ifdef GTE_LOG
	GTE_LOG("GTE CC\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
	gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
	gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
	gteMAC1 = A1(((s64)gteR * gteIR1) >> 8);
	gteMAC2 = A2(((s64)gteG * gteIR2) >> 8);
	gteMAC3 = A3(((s64)gteB * gteIR3) >> 8);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteINTPL() {
	int shift = 12 * GTE_SF(gteop);
	int lm = GTE_LM(gteop);

#ifdef GTE_LOG
	GTE_LOG("GTE INTPL\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1(((gteIR1 << 12) + (gteIR0 * limB1(((s64)gteRFC - gteIR1), 0))) >> shift);
	gteMAC2 = A2(((gteIR2 << 12) + (gteIR0 * limB2(((s64)gteGFC - gteIR2), 0))) >> shift);
	gteMAC3 = A3(((gteIR3 << 12) + (gteIR0 * limB3(((s64)gteBFC - gteIR3), 0))) >> shift);
	gteIR1 = limB1(gteMAC1, lm);
	gteIR2 = limB2(gteMAC2, lm);
	gteIR3 = limB3(gteMAC3, lm);
	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

void gteCDP() {
#ifdef GTE_LOG
	GTE_LOG("GTE CDP\n");
#endif
	gteFLAG = 0;

	gteMAC1 = A1((((s64)gteRBK << 12) + (gteLR1 * gteIR1) + (gteLR2 * gteIR2) + (gteLR3 * gteIR3)) >> 12);
	gteMAC2 = A2((((s64)gteGBK << 12) + (gteLG1 * gteIR1) + (gteLG2 * gteIR2) + (gteLG3 * gteIR3)) >> 12);
	gteMAC3 = A3((((s64)gteBBK << 12) + (gteLB1 * gteIR1) + (gteLB2 * gteIR2) + (gteLB3 * gteIR3)) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);
	gteMAC1 = A1(((((s64)gteR << 4) * gteIR1) + (gteIR0 * limB1(gteRFC - ((gteR * gteIR1) >> 8), 0))) >> 12);
	gteMAC2 = A2(((((s64)gteG << 4) * gteIR2) + (gteIR0 * limB2(gteGFC - ((gteG * gteIR2) >> 8), 0))) >> 12);
	gteMAC3 = A3(((((s64)gteB << 4) * gteIR3) + (gteIR0 * limB3(gteBFC - ((gteB * gteIR3) >> 8), 0))) >> 12);
	gteIR1 = limB1(gteMAC1, 1);
	gteIR2 = limB2(gteMAC2, 1);
	gteIR3 = limB3(gteMAC3, 1);

	gteRGB0 = gteRGB1;
	gteRGB1 = gteRGB2;
	gteCODE2 = gteCODE;
	gteR2 = limC1(gteMAC1 >> 4);
	gteG2 = limC2(gteMAC2 >> 4);
	gteB2 = limC3(gteMAC3 >> 4);
}

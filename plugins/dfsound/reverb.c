/***************************************************************************
                          reverb.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de

 Portions (C) Gražvydas "notaz" Ignotas, 2010-2011
 Portions (C) SPU2-X, gigaherz, Pcsx2 Development Team

 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#include "stdafx.h"
#include "spu.h"
#include <assert.h>

#define _IN_REVERB

// will be included from spu.c
#ifdef _IN_SPU

////////////////////////////////////////////////////////////////////////
// START REVERB
////////////////////////////////////////////////////////////////////////

INLINE void StartREVERB(int ch)
{
 if(spu.s_chan[ch].bReverb && (spu.spuCtrl&0x80))      // reverb possible?
  {
   spu.s_chan[ch].bRVBActive=!!spu_config.iUseReverb;
  }
 else spu.s_chan[ch].bRVBActive=0;                     // else -> no reverb
}

////////////////////////////////////////////////////////////////////////

INLINE int rvb_wrap(int ofs, int space)
{
#if 0
 int mask = (0x3ffff - ofs) >> 31;
 ofs = ofs - (space & mask);
#else
 if (ofs >= 0x40000)
  ofs -= space;
#endif
 //assert(ofs >= 0x40000 - space);
 //assert(ofs < 0x40000);
 return ofs;
}

INLINE int rvb2ram_offs(int curr, int space, int ofs)
{
 ofs += curr;
 return rvb_wrap(ofs, space);
}

// get_buffer content helper: takes care about wraps
#define g_buffer(var) \
 ((int)(signed short)LE16TOH(spuMem[rvb2ram_offs(curr_addr, space, var)]))

// saturate iVal and store it as var
#define s_buffer_w(var, iVal) \
 ssat32_to_16(iVal); \
 spuMem[rvb2ram_offs(curr_addr, space, var)] = HTOLE16(iVal)

////////////////////////////////////////////////////////////////////////

// from nocash psx-spx
static void MixREVERB(int *SSumLR, int *RVB, int ns_to, int curr_addr)
{
 unsigned short *spuMem = spu.spuMem;
 const REVERBInfo *rvb = spu.rvb;
 int space = 0x40000 - rvb->StartAddr;
 int mlsame_m2o = rvb->mLSAME + space - 1;
 int mrsame_m2o = rvb->mRSAME + space - 1;
 int mldiff_m2o = rvb->mLDIFF + space - 1;
 int mrdiff_m2o = rvb->mRDIFF + space - 1;
 int vCOMB1 = rvb->vCOMB1, vCOMB2 = rvb->vCOMB2;
 int vCOMB3 = rvb->vCOMB3, vCOMB4 = rvb->vCOMB4;
 int vAPF1 = rvb->vAPF1, vAPF2 = rvb->vAPF2;
 int vIIR = rvb->vIIR;
 int vWALL = rvb->vWALL;
 int ns;

 if (mlsame_m2o >= space) mlsame_m2o -= space;
 if (mrsame_m2o >= space) mrsame_m2o -= space;
 if (mldiff_m2o >= space) mldiff_m2o -= space;
 if (mrdiff_m2o >= space) mrdiff_m2o -= space;

 for (ns = 0; ns < ns_to * 2; )
  {
   int Lin = RVB[ns]   * rvb->vLIN;
   int Rin = RVB[ns+1] * rvb->vRIN;
   int mlsame_m2 = g_buffer(mlsame_m2o) << 15; // -1
   int mrsame_m2 = g_buffer(mrsame_m2o) << 15;
   int mldiff_m2 = g_buffer(mldiff_m2o) << 15;
   int mrdiff_m2 = g_buffer(mrdiff_m2o) << 15;
   int Lout, Rout;

   mlsame_m2 += ((Lin + g_buffer(rvb->dLSAME) * vWALL - mlsame_m2) >> 15) * vIIR;
   mrsame_m2 += ((Rin + g_buffer(rvb->dRSAME) * vWALL - mrsame_m2) >> 15) * vIIR;
   mldiff_m2 += ((Lin + g_buffer(rvb->dLDIFF) * vWALL - mldiff_m2) >> 15) * vIIR;
   mrdiff_m2 += ((Rin + g_buffer(rvb->dRDIFF) * vWALL - mrdiff_m2) >> 15) * vIIR;
   mlsame_m2 >>= 15; s_buffer_w(rvb->mLSAME, mlsame_m2);
   mrsame_m2 >>= 15; s_buffer_w(rvb->mRSAME, mrsame_m2);
   mldiff_m2 >>= 15; s_buffer_w(rvb->mLDIFF, mldiff_m2);
   mrdiff_m2 >>= 15; s_buffer_w(rvb->mRDIFF, mrdiff_m2);

   Lout = vCOMB1 * g_buffer(rvb->mLCOMB1) + vCOMB2 * g_buffer(rvb->mLCOMB2)
        + vCOMB3 * g_buffer(rvb->mLCOMB3) + vCOMB4 * g_buffer(rvb->mLCOMB4);
   Rout = vCOMB1 * g_buffer(rvb->mRCOMB1) + vCOMB2 * g_buffer(rvb->mRCOMB2)
        + vCOMB3 * g_buffer(rvb->mRCOMB3) + vCOMB4 * g_buffer(rvb->mRCOMB4);

   preload(SSumLR + ns + 64*2/4 - 4);

   Lout -= vAPF1 * g_buffer(rvb->mLAPF1_dAPF1); Lout >>= 15;
   Rout -= vAPF1 * g_buffer(rvb->mRAPF1_dAPF1); Rout >>= 15;
   s_buffer_w(rvb->mLAPF1, Lout);
   s_buffer_w(rvb->mRAPF1, Rout);
   Lout = Lout * vAPF1 + (g_buffer(rvb->mLAPF1_dAPF1) << 15);
   Rout = Rout * vAPF1 + (g_buffer(rvb->mRAPF1_dAPF1) << 15);

   preload(RVB + ns + 64*2/4 - 4);

   Lout -= vAPF2 * g_buffer(rvb->mLAPF2_dAPF2); Lout >>= 15;
   Rout -= vAPF2 * g_buffer(rvb->mRAPF2_dAPF2); Rout >>= 15;
   s_buffer_w(rvb->mLAPF2, Lout);
   s_buffer_w(rvb->mRAPF2, Rout);
   Lout = Lout * vAPF2 + (g_buffer(rvb->mLAPF2_dAPF2) << 15);
   Rout = Rout * vAPF2 + (g_buffer(rvb->mRAPF2_dAPF2) << 15);

   Lout = ((Lout >> 15) * rvb->VolLeft)  >> 15;
   Rout = ((Rout >> 15) * rvb->VolRight) >> 15;

   SSumLR[ns++] += Lout;
   SSumLR[ns++] += Rout;
   SSumLR[ns++] += Lout;
   SSumLR[ns++] += Rout;

   curr_addr++;
   curr_addr = rvb_wrap(curr_addr, space);
  }
}

static void MixREVERB_off(int *SSumLR, int ns_to, int curr_addr)
{
 const REVERBInfo *rvb = spu.rvb;
 unsigned short *spuMem = spu.spuMem;
 int space = 0x40000 - rvb->StartAddr;
 int Lout, Rout, ns;

 for (ns = 0; ns < ns_to * 2; )
  {
   preload(SSumLR + ns + 64*2/4 - 4);

   // todo: is this missing COMB and APF1?
   Lout = g_buffer(rvb->mLAPF2_dAPF2);
   Rout = g_buffer(rvb->mLAPF2_dAPF2);

   Lout = (Lout * rvb->VolLeft)  >> 15;
   Rout = (Rout * rvb->VolRight) >> 15;

   SSumLR[ns++] += Lout;
   SSumLR[ns++] += Rout;
   SSumLR[ns++] += Lout;
   SSumLR[ns++] += Rout;

   curr_addr++;
   if (curr_addr >= 0x40000) curr_addr = rvb->StartAddr;
  }
}

static void REVERBPrep(void)
{
 REVERBInfo *rvb = spu.rvb;
 int space, t;

 t = regAreaGet(H_SPUReverbAddr);
 if (t == 0xFFFF || t <= 0x200)
  spu.rvb->StartAddr = spu.rvb->CurrAddr = 0;
 else if (spu.rvb->StartAddr != (t << 2))
  spu.rvb->StartAddr = spu.rvb->CurrAddr = t << 2;

 space = 0x40000 - rvb->StartAddr;

 #define prep_offs(v, r) \
   t = spu.regArea[(0x1c0 + r) >> 1] * 4; \
   while (t >= space) \
     t -= space; \
   rvb->v = t
 #define prep_offs2(d, r1, r2) \
   t = spu.regArea[(0x1c0 + r1) >> 1] * 4; \
   t -= spu.regArea[(0x1c0 + r2) >> 1] * 4; \
   while (t < 0) \
     t += space; \
   while (t >= space) \
     t -= space; \
   rvb->d = t

 prep_offs(mLSAME,  0x14);
 prep_offs(mRSAME,  0x16);
 prep_offs(mLCOMB1, 0x18);
 prep_offs(mRCOMB1, 0x1a);
 prep_offs(mLCOMB2, 0x1c);
 prep_offs(mRCOMB2, 0x1e);
 prep_offs(dLSAME,  0x20);
 prep_offs(dRSAME,  0x22);
 prep_offs(mLDIFF,  0x24);
 prep_offs(mRDIFF,  0x26);
 prep_offs(mLCOMB3, 0x28);
 prep_offs(mRCOMB3, 0x2a);
 prep_offs(mLCOMB4, 0x2c);
 prep_offs(mRCOMB4, 0x2e);
 prep_offs(dLDIFF,  0x30);
 prep_offs(dRDIFF,  0x32);
 prep_offs(mLAPF1,  0x34);
 prep_offs(mRAPF1,  0x36);
 prep_offs(mLAPF2,  0x38);
 prep_offs(mRAPF2,  0x3a);
 prep_offs2(mLAPF1_dAPF1, 0x34, 0);
 prep_offs2(mRAPF1_dAPF1, 0x36, 0);
 prep_offs2(mLAPF2_dAPF2, 0x38, 2);
 prep_offs2(mRAPF2_dAPF2, 0x3a, 2);

#undef prep_offs
#undef prep_offs2
 rvb->dirty = 0;
}

INLINE void REVERBDo(int *SSumLR, int *RVB, int ns_to, int curr_addr)
{
 if (spu.spuCtrl & 0x80)                               // -> reverb on? oki
 {
  MixREVERB(SSumLR, RVB, ns_to, curr_addr);
 }
 else if (spu.rvb->VolLeft || spu.rvb->VolRight)
 {
  MixREVERB_off(SSumLR, ns_to, curr_addr);
 }
}

////////////////////////////////////////////////////////////////////////

#endif

// vim:shiftwidth=1:expandtab

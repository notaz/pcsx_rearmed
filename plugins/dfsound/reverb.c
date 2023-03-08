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

INLINE int rvb2ram_offs(int curr, int space, int iOff)
{
 iOff += curr;
 if (iOff >= 0x40000) iOff -= space;
 return iOff;
}

// get_buffer content helper: takes care about wraps
#define g_buffer(var) \
 ((int)(signed short)LE16TOH(spu.spuMem[rvb2ram_offs(curr_addr, space, rvb->var)]))

// saturate iVal and store it as var
#define s_buffer(var, iVal) \
 ssat32_to_16(iVal); \
 spu.spuMem[rvb2ram_offs(curr_addr, space, rvb->var)] = HTOLE16(iVal)

#define s_buffer1(var, iVal) \
 ssat32_to_16(iVal); \
 spu.spuMem[rvb2ram_offs(curr_addr, space, rvb->var + 1)] = HTOLE16(iVal)

////////////////////////////////////////////////////////////////////////

// portions based on spu2-x from PCSX2
static void MixREVERB(int *SSumLR, int *RVB, int ns_to, int curr_addr)
{
 const REVERBInfo *rvb = spu.rvb;
 int IIR_ALPHA = rvb->IIR_ALPHA;
 int IIR_COEF = rvb->IIR_COEF;
 int space = 0x40000 - rvb->StartAddr;
 int l, r, ns;

 for (ns = 0; ns < ns_to * 2; )
  {
   int ACC0, ACC1, FB_A0, FB_A1, FB_B0, FB_B1;
   int mix_dest_a0, mix_dest_a1, mix_dest_b0, mix_dest_b1;

   int input_L = RVB[ns]   * rvb->IN_COEF_L;
   int input_R = RVB[ns+1] * rvb->IN_COEF_R;

   int IIR_INPUT_A0 = ((g_buffer(IIR_SRC_A0) * IIR_COEF) + input_L) >> 15;
   int IIR_INPUT_A1 = ((g_buffer(IIR_SRC_A1) * IIR_COEF) + input_R) >> 15;
   int IIR_INPUT_B0 = ((g_buffer(IIR_SRC_B0) * IIR_COEF) + input_L) >> 15;
   int IIR_INPUT_B1 = ((g_buffer(IIR_SRC_B1) * IIR_COEF) + input_R) >> 15;

   int iir_dest_a0 = g_buffer(IIR_DEST_A0);
   int iir_dest_a1 = g_buffer(IIR_DEST_A1);
   int iir_dest_b0 = g_buffer(IIR_DEST_B0);
   int iir_dest_b1 = g_buffer(IIR_DEST_B1);

   int IIR_A0 = iir_dest_a0 + ((IIR_INPUT_A0 - iir_dest_a0) * IIR_ALPHA >> 15);
   int IIR_A1 = iir_dest_a1 + ((IIR_INPUT_A1 - iir_dest_a1) * IIR_ALPHA >> 15);
   int IIR_B0 = iir_dest_b0 + ((IIR_INPUT_B0 - iir_dest_b0) * IIR_ALPHA >> 15);
   int IIR_B1 = iir_dest_b1 + ((IIR_INPUT_B1 - iir_dest_b1) * IIR_ALPHA >> 15);

   preload(SSumLR + ns + 64*2/4 - 4);

   s_buffer1(IIR_DEST_A0, IIR_A0);
   s_buffer1(IIR_DEST_A1, IIR_A1);
   s_buffer1(IIR_DEST_B0, IIR_B0);
   s_buffer1(IIR_DEST_B1, IIR_B1);

   preload(RVB + ns + 64*2/4 - 4);

   ACC0 = (g_buffer(ACC_SRC_A0) * rvb->ACC_COEF_A +
           g_buffer(ACC_SRC_B0) * rvb->ACC_COEF_B +
           g_buffer(ACC_SRC_C0) * rvb->ACC_COEF_C +
           g_buffer(ACC_SRC_D0) * rvb->ACC_COEF_D) >> 15;
   ACC1 = (g_buffer(ACC_SRC_A1) * rvb->ACC_COEF_A +
           g_buffer(ACC_SRC_B1) * rvb->ACC_COEF_B +
           g_buffer(ACC_SRC_C1) * rvb->ACC_COEF_C +
           g_buffer(ACC_SRC_D1) * rvb->ACC_COEF_D) >> 15;

   FB_A0 = g_buffer(FB_SRC_A0);
   FB_A1 = g_buffer(FB_SRC_A1);
   FB_B0 = g_buffer(FB_SRC_B0);
   FB_B1 = g_buffer(FB_SRC_B1);

   mix_dest_a0 = ACC0 - ((FB_A0 * rvb->FB_ALPHA) >> 15);
   mix_dest_a1 = ACC1 - ((FB_A1 * rvb->FB_ALPHA) >> 15);

   mix_dest_b0 = FB_A0 + (((ACC0 - FB_A0) * rvb->FB_ALPHA - FB_B0 * rvb->FB_X) >> 15);
   mix_dest_b1 = FB_A1 + (((ACC1 - FB_A1) * rvb->FB_ALPHA - FB_B1 * rvb->FB_X) >> 15);

   s_buffer(MIX_DEST_A0, mix_dest_a0);
   s_buffer(MIX_DEST_A1, mix_dest_a1);
   s_buffer(MIX_DEST_B0, mix_dest_b0);
   s_buffer(MIX_DEST_B1, mix_dest_b1);

   l = (mix_dest_a0 + mix_dest_b0) / 2;
   r = (mix_dest_a1 + mix_dest_b1) / 2;

   l = (l * rvb->VolLeft)  >> 15; // 15?
   r = (r * rvb->VolRight) >> 15;

   SSumLR[ns++] += l;
   SSumLR[ns++] += r;
   SSumLR[ns++] += l;
   SSumLR[ns++] += r;

   curr_addr++;
   if (curr_addr >= 0x40000) curr_addr = rvb->StartAddr;
  }
}

static void MixREVERB_off(int *SSumLR, int ns_to, int curr_addr)
{
 const REVERBInfo *rvb = spu.rvb;
 int space = 0x40000 - rvb->StartAddr;
 int l, r, ns;

 for (ns = 0; ns < ns_to * 2; )
  {
   preload(SSumLR + ns + 64*2/4 - 4);

   l = (g_buffer(MIX_DEST_A0) + g_buffer(MIX_DEST_B0)) / 2;
   r = (g_buffer(MIX_DEST_A1) + g_buffer(MIX_DEST_B1)) / 2;

   l = (l * rvb->VolLeft)  >> 15;
   r = (r * rvb->VolRight) >> 15;

   SSumLR[ns++] += l;
   SSumLR[ns++] += r;
   SSumLR[ns++] += l;
   SSumLR[ns++] += r;

   curr_addr++;
   if (curr_addr >= 0x40000) curr_addr = rvb->StartAddr;
  }
}

static void REVERBPrep(void)
{
 REVERBInfo *rvb = spu.rvb;
 int space, t;

 t = spu.regArea[(H_SPUReverbAddr - 0xc00) >> 1];
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

 prep_offs(IIR_SRC_A0, 32);
 prep_offs(IIR_SRC_A1, 34);
 prep_offs(IIR_SRC_B0, 36);
 prep_offs(IIR_SRC_B1, 38);
 prep_offs(IIR_DEST_A0, 20);
 prep_offs(IIR_DEST_A1, 22);
 prep_offs(IIR_DEST_B0, 36);
 prep_offs(IIR_DEST_B1, 38);
 prep_offs(ACC_SRC_A0, 24);
 prep_offs(ACC_SRC_A1, 26);
 prep_offs(ACC_SRC_B0, 28);
 prep_offs(ACC_SRC_B1, 30);
 prep_offs(ACC_SRC_C0, 40);
 prep_offs(ACC_SRC_C1, 42);
 prep_offs(ACC_SRC_D0, 44);
 prep_offs(ACC_SRC_D1, 46);
 prep_offs(MIX_DEST_A0, 52);
 prep_offs(MIX_DEST_A1, 54);
 prep_offs(MIX_DEST_B0, 56);
 prep_offs(MIX_DEST_B1, 58);
 prep_offs2(FB_SRC_A0, 52, 0);
 prep_offs2(FB_SRC_A1, 54, 0);
 prep_offs2(FB_SRC_B0, 56, 2);
 prep_offs2(FB_SRC_B1, 58, 2);

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

/*
-----------------------------------------------------------------------------
PSX reverb hardware notes
by Neill Corlett
-----------------------------------------------------------------------------

Yadda yadda disclaimer yadda probably not perfect yadda well it's okay anyway
yadda yadda.

-----------------------------------------------------------------------------

Basics
------

- The reverb buffer is 22khz 16-bit mono PCM.
- It starts at the reverb address given by 1DA2, extends to
  the end of sound RAM, and wraps back to the 1DA2 address.

Setting the address at 1DA2 resets the current reverb work address.

This work address ALWAYS increments every 1/22050 sec., regardless of
whether reverb is enabled (bit 7 of 1DAA set).

And the contents of the reverb buffer ALWAYS play, scaled by the
"reverberation depth left/right" volumes (1D84/1D86).
(which, by the way, appear to be scaled so 3FFF=approx. 1.0, 4000=-1.0)

-----------------------------------------------------------------------------

Register names
--------------

These are probably not their real names.
These are probably not even correct names.
We will use them anyway, because we can.

1DC0: FB_SRC_A       (offset)
1DC2: FB_SRC_B       (offset)
1DC4: IIR_ALPHA      (coef.)
1DC6: ACC_COEF_A     (coef.)
1DC8: ACC_COEF_B     (coef.)
1DCA: ACC_COEF_C     (coef.)
1DCC: ACC_COEF_D     (coef.)
1DCE: IIR_COEF       (coef.)
1DD0: FB_ALPHA       (coef.)
1DD2: FB_X           (coef.)
1DD4: IIR_DEST_A0    (offset)
1DD6: IIR_DEST_A1    (offset)
1DD8: ACC_SRC_A0     (offset)
1DDA: ACC_SRC_A1     (offset)
1DDC: ACC_SRC_B0     (offset)
1DDE: ACC_SRC_B1     (offset)
1DE0: IIR_SRC_A0     (offset)
1DE2: IIR_SRC_A1     (offset)
1DE4: IIR_DEST_B0    (offset)
1DE6: IIR_DEST_B1    (offset)
1DE8: ACC_SRC_C0     (offset)
1DEA: ACC_SRC_C1     (offset)
1DEC: ACC_SRC_D0     (offset)
1DEE: ACC_SRC_D1     (offset)
1DF0: IIR_SRC_B1     (offset)
1DF2: IIR_SRC_B0     (offset)
1DF4: MIX_DEST_A0    (offset)
1DF6: MIX_DEST_A1    (offset)
1DF8: MIX_DEST_B0    (offset)
1DFA: MIX_DEST_B1    (offset)
1DFC: IN_COEF_L      (coef.)
1DFE: IN_COEF_R      (coef.)

The coefficients are signed fractional values.
-32768 would be -1.0
 32768 would be  1.0 (if it were possible... the highest is of course 32767)

The offsets are (byte/8) offsets into the reverb buffer.
i.e. you multiply them by 8, you get byte offsets.
You can also think of them as (samples/4) offsets.
They appear to be signed.  They can be negative.
None of the documented presets make them negative, though.

Yes, 1DF0 and 1DF2 appear to be backwards.  Not a typo.

-----------------------------------------------------------------------------

What it does
------------

We take all reverb sources:
- regular channels that have the reverb bit on
- cd and external sources, if their reverb bits are on
and mix them into one stereo 44100hz signal.

Lowpass/downsample that to 22050hz.  The PSX uses a proper bandlimiting
algorithm here, but I haven't figured out the hysterically exact specifics.
I use an 8-tap filter with these coefficients, which are nice but probably
not the real ones:

0.037828187894
0.157538631280
0.321159685278
0.449322115345
0.449322115345
0.321159685278
0.157538631280
0.037828187894

So we have two input samples (INPUT_SAMPLE_L, INPUT_SAMPLE_R) every 22050hz.

* IN MY EMULATION, I divide these by 2 to make it clip less.
  (and of course the L/R output coefficients are adjusted to compensate)
  The real thing appears to not do this.

At every 22050hz tick:
- If the reverb bit is enabled (bit 7 of 1DAA), execute the reverb
  steady-state algorithm described below
- AFTERWARDS, retrieve the "wet out" L and R samples from the reverb buffer
  (This part may not be exactly right and I guessed at the coefs. TODO: check later.)
  L is: 0.333 * (buffer[MIX_DEST_A0] + buffer[MIX_DEST_B0])
  R is: 0.333 * (buffer[MIX_DEST_A1] + buffer[MIX_DEST_B1])
- Advance the current buffer position by 1 sample

The wet out L and R are then upsampled to 44100hz and played at the
"reverberation depth left/right" (1D84/1D86) volume, independent of the main
volume.

-----------------------------------------------------------------------------

Reverb steady-state
-------------------

The reverb steady-state algorithm is fairly clever, and of course by
"clever" I mean "batshit insane".

buffer[x] is relative to the current buffer position, not the beginning of
the buffer.  Note that all buffer offsets must wrap around so they're
contained within the reverb work area.

Clipping is performed at the end... maybe also sooner, but definitely at
the end.

IIR_INPUT_A0 = buffer[IIR_SRC_A0] * IIR_COEF + INPUT_SAMPLE_L * IN_COEF_L;
IIR_INPUT_A1 = buffer[IIR_SRC_A1] * IIR_COEF + INPUT_SAMPLE_R * IN_COEF_R;
IIR_INPUT_B0 = buffer[IIR_SRC_B0] * IIR_COEF + INPUT_SAMPLE_L * IN_COEF_L;
IIR_INPUT_B1 = buffer[IIR_SRC_B1] * IIR_COEF + INPUT_SAMPLE_R * IN_COEF_R;

IIR_A0 = IIR_INPUT_A0 * IIR_ALPHA + buffer[IIR_DEST_A0] * (1.0 - IIR_ALPHA);
IIR_A1 = IIR_INPUT_A1 * IIR_ALPHA + buffer[IIR_DEST_A1] * (1.0 - IIR_ALPHA);
IIR_B0 = IIR_INPUT_B0 * IIR_ALPHA + buffer[IIR_DEST_B0] * (1.0 - IIR_ALPHA);
IIR_B1 = IIR_INPUT_B1 * IIR_ALPHA + buffer[IIR_DEST_B1] * (1.0 - IIR_ALPHA);

buffer[IIR_DEST_A0 + 1sample] = IIR_A0;
buffer[IIR_DEST_A1 + 1sample] = IIR_A1;
buffer[IIR_DEST_B0 + 1sample] = IIR_B0;
buffer[IIR_DEST_B1 + 1sample] = IIR_B1;

ACC0 = buffer[ACC_SRC_A0] * ACC_COEF_A +
       buffer[ACC_SRC_B0] * ACC_COEF_B +
       buffer[ACC_SRC_C0] * ACC_COEF_C +
       buffer[ACC_SRC_D0] * ACC_COEF_D;
ACC1 = buffer[ACC_SRC_A1] * ACC_COEF_A +
       buffer[ACC_SRC_B1] * ACC_COEF_B +
       buffer[ACC_SRC_C1] * ACC_COEF_C +
       buffer[ACC_SRC_D1] * ACC_COEF_D;

FB_A0 = buffer[MIX_DEST_A0 - FB_SRC_A];
FB_A1 = buffer[MIX_DEST_A1 - FB_SRC_A];
FB_B0 = buffer[MIX_DEST_B0 - FB_SRC_B];
FB_B1 = buffer[MIX_DEST_B1 - FB_SRC_B];

buffer[MIX_DEST_A0] = ACC0 - FB_A0 * FB_ALPHA;
buffer[MIX_DEST_A1] = ACC1 - FB_A1 * FB_ALPHA;
buffer[MIX_DEST_B0] = (FB_ALPHA * ACC0) - FB_A0 * (FB_ALPHA^0x8000) - FB_B0 * FB_X;
buffer[MIX_DEST_B1] = (FB_ALPHA * ACC1) - FB_A1 * (FB_ALPHA^0x8000) - FB_B1 * FB_X;

-----------------------------------------------------------------------------
*/

// vim:shiftwidth=1:expandtab

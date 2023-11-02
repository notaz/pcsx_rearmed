/***************************************************************************
                         externals.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de
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

#ifndef __P_SOUND_EXTERNALS_H__
#define __P_SOUND_EXTERNALS_H__

#include <stdint.h>

/////////////////////////////////////////////////////////
// generic defines
/////////////////////////////////////////////////////////

//#define log_unhandled printf
#define log_unhandled(...)

#ifdef __GNUC__
#define noinline __attribute__((noinline))
#define unlikely(x) __builtin_expect((x), 0)
#else
#define noinline
#define unlikely(x) x
#endif
#if defined(__GNUC__) && !defined(_TMS320C6X)
#define preload __builtin_prefetch
#else
#define preload(...)
#endif

#define PSE_LT_SPU                  4
#define PSE_SPU_ERR_SUCCESS         0
#define PSE_SPU_ERR                 -60
#define PSE_SPU_ERR_NOTCONFIGURED   PSE_SPU_ERR - 1
#define PSE_SPU_ERR_INIT            PSE_SPU_ERR - 2
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

////////////////////////////////////////////////////////////////////////
// spu defines
////////////////////////////////////////////////////////////////////////

// num of channels
#define MAXCHAN     24

// note: must be even due to the way reverb works now
#define NSSIZE ((44100 / 50 + 16) & ~1)

///////////////////////////////////////////////////////////
// struct defines
///////////////////////////////////////////////////////////

enum ADSR_State {
 ADSR_ATTACK = 0,
 ADSR_DECAY = 1,
 ADSR_SUSTAIN = 2,
 ADSR_RELEASE = 3,
};

// ADSR INFOS PER CHANNEL
typedef struct
{
 unsigned char  State:2;                               // ADSR_State
 unsigned char  AttackModeExp:1;
 unsigned char  SustainModeExp:1;
 unsigned char  SustainIncrease:1;
 unsigned char  ReleaseModeExp:1;
 unsigned char  AttackRate;
 unsigned char  DecayRate;
 unsigned char  SustainLevel;
 unsigned char  SustainRate;
 unsigned char  ReleaseRate;
 int            EnvelopeVol;
} ADSRInfoEx;
              
///////////////////////////////////////////////////////////

// Tmp Flags

// used for debug channel muting
#define FLAG_MUTE  1

// used for simple interpolation
#define FLAG_IPOL0 2
#define FLAG_IPOL1 4

///////////////////////////////////////////////////////////

// MAIN CHANNEL STRUCT
typedef struct
{
 int               iSBPos;                             // mixing stuff
 int               spos;
 int               sinc;
 int               sinc_inv;

 unsigned char *   pCurr;                              // current pos in sound mem
 unsigned char *   pLoop;                              // loop ptr in sound mem

 unsigned int      bReverb:1;                          // can we do reverb on this channel? must have ctrl register bit, to get active
 unsigned int      bRVBActive:1;                       // reverb active flag
 unsigned int      bNoise:1;                           // noise active flag
 unsigned int      bFMod:2;                            // freq mod (0=off, 1=sound channel, 2=freq channel)
 unsigned int      prevflags:3;                        // flags from previous block
 unsigned int      bIgnoreLoop:1;                      // Ignore loop
 unsigned int      bStarting:1;                        // starting after keyon
 union {
  struct {
   int             iLeftVolume;                        // left volume
   int             iRightVolume;                       // right volume
  };
  int              iVolume[2];
 };
 ADSRInfoEx        ADSRX;
 int               iRawPitch;                          // raw pitch (0...3fff)
} SPUCHAN;

///////////////////////////////////////////////////////////

typedef struct
{
 int StartAddr;      // reverb area start addr in samples
 int CurrAddr;       // reverb area curr addr in samples

 int VolLeft;
 int VolRight;

 int FB_SRC_A;       // (offset)
 int FB_SRC_B;       // (offset)
 int IIR_ALPHA;      // (coef.)
 int ACC_COEF_A;     // (coef.)
 int ACC_COEF_B;     // (coef.)
 int ACC_COEF_C;     // (coef.)
 int ACC_COEF_D;     // (coef.)
 int IIR_COEF;       // (coef.)
 int FB_ALPHA;       // (coef.)
 int FB_X;           // (coef.)
 int IIR_DEST_A0;    // (offset)
 int IIR_DEST_A1;    // (offset)
 int ACC_SRC_A0;     // (offset)
 int ACC_SRC_A1;     // (offset)
 int ACC_SRC_B0;     // (offset)
 int ACC_SRC_B1;     // (offset)
 int IIR_SRC_A0;     // (offset)
 int IIR_SRC_A1;     // (offset)
 int IIR_DEST_B0;    // (offset)
 int IIR_DEST_B1;    // (offset)
 int ACC_SRC_C0;     // (offset)
 int ACC_SRC_C1;     // (offset)
 int ACC_SRC_D0;     // (offset)
 int ACC_SRC_D1;     // (offset)
 int IIR_SRC_B1;     // (offset)
 int IIR_SRC_B0;     // (offset)
 int MIX_DEST_A0;    // (offset)
 int MIX_DEST_A1;    // (offset)
 int MIX_DEST_B0;    // (offset)
 int MIX_DEST_B1;    // (offset)
 int IN_COEF_L;      // (coef.)
 int IN_COEF_R;      // (coef.)

 int dirty;          // registers changed

 // MIX_DEST_xx - FB_SRC_x
 int FB_SRC_A0, FB_SRC_A1, FB_SRC_B0, FB_SRC_B1;
} REVERBInfo;

///////////////////////////////////////////////////////////

// psx buffers / addresses

typedef union
{
 int SB[28 + 4 + 4];
 struct {
  int sample[28];
  union {
   struct {
    int pos;
    int val[4];
   } gauss;
   int simple[5]; // 28-32
  } interp;
  int sinc_old;
 };
} sample_buf;

typedef struct
{
 unsigned short  spuCtrl;
 unsigned short  spuStat;

 unsigned int    spuAddr;

 unsigned int    cycles_played;
 unsigned int    cycles_dma_end;
 int             decode_pos;
 int             decode_dirty_ch;
 unsigned int    bSpuInit:1;
 unsigned int    bSPUIsOpen:1;
 unsigned int    bMemDirty:1;          // had external write to SPU RAM

 unsigned int    dwNoiseVal;           // global noise generator
 unsigned int    dwNoiseCount;
 unsigned int    dwNewChannel;         // flags for faster testing, if new channel starts
 unsigned int    dwChannelsAudible;    // not silent channels
 unsigned int    dwChannelDead;        // silent+not useful channels

 unsigned int    XARepeat;
 unsigned int    XALastVal;

 int             iLeftXAVol;
 int             iRightXAVol;

 unsigned int    last_keyon_cycles;

 union {
  unsigned char  *spuMemC;
  unsigned short *spuMem;
 };
 unsigned char * pSpuIrq;

 unsigned char * pSpuBuffer;
 short         * pS;

 SPUCHAN       * s_chan;
 REVERBInfo    * rvb;

 int           * SSumLR;

 void (CALLBACK *irqCallback)(int);
 //void (CALLBACK *cddavCallback)(short, short);
 void (CALLBACK *scheduleCallback)(unsigned int);

 const xa_decode_t * xapGlobal;
 unsigned int  * XAFeed;
 unsigned int  * XAPlay;
 unsigned int  * XAStart;
 unsigned int  * XAEnd;

 unsigned int  * CDDAFeed;
 unsigned int  * CDDAPlay;
 unsigned int  * CDDAStart;
 unsigned int  * CDDAEnd;

 unsigned short  regArea[0x400];

 sample_buf      sb[MAXCHAN];
 int             interpolation;
 sample_buf      sb_thread[MAXCHAN];
} SPUInfo;

#define regAreaGet(offset) \
  spu.regArea[((offset) - 0xc00)>>1]
#define regAreaGetCh(ch, offset) \
  spu.regArea[((ch<<4)|(offset))>>1]

///////////////////////////////////////////////////////////
// SPU.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_SPU

extern SPUInfo spu;

void do_samples(unsigned int cycles_to, int do_sync);
void schedule_next_irq(void);
void check_irq_io(unsigned int addr);
void do_irq_io(int cycles_after);

#define do_samples_if_needed(c, sync, samples) \
 do { \
  if (sync || (int)((c) - spu.cycles_played) >= (samples) * 768) \
   do_samples(c, sync); \
 } while (0)

#endif

#endif /* __P_SOUND_EXTERNALS_H__ */

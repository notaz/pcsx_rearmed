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

#include <stdint.h>

/////////////////////////////////////////////////////////
// generic defines
/////////////////////////////////////////////////////////

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

// sound buffer sizes
// 400 ms complete sound buffer
#define SOUNDSIZE   70560
// 137 ms test buffer... if less than that is buffered, a new upload will happen
#define TESTSIZE    24192

// num of channels
#define MAXCHAN     24

// ~ 1 ms of data
#define NSSIZE 45

///////////////////////////////////////////////////////////
// struct defines
///////////////////////////////////////////////////////////

// ADSR INFOS PER CHANNEL
typedef struct
{
 int            AttackModeExp;
 long           AttackTime;
 long           DecayTime;
 long           SustainLevel;
 int            SustainModeExp;
 long           SustainModeDec;
 long           SustainTime;
 int            ReleaseModeExp;
 unsigned long  ReleaseVal;
 long           ReleaseTime;
 long           ReleaseStartTime; 
 long           ReleaseVol; 
 long           lTime;
 long           lVolume;
} ADSRInfo;

typedef struct
{
 unsigned char  State:2;
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

 unsigned char *   pStart;                             // start ptr into sound mem
 unsigned char *   pCurr;                              // current pos in sound mem
 unsigned char *   pLoop;                              // loop ptr in sound mem

 unsigned int      bStop:1;                            // is channel stopped (sample _can_ still be playing, ADSR Release phase)
 unsigned int      bReverb:1;                          // can we do reverb on this channel? must have ctrl register bit, to get active
 unsigned int      bIgnoreLoop:1;                      // ignore loop bit, if an external loop address is used
 unsigned int      bRVBActive:1;                       // reverb active flag
 unsigned int      bNoise:1;                           // noise active flag
 unsigned int      bFMod:2;                            // freq mod (0=off, 1=sound channel, 2=freq channel)

 int               iActFreq;                           // current psx pitch
 int               iUsedFreq;                          // current pc pitch
 int               iLeftVolume;                        // left volume
 int               iRightVolume;                       // right volume
 int               s_1;                                // last decoding infos
 int               s_2;
 ADSRInfoEx        ADSRX;
 int               iRawPitch;                          // raw pitch (0...3fff)

 int               iRVBOffset;                         // reverb offset
 int               iRVBRepeat;                         // reverb repeat
 int               iRVBNum;                            // another reverb helper
 int               iOldNoise;                          // old noise val for this channel   

 int               SB[32+32];
} SPUCHAN;

///////////////////////////////////////////////////////////

typedef struct
{
 int StartAddr;      // reverb area start addr in samples
 int CurrAddr;       // reverb area curr addr in samples

 int VolLeft;
 int VolRight;
 int iLastRVBLeft;
 int iLastRVBRight;
 int iRVBLeft;
 int iRVBRight;

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
} REVERBInfo;

///////////////////////////////////////////////////////////
// SPU.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_SPU

// psx buffers / addresses

extern unsigned short  regArea[];                        
extern unsigned short  spuMem[];
extern unsigned char * spuMemC;
extern unsigned char * pSpuIrq;
extern unsigned char * pSpuBuffer;

// user settings

extern int        iVolume;
extern int        iXAPitch;
extern int        iUseTimer;
extern int        iSPUIRQWait;
extern int        iDebugMode;
extern int        iRecordMode;
extern int        iUseReverb;
extern int        iUseInterpolation;
// MISC

extern int iSpuAsyncWait;

extern SPUCHAN s_chan[];
extern REVERBInfo rvb;

extern unsigned long dwNoiseVal;
extern unsigned short spuCtrl;
extern unsigned short spuStat;
extern unsigned short spuIrq;
extern unsigned long  spuAddr;
extern int      bEndThread; 
extern int      bThreadEnded;
extern int      bSpuInit;
extern unsigned int dwNewChannel;
extern unsigned int dwChannelOn;
extern unsigned int dwPendingChanOff;

extern int      SSumR[];
extern int      SSumL[];
extern int      iCycle;
extern short *  pS;

extern void (CALLBACK *cddavCallback)(unsigned short,unsigned short);

#endif

///////////////////////////////////////////////////////////
// XA.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_XA

extern xa_decode_t   * xapGlobal;

extern uint32_t * XAFeed;
extern uint32_t * XAPlay;
extern uint32_t * XAStart;
extern uint32_t * XAEnd;

extern uint32_t   XARepeat;
extern uint32_t   XALastVal;

extern uint32_t * CDDAFeed;
extern uint32_t * CDDAPlay;
extern uint32_t * CDDAStart;
extern uint32_t * CDDAEnd;

extern int           iLeftXAVol;
extern int           iRightXAVol;

#endif

///////////////////////////////////////////////////////////
// REVERB.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_REVERB

extern int *          sRVBPlay;
extern int *          sRVBEnd;
extern int *          sRVBStart;
extern int            iReverbOff;
extern int            iReverbRepeat;
extern int            iReverbNum;    

#endif

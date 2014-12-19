/***************************************************************************
                            dma.c  -  description
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

#include "stdafx.h"

#define _IN_DMA

#include "externals.h"

////////////////////////////////////////////////////////////////////////
// READ DMA (one value)
////////////////////////////////////////////////////////////////////////

unsigned short CALLBACK SPUreadDMA(void)
{
 unsigned short s=spu.spuMem[spu.spuAddr>>1];
 spu.spuAddr+=2;
 if(spu.spuAddr>0x7ffff) spu.spuAddr=0;

 return s;
}

////////////////////////////////////////////////////////////////////////
// READ DMA (many values)
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUreadDMAMem(unsigned short *pusPSXMem, int iSize,
 unsigned int cycles)
{
 int i;

 do_samples_if_needed(cycles);

 for(i=0;i<iSize;i++)
  {
   *pusPSXMem++=spu.spuMem[spu.spuAddr>>1];            // spu addr got by writeregister
   spu.spuAddr+=2;                                     // inc spu addr
   if(spu.spuAddr>0x7ffff) spu.spuAddr=0;              // wrap
  }
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

// to investigate: do sound data updates by writedma affect spu
// irqs? Will an irq be triggered, if new data is written to
// the memory irq address?

////////////////////////////////////////////////////////////////////////
// WRITE DMA (one value)
////////////////////////////////////////////////////////////////////////
  
void CALLBACK SPUwriteDMA(unsigned short val)
{
 spu.spuMem[spu.spuAddr>>1] = val;                     // spu addr got by writeregister

 spu.spuAddr+=2;                                       // inc spu addr
 if(spu.spuAddr>0x7ffff) spu.spuAddr=0;                // wrap
}

////////////////////////////////////////////////////////////////////////
// WRITE DMA (many values)
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUwriteDMAMem(unsigned short *pusPSXMem, int iSize,
 unsigned int cycles)
{
 int i;
 
 do_samples_if_needed(cycles);

 if(spu.spuAddr + iSize*2 < 0x80000)
  {
   memcpy(&spu.spuMem[spu.spuAddr>>1], pusPSXMem, iSize*2);
   spu.spuAddr += iSize*2;
   return;
  }

 for(i=0;i<iSize;i++)
  {
   spu.spuMem[spu.spuAddr>>1] = *pusPSXMem++;          // spu addr got by writeregister
   spu.spuAddr+=2;                                     // inc spu addr
   spu.spuAddr&=0x7ffff;                               // wrap
  }
}

////////////////////////////////////////////////////////////////////////

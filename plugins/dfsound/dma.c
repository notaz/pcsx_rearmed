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
#include "registers.h"

static void set_dma_end(int iSize, unsigned int cycles)
{
 // this must be > psxdma.c dma irq
 // Road Rash also wants a considerable delay, maybe because of fifo?
 cycles += iSize * 20;  // maybe
 cycles |= 1;           // indicates dma is active
 spu.cycles_dma_end = cycles;
}

////////////////////////////////////////////////////////////////////////
// READ DMA (many values)
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUreadDMAMem(unsigned short *pusPSXMem, int iSize,
 unsigned int cycles)
{
 unsigned int addr = spu.spuAddr, irq_addr = regAreaGet(H_SPUirqAddr) << 3;
 int i, irq;

 do_samples_if_needed(cycles, 1);
 irq = addr <= irq_addr && irq_addr < addr + iSize*2;

 for(i = 0; i < iSize; i++)
 {
  *pusPSXMem++ = *(unsigned short *)(spu.spuMemC + addr);
  addr += 2;
  addr &= 0x7fffe;
 }
 if (irq && (spu.spuCtrl & CTRL_IRQ))
  log_unhandled("rdma spu irq: %x/%x+%x\n", irq_addr, spu.spuAddr, iSize * 2);
 spu.spuAddr = addr;
 set_dma_end(iSize, cycles);
}

////////////////////////////////////////////////////////////////////////
// WRITE DMA (many values)
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUwriteDMAMem(unsigned short *pusPSXMem, int iSize,
 unsigned int cycles)
{
 unsigned int addr = spu.spuAddr, irq_addr = regAreaGet(H_SPUirqAddr) << 3;
 int i, irq;
 
 do_samples_if_needed(cycles, 1);
 spu.bMemDirty = 1;
 irq = addr <= irq_addr && irq_addr < addr + iSize*2;

 if (addr + iSize*2 < 0x80000)
 {
  memcpy(spu.spuMemC + addr, pusPSXMem, iSize*2);
  addr += iSize*2;
 }
 else
 {
  irq |= irq_addr < ((addr + iSize*2) & 0x7ffff);
  for (i = 0; i < iSize; i++)
  {
   *(unsigned short *)(spu.spuMemC + addr) = *pusPSXMem++;
   addr += 2;
   addr &= 0x7fffe;
  }
 }
 if (irq && (spu.spuCtrl & CTRL_IRQ)) // unhandled because need to implement delay
  log_unhandled("wdma spu irq: %x/%x+%x\n", irq_addr, spu.spuAddr, iSize * 2);
 spu.spuAddr = addr;
 set_dma_end(iSize, cycles);
}

////////////////////////////////////////////////////////////////////////

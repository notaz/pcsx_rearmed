/***************************************************************************
                            gpu.h  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    web                  : www.pbernert.com   
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

//*************************************************************************// 
// History of changes:
//
// 2009/03/08 - Pete  
// - generic cleanup for the Peops release
//
//*************************************************************************// 

#ifndef _GPU_PLUGIN_H
#define _GPU_PLUGIN_H

/////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif 

#if !defined(_WINDOWS) && !defined(__NANOGL__)
#define glOrtho(x,y,z,xx,yy,zz) glOrthof(x,y,z,xx,yy,zz)
#endif

#define PRED(x)   ((x << 3) & 0xF8)
#define PBLUE(x)  ((x >> 2) & 0xF8)
#define PGREEN(x) ((x >> 7) & 0xF8)

#define RED(x) (x & 0xff)
#define BLUE(x) ((x>>16) & 0xff)
#define GREEN(x) ((x>>8) & 0xff)
#define COLOR(x) (x & 0xffffff)

#ifdef _WINDOWS
#include "Externals.h"
#include "plugin.h"
#include <gl/gl.h>
#else
#ifndef MAEMO_CHANGES
	#include "psxCommon.h"
#else
	#include "../psxCommon.h"
#endif	
#include "gpuExternals.h"
#ifdef __NANOGL__
#include <gl/gl.h>
#else
#ifdef SOFT_LINKAGE
#pragma softfp_linkage
#endif
#ifndef MAEMO_CHANGES
	#include <gles/gl.h> // for opengl es types 
	#include <gles/egltypes.h>
#else
#include "gpuStdafx.h"
#endif
#ifdef SOFT_LINKAGE
#pragma no_softfp_linkage
#endif
#endif
#endif
/////////////////////////////////////////////////////////////////////////////

#define CALLBACK

#define bool unsigned short

typedef struct {
	u32 ulFreezeVersion;
	u32 ulStatus;
	u32 ulControl[256];
	u8 psxVRam[1024*1024*2];
} GPUFreeze_t;

long CALLBACK GPU_init();
long CALLBACK GPU_shutdown();
long CALLBACK GPU_open(int hwndGPU);                    
long CALLBACK GPU_close();
unsigned long CALLBACK GPU_readData(void);
void CALLBACK GPU_readDataMem(unsigned long * pMem, int iSize);
unsigned long CALLBACK GPU_readStatus(void);
void CALLBACK GPU_writeData(unsigned long gdata);
void CALLBACK GPU_writeDataMem(unsigned long * pMem, int iSize);
void CALLBACK GPU_writeStatus(unsigned long gdata);
long CALLBACK GPU_dmaChain(unsigned long * baseAddrL, unsigned long addr);
void CALLBACK GPU_updateLace(void);
void CALLBACK GPU_makeSnapshot(void);
long CALLBACK GPU_freeze(unsigned long ulGetFreezeData,GPUFreeze_t * pF);
long CALLBACK GPU_getScreenPic(u8 * pMem);
long CALLBACK GPU_showScreenPic(u8 * pMem);
//void CALLBACK GPU_keypressed(int keycode);
//void CALLBACK GPU_displayText(s8 * pText);
//void CALLBACK GPU_clearDynarec(void (CALLBACK *callback)(void));
long CALLBACK GPU_configure(void);
long CALLBACK GPU_test(void);
void CALLBACK GPU_about(void);


void           DoSnapShot(void);
void		   GPU_vSinc(void);
void           updateDisplay(void);
void           updateFrontDisplay(void);
void           SetAutoFrameCap(void);
void           SetAspectRatio(void);
void           CheckVRamRead(int x, int y, int dx, int dy, bool bFront);
void           CheckVRamReadEx(int x, int y, int dx, int dy);
void           SetFixes(void);

void PaintPicDot(u8 * p,u8 c);
//void DrawNumBorPic(u8 *pMem, int lSelectedSlot);
void ResizeWindow();

////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
}
#endif 


#endif // _GPU_INTERNALS_H

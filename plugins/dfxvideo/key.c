/***************************************************************************
                          key.c  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Pete Bernert
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

#define _IN_KEY

#include "externals.h"
#include "menu.h"
#include "gpu.h"
#include "draw.h"
#include "key.h"

#define VK_INSERT      65379
#define VK_HOME        65360
#define VK_PRIOR       65365
#define VK_NEXT        65366
#define VK_END         65367
#define VK_DEL         65535
#define VK_F5          65474

void GPUmakeSnapshot(void);

unsigned long          ulKeybits=0;

void GPUkeypressed(int keycode)
{
 switch(keycode)
  {
   case 0xFFC9:			//X11 key: F12
   case ((1<<29) | 0xFF0D):	//special keycode from pcsx-df: alt-enter
	bChangeWinMode=TRUE;
	break;
   case VK_F5:
       GPUmakeSnapshot();
      break;

   case VK_INSERT:
       if(iUseFixes) {iUseFixes=0;dwActFixes=0;}
       else          {iUseFixes=1;dwActFixes=dwCfgFixes;}
       SetFixes();
       if(iFrameLimit==2) SetAutoFrameCap();
       break;

   case VK_DEL:
       if(ulKeybits&KEY_SHOWFPS)
        {
         ulKeybits&=~KEY_SHOWFPS;
         DoClearScreenBuffer();
        }
       else 
        {
         ulKeybits|=KEY_SHOWFPS;
         szDispBuf[0]=0;
         BuildDispMenu(0);
        }
       break;

   case VK_PRIOR: BuildDispMenu(-1);            break;
   case VK_NEXT:  BuildDispMenu( 1);            break;
   case VK_END:   SwitchDispMenu(1);            break;
   case VK_HOME:  SwitchDispMenu(-1);           break;
   case 0x60:
    {
     iFastFwd = 1 - iFastFwd;
     bSkipNextFrame = FALSE;
     UseFrameSkip = iFastFwd;
     BuildDispMenu(0);
     break;
    }
#ifdef _MACGL
   default: { void HandleKey(int keycode); HandleKey(keycode); }
#endif
  }
}

void SetKeyHandler(void)
{
}

void ReleaseKeyHandler(void)
{
}

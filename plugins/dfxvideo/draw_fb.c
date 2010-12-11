/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#define _IN_DRAW

#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "prim.h"
#include "menu.h"
#include "interp.h"
#include "swap.h"

#include "plugin_lib.h"
#include "pcnt.h"

// misc globals
int            iResX;
int            iResY;
long           lLowerpart;
BOOL           bIsFirstFrame = TRUE;
BOOL           bCheckMask = FALSE;
unsigned short sSetMask = 0;
unsigned long  lSetMask = 0;
int            iDesktopCol = 16;
int            iShowFPS = 0;
int            iWinSize; 
int            iMaintainAspect = 0;
int            iUseNoStretchBlt = 0;
int            iFastFwd = 0;
int            iFVDisplay = 0;
PSXPoint_t     ptCursorPoint[8];
unsigned short usCursorActive = 0;
char *         pCaptionText;

#ifndef __arm__
#define bgr555_to_rgb565 memcpy
#define bgr888_to_rgb888 memcpy
#endif

static void blit(void)
{
 extern void bgr555_to_rgb565(void *dst, void *src, int bytes);
 extern void bgr888_to_rgb888(void *dst, void *src, int bytes);
 int x = PSXDisplay.DisplayPosition.x & ~3; // XXX: align needed by bgr*_to_...
 int y = PSXDisplay.DisplayPosition.y;
 int w = PreviousPSXDisplay.Range.x1;
 int h = PreviousPSXDisplay.DisplayMode.y;
 int pitch = PreviousPSXDisplay.DisplayMode.x;
 unsigned short *srcs = psxVuw + y * 1024 + x;
 unsigned char *dest = pl_fbdev_buf;

 if (w <= 0)
   return;

 // TODO: clear border if centering

 pitch *= PSXDisplay.RGB24 ? 3 : 2;

 // account for centering
 h -= PreviousPSXDisplay.Range.y0;
 dest += PreviousPSXDisplay.Range.y0 / 2 * pitch;
 dest += PreviousPSXDisplay.Range.x0 * 2; // XXX

 if (PSXDisplay.RGB24)
 {
   for (; h-- > 0; dest += pitch, srcs += 1024)
   {
     bgr888_to_rgb888(dest, srcs, w * 3);
   }
 }
 else
 {
   for (; h-- > 0; dest += pitch, srcs += 1024)
   {
     bgr555_to_rgb565(dest, srcs, w * 2);
   }
 }
}

void DoBufferSwap(void)
{
 static int fbw, fb24bpp;

 if (PSXDisplay.DisplayMode.x == 0 || PSXDisplay.DisplayMode.y == 0)
  return;

 /* careful if rearranging this code, we try to set mode and flip
  * to get the hardware apply both changes at the same time */
 if (PSXDisplay.DisplayMode.x != fbw || PSXDisplay.RGB24 != fb24bpp) {
  int fbh = PSXDisplay.DisplayMode.y;
  fbw = PSXDisplay.DisplayMode.x;
  fb24bpp = PSXDisplay.RGB24;
  pl_fbdev_set_mode(fbw, fbh, fb24bpp ? 24 : 16);
 }

 pcnt_start(PCNT_BLIT);
 blit();
 pcnt_end(PCNT_BLIT);

 pl_fbdev_flip();
}

void DoClearScreenBuffer(void)                         // CLEAR DX BUFFER
{
}

void DoClearFrontBuffer(void)                          // CLEAR DX BUFFER
{
}

static int initialize(void)
{
 iDesktopCol=32;

 bUsingTWin=FALSE;
 bIsFirstFrame = FALSE;                                // done

 if(iShowFPS)
  {
   iShowFPS=0;
   ulKeybits|=KEY_SHOWFPS;
   szDispBuf[0]=0;
   BuildDispMenu(0);
  }

 return 0;
}

unsigned long ulInitDisplay(void)
{
 iShowFPS=1;
 initialize();

 if (pl_fbdev_init() != 0)
  return 0;

 return 1; /* ok */
}

void CloseDisplay(void)
{
 CloseMenu();
 pl_fbdev_finish();
 //WriteConfig();
}

void CreatePic(unsigned char * pMem)
{
}

void DestroyPic(void)
{
}

void HandleKey(int keycode)
{
}

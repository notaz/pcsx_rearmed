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


static void blit(void)
{
 int x = PSXDisplay.DisplayPosition.x;
 int y = PSXDisplay.DisplayPosition.y;
 int w = PreviousPSXDisplay.Range.x1;
 int h = PreviousPSXDisplay.DisplayMode.y;
 int pitch = PreviousPSXDisplay.DisplayMode.x * 2;
 unsigned char *dest = pl_fbdev_buf;

 // TODO: clear border if centering

 // account for centering
 h -= PreviousPSXDisplay.Range.y0;
 dest += PreviousPSXDisplay.Range.y0 / 2 * pitch;
 dest += PreviousPSXDisplay.Range.x0 * 2; // XXX

 {
   unsigned short *srcs = psxVuw + y * 1024 + x;
   for (; h-- > 0; dest += pitch, srcs += 1024)
   {
     memcpy(dest, srcs, w * 2);
   }
 }
}

static int fbw, fbh, fb24bpp;

void DoBufferSwap(void)
{
 static float fps_old;
 if (PSXDisplay.DisplayMode.x == 0 || PSXDisplay.DisplayMode.y == 0)
  return;

 if (PSXDisplay.DisplayMode.x != fbw || PSXDisplay.DisplayMode.y != fbh
     || PSXDisplay.RGB24 != fb24bpp) {
  fbw = PSXDisplay.DisplayMode.x;
  fbh = PSXDisplay.DisplayMode.y;
  fb24bpp = PSXDisplay.RGB24;
  pl_fbdev_set_mode(fbw, fbh, fb24bpp ? 24 : 16);
 }

 if (fps_cur != fps_old) {
  printf("%2.1f\n", fps_cur);
  fps_old = fps_cur;
 }

 blit();
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

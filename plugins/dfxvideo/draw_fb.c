/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#define _IN_DRAW

#include "gpu.h"

#include "plugin_lib.h"
#include "pcnt.h"

// misc globals
long           lLowerpart;
BOOL           bCheckMask = FALSE;
unsigned short sSetMask;
unsigned long  lSetMask;

#ifndef __arm__
#define bgr555_to_rgb565 memcpy
#define bgr888_to_rgb888 memcpy
#endif

static void blit(void)
{
 extern void bgr555_to_rgb565(void *dst, void *src, int bytes);
 extern void bgr888_to_rgb888(void *dst, void *src, int bytes);
 int px = PSXDisplay.DisplayPosition.x & ~3; // XXX: align needed by bgr*_to_...
 int py = PSXDisplay.DisplayPosition.y;
 int w = PreviousPSXDisplay.Range.x1;
 int h = PreviousPSXDisplay.DisplayMode.y;
 int pitch = PreviousPSXDisplay.DisplayMode.x;
 unsigned short *srcs = psxVuw + py * 1024 + px;
 unsigned char *dest = pl_fbdev_buf;

 if (w <= 0)
   return;

 // TODO: clear border if centering?

 pitch *= PSXDisplay.RGB24 ? 3 : 2;

 // account for centering
 h -= PreviousPSXDisplay.Range.y0;
 dest += PreviousPSXDisplay.Range.y0 / 2 * pitch;
 dest += (PreviousPSXDisplay.Range.x0 & ~3) * 2; // must align here too..

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
 static int fbw, fbh, fb24bpp;

 if (PreviousPSXDisplay.DisplayMode.x == 0 || PreviousPSXDisplay.DisplayMode.y == 0)
  return;

 /* careful if rearranging this code, we try to set mode and flip
  * to get the hardware apply both changes at the same time */
 if (PreviousPSXDisplay.DisplayMode.x != fbw || PreviousPSXDisplay.DisplayMode.y != fbh
     || PSXDisplay.RGB24 != fb24bpp) {
  fbw = PreviousPSXDisplay.DisplayMode.x;
  fbh = PreviousPSXDisplay.DisplayMode.y;
  fb24bpp = PSXDisplay.RGB24;
  pl_fbdev_set_mode(fbw, fbh, fb24bpp ? 24 : 16);
 }

 pcnt_start(PCNT_BLIT);
 blit();
 pcnt_end(PCNT_BLIT);

 pl_fbdev_flip();
}

void DoClearScreenBuffer(void)
{
}

unsigned long ulInitDisplay(void)
{
 if (pl_fbdev_open() != 0)
  return 0;

 return 1; /* ok */
}

void CloseDisplay(void)
{
 pl_fbdev_close();
}


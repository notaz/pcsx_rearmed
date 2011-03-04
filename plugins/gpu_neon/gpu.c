/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdint.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static struct {
  uint16_t vram[1024 * 512];
  uint16_t guard[1024 * 512]; // overdraw guard
  uint32_t regs[16];
  union {
    uint32_t reg;
    struct {
      uint32_t tx:4;    //  0 texture page
      uint32_t ty:1;
      uint32_t abr:2;
      uint32_t tp:2;    //  7 t.p. mode (4,8,15bpp)
      uint32_t dtd:1;   //  9 dither
      uint32_t dfe:1;
      uint32_t md:1;    // 11 set mask bit when drawing
      uint32_t me:1;    // 12 no draw on mask
      uint32_t unkn:3;
      uint32_t width1:1;// 16
      uint32_t width0:2;
      uint32_t height:1;
      uint32_t video:1; // 20 NTSC,PAL
      uint32_t rgb24:1;
      uint32_t inter:1; // 22 interlace on
      uint32_t den:1;   // 23 display not enabled
      uint32_t unkn2:2;
      uint32_t busy:1;  // 26 !busy drawing
      uint32_t img:1;   // 27 ready to DMA
      uint32_t com:1;   // 28 ready for commands
      uint32_t dma:2;   // 29 off, ?, to vram, from vram
      uint32_t lcf:1;   // 21 odd frame/blanking?
    };
  } status;
  struct {
    int x, y, w, h;
    int y1, y2;
  } screen;
  uint32_t blanking;
} gpu;

long GPUinit(void)
{
  return 0;
}

long GPUshutdown(void)
{
  return 0;
}

uint32_t GPUreadStatus(void)
{
  return gpu.status.reg | (gpu.blanking << 31);
}

void GPUwriteStatus(uint32_t data)
{
  static const short hres[8] = { 256, 368, 320, 384, 512, 512, 640, 640 };
  static const short vres[4] = { 240, 480, 256, 480 };
  uint32_t cmd = data >> 24;

  switch (data >> 24) {
    case 0x00:
      break;
    case 0x03:
      gpu.status.den = data & 1;
      break;
    case 0x04:
      gpu.status.dma = data & 3;
      break;
    case 0x05:
      gpu.screen.x = data & 0x3ff;
      gpu.screen.y = (data >> 10) & 0x3ff;
      break;
    case 0x07:
      gpu.screen.y1 = data & 0x3ff;
      gpu.screen.y2 = (data >> 10) & 0x3ff;
      break;
    case 0x08:
      gpu.status.reg = (gpu.status.reg & ~0x7f0000) | ((data & 0x3F) << 17) | ((data & 0x40) << 10);
      gpu.screen.w = hres[(gpu.status.reg >> 16) & 7];
      gpu.screen.h = vres[(gpu.status.reg >> 19) & 3];
      break;
  }

  if (cmd < ARRAY_SIZE(gpu.regs))
    gpu.regs[cmd] = data;
}

void GPUreadDataMem(uint32_t *mem, int count)
{
}

uint32_t GPUreadData(void)
{
  return 0;
}

void GPUwriteDataMem(uint32_t *mem, int count)
{
}

void GPUwriteData(uint32_t gdata)
{
}

long GPUdmaChain(uint32_t *base, uint32_t addr)
{
  return 0;
}

typedef struct GPUFREEZETAG
{
  uint32_t ulFreezeVersion;      // should be always 1 for now (set by main emu)
  uint32_t ulStatus;             // current gpu status
  uint32_t ulControl[256];       // latest control register values
  unsigned char psxVRam[1024*1024*2]; // current VRam image (full 2 MB for ZN)
} GPUFreeze_t;

long GPUfreeze(uint32_t type, GPUFreeze_t *freeze)
{
  switch (type) {
    case 1: // save
      memcpy(freeze->psxVRam, gpu.vram, sizeof(gpu.vram));
      memcpy(freeze->ulControl, gpu.regs, sizeof(gpu.regs));
      freeze->ulStatus = gpu.status.reg;
      freeze->ulControl[255] = gpu.blanking; // abuse free space
      break;
    case 0: // load
      memcpy(gpu.vram, freeze->psxVRam, sizeof(gpu.vram));
      memcpy(gpu.regs, freeze->ulControl, sizeof(gpu.regs));
      gpu.status.reg = freeze->ulStatus;
      gpu.blanking = freeze->ulControl[255];
      GPUwriteStatus((5 << 24) | gpu.regs[5]);
      GPUwriteStatus((7 << 24) | gpu.regs[7]);
      GPUwriteStatus((8 << 24) | gpu.regs[8]);
      break;
  }

  return 1;
}

void GPUvBlank(int val)
{
  gpu.blanking = !!val;
}

// rearmed specific

#include "../../frontend/plugin_lib.h"
#include "../../frontend/arm_utils.h"

static const struct rearmed_cbs *cbs;
static void *screen_buf;

static void blit(void)
{
  static uint32_t old_status, old_h;
  int x = gpu.screen.x & ~3; // alignment needed by blitter
  int y = gpu.screen.y;
  int w = gpu.screen.w;
  int h;
  uint16_t *srcs;
  uint8_t  *dest;

  srcs = &gpu.vram[y * 1024 + x];

  h = gpu.screen.y2 - gpu.screen.y1;

  if (h <= 0)
    return;

  if ((gpu.status.reg ^ old_status) & ((7<<16)|(1<<21)) || h != old_h) // width|rgb24 change?
  {
    old_status = gpu.status.reg;
    old_h = h;
    screen_buf = cbs->pl_fbdev_set_mode(w, h, gpu.status.rgb24 ? 24 : 16);
  }
  dest = screen_buf;

  if (gpu.status.rgb24)
  {
#ifndef MAEMO
    for (; h-- > 0; dest += w * 3, srcs += 1024)
    {
      bgr888_to_rgb888(dest, srcs, w * 3);
    }
#else
    for (; h-- > 0; dest += w * 2, srcs += 1024)
    {
      bgr888_to_rgb565(dest, srcs, w * 3);
    }
#endif
  }
  else
  {
    for (; h-- > 0; dest += w * 2, srcs += 1024)
    {
      bgr555_to_rgb565(dest, srcs, w * 2);
    }
  }

  screen_buf = cbs->pl_fbdev_flip();
}

void GPUupdateLace(void)
{
  blit();
}

long GPUopen(void)
{
  cbs->pl_fbdev_open();
  screen_buf = cbs->pl_fbdev_flip();
  return 0;
}

long GPUclose(void)
{
  cbs->pl_fbdev_close();
  return 0;
}

void GPUrearmedCallbacks(const struct rearmed_cbs *cbs_)
{
  cbs = cbs_;
}

// vim:shiftwidth=2:expandtab

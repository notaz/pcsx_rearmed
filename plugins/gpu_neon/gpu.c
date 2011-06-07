/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define unlikely(x) __builtin_expect((x), 0)

#define CMD_BUFFER_LEN          1024

static struct __attribute__((aligned(64))) {
  uint16_t vram[1024 * 512];
  uint16_t guard[1024 * 512]; // overdraw guard
  uint32_t cmd_buffer[CMD_BUFFER_LEN];
  uint32_t regs[16];
  union {
    uint32_t reg;
    struct {
      uint32_t tx:4;        //  0 texture page
      uint32_t ty:1;
      uint32_t abr:2;
      uint32_t tp:2;        //  7 t.p. mode (4,8,15bpp)
      uint32_t dtd:1;       //  9 dither
      uint32_t dfe:1;
      uint32_t md:1;        // 11 set mask bit when drawing
      uint32_t me:1;        // 12 no draw on mask
      uint32_t unkn:3;
      uint32_t width1:1;    // 16
      uint32_t width0:2;
      uint32_t dheight:1;   // 19 double height
      uint32_t video:1;     // 20 NTSC,PAL
      uint32_t rgb24:1;
      uint32_t interlace:1; // 22 interlace on
      uint32_t blanking:1;  // 23 display not enabled
      uint32_t unkn2:2;
      uint32_t busy:1;      // 26 !busy drawing
      uint32_t img:1;       // 27 ready to DMA image data
      uint32_t com:1;       // 28 ready for commands
      uint32_t dma:2;       // 29 off, ?, to vram, from vram
      uint32_t lcf:1;       // 31
    };
  } status;
  struct {
    int x, y, w, h;
    int y1, y2;
  } screen;
  struct {
    int x, y, w, h;
    int offset;
  } dma;
  int cmd_len;
  const uint32_t *lcf_hc;
  uint32_t zero;
} gpu;

long GPUinit(void)
{
  gpu.status.reg = 0x14802000;
  return 0;
}

long GPUshutdown(void)
{
  return 0;
}

void GPUwriteStatus(uint32_t data)
{
  static const short hres[8] = { 256, 368, 320, 384, 512, 512, 640, 640 };
  static const short vres[4] = { 240, 480, 256, 480 };
  uint32_t cmd = data >> 24;

  switch (data >> 24) {
    case 0x00:
      gpu.status.reg = 0x14802000;
      break;
    case 0x03:
      gpu.status.blanking = data & 1;
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

static const unsigned char cmd_lengths[256] =
{
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 3, 3, 3, 6, 6, 6, 6, 4, 4, 4, 4, 8, 8, 8, 8, // 20
	5, 5, 5, 5, 8, 8, 8, 8, 7, 7, 7, 7, 11, 11, 11, 11,
	2, 2, 2, 2, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, // 40
	3, 3, 3, 3, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4,
	2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 1, 1, 2, 2, 2, 2, // 60
	1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 80
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // a0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // c0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // e0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void do_cmd(uint32_t *list, int count)
{
  uint32_t *list_end = list + count;
  int cmd;
  //printf("do_cmd    %p, %d\n", data, count);

  for (; list < list_end; list += 1 + cmd_lengths[cmd])
  {
    cmd = list[0] >> 24;
    switch (cmd)
    {
      case 0xe1:
        gpu.status.reg &= ~0x7ff;
        gpu.status.reg |= list[0] & 0x7ff;
        break;
      case 0xe6:
        gpu.status.reg &= ~0x1800;
        gpu.status.reg |= (list[0] & 3) << 11;
        break;
    }
    if ((cmd & 0xf4) == 0x24) {
      // flat textured prim
      gpu.status.reg &= ~0x1ff;
      gpu.status.reg |= list[4] & 0x1ff;
    }
    else if ((cmd & 0xf4) == 0x34) {
      // shaded textured prim
      gpu.status.reg &= ~0x1ff;
      gpu.status.reg |= list[5] & 0x1ff;
    }
  }
}

#define VRAM_MEM_XY(x, y) &gpu.vram[(y) * 1024 + (x)]

static inline void do_vram_line(int x, int y, uint16_t *mem, int l, int is_read)
{
  uint16_t *vram = VRAM_MEM_XY(x, y);
  if (is_read)
    memcpy(mem, vram, l * 2);
  else
    memcpy(vram, mem, l * 2);
}

static int do_vram_io(uint32_t *data, int count, int is_read)
{
  int count_initial = count;
  uint16_t *sdata = (uint16_t *)data;
  int x = gpu.dma.x, y = gpu.dma.y;
  int w = gpu.dma.w, h = gpu.dma.h;
  int l;
  count *= 2; // operate in 16bpp pixels

  if (gpu.dma.offset) {
    l = w - gpu.dma.offset;
    if (l > count)
      l = count;
    do_vram_line(x + gpu.dma.offset, y, sdata, l, is_read);
    sdata += l;
    count -= l;
    y++;
    h--;
  }

  for (; h > 0 && count >= w; sdata += w, count -= w, y++, h--) {
    y &= 511;
    do_vram_line(x, y, sdata, w, is_read);
  }

  if (h > 0 && count > 0) {
    y &= 511;
    do_vram_line(x, y, sdata, count, is_read);
    gpu.dma.offset = count;
    count = 0;
  }
  else
    gpu.dma.offset = 0;
  gpu.dma.y = y;
  gpu.dma.h = h;

  return count_initial - (count + 1) / 2;
}

static void start_vram_transfer(uint32_t pos_word, uint32_t size_word, int is_read)
{
  gpu.dma.x = pos_word & 1023;
  gpu.dma.y = (pos_word >> 16) & 511;
  gpu.dma.w = size_word & 0xffff; // ?
  gpu.dma.h = size_word >> 16;
  gpu.dma.offset = 0;

  if (is_read)
    gpu.status.img = 1;

  //printf("start_vram_transfer %c (%d, %d) %dx%d\n", is_read ? 'r' : 'w',
  //  gpu.dma.x, gpu.dma.y, gpu.dma.w, gpu.dma.h);
}

static int check_cmd(uint32_t *data, int count)
{
  int len, cmd, start, pos;

  //printf("check_cmd %p, %d\n", data, count);

  // process buffer
  for (start = pos = 0;; )
  {
    cmd = -1;
    len = 0;

    if (gpu.dma.h) {
      pos += do_vram_io(data + pos, count - pos, 0);
      start = pos;
    }

    while (pos < count) {
      cmd = data[pos] >> 24;
      len = 1 + cmd_lengths[cmd];
      //printf("  %3d: %02x %d\n", pos, cmd, len);
      if (pos + len > count) {
        cmd = -1;
        break; // incomplete cmd
      }
      if (cmd == 0xa0 || cmd == 0xc0)
        break; // image i/o
      pos += len;
    }

    if (pos - start > 0) {
      do_cmd(data + start, pos - start);
      start = pos;
    }

    if (cmd == 0xa0 || cmd == 0xc0) {
      // consume vram write/read cmd
      start_vram_transfer(data[pos + 1], data[pos + 2], cmd == 0xc0);
      pos += len;
    }

    if (pos == count)
      return 0;

    if (pos + len > count) {
      //printf("discarding %d words\n", pos + len - count);
      return pos + len - count;
    }
  }
}

static void flush_cmd_buffer(void)
{
  int left = check_cmd(gpu.cmd_buffer, gpu.cmd_len);
  if (left > 0)
    memmove(gpu.cmd_buffer, gpu.cmd_buffer + gpu.cmd_len - left, left * 4);
  gpu.cmd_len = left;
}

void GPUwriteDataMem(uint32_t *mem, int count)
{
  int left;

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();
  left = check_cmd(mem, count);
  if (left)
    printf("GPUwriteDataMem: discarded %d/%d words\n", left, count);
}

void GPUwriteData(uint32_t data)
{
  gpu.cmd_buffer[gpu.cmd_len++] = data;
  if (gpu.cmd_len >= CMD_BUFFER_LEN)
    flush_cmd_buffer();
}

long GPUdmaChain(uint32_t *base, uint32_t addr)
{
  uint32_t *list;
  int len;

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  while (addr != 0xffffff) {
    list = base + (addr & 0x1fffff) / 4;
    len = list[0] >> 24;
    addr = list[0] & 0xffffff;
    if (len)
      GPUwriteDataMem(list + 1, len);
  }

  return 0;
}

void GPUreadDataMem(uint32_t *mem, int count)
{
  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();
  if (gpu.dma.h)
    do_vram_io(mem, count, 1);
}

uint32_t GPUreadData(void)
{
  uint32_t v = 0;
  GPUreadDataMem(&v, 1);
  return v;
}

uint32_t GPUreadStatus(void)
{
  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  return gpu.status.reg | (*gpu.lcf_hc << 31);
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
      if (gpu.cmd_len > 0)
        flush_cmd_buffer();
      memcpy(freeze->psxVRam, gpu.vram, sizeof(gpu.vram));
      memcpy(freeze->ulControl, gpu.regs, sizeof(gpu.regs));
      freeze->ulStatus = gpu.status.reg;
      break;
    case 0: // load
      memcpy(gpu.vram, freeze->psxVRam, sizeof(gpu.vram));
      memcpy(gpu.regs, freeze->ulControl, sizeof(gpu.regs));
      gpu.status.reg = freeze->ulStatus;
      GPUwriteStatus((5 << 24) | gpu.regs[5]);
      GPUwriteStatus((7 << 24) | gpu.regs[7]);
      GPUwriteStatus((8 << 24) | gpu.regs[8]);
      break;
  }

  return 1;
}

void GPUvBlank(int val, uint32_t *hcnt)
{
  gpu.lcf_hc = &gpu.zero;
  if (gpu.status.interlace) {
    if (val)
      gpu.status.lcf ^= 1;
  }
  else {
    gpu.status.lcf = 0;
    if (!val)
      gpu.lcf_hc = hcnt;
  }
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
  if (gpu.status.dheight)
    h *= 2;

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
  if (!gpu.status.blanking)
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

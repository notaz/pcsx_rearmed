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
#include <string.h>
#include "gpu.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define unlikely(x) __builtin_expect((x), 0)
#define noinline __attribute__((noinline))

//#define log_io printf
#define log_io(...)
#define log_anomaly printf

struct psx_gpu gpu __attribute__((aligned(64)));

long GPUinit(void)
{
  int ret = vout_init();
  gpu.status.reg = 0x14802000;
  gpu.lcf_hc = &gpu.zero;
  return ret;
}

long GPUshutdown(void)
{
  return vout_finish();
}

static noinline void update_width(void)
{
  int sw = gpu.screen.x2 - gpu.screen.x1;
  if (sw <= 0 || sw >= 2560)
    // full width
    gpu.screen.w = gpu.screen.hres;
  else
    gpu.screen.w = sw * gpu.screen.hres / 2560;
}

static noinline void update_height(void)
{
  int sh = gpu.screen.y2 - gpu.screen.y1;
  if (gpu.status.dheight)
    sh *= 2;
  if (sh <= 0)
    sh = gpu.screen.vres;

  gpu.screen.h = sh;
}

void GPUwriteStatus(uint32_t data)
{
  static const short hres[8] = { 256, 368, 320, 384, 512, 512, 640, 640 };
  static const short vres[4] = { 240, 480, 256, 480 };
  uint32_t cmd = data >> 24;

  if (cmd < ARRAY_SIZE(gpu.regs))
    gpu.regs[cmd] = data;

  switch (cmd) {
    case 0x00:
      gpu.status.reg = 0x14802000;
      gpu.status.blanking = 1;
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
    case 0x06:
      gpu.screen.x1 = data & 0xfff;
      gpu.screen.x2 = (data >> 12) & 0xfff;
      update_width();
      break;
    case 0x07:
      gpu.screen.y1 = data & 0x3ff;
      gpu.screen.y2 = (data >> 10) & 0x3ff;
      update_height();
      break;
    case 0x08:
      gpu.status.reg = (gpu.status.reg & ~0x7f0000) | ((data & 0x3F) << 17) | ((data & 0x40) << 10);
      gpu.screen.hres = hres[(gpu.status.reg >> 16) & 7];
      gpu.screen.vres = vres[(gpu.status.reg >> 19) & 3];
      update_width();
      update_height();
      break;
  }
}

const unsigned char cmd_lengths[256] =
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
  int o = gpu.dma.offset;
  int l;
  count *= 2; // operate in 16bpp pixels

  if (gpu.dma.offset) {
    l = w - gpu.dma.offset;
    if (count < l)
      l = count;

    do_vram_line(x + o, y, sdata, l, is_read);

    if (o + l < w)
      o += l;
    else {
      o = 0;
      y++;
      h--;
    }
    sdata += l;
    count -= l;
  }

  for (; h > 0 && count >= w; sdata += w, count -= w, y++, h--) {
    y &= 511;
    do_vram_line(x, y, sdata, w, is_read);
  }

  if (h > 0 && count > 0) {
    y &= 511;
    do_vram_line(x, y, sdata, count, is_read);
    o = count;
    count = 0;
  }
  gpu.dma.y = y;
  gpu.dma.h = h;
  gpu.dma.offset = o;

  return count_initial - (count + 1) / 2;
}

static void start_vram_transfer(uint32_t pos_word, uint32_t size_word, int is_read)
{
  if (gpu.dma.h)
    log_anomaly("start_vram_transfer while old unfinished\n");

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

  // process buffer
  for (start = pos = 0; pos < count; )
  {
    cmd = -1;
    len = 0;

    if (gpu.dma.h) {
      pos += do_vram_io(data + pos, count - pos, 0);
      if (pos == count)
        break;
      start = pos;
    }

    // do look-ahead pass to detect SR changes and VRAM i/o
    while (pos < count) {
      uint32_t *list = data + pos;
      cmd = list[0] >> 24;
      len = 1 + cmd_lengths[cmd];

      //printf("  %3d: %02x %d\n", pos, cmd, len);
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
      else switch (cmd)
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

      if (pos + len > count) {
        cmd = -1;
        break; // incomplete cmd
      }
      if (cmd == 0xa0 || cmd == 0xc0)
        break; // image i/o
      pos += len;
    }

    if (pos - start > 0) {
      do_cmd_list(data + start, pos - start);
      start = pos;
    }

    if (cmd == 0xa0 || cmd == 0xc0) {
      // consume vram write/read cmd
      start_vram_transfer(data[pos + 1], data[pos + 2], cmd == 0xc0);
      pos += len;
    }

    if (cmd == -1)
      break;
  }

  return count - pos;
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

  log_io("gpu_dma_write %p %d\n", mem, count);

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  left = check_cmd(mem, count);
  if (left)
    log_anomaly("GPUwriteDataMem: discarded %d/%d words\n", left, count);
}

void GPUwriteData(uint32_t data)
{
  log_io("gpu_write %08x\n", data);
  gpu.cmd_buffer[gpu.cmd_len++] = data;
  if (gpu.cmd_len >= CMD_BUFFER_LEN)
    flush_cmd_buffer();
}

long GPUdmaChain(uint32_t *rambase, uint32_t start_addr)
{
  uint32_t addr, *list;
  int len, left, count;

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  log_io("gpu_dma_chain\n");
  addr = start_addr & 0xffffff;
  for (count = 0; addr != 0xffffff; count++)
  {
    log_io(".chain %08x\n", addr);

    list = rambase + (addr & 0x1fffff) / 4;
    len = list[0] >> 24;
    addr = list[0] & 0xffffff;

    // loop detection marker
    // (bit23 set causes DMA error on real machine, so
    //  unlikely to be ever set by the game)
    list[0] |= 0x800000;

    if (len) {
      left = check_cmd(list + 1, len);
      if (left)
        log_anomaly("GPUwriteDataMem: discarded %d/%d words\n", left, len);
    }

    if (addr & 0x800000)
      break;
  }

  // remove loop detection markers
  addr = start_addr & 0x1fffff;
  while (count-- > 0) {
    list = rambase + addr / 4;
    addr = list[0] & 0x1fffff;
    list[0] &= ~0x800000;
  }

  return 0;
}

void GPUreadDataMem(uint32_t *mem, int count)
{
  log_io("gpu_dma_read  %p %d\n", mem, count);

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  if (gpu.dma.h)
    do_vram_io(mem, count, 1);
}

uint32_t GPUreadData(void)
{
  uint32_t v = 0;

  log_io("gpu_read\n");

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  if (gpu.dma.h)
    do_vram_io(&v, 1, 1);

  return v;
}

uint32_t GPUreadStatus(void)
{
  uint32_t ret;

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  ret = gpu.status.reg | (*gpu.lcf_hc << 31);
  log_io("gpu_read_status %08x\n", ret);
  return ret;
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

// vim:shiftwidth=2:expandtab

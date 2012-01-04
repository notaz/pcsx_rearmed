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

#define gpu_log(fmt, ...) \
  printf("%d:%03d: " fmt, *gpu.state.frame_count, *gpu.state.hcnt, ##__VA_ARGS__)

//#define log_io gpu_log
#define log_io(...)
//#define log_anomaly gpu_log
#define log_anomaly(...)

struct psx_gpu gpu __attribute__((aligned(2048)));

static noinline void do_reset(void)
{
  memset(gpu.regs, 0, sizeof(gpu.regs));
  memset(gpu.ex_regs, 0, sizeof(gpu.ex_regs));
  gpu.status.reg = 0x14802000;
  gpu.gp0 = 0;
  gpu.regs[3] = 1;
  gpu.screen.hres = gpu.screen.w = 256;
  gpu.screen.vres = gpu.screen.h = 240;
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

static noinline void decide_frameskip(void)
{
  if (gpu.frameskip.active)
    gpu.frameskip.cnt++;
  else {
    gpu.frameskip.cnt = 0;
    gpu.frameskip.frame_ready = 1;
  }

  if (!gpu.frameskip.active && *gpu.frameskip.advice)
    gpu.frameskip.active = 1;
  else if (gpu.frameskip.set > 0 && gpu.frameskip.cnt < gpu.frameskip.set)
    gpu.frameskip.active = 1;
  else
    gpu.frameskip.active = 0;
}

static noinline void decide_frameskip_allow(uint32_t cmd_e3)
{
  // no frameskip if it decides to draw to display area,
  // but not for interlace since it'll most likely always do that
  uint32_t x = cmd_e3 & 0x3ff;
  uint32_t y = (cmd_e3 >> 10) & 0x3ff;
  gpu.frameskip.allow = gpu.status.interlace ||
    (uint32_t)(x - gpu.screen.x) >= (uint32_t)gpu.screen.w ||
    (uint32_t)(y - gpu.screen.y) >= (uint32_t)gpu.screen.h;
}

static noinline void get_gpu_info(uint32_t data)
{
  switch (data & 0x0f) {
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
      gpu.gp0 = gpu.ex_regs[data & 7] & 0xfffff;
      break;
    case 0x06:
      gpu.gp0 = gpu.ex_regs[5] & 0xfffff;
      break;
    case 0x07:
      gpu.gp0 = 2;
      break;
    default:
      gpu.gp0 = 0;
      break;
  }
}

long GPUinit(void)
{
  int ret;
  ret  = vout_init();
  ret |= renderer_init();

  gpu.state.frame_count = &gpu.zero;
  gpu.state.hcnt = &gpu.zero;
  do_reset();
  return ret;
}

long GPUshutdown(void)
{
  return vout_finish();
}

void GPUwriteStatus(uint32_t data)
{
  static const short hres[8] = { 256, 368, 320, 384, 512, 512, 640, 640 };
  static const short vres[4] = { 240, 480, 256, 480 };
  uint32_t cmd = data >> 24;

  if (cmd < ARRAY_SIZE(gpu.regs)) {
    if (cmd != 0 && cmd != 5 && gpu.regs[cmd] == data)
      return;
    gpu.regs[cmd] = data;
  }

  gpu.state.fb_dirty = 1;

  switch (cmd) {
    case 0x00:
      do_reset();
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
      if (gpu.frameskip.set) {
        decide_frameskip_allow(gpu.ex_regs[3]);
        if (gpu.frameskip.last_flip_frame != *gpu.state.frame_count) {
          decide_frameskip();
          gpu.frameskip.last_flip_frame = *gpu.state.frame_count;
        }
      }
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
    default:
      if ((cmd & 0xf0) == 0x10)
        get_gpu_info(data);
      break;
  }
}

const unsigned char cmd_lengths[256] =
{
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 3, 3, 3, 6, 6, 6, 6, 4, 4, 4, 4, 8, 8, 8, 8, // 20
	5, 5, 5, 5, 8, 8, 8, 8, 7, 7, 7, 7, 11, 11, 11, 11,
	2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, // 40
	3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
	2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 1, 1, 0, 0, 0, 0, // 60
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

  return count_initial - count / 2;
}

static void start_vram_transfer(uint32_t pos_word, uint32_t size_word, int is_read)
{
  if (gpu.dma.h)
    log_anomaly("start_vram_transfer while old unfinished\n");

  gpu.dma.x = pos_word & 0x3ff;
  gpu.dma.y = (pos_word >> 16) & 0x1ff;
  gpu.dma.w = size_word & 0x3ff;
  gpu.dma.h = (size_word >> 16) & 0x1ff;
  gpu.dma.offset = 0;

  renderer_flush_queues();
  if (is_read) {
    gpu.status.img = 1;
    // XXX: wrong for width 1
    memcpy(&gpu.gp0, VRAM_MEM_XY(gpu.dma.x, gpu.dma.y), 4);
    gpu.state.last_vram_read_frame = *gpu.state.frame_count;
  }
  else {
    renderer_invalidate_caches(gpu.dma.x, gpu.dma.y, gpu.dma.w, gpu.dma.h);
  }

  log_io("start_vram_transfer %c (%d, %d) %dx%d\n", is_read ? 'r' : 'w',
    gpu.dma.x, gpu.dma.y, gpu.dma.w, gpu.dma.h);
}

static int check_cmd(uint32_t *data, int count)
{
  int len, cmd, start, pos;
  int vram_dirty = 0;

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
        gpu.ex_regs[1] &= ~0x1ff;
        gpu.ex_regs[1] |= list[4] & 0x1ff;
      }
      else if ((cmd & 0xf4) == 0x34) {
        // shaded textured prim
        gpu.ex_regs[1] &= ~0x1ff;
        gpu.ex_regs[1] |= list[5] & 0x1ff;
      }
      else if (cmd == 0xe3)
        decide_frameskip_allow(list[0]);

      if (2 <= cmd && cmd < 0xc0)
        vram_dirty = 1;
      else if ((cmd & 0xf8) == 0xe0)
        gpu.ex_regs[cmd & 7] = list[0];

      if (pos + len > count) {
        cmd = -1;
        break; // incomplete cmd
      }
      if (cmd == 0xa0 || cmd == 0xc0)
        break; // image i/o
      pos += len;
    }

    if (pos - start > 0) {
      if (!gpu.frameskip.active || !gpu.frameskip.allow)
        do_cmd_list(data + start, pos - start);
      start = pos;
    }

    if (cmd == 0xa0 || cmd == 0xc0) {
      // consume vram write/read cmd
      start_vram_transfer(data[pos + 1], data[pos + 2], cmd == 0xc0);
      pos += len;
    }
    else if (cmd == -1)
      break;
  }

  gpu.status.reg &= ~0x1fff;
  gpu.status.reg |= gpu.ex_regs[1] & 0x7ff;
  gpu.status.reg |= (gpu.ex_regs[6] & 3) << 11;

  if (gpu.frameskip.active)
    renderer_sync_ecmds(gpu.ex_regs);
  gpu.state.fb_dirty |= vram_dirty;

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
  uint32_t *llist_entry = NULL;
  int len, left, count;
  long cpu_cycles = 0;

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  // ff7 sends it's main list twice, detect this
  if (*gpu.state.frame_count == gpu.state.last_list.frame &&
      *gpu.state.hcnt - gpu.state.last_list.hcnt <= 1 &&
       gpu.state.last_list.cycles > 2048)
  {
    llist_entry = rambase + (gpu.state.last_list.addr & 0x1fffff) / 4;
    *llist_entry |= 0x800000;
  }

  log_io("gpu_dma_chain\n");
  addr = start_addr & 0xffffff;
  for (count = 0; addr != 0xffffff; count++)
  {
    list = rambase + (addr & 0x1fffff) / 4;
    len = list[0] >> 24;
    addr = list[0] & 0xffffff;
    cpu_cycles += 10;
    if (len > 0)
      cpu_cycles += 5 + len;

    log_io(".chain %08x #%d\n", (list - rambase) * 4, len);

    // loop detection marker
    // (bit23 set causes DMA error on real machine, so
    //  unlikely to be ever set by the game)
    list[0] |= 0x800000;

    if (len) {
      left = check_cmd(list + 1, len);
      if (left)
        log_anomaly("GPUdmaChain: discarded %d/%d words\n", left, len);
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
  if (llist_entry)
    *llist_entry &= ~0x800000;

  gpu.state.last_list.frame = *gpu.state.frame_count;
  gpu.state.last_list.hcnt = *gpu.state.hcnt;
  gpu.state.last_list.cycles = cpu_cycles;
  gpu.state.last_list.addr = start_addr;

  return cpu_cycles;
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
  uint32_t ret;

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  ret = gpu.gp0;
  if (gpu.dma.h)
    do_vram_io(&ret, 1, 1);

  log_io("gpu_read %08x\n", ret);
  return ret;
}

uint32_t GPUreadStatus(void)
{
  uint32_t ret;

  if (unlikely(gpu.cmd_len > 0))
    flush_cmd_buffer();

  ret = gpu.status.reg;
  log_io("gpu_read_status %08x\n", ret);
  return ret;
}

struct GPUFreeze
{
  uint32_t ulFreezeVersion;      // should be always 1 for now (set by main emu)
  uint32_t ulStatus;             // current gpu status
  uint32_t ulControl[256];       // latest control register values
  unsigned char psxVRam[1024*1024*2]; // current VRam image (full 2 MB for ZN)
};

long GPUfreeze(uint32_t type, struct GPUFreeze *freeze)
{
  int i;

  switch (type) {
    case 1: // save
      if (gpu.cmd_len > 0)
        flush_cmd_buffer();
      memcpy(freeze->psxVRam, gpu.vram, sizeof(gpu.vram));
      memcpy(freeze->ulControl, gpu.regs, sizeof(gpu.regs));
      memcpy(freeze->ulControl + 0xe0, gpu.ex_regs, sizeof(gpu.ex_regs));
      freeze->ulStatus = gpu.status.reg;
      break;
    case 0: // load
      renderer_invalidate_caches(0, 0, 1024, 512);
      memcpy(gpu.vram, freeze->psxVRam, sizeof(gpu.vram));
      memcpy(gpu.regs, freeze->ulControl, sizeof(gpu.regs));
      memcpy(gpu.ex_regs, freeze->ulControl + 0xe0, sizeof(gpu.ex_regs));
      gpu.status.reg = freeze->ulStatus;
      for (i = 8; i > 0; i--) {
        gpu.regs[i] ^= 1; // avoid reg change detection
        GPUwriteStatus((i << 24) | (gpu.regs[i] ^ 1));
      }
      renderer_sync_ecmds(gpu.ex_regs);
      break;
  }

  return 1;
}

void GPUupdateLace(void)
{
  if (gpu.cmd_len > 0)
    flush_cmd_buffer();
  renderer_flush_queues();

  if (gpu.status.blanking || !gpu.state.fb_dirty)
    return;

  if (gpu.frameskip.set) {
    if (!gpu.frameskip.frame_ready) {
      if (*gpu.state.frame_count - gpu.frameskip.last_flip_frame < 9)
        return;
      gpu.frameskip.active = 0;
    }
    gpu.frameskip.frame_ready = 0;
  }

  vout_update();
  gpu.state.fb_dirty = 0;
}

void GPUvBlank(int is_vblank, int lcf)
{
  int interlace = gpu.state.allow_interlace
    && gpu.status.interlace && gpu.status.dheight;
  // interlace doesn't look nice on progressive displays,
  // so we have this "auto" mode here for games that don't read vram
  if (gpu.state.allow_interlace == 2
      && *gpu.state.frame_count - gpu.state.last_vram_read_frame > 1)
  {
    interlace = 0;
  }
  if (interlace || interlace != gpu.state.old_interlace) {
    gpu.state.old_interlace = interlace;

    if (gpu.cmd_len > 0)
      flush_cmd_buffer();
    renderer_flush_queues();
    renderer_set_interlace(interlace, !lcf);
  }
}

#include "../../frontend/plugin_lib.h"

void GPUrearmedCallbacks(const struct rearmed_cbs *cbs)
{
  gpu.frameskip.set = cbs->frameskip;
  gpu.frameskip.advice = &cbs->fskip_advice;
  gpu.frameskip.active = 0;
  gpu.frameskip.frame_ready = 1;
  gpu.state.hcnt = cbs->gpu_hcnt;
  gpu.state.frame_count = cbs->gpu_frame_count;
  gpu.state.allow_interlace = cbs->gpu_neon.allow_interlace;

  if (cbs->pl_vout_set_raw_vram)
    cbs->pl_vout_set_raw_vram(gpu.vram);
  renderer_set_config(cbs);
  vout_set_config(cbs);
}

// vim:shiftwidth=2:expandtab

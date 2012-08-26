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
#include <sys/mman.h>

extern const unsigned char cmd_lengths[256];
#define command_lengths cmd_lengths

static unsigned int *ex_regs;
static int initialized;

#define PCSX
#define SET_Ex(r, v) \
  ex_regs[r] = v

#include "psx_gpu/psx_gpu.c"
#include "psx_gpu/psx_gpu_parse.c"
#include "../gpulib/gpu.h"

static psx_gpu_struct egpu __attribute__((aligned(256)));

int do_cmd_list(uint32_t *list, int count, int *last_cmd)
{
  int ret;

  if (gpu.state.enhancement_active)
    ret = gpu_parse_enhanced(&egpu, list, count * 4, (u32 *)last_cmd);
  else
    ret = gpu_parse(&egpu, list, count * 4, (u32 *)last_cmd);

  ex_regs[1] &= ~0x1ff;
  ex_regs[1] |= egpu.texture_settings & 0x1ff;
  return ret;
}

#define ENHANCEMENT_BUF_SIZE (1024 * 1024 * 2 * 4 + 4096 * 2)

static void map_enhancement_buffer(void)
{
  // currently we use 4x 1024*1024 buffers instead of single 2048*1024
  // to be able to reuse 1024-width code better (triangle setup,
  // dithering phase, lines).
  gpu.enhancement_bufer = gpu.mmap(ENHANCEMENT_BUF_SIZE);
  if (gpu.enhancement_bufer == NULL)
    fprintf(stderr, "failed to map enhancement buffer\n");
  else
    gpu.enhancement_bufer += 4096 / 2;
  egpu.enhancement_buf_ptr = gpu.enhancement_bufer;
}

int renderer_init(void)
{
  if (gpu.vram != NULL) {
    initialize_psx_gpu(&egpu, gpu.vram);
    initialized = 1;
  }

  if (gpu.mmap != NULL && gpu.enhancement_bufer == NULL)
    map_enhancement_buffer();

  ex_regs = gpu.ex_regs;
  return 0;
}

void renderer_finish(void)
{
  if (gpu.enhancement_bufer != NULL) {
    gpu.enhancement_bufer -= 4096 / 2;
    gpu.munmap(gpu.enhancement_bufer, ENHANCEMENT_BUF_SIZE);
  }
  gpu.enhancement_bufer = NULL;
  egpu.enhancement_buf_ptr = NULL;
  egpu.enhancement_current_buf_ptr = NULL;
  initialized = 0;
}

static __attribute__((noinline)) void
sync_enhancement_buffers(int x, int y, int w, int h)
{
  int xt = egpu.enhancement_x_threshold;
  u16 *src, *dst;
  int wb, i;

  w += x & 7;
  x &= ~7;
  w = (w + 7) & ~7;
  if (y + h > 512)
    h = 512 - y;

  for (i = 0; i < 4 && w > 0; i++) {
    if (x < 512) {
      wb = w;
      if (x + w > 512)
        wb = 512 - x;
      src = gpu.vram + xt * i + y * 1024 + x;
      dst = egpu.enhancement_buf_ptr +
        (1024*1024 + xt * 2) * i + (y * 1024 + x) * 2;
      scale2x_tiles8(dst, src, wb / 8, h);
    }

    x -= xt;
    if (x < 0) {
      w += x;
      x = 0;
    }
  }
}

void renderer_sync_ecmds(uint32_t *ecmds)
{
  gpu_parse(&egpu, ecmds + 1, 6 * 4, NULL);
}

void renderer_update_caches(int x, int y, int w, int h)
{
  update_texture_cache_region(&egpu, x, y, x + w - 1, y + h - 1);
  if (gpu.state.enhancement_active && !gpu.status.rgb24)
    sync_enhancement_buffers(x, y, w, h);
}

void renderer_flush_queues(void)
{
  flush_render_block_buffer(&egpu);
}

void renderer_set_interlace(int enable, int is_odd)
{
  egpu.render_mode &= ~(RENDER_INTERLACE_ENABLED|RENDER_INTERLACE_ODD);
  if (enable)
    egpu.render_mode |= RENDER_INTERLACE_ENABLED;
  if (is_odd)
    egpu.render_mode |= RENDER_INTERLACE_ODD;
}

void renderer_notify_res_change(void)
{
  // note: must keep it multiple of 8
  egpu.enhancement_x_threshold = gpu.screen.hres;
}

#include "../../frontend/plugin_lib.h"

void renderer_set_config(const struct rearmed_cbs *cbs)
{
  static int enhancement_was_on;

  disable_main_render = cbs->gpu_neon.enhancement_no_main;
  if (egpu.enhancement_buf_ptr != NULL && cbs->gpu_neon.enhancement_enable
      && !enhancement_was_on)
  {
    sync_enhancement_buffers(0, 0, 1024, 512);
  }
  enhancement_was_on = cbs->gpu_neon.enhancement_enable;

  if (!initialized) {
    initialize_psx_gpu(&egpu, gpu.vram);
    initialized = 1;
  }

  if (gpu.enhancement_bufer == NULL)
    map_enhancement_buffer();
}

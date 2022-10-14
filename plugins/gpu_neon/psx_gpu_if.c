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

#ifdef _WIN32
#include <mman.h>
#else
#include <sys/mman.h>
#endif

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

#if defined(__arm__) && defined(NEON_BUILD) && !defined(SIMD_BUILD)
  // the asm doesn't bother to save callee-save vector regs, so do it here
  __asm__ __volatile__("":::"q4","q5","q6","q7");
#endif

  if (gpu.state.enhancement_active)
    ret = gpu_parse_enhanced(&egpu, list, count * 4, (u32 *)last_cmd);
  else
    ret = gpu_parse(&egpu, list, count * 4, (u32 *)last_cmd);

#if defined(__arm__) && defined(NEON_BUILD) && !defined(SIMD_BUILD)
  __asm__ __volatile__("":::"q4","q5","q6","q7");
#endif

  ex_regs[1] &= ~0x1ff;
  ex_regs[1] |= egpu.texture_settings & 0x1ff;
  return ret;
}

#define ENHANCEMENT_BUF_SIZE (1024 * 1024 * 2 * 4 + 4096 * 2)

static uint16_t *get_enhancement_bufer(int *x, int *y, int *w, int *h,
 int *vram_h)
{
  uint16_t *ret = select_enhancement_buf_ptr(&egpu, *x);

  *x *= 2;
  *y *= 2;
  *w = *w * 2;
  *h = *h * 2;
  *vram_h = 1024;
  return ret;
}

static void map_enhancement_buffer(void)
{
  // currently we use 4x 1024*1024 buffers instead of single 2048*1024
  // to be able to reuse 1024-width code better (triangle setup,
  // dithering phase, lines).
  egpu.enhancement_buf_ptr = gpu.mmap(ENHANCEMENT_BUF_SIZE);
  if (egpu.enhancement_buf_ptr == NULL) {
    fprintf(stderr, "failed to map enhancement buffer\n");
    gpu.get_enhancement_bufer = NULL;
  }
  else {
    egpu.enhancement_buf_ptr += 4096 / 2;
    gpu.get_enhancement_bufer = get_enhancement_bufer;
  }
}

int renderer_init(void)
{
  if (gpu.vram != NULL) {
    initialize_psx_gpu(&egpu, gpu.vram);
    initialized = 1;
  }

  if (gpu.mmap != NULL && egpu.enhancement_buf_ptr == NULL)
    map_enhancement_buffer();

  ex_regs = gpu.ex_regs;
  return 0;
}

void renderer_finish(void)
{
  if (egpu.enhancement_buf_ptr != NULL) {
    egpu.enhancement_buf_ptr -= 4096 / 2;
    gpu.munmap(egpu.enhancement_buf_ptr, ENHANCEMENT_BUF_SIZE);
  }
  egpu.enhancement_buf_ptr = NULL;
  egpu.enhancement_current_buf_ptr = NULL;
  initialized = 0;
}

static __attribute__((noinline)) void
sync_enhancement_buffers(int x, int y, int w, int h)
{
  const int step_x = 1024 / sizeof(egpu.enhancement_buf_by_x16);
  u16 *src, *dst;
  int w1, fb_index;

  w += x & (step_x - 1);
  x &= ~(step_x - 1);
  w = (w + step_x - 1) & ~(step_x - 1);
  if (y + h > 512)
    h = 512 - y;

  while (w > 0) {
    fb_index = egpu.enhancement_buf_by_x16[x / step_x];
    for (w1 = 0; w > 0; w1++, w -= step_x)
      if (fb_index != egpu.enhancement_buf_by_x16[x / step_x + w1])
        break;

    src = gpu.vram + y * 1024 + x;
    dst = select_enhancement_buf_ptr(&egpu, x);
    dst += (y * 1024 + x) * 2;
    scale2x_tiles8(dst, src, w1 * step_x / 8, h);

    x += w1 * step_x;
  }
}

void renderer_sync_ecmds(uint32_t *ecmds)
{
  gpu_parse(&egpu, ecmds + 1, 6 * 4, NULL);
}

void renderer_update_caches(int x, int y, int w, int h)
{
  update_texture_cache_region(&egpu, x, y, x + w - 1, y + h - 1);
  if (gpu.state.enhancement_active && !(gpu.status & PSX_GPU_STATUS_RGB24))
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
  if (egpu.enhancement_x_threshold != gpu.screen.hres)
  {
    egpu.enhancement_x_threshold = gpu.screen.hres;
    update_enhancement_buf_table_from_hres(&egpu);
  }
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

  if (gpu.mmap != NULL && egpu.enhancement_buf_ptr == NULL)
    map_enhancement_buffer();
  if (cbs->pl_set_gpu_caps)
    cbs->pl_set_gpu_caps(GPU_CAP_SUPPORTS_2X);
  
  egpu.use_dithering = cbs->gpu_neon.allow_dithering;
  if(!egpu.use_dithering) {
    egpu.dither_table[0] = dither_table_row(0, 0, 0, 0);
    egpu.dither_table[1] = dither_table_row(0, 0, 0, 0);
    egpu.dither_table[2] = dither_table_row(0, 0, 0, 0);
    egpu.dither_table[3] = dither_table_row(0, 0, 0, 0);
  } else {
    egpu.dither_table[0] = dither_table_row(-4, 0, -3, 1);
    egpu.dither_table[1] = dither_table_row(2, -2, 3, -1);
    egpu.dither_table[2] = dither_table_row(-3, 1, -4, 0);
    egpu.dither_table[3] = dither_table_row(3, -1, 2, -2); 
  }

}
void renderer_sync(void)
{
}
void renderer_notify_update_lace(int updated)
{
}

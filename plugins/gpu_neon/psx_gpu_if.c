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
#include <assert.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

extern const unsigned char cmd_lengths[256];
#define command_lengths cmd_lengths

static unsigned int *ex_regs;
static int initialized;

#define PCSX
#define SET_Ex(r, v) \
  ex_regs[r] = v

static __attribute__((noinline)) void
sync_enhancement_buffers(int x, int y, int w, int h);

#include "../gpulib/gpu.h"
#include "psx_gpu/psx_gpu.c"
#include "psx_gpu/psx_gpu_parse.c"

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

static void *get_enhancement_bufer(int *x, int *y, int *w, int *h,
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
  int hres = egpu.saved_hres;
  int x_buf, w1, s, fb_index;
  u16 *src, *dst;

  if (egpu.enhancement_buf_ptr == NULL)
    return;

  w += x & (step_x - 1);
  x &= ~(step_x - 1);
  w = (w + step_x - 1) & ~(step_x - 1);
  if (y + h > 512)
    h = 512 - y;

  // find x_buf which is an offset into this enhancement_buf
  fb_index = egpu.enhancement_buf_by_x16[x / step_x];
  x_buf = x - egpu.enhancement_buf_start[fb_index];

  while (w > 0) {
    fb_index = egpu.enhancement_buf_by_x16[x / step_x];
    for (w1 = 0; w > 0 && x_buf < hres; x_buf += step_x, w1++, w -= step_x)
      if (fb_index != egpu.enhancement_buf_by_x16[x / step_x + w1])
        break;
    // skip further unneeded data, if any
    for (s = 0; w > 0; s++, w -= step_x)
      if (fb_index != egpu.enhancement_buf_by_x16[x / step_x + w1 + s])
        break;

    if (w1 > 0) {
      src = gpu.vram + y * 1024 + x;
      dst = select_enhancement_buf_ptr(&egpu, x);
      dst += (y * 1024 + x) * 2;
      scale2x_tiles8(dst, src, w1 * step_x / 8, h);
    }

    x += (w1 + s) * step_x;
    x &= 0x3ff;
    x_buf = 0;
  }
}

void renderer_sync_ecmds(uint32_t *ecmds)
{
  gpu_parse(&egpu, ecmds + 1, 6 * 4, NULL);
}

void renderer_update_caches(int x, int y, int w, int h, int state_changed)
{
  update_texture_cache_region(&egpu, x, y, x + w - 1, y + h - 1);

  if (gpu.state.enhancement_active) {
    if (state_changed) {
      egpu.saved_hres = 0;
      renderer_notify_res_change();
      return;
    }
    sync_enhancement_buffers(x, y, w, h);
  }
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
  renderer_notify_scanout_x_change(gpu.screen.src_x, gpu.screen.hres);
}

void renderer_notify_scanout_x_change(int x, int w)
{
  int hres = (w + 15) & ~15;
  int max_bufs = ARRAY_SIZE(egpu.enhancement_scanout_x);
  int need_update = 0;
  int i;

  if (!gpu.state.enhancement_active)
    return;

  assert(!(max_bufs & (max_bufs - 1)));
  if (egpu.saved_hres != hres) {
    for (i = 0; i < max_bufs; i++)
      egpu.enhancement_scanout_x[i] = x;
    need_update = 1;
  }

  if (egpu.enhancement_scanout_x[egpu.enhancement_scanout_select] != x)
  {
    // maybe triple buffering?
    for (i = 0; i < max_bufs; i++)
      if (egpu.enhancement_scanout_x[i] == x)
        break;
    if (i == max_bufs)
      need_update = 1;

    egpu.enhancement_scanout_x[egpu.enhancement_scanout_select] = x;
  }
  egpu.enhancement_scanout_select++;
  egpu.enhancement_scanout_select &= max_bufs - 1;
  if (need_update)
  {
    egpu.saved_hres = hres;
    update_enhancement_buf_table_from_hres(&egpu);
    sync_enhancement_buffers(0, 0, 1024, 512);
  }
}

#include "../../frontend/plugin_lib.h"

void renderer_set_config(const struct rearmed_cbs *cbs)
{
  if (!initialized) {
    initialize_psx_gpu(&egpu, gpu.vram);
    initialized = 1;
  }
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

  disable_main_render = cbs->gpu_neon.enhancement_no_main;
  if (gpu.state.enhancement_enable) {
    if (gpu.mmap != NULL && egpu.enhancement_buf_ptr == NULL)
      map_enhancement_buffer();
  }
}

void renderer_sync(void)
{
}

void renderer_notify_update_lace(int updated)
{
}

// vim:ts=2:sw=2:expandtab

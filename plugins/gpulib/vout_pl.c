/*
 * video output handling using plugin_lib
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <string.h>
#include "gpu.h"
#include "cspace.h"
#include "../../frontend/plugin_lib.h"

static const struct rearmed_cbs *cbs;
static void *screen_buf;

int vout_init(void)
{
  return 0;
}

int vout_finish(void)
{
  return 0;
}

static void check_mode_change(void)
{
  static uint32_t old_status;
  static int old_h;
  int w = gpu.screen.hres;
  int h = gpu.screen.h;

  gpu.state.enhancement_active =
    gpu.enhancement_bufer != NULL && gpu.state.enhancement_enable
    && w <= 512 && h <= 256 && !gpu.status.rgb24;

  if (gpu.state.enhancement_active) {
    w *= 2;
    h *= 2;
  }

  // width|rgb24 change?
  if ((gpu.status.reg ^ old_status) & ((7<<16)|(1<<21)) || h != old_h)
  {
    old_status = gpu.status.reg;
    old_h = h;

    screen_buf = cbs->pl_vout_set_mode(w, h,
      (gpu.status.rgb24 && !cbs->only_16bpp) ? 24 : 16);
  }
}

static void blit(void)
{
  int x = gpu.screen.x & ~1; // alignment needed by blitter
  int y = gpu.screen.y;
  int w = gpu.screen.w;
  int h = gpu.screen.h;
  uint16_t *vram = gpu.vram;
  int stride = gpu.screen.hres;
  int vram_stride = 1024;
  int vram_mask = 1024 * 512 - 1;
  int fb_offs, doffs;
  uint8_t *dest;

  dest = (uint8_t *)screen_buf;
  if (dest == NULL || w == 0 || stride == 0)
    return;

  if (gpu.state.enhancement_active) {
    // this layout is gpu_neon specific..
    vram = gpu.enhancement_bufer +
      (x + 8) / stride * 1024 * 1024;
    x *= 2;
    y *= 2;
    w = (w - 2) * 2;
    h = (h * 2) - 1;
    stride *= 2;
    vram_mask = 1024 * 1024 - 1;
  }
  fb_offs = y * vram_stride + x;

  // only do centering, at least for now
  doffs = (stride - w) / 2 & ~1;

  if (gpu.status.rgb24)
  {
    if (cbs->only_16bpp) {
      dest += doffs * 2;
      for (; h-- > 0; dest += stride * 2, fb_offs += vram_stride)
      {
        fb_offs &= vram_mask;
        bgr888_to_rgb565(dest, vram + fb_offs, w * 3);
      }
    }
    else {
      dest += (doffs / 8) * 24;
      for (; h-- > 0; dest += stride * 3, fb_offs += vram_stride)
      {
        fb_offs &= vram_mask;
        bgr888_to_rgb888(dest, vram + fb_offs, w * 3);
      }
    }
  }
  else
  {
    dest += doffs * 2;
    for (; h-- > 0; dest += stride * 2, fb_offs += vram_stride)
    {
      fb_offs &= vram_mask;
      bgr555_to_rgb565(dest, vram + fb_offs, w * 2);
    }
  }

  screen_buf = cbs->pl_vout_flip();
}

void vout_update(void)
{
  check_mode_change();
  if (cbs->pl_vout_raw_flip)
    cbs->pl_vout_raw_flip(gpu.screen.x, gpu.screen.y);
  else
    blit();
}

void vout_blank(void)
{
  check_mode_change();
  if (cbs->pl_vout_raw_flip == NULL) {
    int bytespp = gpu.status.rgb24 ? 3 : 2;
    memset(screen_buf, 0, gpu.screen.hres * gpu.screen.h * bytespp);
    screen_buf = cbs->pl_vout_flip();
  }
}

long GPUopen(void **unused)
{
  gpu.frameskip.active = 0;
  gpu.frameskip.frame_ready = 1;

  cbs->pl_vout_open();
  screen_buf = cbs->pl_vout_flip();
  return 0;
}

long GPUclose(void)
{
  cbs->pl_vout_close();
  return 0;
}

void vout_set_config(const struct rearmed_cbs *cbs_)
{
  cbs = cbs_;
}

// vim:shiftwidth=2:expandtab

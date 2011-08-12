/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include "gpu.h"
#include "../../frontend/plugin_lib.h"
#include "../../frontend/arm_utils.h"

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

static void blit(void)
{
  static uint32_t old_status, old_h;
  int x = gpu.screen.x & ~1; // alignment needed by blitter
  int y = gpu.screen.y;
  int w = gpu.screen.w;
  int h = gpu.screen.h;
  int stride = gpu.screen.hres;
  int doffs;
  uint16_t *srcs;
  uint8_t  *dest;

  srcs = &gpu.vram[y * 1024 + x];

  if ((gpu.status.reg ^ old_status) & ((7<<16)|(1<<21)) || h != old_h) // width|rgb24 change?
  {
    old_status = gpu.status.reg;
    old_h = h;
    screen_buf = cbs->pl_vout_set_mode(stride, h, gpu.status.rgb24 ? 24 : 16);
  }

  dest = screen_buf;

  // only do centering, at least for now
  doffs = (stride - w) / 2 & ~1;

  if (gpu.status.rgb24)
  {
#ifndef MAEMO
    dest += (doffs / 8) * 24;
    for (; h-- > 0; dest += stride * 3, srcs += 1024)
    {
      bgr888_to_rgb888(dest, srcs, w * 3);
    }
#else
    dest += doffs * 2;
    for (; h-- > 0; dest += stride * 2, srcs += 1024)
    {
      bgr888_to_rgb565(dest, srcs, w * 3);
    }
#endif
  }
  else
  {
    dest += doffs * 2;
    for (; h-- > 0; dest += stride * 2, srcs += 1024)
    {
      bgr555_to_rgb565(dest, srcs, w * 2);
    }
  }

  screen_buf = cbs->pl_vout_flip();
}

void GPUupdateLace(void)
{
  if (gpu.status.blanking || !gpu.state.fb_dirty)
    return;

  if (gpu.frameskip.enabled) {
    if (!gpu.frameskip.frame_ready && gpu.frameskip.skipped_blits < 6) {
      gpu.frameskip.skipped_blits++;
      return;
    }
    gpu.frameskip.frame_ready = 0;
    gpu.frameskip.skipped_blits = 0;
  }

  renderer_flush_queues();
  blit();
  gpu.state.fb_dirty = 0;
}

long GPUopen(void)
{
  gpu.frameskip.enabled = cbs->frameskip;
  gpu.frameskip.advice = &cbs->fskip_advice;
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

void GPUrearmedCallbacks(const struct rearmed_cbs *cbs_)
{
  cbs = cbs_;
}

// vim:shiftwidth=2:expandtab

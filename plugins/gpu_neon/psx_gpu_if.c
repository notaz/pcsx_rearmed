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

extern const unsigned char cmd_lengths[256];
#define command_lengths cmd_lengths

static unsigned int *ex_regs;

#define PCSX
#define SET_Ex(r, v) \
  ex_regs[r] = v

#include "psx_gpu/psx_gpu.c"
#include "psx_gpu/psx_gpu_parse.c"
#include "../gpulib/gpu.h"

static psx_gpu_struct egpu __attribute__((aligned(256)));

int do_cmd_list(uint32_t *list, int count, int *last_cmd)
{
  int ret = gpu_parse(&egpu, list, count * 4, (u32 *)last_cmd);

  ex_regs[1] &= ~0x1ff;
  ex_regs[1] |= egpu.texture_settings & 0x1ff;
  return ret;
}

int renderer_init(void)
{
  initialize_psx_gpu(&egpu, gpu.vram);
  ex_regs = gpu.ex_regs;
  return 0;
}

void renderer_sync_ecmds(uint32_t *ecmds)
{
  gpu_parse(&egpu, ecmds + 1, 6 * 4, NULL);
}

void renderer_update_caches(int x, int y, int w, int h)
{
  update_texture_cache_region(&egpu, x, y, x + w - 1, y + h - 1);
}

void renderer_flush_queues(void)
{
  flush_render_block_buffer(&egpu);
}

void renderer_set_interlace(int enable, int is_odd)
{
  egpu.interlace_mode &= ~(RENDER_INTERLACE_ENABLED|RENDER_INTERLACE_ODD);
  if (enable)
    egpu.interlace_mode |= RENDER_INTERLACE_ENABLED;
  if (is_odd)
    egpu.interlace_mode |= RENDER_INTERLACE_ODD;
}

void renderer_set_config(const struct rearmed_cbs *cbs)
{
}

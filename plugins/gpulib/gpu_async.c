/*
 * (C) Gražvydas "notaz" Ignotas, 2025
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "gpu.h"
#include "gpu_async.h"
#include "gpu_timing.h"
#include "../../include/arm_features.h"
#include "../../include/compiler_features.h"
#include "../../frontend/pcsxr-threads.h"

//#define agpu_log gpu_log
#define agpu_log(...)

#define AGPU_BUF_LEN  (128*1024/4u)  // must be power of 2
#define AGPU_BUF_MASK (AGPU_BUF_LEN - 1)
#ifndef min
#define min(a, b) ((b) < (a) ? (b) : (a))
#endif

// must be in 0xc0...0xdf range that can't appear in thread's real cmd stream
#define FAKECMD_SCREEN_CHANGE 0xdfu

#if defined(__aarch64__) || defined(HAVE_ARMV6)
#define BARRIER() __asm__ __volatile__ ("dmb ishst" ::: "memory")
#else
#define BARRIER() __asm__ __volatile__ ("" ::: "memory")
#endif

enum waitmode {
  waitmode_none = 0,
  waitmode_progress,
  waitmode_full,
};

struct psx_gpu_async
{
  uint32_t pos_added;
  uint32_t pos_used;
  enum waitmode wait_mode;
  uint8_t exit;
  uint8_t idle;
  sthread_t *thread;
  slock_t *lock;
  scond_t *cond_use;
  scond_t *cond_add;
  uint32_t ex_regs[8]; // used by vram copy at least
  uint32_t cmd_buffer[AGPU_BUF_LEN];
};

union cmd_screen_change
{
  uint32_t u32s[4];
  struct {
    uint32_t cmd;
    short x, y;
    short src_x, src_y;
    short hres, vres;
  };
};

static int noinline do_notify_screen_change(struct psx_gpu *gpu,
    const union cmd_screen_change *cmd);

static int calc_space_for_add(struct psx_gpu_async *agpu)
{
  int pos_used, space;
  pos_used = *(volatile uint32_t *)&agpu->pos_used;
  space = AGPU_BUF_LEN - (agpu->pos_added - pos_used);
  assert(space >= 0);
  assert(space <= AGPU_BUF_LEN);
  return space;
}

// adds everything or nothing, else we may get incomplete cmd
static int do_add(struct psx_gpu_async *agpu, const uint32_t *list, int len)
{
  int pos, space, left, retval = 0;
  uint32_t pos_added = agpu->pos_added;

  assert(len < AGPU_BUF_LEN);
  space = calc_space_for_add(agpu);
  if (space < len)
    return 0;

  pos = pos_added & AGPU_BUF_MASK;
  left = AGPU_BUF_LEN - pos;
  if (left < len) {
    memset(&agpu->cmd_buffer[pos], 0, left * 4);
    pos_added += left;
    pos = 0;
    space = calc_space_for_add(agpu);
  }
  if (space >= len) {
    memcpy(&agpu->cmd_buffer[pos], list, len * 4);
    pos_added += len;
    retval = len;
  }
  BARRIER();
  *(volatile uint32_t *)&agpu->pos_added = pos_added;
  return retval;
}

static void do_add_with_wait(struct psx_gpu_async *agpu, const uint32_t *list, int len)
{
  for (;;)
  {
    if (do_add(agpu, list, len))
      break;
    slock_lock(agpu->lock);
    while (len > AGPU_BUF_LEN - (agpu->pos_added - agpu->pos_used)) {
      assert(!agpu->idle);
      assert(agpu->wait_mode == waitmode_none);
      agpu->wait_mode = waitmode_progress;
      scond_wait(agpu->cond_add, agpu->lock);
      agpu->wait_mode = waitmode_none;
    }
    slock_unlock(agpu->lock);
  }
}

static void run_thread_nolock(struct psx_gpu_async *agpu)
{
  if (agpu->idle) {
    agpu->idle = 0;
    scond_signal(agpu->cond_use);
  }
}

static void run_thread(struct psx_gpu_async *agpu)
{
  slock_lock(agpu->lock);
  run_thread_nolock(agpu);
  slock_unlock(agpu->lock);
}

int gpu_async_do_cmd_list(struct psx_gpu *gpu, uint32_t *list_data, int list_len,
 int *cpu_cycles_sum_out, int *cpu_cycles_last, int *last_cmd)
{
  uint32_t cyc_sum = 0, cyc = *cpu_cycles_last;
  struct psx_gpu_async *agpu = gpu->async;
  int dst_added = 0, dst_can_add = 1;
  int rendered_anything = 0;
  int cmd = -1, pos, len;

  assert(agpu);
  for (pos = 0; pos < list_len; pos += len)
  {
    const uint32_t *list = list_data + pos;
    const int16_t *slist = (void *)list;
    int rendered = 1, skip = 0;
    int num_vertexes, w, h;

    cmd = LE32TOH(list[0]) >> 24;
    len = 1 + cmd_lengths[cmd];
    if (pos + len > list_len) {
      cmd = -1;
      break; // incomplete cmd
    }

    switch (cmd) {
      case 0x02:
        w = LE16TOH(slist[4]) & 0x3FF;
        h = LE16TOH(slist[5]) & 0x1FF;
        gput_sum(cyc_sum, cyc, gput_fill(w, h));
        break;
      case 0x1f: // irq?
        goto breakloop;
      case 0x20 ... 0x23: gput_sum(cyc_sum, cyc, gput_poly_base());    break;
      case 0x24 ... 0x27: gput_sum(cyc_sum, cyc, gput_poly_base_t());  goto do_texpage;
      case 0x28 ... 0x2b: gput_sum(cyc_sum, cyc, gput_quad_base());    break;
      case 0x2c ... 0x2f: gput_sum(cyc_sum, cyc, gput_quad_base_t());  goto do_texpage;
      case 0x30 ... 0x33: gput_sum(cyc_sum, cyc, gput_poly_base_g());  break;
      case 0x34 ... 0x37: gput_sum(cyc_sum, cyc, gput_poly_base_gt()); goto do_texpage;
      case 0x38 ... 0x3b: gput_sum(cyc_sum, cyc, gput_quad_base_g());  break;
      case 0x3c ... 0x3f: gput_sum(cyc_sum, cyc, gput_quad_base_gt());
      do_texpage:
        gpu->ex_regs[1] &= ~0x1ff;
        gpu->ex_regs[1] |= (LE32TOH(list[4 + ((cmd >> 4) & 1)]) >> 16) & 0x1ff;
        break;
      case 0x40 ... 0x47:
        gput_sum(cyc_sum, cyc, gput_line(0));
        break;
      case 0x48 ... 0x4F:
        for (num_vertexes = 2; ; num_vertexes++)
        {
          gput_sum(cyc_sum, cyc, gput_line(0));
          if (pos + num_vertexes + 1 >= list_len) {
            cmd = -1;
            goto breakloop;
          }
          if ((list[num_vertexes + 1] & LE32TOH(0xf000f000)) == LE32TOH(0x50005000))
            break;
        }
        len += (num_vertexes - 2);
        break;
      case 0x50 ... 0x57:
        gput_sum(cyc_sum, cyc, gput_line(0));
        break;
      case 0x58 ... 0x5f:
        for (num_vertexes = 2; ; num_vertexes++)
        {
          gput_sum(cyc_sum, cyc, gput_line(0));
          if (pos + num_vertexes*2 >= list_len) {
            cmd = -1;
            goto breakloop;
          }
          if ((list[num_vertexes * 2] & LE32TOH(0xf000f000)) == LE32TOH(0x50005000))
            break;
        }
        len += (num_vertexes - 2) * 2;
        break;
      case 0x60 ... 0x63:
        w = LE16TOH(slist[4]) & 0x3FF;
        h = LE16TOH(slist[5]) & 0x1FF;
        gput_sum(cyc_sum, cyc, gput_sprite(w, h));
        break;
      case 0x64 ... 0x67:
        w = LE16TOH(slist[6]) & 0x3FF;
        h = LE16TOH(slist[7]) & 0x1FF;
        gput_sum(cyc_sum, cyc, gput_sprite(w, h));
        break;
      case 0x68 ... 0x6b: gput_sum(cyc_sum, cyc, gput_sprite(1, 1));   break;
      case 0x70 ... 0x73:
      case 0x74 ... 0x77: gput_sum(cyc_sum, cyc, gput_sprite(8, 8));   break;
      case 0x78 ... 0x7b:
      case 0x7C ... 0x7f: gput_sum(cyc_sum, cyc, gput_sprite(16, 16)); break;
      case 0x80 ... 0x9f: // vid -> vid
        w = ((LE16TOH(slist[6]) - 1) & 0x3ff) + 1;
        h = ((LE16TOH(slist[7]) - 1) & 0x1ff) + 1;
        gput_sum(cyc_sum, cyc, gput_copy(w, h));
        break;
      case 0xa0 ... 0xbf: // sys -> vid
      case 0xc0 ... 0xdf: // vid -> sys
        goto breakloop;
      case 0xe0 ... 0xe7:
        gpu->ex_regs[cmd & 7] = LE32TOH(list[0]);
        rendered = 0;
        break;
      default:
        rendered = 0;
        skip = 1;
        break;
    }
    rendered_anything |= rendered;
    if (dst_can_add) {
      if (!skip) {
        int added = dst_can_add = do_add(agpu, list, len);
        dst_added += added;
      }
      else
        dst_added += len;
    }
  }
breakloop:
  if (dst_added && (rendered_anything || dst_added < pos))
    run_thread(agpu);
  if (dst_added < pos) {
    int left = pos - dst_added;
    agpu_log(gpu, "agpu: wait %d left %d\n", agpu->pos_added - agpu->pos_used, left);
    do_add_with_wait(agpu, list_data + dst_added, left);
  }

  *cpu_cycles_sum_out += cyc_sum;
  *cpu_cycles_last = cyc;
  *last_cmd = cmd;
  return pos;
}

static STRHEAD_RET_TYPE gpu_async_thread(void *unused)
{
  struct psx_gpu *gpup = &gpu;
  struct psx_gpu_async *agpu = gpup->async;
  int dirty = 0;

  assert(agpu);
  slock_lock(agpu->lock);
  while (!agpu->exit)
  {
    int len = agpu->pos_added - agpu->pos_used;
    int pos, done, cycles_dummy = 0, cmd = -1;
    assert(len >= 0);
    if (len == 0 && !dirty) {
      if (agpu->wait_mode == waitmode_full)
        scond_signal(agpu->cond_add);
      agpu->idle = 1;
      scond_wait(agpu->cond_use, agpu->lock);
      continue;
    }
    slock_unlock(agpu->lock);

    if (len == 0 && dirty) {
      renderer_flush_queues();
      dirty = 0;
      slock_lock(agpu->lock);
      continue;
    }

    pos = agpu->pos_used & AGPU_BUF_MASK;
    len = min(len, AGPU_BUF_LEN - pos);
    done = renderer_do_cmd_list(agpu->cmd_buffer + pos, len, agpu->ex_regs,
             &cycles_dummy, &cycles_dummy, &cmd);
    if (done != len) {
      if (0x80 <= cmd && cmd < 0xa0)
        done += do_vram_copy(gpup->vram, agpu->ex_regs,
                  agpu->cmd_buffer + pos + done, &cycles_dummy);
      else if (cmd == FAKECMD_SCREEN_CHANGE)
        done += do_notify_screen_change(gpup,
                  (const void *)(agpu->cmd_buffer + pos + done));
      else if (0xa0 <= cmd && cmd < 0xec)
        assert(0); // todo?
      else
        assert(0); // should not happen
    }

    dirty = 1;
    assert(done > 0);
    slock_lock(agpu->lock);
    agpu->pos_used += done;
    if (agpu->wait_mode == waitmode_progress)
      scond_signal(agpu->cond_add);
  }
  slock_unlock(agpu->lock);
  STRHEAD_RETURN();
}

void gpu_async_notify_screen_change(struct psx_gpu *gpu)
{
  union cmd_screen_change cmd;

  if (!gpu->async || !gpu->state.enhancement_active) // gpu_neon only
    return;
  cmd.cmd = HTOLE32(FAKECMD_SCREEN_CHANGE << 24);
  cmd.x = gpu->screen.x;
  cmd.y = gpu->screen.y;
  cmd.hres = gpu->screen.hres;
  cmd.vres = gpu->screen.vres;
  cmd.src_x = gpu->screen.src_x;
  cmd.src_y = gpu->screen.src_y;
  do_add_with_wait(gpu->async, cmd.u32s, sizeof(cmd) / 4);
}

static int noinline do_notify_screen_change(struct psx_gpu *gpu,
    const union cmd_screen_change *cmd)
{
  struct psx_gpu_screen screen = gpu->screen;
  screen.x = cmd->x;
  screen.y = cmd->y;
  screen.hres = cmd->hres;
  screen.vres = cmd->vres;
  screen.src_x = cmd->src_x;
  screen.src_y = cmd->src_y;
  renderer_notify_screen_change(&screen);
  return sizeof(*cmd) / 4;
}

void gpu_async_sync(struct psx_gpu *gpu)
{
  struct psx_gpu_async *agpu = gpu->async;

  if (!agpu || (agpu->idle && agpu->pos_added == agpu->pos_used))
    return;
  agpu_log(gpu, "agpu: stall %d\n", agpu->pos_added - agpu->pos_used);
  slock_lock(agpu->lock);
  if (agpu->idle && agpu->pos_added != agpu->pos_used)
    run_thread_nolock(agpu);
  if (!agpu->idle) {
    assert(agpu->wait_mode == waitmode_none);
    agpu->wait_mode = waitmode_full;
    scond_wait(agpu->cond_add, agpu->lock);
    agpu->wait_mode = waitmode_none;
  }
  slock_unlock(agpu->lock);
  assert(agpu->pos_added == agpu->pos_used);
  assert(agpu->idle);
}

void gpu_async_sync_ecmds(struct psx_gpu *gpu)
{
  struct psx_gpu_async *agpu = gpu->async;
  int i;

  if (agpu) {
    for (i = 0; i < 6 && agpu->pos_added - agpu->pos_used < AGPU_BUF_LEN; i++)
      agpu->cmd_buffer[agpu->pos_added++ & AGPU_BUF_MASK] = gpu->ex_regs[i + 1];
    assert(i == 6);
  }
}

static void psx_gpu_async_free(struct psx_gpu_async *agpu)
{
  agpu->exit = 1;
  if (agpu->lock) {
    slock_lock(agpu->lock);
    if (agpu->cond_use)
      scond_signal(agpu->cond_use);
    slock_unlock(agpu->lock);
  }
  if (agpu->thread) {
    sthread_join(agpu->thread);
    agpu->thread = NULL;
  }
  if (agpu->cond_add) { scond_free(agpu->cond_add); agpu->cond_add = NULL; }
  if (agpu->cond_use) { scond_free(agpu->cond_use); agpu->cond_use = NULL; }
  if (agpu->lock)     { slock_free(agpu->lock); agpu->lock = NULL; }
  free(agpu);
}

void gpu_async_stop(struct psx_gpu *gpu)
{
  if (gpu->async) {
    gpu_async_sync(gpu);
    psx_gpu_async_free(gpu->async);
    gpu->async = NULL;
  }
}

void gpu_async_start(struct psx_gpu *gpu)
{
  struct psx_gpu_async *agpu;
  if (gpu->async)
    return;

  agpu = calloc(1, sizeof(*agpu));
  if (agpu) {
    agpu->lock = slock_new();
    agpu->cond_add = scond_new();
    agpu->cond_use = scond_new();
    if (agpu->lock && agpu->cond_add && agpu->cond_use) {
      gpu->async = agpu;
      agpu->thread = pcsxr_sthread_create(gpu_async_thread, PCSXRT_GPU);
    }
    if (agpu->thread) {
      gpu_async_sync_ecmds(gpu);
      return;
    }
  }

  SysPrintf("gpu thread init failed\n");
  gpu->async = NULL;
  if (agpu)
    psx_gpu_async_free(agpu);
}

// vim:shiftwidth=2:expandtab

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

// these constants must be power of 2
#define AGPU_BUF_LEN    (128*1024/4u)
#define AGPU_BUF_MASK   (AGPU_BUF_LEN - 1)
#define AGPU_AREAS_CNT  8u
#define AGPU_AREAS_MASK (AGPU_AREAS_CNT - 1)

#ifndef min
#define min(a, b) ((b) < (a) ? (b) : (a))
#endif

// must be in 0xc0...0xdf range that can't appear in thread's real cmd stream;
// must be at least 3 words due to cmd_lengths[]
#define FAKECMD_SCREEN_CHANGE 0xdfu
#define FAKECMD_SET_INTERLACE 0xdeu
#define FAKECMD_DMA_WRITE     0xddu
#define FAKECMD_BREAK         0xdcu

#if defined(__aarch64__) || defined(HAVE_ARMV7)
#define BARRIER() __asm__ __volatile__ ("dmb ishst" ::: "memory")
#elif defined(HAVE_ARMV6)
#define BARRIER() __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" :: "r"(0) : "memory")
#else
#define BARRIER() __asm__ __volatile__ ("" ::: "memory")
#endif
#define RDPOS(pos_) *(volatile uint32_t *)&(pos_)
#define WRPOS(pos_, d_) *(volatile uint32_t *)&(pos_) = (d_)

enum waitmode {
  waitmode_none = 0,
  waitmode_progress,
  waitmode_target,
  waitmode_full,
};

struct pos_drawarea {
  uint32_t pos;
  uint16_t x0, y0;
  uint16_t x1, y1;
};

struct psx_gpu_async
{
  uint32_t pos_added;
  uint32_t pos_used;
  uint32_t pos_target;
  enum waitmode wait_mode;
  uint8_t exit;
  uint8_t idle;
  sthread_t *thread;
  slock_t *lock;
  scond_t *cond_use;
  scond_t *cond_add;
  uint32_t ex_regs[8]; // used by vram copy at least
  uint32_t cmd_buffer[AGPU_BUF_LEN];
  uint32_t pos_area;
  struct pos_drawarea draw_areas[AGPU_AREAS_CNT];
};

// cmd_* must be at least 3 words long
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

union cmd_set_interlace
{
  uint32_t u32s[3];
  struct {
    uint32_t cmd;
    int enable, is_odd;
  };
};

union cmd_dma_write
{
  uint32_t u32s[3];
  struct {
    uint32_t cmd;
    short x, y, w, h;
  };
};

struct cmd_break
{
  uint32_t u32s[3];
};

static int noinline do_notify_screen_change(struct psx_gpu *gpu,
    const union cmd_screen_change *cmd);
static int do_set_interlace(struct psx_gpu *gpu,
    const union cmd_set_interlace *cmd);
static int do_dma_write(struct psx_gpu *gpu,
    const union cmd_dma_write *cmd, uint32_t pos);

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

static int calc_space_for_add(struct psx_gpu_async *agpu, uint32_t pos_added)
{
  int space = AGPU_BUF_LEN - (pos_added - RDPOS(agpu->pos_used));
  assert(space >= 0);
  assert(space <= AGPU_BUF_LEN);
  return space;
}

// adds everything or nothing, else we may get incomplete cmd
static int do_add_pos(struct psx_gpu_async *agpu, const void *list, int list_words,
    uint32_t *pos_added_)
{
  int pos, space, left, retval = 0;
  uint32_t pos_added = *pos_added_;

  assert(list_words < AGPU_BUF_LEN);
  space = calc_space_for_add(agpu, pos_added);
  if (space < list_words)
    return 0;

  pos = pos_added & AGPU_BUF_MASK;
  left = AGPU_BUF_LEN - pos;
  if (left < list_words) {
    memset(&agpu->cmd_buffer[pos], 0, left * 4);
    pos_added += left;
    pos = 0;
    space = calc_space_for_add(agpu, pos_added);
  }
  if (space >= list_words) {
    memcpy(&agpu->cmd_buffer[pos], list, list_words * 4);
    pos_added += list_words;
    retval = list_words;
  }
  *pos_added_ = pos_added;
  return retval;
}

static int do_add(struct psx_gpu_async *agpu, const void *list, int list_words)
{
  uint32_t pos_added = agpu->pos_added;
  int ret = do_add_pos(agpu, list, list_words, &pos_added);
  BARRIER();
  WRPOS(agpu->pos_added, pos_added);
  return ret;
}

static void do_add_with_wait(struct psx_gpu_async *agpu,
    const void *list, int list_words)
{
  for (;;)
  {
    if (do_add(agpu, list, list_words))
      break;
    slock_lock(agpu->lock);
    run_thread_nolock(agpu);
    while (list_words > AGPU_BUF_LEN - (agpu->pos_added - RDPOS(agpu->pos_used))) {
      assert(!agpu->idle);
      assert(agpu->wait_mode == waitmode_none);
      agpu->wait_mode = waitmode_progress;
      scond_wait(agpu->cond_add, agpu->lock);
    }
    slock_unlock(agpu->lock);
  }
}

static void add_draw_area(struct psx_gpu_async *agpu, uint32_t pos, int force,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint32_t pos_area = agpu->pos_area;
  if (pos - agpu->draw_areas[pos_area].pos > 1u || force)
    pos_area = agpu->pos_area = (pos_area + 1) & AGPU_AREAS_MASK;
  agpu->draw_areas[pos_area].pos = pos;
  agpu->draw_areas[pos_area].x0 = x0;
  agpu->draw_areas[pos_area].y0 = y0;
  agpu->draw_areas[pos_area].x1 = x1;
  agpu->draw_areas[pos_area].y1 = y1;
}

static void add_draw_area_e(struct psx_gpu_async *agpu, uint32_t pos, int force,
    const uint32_t *ex_regs)
{
  add_draw_area(agpu, pos, force,
       ex_regs[3] & 0x3ff,       (ex_regs[3] >> 10) & 0x1ff,
      (ex_regs[4] & 0x3ff) + 1, ((ex_regs[4] >> 10) & 0x1ff) + 1);
}

int gpu_async_do_cmd_list(struct psx_gpu *gpu, const uint32_t *list_data, int list_len,
 int *cpu_cycles_sum_out, int *cpu_cycles_last, int *last_cmd)
{
  uint32_t cyc_sum = 0, cyc = *cpu_cycles_last;
  struct psx_gpu_async *agpu = gpu->async;
  int pos_handled = 0, dst_can_add = 1;
  int rendered_anything = 0;
  int insert_break = 0;
  int cmd = -1, pos, len;

  assert(agpu);
  for (pos = 0; pos < list_len; pos += len)
  {
    const uint32_t *list = list_data + pos;
    const int16_t *slist = (void *)list;
    const struct pos_drawarea *darea;
    int rendered = 1, skip = 0;
    int num_vertexes, x, y, w, h;

    cmd = LE32TOH(list[0]) >> 24;
    len = 1 + cmd_lengths[cmd];
    if (pos + len > list_len) {
      cmd = -1;
      break; // incomplete cmd
    }

    switch (cmd) {
      case 0x02:
        x =  (LE16TOH(slist[2]) & 0x3ff) & ~0xf;
        y =   LE16TOH(slist[3]) & 0x1ff;
        w = ((LE16TOH(slist[4]) & 0x3ff) + 0xf) & ~0xf;
        h =   LE16TOH(slist[5]) & 0x1ff;
        darea = &agpu->draw_areas[agpu->pos_area];
        if (x < darea->x0 || x + w > darea->x1 || y < darea->y0 || y + h > darea->y1) {
          // let the main thread know about changes outside of drawing area
          agpu_log(gpu, "agpu: fill %d,%d %dx%d vs area %d,%d %dx%d\n", x, y, w, h,
            darea->x0, darea->y0, darea->x1 - darea->x0, darea->y1 - darea->y0);
          add_draw_area(agpu, agpu->pos_added, 1, x, y, x + w, y + h);
          add_draw_area_e(agpu, agpu->pos_added + 1, 1, gpu->ex_regs);
        }
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
        x =   LE16TOH(slist[4]) & 0x3ff;
        y =   LE16TOH(slist[5]) & 0x1ff;
        w = ((LE16TOH(slist[6]) - 1) & 0x3ff) + 1;
        h = ((LE16TOH(slist[7]) - 1) & 0x1ff) + 1;
        darea = &agpu->draw_areas[agpu->pos_area];
        if ((w > 2 || h > 1) &&
            (x < darea->x0 || x + w > darea->x1 || y < darea->y0 || y + h > darea->y1))
        {
          add_draw_area(agpu, agpu->pos_added, 1, x, y, x + w, y + h);
          add_draw_area_e(agpu, agpu->pos_added + 1, 1, gpu->ex_regs);
        }
        gput_sum(cyc_sum, cyc, gput_copy(w, h));
        break;
      case 0xa0 ... 0xbf: // sys -> vid
      case 0xc0 ... 0xdf: // vid -> sys
        goto breakloop;
      case 0xe0 ... 0xe2:
      case 0xe5 ... 0xe7:
        gpu->ex_regs[cmd & 7] = LE32TOH(list[0]);
        rendered = 0;
        break;
      case 0xe3:
      case 0xe4:
        rendered = 0;
        if (gpu->ex_regs[cmd & 7] == LE32TOH(list[0])) {
          skip = 1;
          break;
        }
        gpu->ex_regs[cmd & 7] = LE32TOH(list[0]);
        add_draw_area_e(agpu, agpu->pos_added, 0, gpu->ex_regs);
        insert_break = 1;
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
        pos_handled += added;
      }
      else
        pos_handled += len;
    }
  }
breakloop:
  if (pos_handled && (rendered_anything || pos_handled < pos))
    run_thread(agpu);
  if (pos_handled < pos) {
    // note: this is poorly implemented (wrong pos_added for draw_areas)
    int left = pos - pos_handled;
    agpu_log(gpu, "agpu: full %d left %d\n", agpu->pos_added - agpu->pos_used, left);
    do_add_with_wait(agpu, list_data + pos_handled, left);
  }
  if (insert_break) {
    struct cmd_break cmd = {{ HTOLE32(FAKECMD_BREAK << 24), }};
    do_add(agpu, cmd.u32s, sizeof(cmd.u32s) / sizeof(cmd.u32s[0]));
  }

  *cpu_cycles_sum_out += cyc_sum;
  *cpu_cycles_last = cyc;
  *last_cmd = cmd;
  return pos;
}

int gpu_async_try_dma(struct psx_gpu *gpu, const uint32_t *data, int words)
{
  struct psx_gpu_async *agpu = gpu->async;
  int used, w = gpu->dma.w, h = gpu->dma.h;
  uint32_t pos_added = agpu->pos_added;
  union cmd_dma_write cmd;
  int bad = 0;

  if (!agpu)
    return 0;
  // avoid double copying
  used = agpu->pos_added - RDPOS(agpu->pos_used);
  if (agpu->idle && used == 0)
    return 0;
  // only proceed if there is space to avoid messy sync
  if (AGPU_BUF_LEN - used < sizeof(cmd) / 4 + ((w + 1) / 2) * (h + 1)) {
    agpu_log(gpu, "agpu: dma: used %d\n", used);
    return 0;
  }

  cmd.cmd = HTOLE32(FAKECMD_DMA_WRITE << 24);
  cmd.x = gpu->dma.x; cmd.y = gpu->dma.y;
  cmd.w = gpu->dma.w; cmd.h = gpu->dma.h;
  bad |= !do_add_pos(agpu, cmd.u32s, sizeof(cmd) / 4, &pos_added);
  if (w & 1) {
    // align lines to psx dma word units
    const uint16_t *sdata = (const uint16_t *)data;
    for (; h > 0; sdata += w, h--)
      bad |= !do_add_pos(agpu, sdata, w / 2 + 1, &pos_added);
  }
  else {
    for (; h > 0; data += w / 2, h--)
      bad |= !do_add_pos(agpu, data, w / 2, &pos_added);
  }
  assert(!bad); (void)bad;

  slock_lock(agpu->lock);
  agpu->pos_added = pos_added;
  run_thread_nolock(agpu);
  slock_unlock(agpu->lock);

  return 1;
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
    int len = RDPOS(agpu->pos_added) - agpu->pos_used;
    int pos = agpu->pos_used & AGPU_BUF_MASK;
    int done, cycles_dummy = 0, cmd = -1;
    assert(len >= 0);
    if (len == 0 && !dirty) {
      switch (agpu->wait_mode) {
        case waitmode_full:
        case waitmode_target:
          agpu->wait_mode = waitmode_none;
          scond_signal(agpu->cond_add);
          break;
        case waitmode_none:
          break;
        default:
          assert(0);
      }
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

    len = min(len, AGPU_BUF_LEN - pos);
    done = renderer_do_cmd_list(agpu->cmd_buffer + pos, len, agpu->ex_regs,
             &cycles_dummy, &cycles_dummy, &cmd);
    if (done != len) {
      const void *list = agpu->cmd_buffer + pos + done;
      switch (cmd) {
        case 0x80 ... 0x9f:
          done += do_vram_copy(gpup->vram, agpu->ex_regs, list, &cycles_dummy);
          break;
        case FAKECMD_SCREEN_CHANGE:
          done += do_notify_screen_change(gpup, list);
          break;
        case FAKECMD_SET_INTERLACE:
          done += do_set_interlace(gpup, list);
          break;
        case FAKECMD_DMA_WRITE:
          done += do_dma_write(gpup, list, pos + done);
          break;
        case FAKECMD_BREAK:
          done += sizeof(struct cmd_break) / 4;
          break;
        default:
          assert(0);
          if (!done)
            done = 1;
          break;
      }
    }

    dirty = 1;
    assert(done > 0);
    slock_lock(agpu->lock);
    agpu->pos_used += done;
    switch (agpu->wait_mode) {
      case waitmode_target:
        if ((int32_t)(agpu->pos_used - agpu->pos_target) < 0)
          break;
        // fallthrough
      case waitmode_progress:
        agpu->wait_mode = waitmode_none;
        scond_signal(agpu->cond_add);
        break;
      default:
        break;
    }
  }
  slock_unlock(agpu->lock);
  STRHEAD_RETURN();
}

void gpu_async_notify_screen_change(struct psx_gpu *gpu)
{
  union cmd_screen_change cmd;

  if (!gpu->async)
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

void gpu_async_set_interlace(struct psx_gpu *gpu, int enable, int is_odd)
{
  union cmd_set_interlace cmd;

  if (!gpu->async)
    return;
  cmd.cmd = HTOLE32(FAKECMD_SET_INTERLACE << 24);
  cmd.enable = enable;
  cmd.is_odd = is_odd;
  do_add_with_wait(gpu->async, cmd.u32s, sizeof(cmd) / 4);
}

static int do_set_interlace(struct psx_gpu *gpu,
    const union cmd_set_interlace *cmd)
{
  renderer_flush_queues();
  renderer_set_interlace(cmd->enable, cmd->is_odd);
  return sizeof(*cmd) / 4;
}

static int do_dma_write(struct psx_gpu *gpu,
    const union cmd_dma_write *cmd, uint32_t pos)
{
  int x = cmd->x, y = cmd->y, w = cmd->w, h = cmd->h;
  struct psx_gpu_async *agpu = gpu->async;
  uint32_t r6 = agpu->ex_regs[6] & 3;
  uint16_t *vram = gpu->vram;
  int stride = (w + 1) / 2;
  int done = 0;

  pos += sizeof(*cmd) / 4u;
  done += sizeof(*cmd) / 4u;
  assert(pos <= AGPU_BUF_LEN);
  for (; h > 0; h--, y++) {
    if (stride > AGPU_BUF_LEN - pos) {
      done += AGPU_BUF_LEN - pos;
      pos = 0;
    }

    y &= 511;
    do_vram_line(vram, x, y, (uint16_t *)&agpu->cmd_buffer[pos], w, 0, r6);
    pos += stride;
    done += stride;
  }
  renderer_update_caches(x, cmd->y, w, cmd->h, 0);
  return done;
}

void gpu_async_sync(struct psx_gpu *gpu)
{
  struct psx_gpu_async *agpu = gpu->async;

  if (!agpu || (agpu->idle && agpu->pos_added == RDPOS(agpu->pos_used)))
    return;
  agpu_log(gpu, "agpu: sync %d\n", agpu->pos_added - agpu->pos_used);
  slock_lock(agpu->lock);
  if (agpu->idle && agpu->pos_added != RDPOS(agpu->pos_used)) {
    agpu_log(gpu, "agpu: idle %d\n", agpu->pos_added - agpu->pos_used);
    run_thread_nolock(agpu);
  }
  if (!agpu->idle) {
    assert(agpu->wait_mode == waitmode_none);
    agpu->wait_mode = waitmode_full;
    scond_wait(agpu->cond_add, agpu->lock);
  }
  slock_unlock(agpu->lock);
  assert(agpu->pos_added == agpu->pos_used);
  assert(agpu->idle);
}

void gpu_async_sync_scanout(struct psx_gpu *gpu)
{
  struct psx_gpu_async *agpu = gpu->async;
  int so_x0 = gpu->screen.src_x, so_y0 = gpu->screen.src_y;
  int so_x1 = so_x0 + gpu->screen.hres, so_y1 = so_y0 + gpu->screen.vres;
  uint32_t pos;
  int c, i;

  if (!agpu)
    return;
  pos = RDPOS(agpu->pos_used);
  if (agpu->idle && agpu->pos_added == pos)
    return;
  i = agpu->pos_area;
  if (agpu->idle)
    /* unlikely but possible - do a full sync */;
  else if (so_x1 > 1024 || so_y1 > 512) {
    agpu_log(gpu, "agpu: wrap %d,%d %dx%d\n",
      so_x0, so_y0, so_x1 - so_x0, so_y1 - so_y0);
  }
  else if (agpu->draw_areas[(i+1) & AGPU_AREAS_MASK].pos > pos) {
    agpu_log(gpu, "agpu: oldest draw area %d > %d\n",
      agpu->draw_areas[(i+1) & AGPU_AREAS_MASK].pos, pos);
  }
  else {
    for (c = 0, i = agpu->pos_area; c < AGPU_AREAS_CNT;
         c++, i = (i - 1) & AGPU_AREAS_MASK)
    {
      int area_x0 = agpu->draw_areas[i].x0, area_y0 = agpu->draw_areas[i].y0;
      int area_x1 = agpu->draw_areas[i].x1, area_y1 = agpu->draw_areas[i].y1;
      if (so_x1 <= area_x0 || area_x1 <= so_x0)
        /* no x intersect */;
      else if (so_y1 <= area_y0 || area_y1 <= so_y0)
        /* no y intersect */;
      else {
        agpu_log(gpu, "agpu: scanout #%d %d,%d %dx%d hit %d,%d %dx%d\n",
          c, so_x0, so_y0, so_x1 - so_x0, so_y1 - so_y0,
          area_x0, area_y0, area_x1 - area_x0, area_y1 - area_y0);
        break;
      }
      pos = RDPOS(agpu->pos_used);
      if (pos >= agpu->draw_areas[i].pos)
        return;
    }
    if (c > 0) {
      i = (i + 1) & AGPU_AREAS_MASK;
      agpu_log(gpu, "agpu: wait %d/%d\n", agpu->draw_areas[i].pos - agpu->pos_used,
          agpu->pos_added - agpu->pos_used);
      slock_lock(agpu->lock);
      if (!agpu->idle) {
        assert(agpu->wait_mode == waitmode_none);
        agpu->pos_target = agpu->draw_areas[i].pos + 1;
        agpu->wait_mode = waitmode_target;
        scond_wait(agpu->cond_add, agpu->lock);
      }
      slock_unlock(agpu->lock);
      return;
    }
  }
  gpu_async_sync(gpu);
}

void gpu_async_sync_ecmds(struct psx_gpu *gpu)
{
  struct psx_gpu_async *agpu = gpu->async;
  if (agpu)
    do_add_with_wait(agpu, gpu->ex_regs + 1, 6);
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

  assert(AGPU_DMA_MAX <= AGPU_BUF_LEN / 2);

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

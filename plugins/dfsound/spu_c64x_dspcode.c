/*
 * SPU processing offload to TI C64x DSP using bsp's c64_tools
 * (C) Gražvydas "notaz" Ignotas, 2015
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of
 *  this software and associated documentation files (the "Software"), to deal in
 *  the Software without restriction, including without limitation the rights to
 *  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is furnished to do
 *  so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#define SYSCALLS_C
#include <libc64_dsp/include/inc_overlay.h>
#include <stddef.h>

#include "spu.c"
#include "spu_c64x.h"

/* dummy deps, some bloat but avoids ifdef hell in SPU code.. */
static void thread_work_start(void) {}
static void thread_work_wait_sync(struct work_item *work, int force) {}
static int  thread_get_i_done(void) { return 0; }
struct out_driver *out_current;
void SetupSound(void) {}


static void invalidate_cache(struct work_item *work)
{
 syscalls.cache_inv(work, offsetof(typeof(*work), RVB), 1);
 syscalls.cache_inv(spu.s_chan, sizeof(spu.s_chan[0]) * 24, 0);
 syscalls.cache_inv(work->SSumLR,
   sizeof(work->SSumLR[0]) * 2 * work->ns_to, 0);
}

static void writeout_cache(struct work_item *work)
{
 int ns_to = work->ns_to;

 syscalls.cache_wb(work->RVB, sizeof(work->RVB[0]) * 2 * ns_to, 1);
 syscalls.cache_wb(work->SSumLR, sizeof(work->SSumLR[0]) * 2 * ns_to, 1);
}

static void do_processing(void)
{
 struct work_item *work;
 int left, dirty = 0;

 while (worker->active)
 {
  // i_ready is in first cacheline
  syscalls.cache_inv(worker, 64, 1);

  left = worker->i_ready - worker->i_done;
  if (left > 0) {
   dirty = 1;
   worker->active = ACTIVE_CNT;
   syscalls.cache_wb(&worker->active, 4, 1);

   work = &worker->i[worker->i_done & WORK_I_MASK];
   invalidate_cache(work);
   do_channel_work(work);
   writeout_cache(work);

   worker->i_done++;
   syscalls.cache_wb(&worker->i_done, 4, 1);
   continue;
  }

  // nothing to do? Write out non-critical caches
  if (dirty) {
   syscalls.cache_wb(spu.spuMemC + 0x800, 0x800, 1);
   syscalls.cache_wb(spu.SB, sizeof(spu.SB[0]) * SB_SIZE * 24, 1);
   dirty = 0;
   continue;
  }

  // this ->active loop thing is to avoid a race where we miss
  // new work and clear ->active just after ARM checks it
  worker->active--;
  syscalls.cache_wb(&worker->active, 4, 1);
 }
}

static unsigned int exec(dsp_component_cmd_t cmd,
  unsigned int arg1, unsigned int arg2,
  unsigned int *ret1, unsigned int *ret2)
{
 struct region_mem *mem = (void *)arg1;

 switch (cmd) {
  case CCMD_INIT:
   InitADSR();

   spu.spuMemC = mem->spu_ram;
   spu.SB = mem->SB;
   spu.s_chan = mem->s_chan;
   worker = &mem->worker;
   memcpy(&spu_config, &mem->spu_config, sizeof(spu_config));

   mem->sizeof_region_mem = sizeof(*mem);
   mem->offsetof_s_chan1 = offsetof(typeof(*mem), s_chan[1]);
   mem->offsetof_spos_3_20 = offsetof(typeof(*mem), worker.i[3].ch[20]);
   // seems to be unneeded, no write-alloc? but just in case..
   syscalls.cache_wb(&mem->sizeof_region_mem, 3 * 4, 1);
   break;

  case CCMD_DOIT:
   worker->active = ACTIVE_CNT;
   worker->boot_cnt++;
   syscalls.cache_wb(&worker->i_done, 64, 1);
   memcpy(&spu_config, &mem->spu_config, sizeof(spu_config));

   do_processing();

   // c64_tools lib does BCACHE_wbInvAll() when it receives mailbox irq,
   // but invalidate anyway in case c64_tools is ever fixed..
   syscalls.cache_inv(mem, sizeof(mem->spu_ram) + sizeof(mem->SB), 0);
   break;

  default:
   syscalls.printf("bad cmd: %x\n", cmd);
   break;
 }

 return 0;
}

#pragma DATA_SECTION(component_test_dsp, ".sec_com");
dsp_component_t component_test_dsp = {
 {
  NULL,       /* init */
  exec,
  NULL,       /* exec fastcall RPC */
  NULL,       /* exit */
 },

 COMPONENT_NAME,
};

DSP_COMPONENT_MAIN

// vim:shiftwidth=1:expandtab

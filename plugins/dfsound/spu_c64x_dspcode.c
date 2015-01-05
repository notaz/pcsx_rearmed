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
static void thread_work_wait_sync(void) {}
static void thread_sync_caches(void) {}
struct out_driver *out_current;
void SetupSound(void) {}

#if 0
// no use, c64_tools does BCACHE_wbInvAll..
static void sync_caches(void)
{
 int ns_to = worker->ns_to;

 syscalls.cache_wb(spu.sRVBStart, sizeof(spu.sRVBStart[0]) * 2 * ns_to, 1);
 syscalls.cache_wb(SSumLR, sizeof(SSumLR[0]) * 2 * ns_to, 1);

 syscalls.cache_wbInv(worker, sizeof(*worker), 1);
}
#endif

static unsigned int exec(dsp_component_cmd_t cmd,
  unsigned int arg1, unsigned int arg2,
  unsigned int *ret1, unsigned int *ret2)
{
 struct region_mem *mem = (void *)arg1;
 int i;

 switch (cmd) {
  case CCMD_INIT:
   InitADSR();

   spu.spuMemC = mem->spu_ram;
   spu.sRVBStart = mem->RVB;
   SSumLR = mem->SSumLR;
   spu.SB = mem->SB;
   spu.s_chan = mem->s_chan;
   worker = &mem->worker;
   memcpy(&spu_config, &mem->spu_config, sizeof(spu_config));

   mem->sizeof_region_mem = sizeof(*mem);
   mem->offsetof_s_chan1 = offsetof(typeof(*mem), s_chan[1]);
   mem->offsetof_worker_ram = offsetof(typeof(*mem), worker.ch[1]);
   // seems to be unneeded, no write-alloc? but just in case..
   syscalls.cache_wb(&mem->sizeof_region_mem, 3 * 4, 1);
   break;

  case CCMD_DOIT:
   do_channel_work();
   // c64_tools lib does BCACHE_wbInvAll() when it receives mailbox irq,
   // so there is no benefit of syncing only what's needed.
   // But call wbInvAll() anyway in case c64_tools is ever fixed..
   //sync_caches();
   syscalls.cache_wbInvAll();
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

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

#include <dlfcn.h>
#include <inc_libc64_mini.h>
#include "spu_c64x.h"

static dsp_mem_region_t region;

static struct {
 void *handle;
 int  (*dsp_open)(void);
 dsp_mem_region_t (*dsp_shm_alloc)(dsp_cache_t _type, sU32 _numBytes);
 int  (*dsp_shm_free)(dsp_mem_region_t _mem);
 void (*dsp_close)(void);
 int  (*dsp_component_load)(const char *_path, const char *_name, dsp_component_id_t *_id);
 int  (*dsp_cache_inv_virt)(void *_virtAddr, sU32 _size);
 int  (*dsp_rpc_send)(const dsp_msg_t *_msgTo);
 int  (*dsp_rpc_recv)(dsp_msg_t *_msgFrom);
 void (*dsp_logbuf_print)(void);
} f;

static void thread_work_start(void)
{
 do_channel_work();
}

static void thread_work_wait_sync(void)
{
}

static void init_spu_thread(void)
{
 struct region_mem *mem;
 int ret;

 if (f.handle == NULL) {
  const char lib[] = "libc64.so.1";
  int failed = 0;

  f.handle = dlopen(lib, RTLD_NOW);
  if (f.handle == NULL) {
   fprintf(stderr, "can't load %s: %s\n", lib, dlerror());
   return;
  }
  #define LDS(name) \
    failed |= (f.name = dlsym(f.handle, #name)) == NULL
  LDS(dsp_open);
  LDS(dsp_close);
  LDS(dsp_shm_alloc);
  LDS(dsp_shm_free);
  LDS(dsp_cache_inv_virt);
  LDS(dsp_component_load);
  LDS(dsp_rpc_send);
  LDS(dsp_rpc_recv);
  LDS(dsp_logbuf_print);
  #undef LDS
  if (failed) {
   fprintf(stderr, "missing symbol(s) in %s\n", lib);
   dlclose(f.handle);
   f.handle = NULL;
   return;
  }
 }

 ret = f.dsp_open();
 if (ret != 0) {
  fprintf(stderr, "dsp_open failed: %d\n", ret);
  return;
 }

 region = f.dsp_shm_alloc(DSP_CACHE_R, sizeof(*mem)); // writethrough
 if (region.size < sizeof(*mem) || region.virt_addr == 0) {
  fprintf(stderr, "dsp_shm_alloc failed\n");
  goto fail_mem;
 }
 mem = (void *)region.virt_addr;

 // override default allocations
 free(spu.spuMemC);
 spu.spuMemC = mem->spu_ram;
 free(spu.sRVBStart);
 spu.sRVBStart = mem->RVB;
 free(SSumLR);
 SSumLR = mem->SSumLR;
 free(spu.s_chan);
 spu.s_chan = mem->s_chan;
 worker = &mem->worker;

 printf("C64x DSP ready.\n");
 return;

fail_mem:
 f.dsp_close();
 worker = NULL;
}

static void exit_spu_thread(void)
{
 if (worker == NULL)
  return;

 if (worker->pending)
  thread_work_wait_sync();
 f.dsp_shm_free(region);
 f.dsp_close();

 spu.spuMemC = NULL;
 spu.sRVBStart = NULL;
 SSumLR = NULL;
 spu.s_chan = NULL;
 worker = NULL;
}

// vim:shiftwidth=1:expandtab

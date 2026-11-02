/* SPDX-License-Identifier: GPL-2.0-or-later */
#define renderer_init peops_renderer_init
#define renderer_finish peops_renderer_finish
#define renderer_do_cmd_list peops_renderer_do_cmd_list
#define renderer_sync_ecmds peops_renderer_sync_ecmds
#define renderer_update_caches peops_renderer_update_caches
#define renderer_flush_queues peops_renderer_flush_queues
#define renderer_set_interlace peops_renderer_set_interlace
#define renderer_set_config peops_renderer_set_config
#define renderer_notify_screen_change peops_renderer_notify_screen_change
#include "../dfxvideo/gpulib_if.c"
#undef renderer_init
#undef renderer_finish
#undef renderer_do_cmd_list
#undef renderer_sync_ecmds
#undef renderer_update_caches
#undef renderer_flush_queues
#undef renderer_set_interlace
#undef renderer_set_config
#undef renderer_notify_screen_change
#include "rsx.h"
static void (*reference_prims[256])(unsigned char *);
static int reference_saved;
void rsx_peops_vram(void *vram) { set_vram(vram); }
void rsx_peops_handlers(void (*handler)(unsigned char *)) {
  unsigned i;
  if (!reference_saved) {
    memcpy(reference_prims, primTableJ, sizeof(reference_prims));
    reference_saved = 1;
  }
  memcpy(primTableJ, reference_prims, sizeof(reference_prims));
  if (handler) {
    primTableJ[2] = handler;
    for (i = 0x20; i < 0x80; i++) primTableJ[i] = handler;
  }
}
void rsx_peops_call(unsigned cmd, unsigned char *data) { reference_prims[cmd](data); }
void rsx_peops_tpage(uint32_t mode) {
  lGPUstatusRet = (lGPUstatusRet & ~0x1ff) | (mode & 0x1ff);
}

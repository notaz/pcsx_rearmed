/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PCSX_GPU_RSX_H
#define PCSX_GPU_RSX_H
#include <stdint.h>
#include "libretro_rsx.h"
#ifdef RSX_DIAGNOSTICS
extern uint64_t gpu_rsx_diagnostics[16];
#define RSX_COUNT(i,n) (gpu_rsx_diagnostics[i]+=(n))
#else
#define RSX_COUNT(i,n) ((void)0)
#endif
struct rearmed_cbs;
struct rsx_rect { int x, y, w, h; };
struct rsx_vertex { float x, y, z, w, u, v; uint32_t color, pad; };
struct rsx_primitive {
  struct rsx_vertex v[6];
  struct rsx_rect bounds;
  unsigned count, mode, tpage, clut, replace, semi, raw, fill, dither;
};
static inline uint32_t rsx_word(const uint32_t *p) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return __builtin_bswap32(*p);
#else
  return *p;
#endif
}
int rsx_decode(struct rsx_primitive *, const uint32_t *, unsigned, const uint32_t *);
void gpu_rsx_negotiate(retro_environment_t, int);
int gpu_rsx_active(void);
void gpu_rsx_suspend_output(void);
void rsx_hw_suspend_output(void);
int gpu_rsx_output(const void *, int, int, int, int, int, int, int, int);
int gpu_rsx_present(retro_video_refresh_t);
void gpu_rsx_begin_frame(void);
void gpu_rsx_prepare_vram(int, int, int, int);
int gpu_rsx_copy_vram(const uint32_t *, const uint32_t *);
void gpu_rsx_shadow_changed(int, int, int, int);
int gpu_rsx_is_replaying(void);
void rsx_peops_vram(void *);
void rsx_peops_handlers(void (*)(unsigned char *));
void rsx_peops_call(unsigned, unsigned char *);
void rsx_peops_tpage(uint32_t);
int peops_renderer_init(void);
void peops_renderer_finish(void);
int peops_renderer_do_cmd_list(uint32_t *, int, uint32_t *, int *, int *, int *);
void peops_renderer_sync_ecmds(uint32_t *);
void peops_renderer_update_caches(int,int,int,int,int);
void peops_renderer_flush_queues(void);
void peops_renderer_set_interlace(int,int);
void peops_renderer_set_config(const struct rearmed_cbs *);
struct psx_gpu_screen;
void peops_renderer_notify_screen_change(const struct psx_gpu_screen *);
int rsx_hw_create(const struct retro_hw_render_interface_rsx *);
void rsx_hw_destroy(void);
int rsx_hw_draw(const struct rsx_primitive *, const uint32_t *);
int rsx_hw_copy(const uint32_t *, const uint32_t *);
int rsx_hw_upload(int,int,int,int,const uint16_t *);
int rsx_hw_output(int,int,int,int,int,int,int,int,int);
int rsx_hw_present(retro_video_refresh_t);
void rsx_hw_flush(void);
void rsx_hw_set_dither(int);
#endif

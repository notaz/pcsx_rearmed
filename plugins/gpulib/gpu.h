/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_BUFFER_LEN          1024

struct psx_gpu {
  uint32_t cmd_buffer[CMD_BUFFER_LEN];
  uint32_t regs[16];
  uint16_t *vram;
  union {
    uint32_t reg;
    struct {
      uint32_t tx:4;        //  0 texture page
      uint32_t ty:1;
      uint32_t abr:2;
      uint32_t tp:2;        //  7 t.p. mode (4,8,15bpp)
      uint32_t dtd:1;       //  9 dither
      uint32_t dfe:1;
      uint32_t md:1;        // 11 set mask bit when drawing
      uint32_t me:1;        // 12 no draw on mask
      uint32_t unkn:3;
      uint32_t width1:1;    // 16
      uint32_t width0:2;
      uint32_t dheight:1;   // 19 double height
      uint32_t video:1;     // 20 NTSC,PAL
      uint32_t rgb24:1;
      uint32_t interlace:1; // 22 interlace on
      uint32_t blanking:1;  // 23 display not enabled
      uint32_t unkn2:2;
      uint32_t busy:1;      // 26 !busy drawing
      uint32_t img:1;       // 27 ready to DMA image data
      uint32_t com:1;       // 28 ready for commands
      uint32_t dma:2;       // 29 off, ?, to vram, from vram
      uint32_t lcf:1;       // 31
    };
  } status;
  uint32_t gp0;
  uint32_t ex_regs[8];
  struct {
    int hres, vres;
    int x, y, w, h;
    int x1, x2;
    int y1, y2;
  } screen;
  struct {
    int x, y, w, h;
    short int offset, is_read;
  } dma, dma_start;
  int cmd_len;
  uint32_t zero;
  struct {
    uint32_t fb_dirty:1;
    uint32_t old_interlace:1;
    uint32_t allow_interlace:2;
    uint32_t blanked:1;
    uint32_t enhancement_enable:1;
    uint32_t enhancement_active:1;
    uint32_t *frame_count;
    uint32_t *hcnt; /* hsync count */
    struct {
      uint32_t addr;
      uint32_t cycles;
      uint32_t frame;
      uint32_t hcnt;
    } last_list;
    uint32_t last_vram_read_frame;
  } state;
  struct {
    int32_t set:3; /* -1 auto, 0 off, 1-3 fixed */
    int32_t cnt:3; /* amount skipped in a row */
    uint32_t active:1;
    uint32_t allow:1;
    uint32_t frame_ready:1;
    const int *advice;
    uint32_t last_flip_frame;
    uint32_t pending_fill[3];
  } frameskip;
  int useDithering:1; /* 0 - off , 1 - on */
  uint16_t *(*get_enhancement_bufer)
    (int *x, int *y, int *w, int *h, int *vram_h);
  void *(*mmap)(unsigned int size);
  void  (*munmap)(void *ptr, unsigned int size);
};

extern struct psx_gpu gpu;

extern const unsigned char cmd_lengths[256];

int do_cmd_list(uint32_t *list, int count, int *last_cmd);

struct rearmed_cbs;

int  renderer_init(void);
void renderer_finish(void);
void renderer_sync_ecmds(uint32_t * ecmds);
void renderer_update_caches(int x, int y, int w, int h);
void renderer_flush_queues(void);
void renderer_set_interlace(int enable, int is_odd);
void renderer_set_config(const struct rearmed_cbs *config);
void renderer_notify_res_change(void);

int  vout_init(void);
int  vout_finish(void);
void vout_update(void);
void vout_blank(void);
void vout_set_config(const struct rearmed_cbs *config);

/* listing these here for correct linkage if rasterizer uses c++ */
struct GPUFreeze;

long GPUinit(void);
long GPUshutdown(void);
void GPUwriteDataMem(uint32_t *mem, int count);
long GPUdmaChain(uint32_t *rambase, uint32_t addr);
void GPUwriteData(uint32_t data);
void GPUreadDataMem(uint32_t *mem, int count);
uint32_t GPUreadData(void);
uint32_t GPUreadStatus(void);
void GPUwriteStatus(uint32_t data);
long GPUfreeze(uint32_t type, struct GPUFreeze *freeze);
void GPUupdateLace(void);
long GPUopen(void **dpy);
long GPUclose(void);
void GPUvBlank(int is_vblank, int lcf);
void GPUrearmedCallbacks(const struct rearmed_cbs *cbs_);

#ifdef __cplusplus
}
#endif

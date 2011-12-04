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
  uint16_t vram[1024 * 512];
  uint16_t guard[1024 * 512]; // overdraw guard
  uint32_t cmd_buffer[CMD_BUFFER_LEN];
  uint32_t regs[16];
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
    int offset;
  } dma;
  int cmd_len;
  uint32_t zero;
  struct {
    uint32_t fb_dirty:1;
    uint32_t frame_count;
    uint32_t *hcnt;
    struct {
      uint32_t addr;
      uint32_t words;
      uint32_t frame;
      uint32_t hcnt;
    } last_list;
  } state;
  struct {
    int32_t set:3; /* -1 auto, 0 off, 1 fixed */
    uint32_t active:1;
    uint32_t frame_ready:1;
    uint32_t skipped_blits:5;
    const int *advice;
  } frameskip;
};

extern struct psx_gpu gpu;

extern const unsigned char cmd_lengths[256];

void do_cmd_list(uint32_t *list, int count);

int  renderer_init(void);
void renderer_sync_ecmds(uint32_t * ecmds);
void renderer_invalidate_caches(int x, int y, int w, int h);
void renderer_flush_queues(void);

int vout_init(void);
int vout_finish(void);

#ifdef __cplusplus
}
#endif

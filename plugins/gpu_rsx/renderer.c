/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdlib.h>
#include <string.h>
#include "../gpulib/gpu.h"
#include "../../frontend/plugin_lib.h"
#include "rsx.h"
#include "journal.h"
#ifdef RSX_DIAGNOSTICS
uint64_t gpu_rsx_diagnostics[16];
#endif
#define VRAM_BYTES (1024 * 512 * 2)
#define JOURNAL_BYTES (8 * 1024 * 1024)
#define JOURNAL_RECORDS 16384
static struct {
  retro_environment_t environ;
  uint16_t *base, *scratch, *upload;
  unsigned char *data;
  struct rsx_journal_record *records;
  unsigned count, used;
  uint32_t *regs, *end;
  int initialized, active, replaying, requested;
  uint32_t needed[RSX_JOURNAL_WORDS];
} renderer;
static void hardware_primitive(unsigned char *);
int gpu_rsx_active(void) { return renderer.active; }
void gpu_rsx_suspend_output(void) { rsx_hw_suspend_output(); }
int gpu_rsx_is_replaying(void) { return renderer.replaying; }
static void select_records(struct rsx_rect wanted) {
  rsx_journal_select(renderer.records,renderer.count,renderer.data,wanted,renderer.needed);
}
static void replay(struct rsx_rect wanted) {
  RSX_COUNT(0,1);
  unsigned i;
  uint32_t saved_regs[8];
  memcpy(saved_regs,renderer.regs?renderer.regs:gpu.ex_regs,sizeof(saved_regs));
  select_records(wanted);
  memcpy(renderer.scratch,renderer.base,VRAM_BYTES);
  renderer.replaying=1;
  rsx_peops_handlers(NULL); rsx_peops_vram(renderer.scratch);
  for(i=0;i<renderer.count;i++) {
    struct rsx_journal_record *r=&renderer.records[i];
    const unsigned char *p=renderer.data+r->offset;
    if(!r->keep) continue;
    if(r->type==0) {
      uint32_t regs[8]; int cycles=0,last=0,cmd;
      memcpy(regs,r->regs,sizeof(regs));
      peops_renderer_sync_ecmds(regs);
      peops_renderer_do_cmd_list((uint32_t *)p,r->bytes/4,regs,&cycles,&last,&cmd);
    } else if(r->type==1) {
      int x,y;
      const uint16_t *src=(const uint16_t *)p;
      for(y=0;y<r->rect.h;y++) for(x=0;x<r->rect.w;x++)
        renderer.scratch[((r->rect.y+y)&511)*1024+((r->rect.x+x)&1023)]=*src++;
    } else do_vram_copy(renderer.scratch,r->regs,(const uint32_t *)p);
  }
  rsx_peops_vram(gpu.vram); peops_renderer_sync_ecmds(saved_regs);
  renderer.replaying=0;
  if(renderer.active) rsx_peops_handlers(hardware_primitive);
}
static void checkpoint(void) {
  RSX_COUNT(1,1);
  struct rsx_rect all={0,0,1024,512};
  if(!renderer.count) return;
  replay(all);
  memcpy(gpu.vram,renderer.scratch,VRAM_BYTES);
  memcpy(renderer.base,renderer.scratch,VRAM_BYTES);
  renderer.count=renderer.used=0;
}
static void compact(void) {
  RSX_COUNT(2,1);
  struct rsx_rect all={0,0,1024,512};
  unsigned i,n=0,used=0;
  select_records(all);
  for(i=0;i<renderer.count;i++) {
    struct rsx_journal_record r=renderer.records[i];
    if(!r.keep) continue;
    memmove(renderer.data+used,renderer.data+r.offset,r.bytes);
    r.offset=used; used=(used+r.bytes+3)&~3u; renderer.records[n++]=r;
  }
  renderer.used=used; renderer.count=n;
}
static struct rsx_journal_record *record(unsigned bytes) {
  struct rsx_journal_record *r;
  if(renderer.count==JOURNAL_RECORDS || renderer.used+bytes>JOURNAL_BYTES) compact();
  if(renderer.count==JOURNAL_RECORDS || renderer.used+bytes>JOURNAL_BYTES) checkpoint();
  if(bytes>JOURNAL_BYTES) return NULL;
  r=&renderer.records[renderer.count++]; memset(r,0,sizeof(*r));
  memcpy(r->regs,renderer.regs?renderer.regs:gpu.ex_regs,sizeof(r->regs));
  r->offset=renderer.used; r->bytes=bytes; renderer.used=(renderer.used+bytes+3)&~3u;
  return r;
}
static void disable(void) {
  if(!renderer.active) return;
  checkpoint(); renderer.active=0;
  rsx_peops_handlers(NULL); rsx_hw_destroy();
}
static void fallback(void) {
  if(!renderer.active) return;
  disable(); SysPrintf("PCSX RSX: using software rendering after a resource failure\n");
}
void gpu_rsx_prepare_vram(int x,int y,int w,int h) {
  struct rsx_rect rect={x,y,w,h};
  int xx,yy;
  if(!renderer.active || renderer.replaying || !renderer.count) return;
  if(w>=1024 && h>=512) { checkpoint(); return; }
  RSX_COUNT(3,w*h);
  replay(rect);
  for(yy=0;yy<h;yy++) for(xx=0;xx<w;xx++) {
    unsigned at=((y+yy)&511)*1024+((x+xx)&1023);
    gpu.vram[at]=renderer.scratch[at];
  }
}
void gpu_rsx_shadow_changed(int x,int y,int w,int h) {
  struct rsx_journal_record *r;
  uint16_t *dst;
  int xx,yy;
  if(!renderer.active || renderer.replaying) return;
  for(yy=0;yy<h;yy++) for(xx=0;xx<w;xx++)
    renderer.upload[yy*w+xx]=gpu.vram[((y+yy)&511)*1024+((x+xx)&1023)];
  r=record(w*h*2);
  if(!r) { fallback(); return; }
  r->type=1; r->replace=1; r->rect=(struct rsx_rect){x,y,w,h};
  dst=(uint16_t *)(renderer.data+r->offset);
  memcpy(dst,renderer.upload,w*h*2);
  for(yy=0;yy<h;yy++) for(xx=0;xx<w;xx++)
    gpu.vram[((y+yy)&511)*1024+((x+xx)&1023)]=dst[yy*w+xx];
  if(!rsx_hw_upload(x,y,w,h,gpu.vram)) fallback();
}
static int line_strip(struct rsx_rect *bounds,const uint32_t *words,unsigned n,int draw) {
  struct rsx_primitive p;
  uint32_t packet[4];
  unsigned cmd=rsx_word(words)>>24,gouraud=!!(cmd&16),i=2;
  uint32_t color=rsx_word(words),xy=words[1];
  int first=1;
  while(i+(gouraud?1:0)<n-1) {
    uint32_t next_color=gouraud?rsx_word(words+i):color;
    uint32_t next_xy=words[i+(gouraud?1:0)];
    packet[0]=HTOLE32((color&0xffffff)|((gouraud?0x50:0x40)|(cmd&7))<<24);
    packet[1]=xy;
    if(gouraud) { packet[2]=HTOLE32(next_color); packet[3]=next_xy; }
    else packet[2]=next_xy;
    if(!rsx_decode(&p,packet,gouraud?4:3,renderer.regs)) return 0;
    if(p.count) {
      if(first) { *bounds=p.bounds; first=0; }
      else {
        int x1=bounds->x+bounds->w,y1=bounds->y+bounds->h;
        if(x1<p.bounds.x+p.bounds.w) x1=p.bounds.x+p.bounds.w;
        if(y1<p.bounds.y+p.bounds.h) y1=p.bounds.y+p.bounds.h;
        if(bounds->x>p.bounds.x) bounds->x=p.bounds.x;
        if(bounds->y>p.bounds.y) bounds->y=p.bounds.y;
        bounds->w=x1-bounds->x; bounds->h=y1-bounds->y;
      }
      if(draw && !rsx_hw_draw(&p,renderer.regs)) return 0;
    }
    color=next_color; xy=next_xy; i+=gouraud?2:1;
  }
  return 1;
}
static void hardware_primitive(unsigned char *data) {
  RSX_COUNT(4,1);
  uint32_t *words=(uint32_t *)data;
  unsigned cmd=rsx_word(words)>>24, n=cmd_lengths[cmd]+1;
  struct rsx_primitive p;
  struct rsx_journal_record *r;
  int strip=cmd>=0x48 && cmd<0x60 && (cmd&8);
  if(!renderer.active) { rsx_peops_call(cmd,data); return; }
  if(strip) {
    unsigned at=(cmd&16)?4:3,step=(cmd&16)?2:1;
    for(;words+at<renderer.end;at+=step)
      if((rsx_word(words+at)&0xf000f000)==0x50005000) break;
    if(words+at>=renderer.end) strip=0;
    else n=at+1;
  }
  if(!rsx_decode(&p,words,n,renderer.regs)) {
    RSX_COUNT(5,1);
    /* Unhandled commands retain the reference rasterizer. */
    checkpoint(); rsx_peops_call(cmd,data);
    memcpy(renderer.base,gpu.vram,VRAM_BYTES);
    if(!rsx_hw_upload(0,0,1024,512,gpu.vram)) fallback();
    return;
  }
  if(strip && !line_strip(&p.bounds,words,n,0)) { fallback(); rsx_peops_call(cmd,data); return; }
  r=record(n*4);
  if(!r) { fallback(); rsx_peops_call(cmd,data); return; }
  r->type=0; r->rect=p.bounds; r->replace=p.replace;
  r->mode=p.mode; r->tpage=p.tpage; r->clut=p.clut;
  memcpy(renderer.data+r->offset,words,n*4);
  if(!(strip?line_strip(&p.bounds,words,n,1):rsx_hw_draw(&p,renderer.regs))) { fallback(); return; }
  if(p.mode && cmd<0x40) {
    renderer.regs[1]=(renderer.regs[1]&~511u)|p.tpage;
    rsx_peops_tpage(p.tpage);
  }
}
int gpu_rsx_copy_vram(const uint32_t *regs,const uint32_t *data) {
  struct rsx_journal_record *r;
  int sx,sy,dx,dy,w,h;
  if(!renderer.active || renderer.replaying) return 0;
  sx=rsx_word(data+1)&1023; sy=(rsx_word(data+1)>>16)&511;
  dx=rsx_word(data+2)&1023; dy=(rsx_word(data+2)>>16)&511;
  w=((rsx_word(data+3)-1)&1023)+1; h=(((rsx_word(data+3)>>16)-1)&511)+1;
  if(sx+w>1024 || dx+w>1024 || sy+h>512 || dy+h>512 ||
      (sx<dx+w && dx<sx+w && sy<dy+h && dy<sy+h)) {
    gpu_rsx_prepare_vram(0,0,1024,512); return 0;
  }
  r=record(16);
  if(!r) { fallback(); return 0; }
  r->type=2; r->replace=!(regs[6]&2); r->rect=(struct rsx_rect){dx,dy,w,h};
  memcpy(r->regs,regs,sizeof(r->regs)); memcpy(renderer.data+r->offset,data,16);
  if(!rsx_hw_copy(data,regs)) fallback();
  return 1;
}
static void context_destroy(void);
static void context_reset(void) {
  const struct retro_hw_render_interface *base=NULL;
  const struct retro_hw_render_interface_rsx *iface;
  context_destroy();
  if(!renderer.initialized || !renderer.requested) return;
  if(!renderer.environ(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE,&base) || !base ||
      base->interface_type!=RETRO_HW_RENDER_INTERFACE_RSX ||
      base->interface_version<RETRO_HW_RENDER_INTERFACE_RSX_VERSION) return;
  iface=(const struct retro_hw_render_interface_rsx *)base;
  if(!iface->context || !iface->io_address || !iface->set_image || !iface->retire_memory) return;
  renderer.base=malloc(VRAM_BYTES); renderer.scratch=malloc(VRAM_BYTES); renderer.upload=malloc(VRAM_BYTES);
  renderer.data=malloc(JOURNAL_BYTES); renderer.records=calloc(JOURNAL_RECORDS,sizeof(*renderer.records));
  if(!renderer.base || !renderer.scratch || !renderer.upload || !renderer.data || !renderer.records) goto fail;
  memcpy(renderer.base,gpu.vram,VRAM_BYTES);
  if(!rsx_hw_create(iface)) goto fail;
  renderer.active=1; rsx_peops_handlers(hardware_primitive);
  SysPrintf("PCSX RSX: hardware rasterizer active (libretro RSX v1)\n");
  return;
fail:
  free(renderer.base); free(renderer.scratch); free(renderer.upload); free(renderer.data); free(renderer.records);
  renderer.base=renderer.scratch=renderer.upload=NULL; renderer.data=NULL; renderer.records=NULL;
  SysPrintf("PCSX RSX: initialization failed; using software rendering\n");
}
static void context_destroy(void) {
  disable();
  free(renderer.base); free(renderer.scratch); free(renderer.upload); free(renderer.data); free(renderer.records);
  renderer.base=renderer.scratch=renderer.upload=NULL; renderer.data=NULL; renderer.records=NULL;
  renderer.count=renderer.used=0;
}
void gpu_rsx_negotiate(retro_environment_t environ,int enabled) {
  struct retro_hw_render_callback cb;
  context_destroy(); renderer.environ=environ; renderer.requested=0;
  if(!enabled) return;
  memset(&cb,0,sizeof(cb)); cb.context_type=RETRO_HW_CONTEXT_RSX; cb.version_major=RETRO_HW_RENDER_INTERFACE_RSX_VERSION;
  cb.context_reset=context_reset; cb.context_destroy=context_destroy;
  renderer.requested=environ(RETRO_ENVIRONMENT_SET_HW_RENDER,&cb);
}
int gpu_rsx_output(const void *vram,int ofs,int bgr24,int x,int y,int w,int h,int outw,int outh) {
  if(!renderer.active) return 0;
  if(rsx_hw_output(vram==NULL,ofs,bgr24,x,y,w,h,outw,outh)) return 1;
  fallback(); return 0;
}
int gpu_rsx_present(retro_video_refresh_t cb) {
  return renderer.active && rsx_hw_present(cb);
}
int renderer_init(void) {
  renderer.initialized=1;
  return peops_renderer_init();
}
void renderer_finish(void) {
  context_destroy(); renderer.initialized=0; peops_renderer_finish();
}
int renderer_do_cmd_list(uint32_t *list,int count,uint32_t *regs,int *sum,int *last,int *cmd) {
  int result;
  renderer.regs=regs; renderer.end=list+count;
  result=peops_renderer_do_cmd_list(list,count,regs,sum,last,cmd);
  renderer.regs=NULL; return result;
}
void renderer_sync_ecmds(uint32_t *regs) { peops_renderer_sync_ecmds(regs); }
void renderer_update_caches(int x,int y,int w,int h,int state_changed) {
  if(renderer.replaying) return;
  if(state_changed && renderer.active) {
    RSX_COUNT(6,1);
    renderer.count=renderer.used=0; memcpy(renderer.base,gpu.vram,VRAM_BYTES);
    if(!rsx_hw_upload(0,0,1024,512,gpu.vram)) fallback();
  } else gpu_rsx_shadow_changed(x,y,w,h);
  peops_renderer_update_caches(x,y,w,h,state_changed);
}
void renderer_flush_queues(void) { if(renderer.active && !renderer.replaying) rsx_hw_flush(); }
void renderer_set_interlace(int enable,int odd) { peops_renderer_set_interlace(enable,odd); }
void renderer_notify_screen_change(const struct psx_gpu_screen *s) { peops_renderer_notify_screen_change(s); }
void renderer_set_config(const struct rearmed_cbs *cbs) {
  static int dithering=-1;
  static uint32_t fixes;
  if(renderer.active && (dithering!=cbs->dithering || fixes!=cbs->gpu_peops.dwActFixes)) checkpoint();
  rsx_hw_set_dither(cbs->dithering);
  dithering=cbs->dithering; fixes=cbs->gpu_peops.dwActFixes;
  peops_renderer_set_config(cbs);
}

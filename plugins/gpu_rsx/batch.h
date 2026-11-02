/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PCSX_RSX_BATCH_H
#define PCSX_RSX_BATCH_H
#include <string.h>
#include "rsx.h"
#define RSX_BATCH_CAPACITY 64
#define RSX_BATCH_READS 6
struct rsx_batch_item {
  struct rsx_primitive primitive;
  uint32_t regs[8];
};
struct rsx_primitive_batch {
  struct rsx_batch_item items[RSX_BATCH_CAPACITY];
  unsigned count;
};
static int rsx_rect_intersects(struct rsx_rect a,struct rsx_rect b) {
  return a.w>0 && a.h>0 && b.w>0 && b.h>0 &&
    a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}
static unsigned rsx_read_rows(struct rsx_rect *rects,unsigned count,int x,int y,int w,int h) {
  int first;
  y&=511; first=512-y;
  if(h>first) {
    rects[count++]=(struct rsx_rect){x,y,w,first};
    rects[count++]=(struct rsx_rect){x,0,w,h-first};
  } else rects[count++]=(struct rsx_rect){x,y,w,h};
  return count;
}
static unsigned rsx_read_rect(struct rsx_rect *rects,unsigned count,int x,int y,int w,int h) {
  int first=1024-x;
  if(w>first) {
    count=rsx_read_rows(rects,count,x,y,first,h);
    count=rsx_read_rows(rects,count,0,y+1,w-first,h);
  } else count=rsx_read_rows(rects,count,x,y,w,h);
  return count;
}
static unsigned rsx_primitive_reads(const struct rsx_primitive *p,const uint32_t regs[8],struct rsx_rect rects[RSX_BATCH_READS]) {
  unsigned count=0;
  if(p->semi || (regs[6]&2)) rects[count++]=p->bounds;
  if(p->mode) {
    count=rsx_read_rect(rects,count,(p->tpage&15)*64,((p->tpage>>4)&1)*256,64u<<(p->mode-1),256);
    if(p->mode<3) count=rsx_read_rect(rects,count,(p->clut&63)*16,(p->clut>>6)&511,p->mode==1?16:256,1);
  }
  return count;
}
static int rsx_batch_can_add(const struct rsx_primitive_batch *batch,const struct rsx_primitive *p,const uint32_t regs[8]) {
  struct rsx_rect reads[RSX_BATCH_READS];
  unsigned i,j,count;
  if(batch->count==RSX_BATCH_CAPACITY) return 0;
  count=rsx_primitive_reads(p,regs,reads);
  /* All reads use the snapshot taken before the group. */
  for(i=0;i<batch->count;i++) for(j=0;j<count;j++)
    if(rsx_rect_intersects(batch->items[i].primitive.bounds,reads[j])) return 0;
  return 1;
}
static void rsx_batch_tiles(uint32_t bits[64],struct rsx_rect r) {
  int x,y;
  for(y=r.y/16;y<(r.y+r.h+15)/16;y++) for(x=r.x/16;x<(r.x+r.w+15)/16;x++) {
    unsigned bit=(y&31)*64+(x&63);
    bits[bit/32]|=1u<<(bit%32);
  }
}
static void rsx_batch_snapshot_tiles(const struct rsx_primitive_batch *batch,uint32_t bits[64]) {
  struct rsx_rect reads[RSX_BATCH_READS];
  unsigned i,j,count;
  memset(bits,0,64*sizeof(*bits));
  for(i=0;i<batch->count;i++) {
    count=rsx_primitive_reads(&batch->items[i].primitive,batch->items[i].regs,reads);
    for(j=0;j<count;j++) rsx_batch_tiles(bits,reads[j]);
  }
}
#endif

/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PCSX_RSX_JOURNAL_H
#define PCSX_RSX_JOURNAL_H
#include <string.h>
#include "rsx.h"
#define RSX_JOURNAL_TILE 4
#define RSX_JOURNAL_COLUMNS (1024 / RSX_JOURNAL_TILE)
#define RSX_JOURNAL_ROWS (512 / RSX_JOURNAL_TILE)
#define RSX_JOURNAL_ROW_WORDS (RSX_JOURNAL_COLUMNS / 32)
#define RSX_JOURNAL_WORDS (RSX_JOURNAL_ROWS * RSX_JOURNAL_ROW_WORDS)
struct rsx_journal_record {
  struct rsx_rect rect;
  uint32_t regs[8];
  unsigned offset, bytes, type, replace, keep, mode, tpage, clut;
};
/* Only completely overwritten cells lose their dependencies. */
static void rsx_journal_tiles(uint32_t bits[RSX_JOURNAL_WORDS],struct rsx_rect r,int clear) {
  int x,y,x0,y0,x1,y1;
  if(r.w<=0 || r.h<=0) return;
  x0=r.x/RSX_JOURNAL_TILE; y0=r.y/RSX_JOURNAL_TILE;
  x1=(r.x+r.w+RSX_JOURNAL_TILE-1)/RSX_JOURNAL_TILE;
  y1=(r.y+r.h+RSX_JOURNAL_TILE-1)/RSX_JOURNAL_TILE;
  if(clear) {
    x0=(r.x+RSX_JOURNAL_TILE-1)/RSX_JOURNAL_TILE;
    y0=(r.y+RSX_JOURNAL_TILE-1)/RSX_JOURNAL_TILE;
    x1=(r.x+r.w)/RSX_JOURNAL_TILE; y1=(r.y+r.h)/RSX_JOURNAL_TILE;
  }
  for(y=y0;y<y1;y++) for(x=x0;x<x1;) {
    unsigned column=x&(RSX_JOURNAL_COLUMNS-1),n=32-(column&31);
    unsigned word=(y&(RSX_JOURNAL_ROWS-1))*RSX_JOURNAL_ROW_WORDS+column/32;
    uint32_t mask;
    if(n>(unsigned)(x1-x)) n=x1-x;
    mask=(UINT32_MAX>>(32-n))<<(column&31);
    if(clear) bits[word]&=~mask; else bits[word]|=mask;
    x+=n;
  }
}
static void rsx_journal_texture_tiles(uint32_t bits[RSX_JOURNAL_WORDS],struct rsx_rect r) {
  if(r.x+r.w>1024) {
    struct rsx_rect tail={0,r.y+1,r.x+r.w-1024,r.h};
    r.w=1024-r.x;
    rsx_journal_tiles(bits,tail,0);
  }
  rsx_journal_tiles(bits,r,0);
}
static int rsx_journal_intersects(const uint32_t bits[RSX_JOURNAL_WORDS],struct rsx_rect r) {
  int x,y,x0,y0,x1,y1;
  if(r.w<=0 || r.h<=0) return 0;
  x0=r.x/RSX_JOURNAL_TILE; y0=r.y/RSX_JOURNAL_TILE;
  x1=(r.x+r.w+RSX_JOURNAL_TILE-1)/RSX_JOURNAL_TILE;
  y1=(r.y+r.h+RSX_JOURNAL_TILE-1)/RSX_JOURNAL_TILE;
  for(y=y0;y<y1;y++) for(x=x0;x<x1;) {
    unsigned column=x&(RSX_JOURNAL_COLUMNS-1),n=32-(column&31);
    unsigned word=(y&(RSX_JOURNAL_ROWS-1))*RSX_JOURNAL_ROW_WORDS+column/32;
    uint32_t mask;
    if(n>(unsigned)(x1-x)) n=x1-x;
    mask=(UINT32_MAX>>(32-n))<<(column&31);
    if(bits[word]&mask) return 1;
    x+=n;
  }
  return 0;
}
static void rsx_journal_select(struct rsx_journal_record *records,unsigned count,const unsigned char *data,
                               struct rsx_rect wanted,uint32_t needed[RSX_JOURNAL_WORDS]) {
  unsigned i=count;
  memset(needed,0,RSX_JOURNAL_WORDS*sizeof(*needed));
  rsx_journal_tiles(needed,wanted,0);
  while(i--) {
    struct rsx_journal_record *r=&records[i];
    r->keep=rsx_journal_intersects(needed,r->rect);
    if(!r->keep) continue;
    if(r->replace) rsx_journal_tiles(needed,r->rect,1);
    if(r->type==0 && r->mode) {
      struct rsx_rect page={(r->tpage&15)*64,((r->tpage>>4)&1)*256,64u<<(r->mode-1),256};
      struct rsx_rect clut={(r->clut&63)*16,(r->clut>>6)&511,r->mode==1?16:256,1};
      rsx_journal_texture_tiles(needed,page);
      if(r->mode<3) rsx_journal_texture_tiles(needed,clut);
    } else if(r->type==2) {
      const uint32_t *p=(const uint32_t *)(data+r->offset);
      struct rsx_rect src={rsx_word(p+1)&1023,(rsx_word(p+1)>>16)&511,r->rect.w,r->rect.h};
      rsx_journal_tiles(needed,src,0);
    }
  }
}
#endif

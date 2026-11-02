/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../batch.h"
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
#define PIXELS (1024*512)
static unsigned random_state=0x7824a19b;
static unsigned random_word(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
static struct rsx_primitive primitive(int x,int y,int mode) {
  struct rsx_primitive p;
  memset(&p,0,sizeof(p));p.count=6;p.semi=1;p.mode=mode;p.bounds=(struct rsx_rect){x,y,4,4};p.tpage=8|16;p.clut=400*64;
  return p;
}
static void append(struct rsx_primitive_batch *b,struct rsx_primitive p,const uint32_t regs[8]) {
  CHECK(b->count<RSX_BATCH_CAPACITY);b->items[b->count].primitive=p;memcpy(b->items[b->count].regs,regs,32);b->count++;
}
static void dependencies(void) {
  struct rsx_primitive_batch *b=calloc(1,sizeof(*b));
  struct rsx_primitive p=primitive(0,0,0),q=primitive(4,0,0);
  struct rsx_rect reads[RSX_BATCH_READS];uint32_t regs[8]={0},bits[64];unsigned i;
  CHECK(b!=NULL);append(b,p,regs);CHECK(rsx_batch_can_add(b,&q,regs));
  q.bounds.x=3;CHECK(!rsx_batch_can_add(b,&q,regs));
  q=primitive(16,16,1);q.tpage=0;CHECK(!rsx_batch_can_add(b,&q,regs));
  q.tpage=8|16;q.clut=0;CHECK(!rsx_batch_can_add(b,&q,regs));
  b->count=0;p=primitive(32,257,0);append(b,p,regs);q=primitive(16,16,3);q.tpage=15|16;
  CHECK(!rsx_batch_can_add(b,&q,regs));
  b->count=0;p=primitive(32,0,0);append(b,p,regs);q=primitive(16,16,3);q.tpage=15|16;
  CHECK(!rsx_batch_can_add(b,&q,regs));
  q=primitive(16,16,2);q.clut=511*64+63;
  CHECK(!rsx_batch_can_add(b,&q,regs));
  b->count=0;p=primitive(0,0,1);p.tpage=8|16;append(b,p,regs);q=primitive(512,256,0);
  CHECK(rsx_batch_can_add(b,&q,regs));
  b->count=0;p=primitive(16,16,2);p.tpage=15|16;p.clut=511*64+63;
  CHECK(rsx_primitive_reads(&p,regs,reads)==6);append(b,p,regs);rsx_batch_snapshot_tiles(b,bits);
  CHECK(bits[0]&1u);CHECK(bits[2]&2u);CHECK(bits[63]&(1u<<31));
  b->count=0;
  for(i=0;i<RSX_BATCH_CAPACITY;i++) {p=primitive(i*4,0,0);CHECK(rsx_batch_can_add(b,&p,regs));append(b,p,regs);}
  p=primitive(300,0,0);CHECK(!rsx_batch_can_add(b,&p,regs));
  free(b);puts("dependency and wrapping checks passed");
}
static uint16_t load_word(const uint16_t *vram,unsigned x,unsigned y) {return vram[(y*1024+x)&(PIXELS-1)];}
static unsigned blend(unsigned s,unsigned d,unsigned mode) {
  switch(mode) {case 0:return s/2+d/2;case 1:return s+d>31?31:s+d;case 2:return d>s?d-s:0;default:return d+s/4>31?31:d+s/4;}
}
static void render(uint16_t *dst,const uint16_t *src,const struct rsx_batch_item *item) {
  const struct rsx_primitive *p=&item->primitive;int x,y;unsigned color=p->v[0].color,mask=item->regs[6];
  for(y=p->bounds.y;y<p->bounds.y+p->bounds.h;y++) for(x=p->bounds.x;x<p->bounds.x+p->bounds.w;x++) {
    unsigned at=y*1024+x,old=src[at],texel=0,c[3],back[3]={old&31,(old>>5)&31,(old>>10)&31},j;
    if((mask&2) && (old&0x8000)) continue;
    c[0]=(color>>24)/8;c[1]=((color>>16)&255)/8;c[2]=((color>>8)&255)/8;
    if(p->mode) {
      unsigned u=((unsigned)p->v[0].u+x-p->bounds.x)&255,v=((unsigned)p->v[0].v+y-p->bounds.y)&255;
      unsigned px=(p->tpage&15)*64,py=((p->tpage>>4)&1)*256;
      if(p->mode==3) texel=load_word(src,px+u,py+v);
      else {
        unsigned index,word=load_word(src,px+u/(p->mode==1?4:2),py+v);
        if(p->mode==1) index=(word>>((u&3)*4))&15;else index=(word>>((u&1)*8))&255;
        texel=load_word(src,(p->clut&63)*16+index,(p->clut>>6)&511);
      }
      if(!texel) continue;
      c[0]=texel&31;c[1]=(texel>>5)&31;c[2]=(texel>>10)&31;
      if(!p->raw) for(j=0;j<3;j++) {unsigned byte=(color>>(24-j*8))&255;c[j]=c[j]*byte/128;if(c[j]>31)c[j]=31;}
    }
    if(p->semi && (!p->mode || (texel&0x8000))) for(j=0;j<3;j++)c[j]=blend(c[j],back[j],(p->tpage>>5)&3);
    dst[at]=c[0]|c[1]<<5|c[2]<<10|(((mask&1)||(texel&0x8000))?0x8000:0);
  }
}
static void execute_batch(uint16_t *dst,uint16_t *snapshot,struct rsx_primitive_batch *batch) {
  uint32_t bits[64];unsigned i,y;
  if(!batch->count)return;
  rsx_batch_snapshot_tiles(batch,bits);
  for(i=0;i<2048;i++) if(bits[i/32]&(1u<<(i%32))) for(y=0;y<16;y++) {
    unsigned at=((i/64)*16+y)*1024+(i%64)*16;memcpy(snapshot+at,dst+at,32);
  }
  for(i=0;i<batch->count;i++)render(dst,snapshot,&batch->items[i]);
  batch->count=0;
}
static void equivalence(void) {
  uint16_t *a=malloc(PIXELS*2),*b=malloc(PIXELS*2),*sa=malloc(PIXELS*2),*sb=malloc(PIXELS*2);
  struct rsx_primitive_batch *batch=calloc(1,sizeof(*batch));unsigned i;
  CHECK(a && b && sa && sb && batch);
  for(i=0;i<PIXELS;i++) a[i]=(uint16_t)random_word();
  memcpy(b,a,PIXELS*2);memset(sb,0xcd,PIXELS*2);
  for(i=0;i<2000;i++) {
    struct rsx_batch_item item;memset(&item,0,sizeof(item));
    item.primitive=primitive(random_word()%60,random_word()%60,random_word()%4);
    item.primitive.bounds.w=1+random_word()%5;item.primitive.bounds.h=1+random_word()%5;
    item.primitive.tpage=((i&7)?8|16:0)|((random_word()%4)<<5);
    item.primitive.clut=(i&15)?400*64:511*64+63;
    item.primitive.raw=random_word()&1;item.primitive.v[0].u=random_word()%256;item.primitive.v[0].v=random_word()%256;
    item.primitive.v[0].color=random_word();item.regs[6]=random_word()%4;
    memcpy(sa,a,PIXELS*2);render(a,sa,&item);
    if(!rsx_batch_can_add(batch,&item.primitive,item.regs)) execute_batch(b,sb,batch);
    append(batch,item.primitive,item.regs);
    if(i%37==0) {execute_batch(b,sb,batch);CHECK(!memcmp(a,b,PIXELS*2));}
  }
  execute_batch(b,sb,batch);CHECK(!memcmp(a,b,PIXELS*2));
  free(batch);free(sb);free(sa);free(b);free(a);puts("2000 ordered RGB5/STP/mask operations match sequential rendering");
}
int main(void) {dependencies();equivalence();return 0;}

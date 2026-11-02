/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../journal.h"
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
#define PIXELS (1024 * 512)
#define RECORDS 512
static unsigned random_state=0x18253749;
static unsigned random_word(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
static void slow_tiles(uint32_t *bits,struct rsx_rect r,int clear) {
  int x,y,x0=r.x/4,y0=r.y/4,x1=(r.x+r.w+3)/4,y1=(r.y+r.h+3)/4;
  if(r.w<=0 || r.h<=0) return;
  if(clear) { x0=(r.x+3)/4;y0=(r.y+3)/4;x1=(r.x+r.w)/4;y1=(r.y+r.h)/4; }
  for(y=y0;y<y1;y++) for(x=x0;x<x1;x++) {
    unsigned bit=(y&127)*256+(x&255);
    if(clear) bits[bit/32]&=~(1u<<(bit%32));else bits[bit/32]|=1u<<(bit%32);
  }
}
static void boundaries(void) {
  uint32_t *bits=calloc(RSX_JOURNAL_WORDS,sizeof(*bits)),*slow=calloc(RSX_JOURNAL_WORDS,sizeof(*slow));
  unsigned i;
  struct rsx_rect all={0,0,1024,512},fill={0,12,512,216},small={1,1,2,2};
  CHECK(bits && slow);CHECK(sizeof(struct rsx_journal_record)==80);CHECK(RSX_JOURNAL_WORDS*sizeof(*bits)==4096);
  rsx_journal_tiles(bits,all,0);for(i=0;i<RSX_JOURNAL_WORDS;i++)CHECK(bits[i]==UINT32_MAX);
  rsx_journal_tiles(bits,fill,1);CHECK(!rsx_journal_intersects(bits,fill));
  CHECK(rsx_journal_intersects(bits,(struct rsx_rect){0,8,4,4}));CHECK(rsx_journal_intersects(bits,(struct rsx_rect){0,228,4,4}));
  rsx_journal_tiles(bits,small,1);CHECK(rsx_journal_intersects(bits,small));
  rsx_journal_tiles(bits,all,1);for(i=0;i<RSX_JOURNAL_WORDS;i++)CHECK(!bits[i]);
  rsx_journal_texture_tiles(bits,(struct rsx_rect){1008,511,256,1});
  CHECK(rsx_journal_intersects(bits,(struct rsx_rect){1008,511,16,1}));CHECK(rsx_journal_intersects(bits,(struct rsx_rect){16,0,4,1}));
  CHECK(!rsx_journal_intersects(bits,(struct rsx_rect){16,511,4,1}));
  memset(bits,0,RSX_JOURNAL_WORDS*sizeof(*bits));
  rsx_journal_texture_tiles(bits,(struct rsx_rect){960,256,256,256});
  CHECK(rsx_journal_intersects(bits,(struct rsx_rect){32,0,4,1}));CHECK(!rsx_journal_intersects(bits,(struct rsx_rect){32,252,4,4}));
  for(i=0;i<2000;i++) {
    struct rsx_rect r={(int)(random_word()%1024),(int)(random_word()%512),(int)(random_word()%1025),(int)(random_word()%513)};
    int clear=random_word()&1;
    if(!(i%13)) {memset(bits,0,RSX_JOURNAL_WORDS*sizeof(*bits));memset(slow,0,RSX_JOURNAL_WORDS*sizeof(*slow));}
    rsx_journal_tiles(bits,r,clear);slow_tiles(slow,r,clear);CHECK(!memcmp(bits,slow,RSX_JOURNAL_WORDS*sizeof(*bits)));
  }
  free(slow);free(bits);puts("journal word spans, partial cells and VRAM wrapping passed");
}
static void pixel_rect(unsigned char *needed,struct rsx_rect r,int value) {
  int x,y;
  for(y=0;y<r.h;y++)for(x=0;x<r.w;x++)needed[((r.y+y)&511)*1024+((r.x+x)&1023)]=(unsigned char)value;
}
static int pixel_hit(const unsigned char *needed,struct rsx_rect r) {
  int x,y;
  for(y=0;y<r.h;y++)for(x=0;x<r.w;x++)if(needed[((r.y+y)&511)*1024+((r.x+x)&1023)])return 1;
  return 0;
}
static void pixel_texture(unsigned char *needed,int x,int y,int w,int h) {
  int xx,yy;
  for(yy=0;yy<h;yy++)for(xx=0;xx<w;xx++)needed[((y+yy)*1024+x+xx)&(PIXELS-1)]=1;
}
static void pixel_select(const struct rsx_journal_record *records,unsigned count,const uint32_t *data,
                         struct rsx_rect wanted,unsigned char *keep) {
  unsigned char *needed=calloc(PIXELS,1);unsigned i=count;
  CHECK(needed);pixel_rect(needed,wanted,1);
  while(i--) {
    const struct rsx_journal_record *r=&records[i];
    keep[i]=(unsigned char)pixel_hit(needed,r->rect);if(!keep[i])continue;
    if(r->replace)pixel_rect(needed,r->rect,0);
    if(r->type==0 && r->mode) {
      pixel_texture(needed,(r->tpage&15)*64,((r->tpage>>4)&1)*256,64u<<(r->mode-1),256);
      if(r->mode<3)pixel_texture(needed,(r->clut&63)*16,(r->clut>>6)&511,r->mode==1?16:256,1);
    } else if(r->type==2) {
      const uint32_t *p=(const uint32_t *)((const unsigned char *)data+r->offset);uint32_t src=rsx_word(p+1);
      pixel_rect(needed,(struct rsx_rect){src&1023,(src>>16)&511,r->rect.w,r->rect.h},1);
    }
  }
  free(needed);
}
static unsigned selected(struct rsx_journal_record *r,unsigned count,const uint32_t *data,uint32_t *bits) {
  unsigned i,n=0;struct rsx_rect all={0,0,1024,512};
  rsx_journal_select(r,count,(const unsigned char *)data,all,bits);for(i=0;i<count;i++)n+=!!r[i].keep;return n;
}
static void retention(void) {
  struct rsx_journal_record *r=calloc(80,sizeof(*r));uint32_t *bits=calloc(RSX_JOURNAL_WORDS,sizeof(*bits));unsigned frame,i;
  CHECK(r && bits);
  for(frame=0;frame<20;frame++) {
    unsigned n=frame*4;int x=(frame&1)*512;
    r[n].rect=(struct rsx_rect){x,12,512,216};r[n].replace=1;
    r[n+1].rect=(struct rsx_rect){x+40,12,8,8};r[n+2].rect=(struct rsx_rect){x+128,220,20,8};r[n+3].rect=(struct rsx_rect){x+200,64,20,8};
  }
  CHECK(selected(r,80,NULL,bits)==8);for(i=0;i<80;i++)CHECK(r[i].keep==(i>=72));
  /* An incomplete four-pixel cell remains conservative. */
  memset(r,0,80*sizeof(*r));r[0].rect=(struct rsx_rect){0,0,4,4};r[0].replace=1;
  r[1].rect=(struct rsx_rect){1,1,2,2};r[1].replace=1;CHECK(selected(r,2,NULL,bits)==2);
  free(bits);free(r);puts("unaligned y12..228 clears retain only two current framebuffers");
}
static void dependencies(void) {
  struct rsx_journal_record r[4];uint32_t data[16]={0},*bits=calloc(RSX_JOURNAL_WORDS,sizeof(*bits));struct rsx_rect wanted={64,64,8,1};
  CHECK(bits);memset(r,0,sizeof(r));
  r[0].rect=(struct rsx_rect){1020,511,4,1};r[0].replace=1;
  r[1].rect=(struct rsx_rect){0,511,4,1};r[1].replace=1;
  r[2].rect=(struct rsx_rect){0,0,4,1};r[2].replace=1;
  r[3].rect=wanted;r[3].type=2;r[3].replace=1;data[1]=1020|(511u<<16);
  rsx_journal_select(r,4,(const unsigned char *)data,wanted,bits);CHECK(r[0].keep && r[1].keep && !r[2].keep && r[3].keep);
  memset(r,0,sizeof(r));r[0].rect=(struct rsx_rect){32,0,4,1};r[0].replace=1;
  r[1].rect=(struct rsx_rect){32,252,4,4};r[1].replace=1;r[2].rect=wanted;r[2].mode=3;r[2].tpage=15|16;
  rsx_journal_select(r,3,(const unsigned char *)data,wanted,bits);CHECK(r[0].keep && !r[1].keep && r[2].keep);
  memset(r,0,sizeof(r));r[0].rect=(struct rsx_rect){16,0,4,1};r[0].replace=1;
  r[1].rect=(struct rsx_rect){16,508,4,4};r[1].replace=1;r[2].rect=wanted;r[2].mode=2;r[2].tpage=13|16;r[2].clut=511*64+63;
  rsx_journal_select(r,3,(const unsigned char *)data,wanted,bits);CHECK(r[0].keep && !r[1].keep && r[2].keep);
  free(bits);puts("texture, palette linear carry and copy-axis wrapping dependencies passed");
}
struct operation {unsigned color,u,v,mask;};
static unsigned sample_word(const uint16_t *vram,unsigned x,unsigned y) {return vram[(y*1024+x)&(PIXELS-1)];}
static unsigned blend(unsigned a,unsigned b,unsigned mode) {
  if(mode==0)return a/2+b/2;
  if(mode==1)return a+b>31?31:a+b;
  if(mode==2)return b>a?b-a:0;
  return b+a/4>31?31:b+a/4;
}
static void render(uint16_t *vram,const struct rsx_journal_record *r,const struct operation *op,const uint32_t *data) {
  int x,y;
  if(r->type==2) {
    const uint32_t *p=(const uint32_t *)((const unsigned char *)data+r->offset);unsigned src=rsx_word(p+1);
    uint16_t pixels[256];CHECK(r->rect.w*r->rect.h<=256);
    for(y=0;y<r->rect.h;y++)for(x=0;x<r->rect.w;x++)pixels[y*r->rect.w+x]=vram[(((src>>16)+y)&511)*1024+((src+x)&1023)];
    for(y=0;y<r->rect.h;y++)for(x=0;x<r->rect.w;x++)vram[((r->rect.y+y)&511)*1024+((r->rect.x+x)&1023)]=pixels[y*r->rect.w+x];
    return;
  }
  for(y=0;y<r->rect.h;y++)for(x=0;x<r->rect.w;x++) {
    unsigned at=((r->rect.y+y)&511)*1024+((r->rect.x+x)&1023),old=vram[at],color=op->color,alpha=0;
    if(r->type==1 || r->replace) {vram[at]=(uint16_t)color;continue;}
    if(op->mask&2 && old&0x8000)continue;
    if(r->mode) {
      unsigned u=(op->u+x)&255,v=(op->v+y)&255,px=(r->tpage&15)*64,py=((r->tpage>>4)&1)*256;
      if(r->mode==3)color=sample_word(vram,px+u,py+v);
      else {
        unsigned word=sample_word(vram,px+u/(r->mode==1?4:2),py+v),index;
        index=r->mode==1?(word>>((u&3)*4))&15:(word>>((u&1)*8))&255;
        color=sample_word(vram,(r->clut&63)*16+index,(r->clut>>6)&511);
      }
      if(!color)continue;
      alpha=color&0x8000;
    }
    if(!r->mode || alpha) {
      unsigned red=blend(color&31,old&31,(r->tpage>>5)&3),green=blend((color>>5)&31,(old>>5)&31,(r->tpage>>5)&3),blue=blend((color>>10)&31,(old>>10)&31,(r->tpage>>5)&3);
      color=red|(green<<5)|(blue<<10);
    }
    vram[at]=(uint16_t)((color&0x7fff)|alpha|((op->mask&1)?0x8000:0));
  }
}
static void equivalence(void) {
  struct rsx_journal_record *records=calloc(RECORDS,sizeof(*records));struct operation *ops=calloc(RECORDS,sizeof(*ops));
  uint32_t *data=calloc(RECORDS*4,sizeof(*data)),*bits=calloc(RSX_JOURNAL_WORDS,sizeof(*bits));
  uint16_t *base=malloc(PIXELS*2),*full=malloc(PIXELS*2),*pruned=malloc(PIXELS*2);unsigned char *exact=malloc(RECORDS);unsigned pass,i;
  CHECK(records && ops && data && bits && base && full && pruned && exact);
  for(pass=0;pass<8;pass++) {
    for(i=0;i<PIXELS;i++)base[i]=(uint16_t)random_word();
    memset(records,0,RECORDS*sizeof(*records));
    for(i=0;i<RECORDS;i++) {
      struct rsx_journal_record *r=&records[i];struct operation *op=&ops[i];unsigned kind=random_word()%7;
      r->rect=(struct rsx_rect){(int)(random_word()%64),(int)(random_word()%64),1+(int)(random_word()%12),1+(int)(random_word()%12)};
      r->offset=i*16;r->bytes=16;op->color=random_word()&65535;op->u=random_word()&255;op->v=random_word()&255;op->mask=random_word()&3;
      if(kind<2) {r->type=1;r->replace=1;}
      else if(kind==2) {
        r->type=2;r->replace=1;data[i*4+1]=(random_word()%1024)|((random_word()%512)<<16);
      } else {
        r->mode=kind==3?0:kind-3;r->tpage=((random_word()&15)|16|((random_word()&3)<<5));r->clut=((random_word()%512)<<6)|(random_word()&63);
      }
      if(!(i%23)) {r->type=1;r->mode=0;r->replace=1;r->rect=(struct rsx_rect){(int)(random_word()%1024),(int)(random_word()%512),16,8};}
    }
    selected(records,RECORDS,data,bits);pixel_select(records,RECORDS,data,(struct rsx_rect){0,0,1024,512},exact);
    memcpy(full,base,PIXELS*2);memcpy(pruned,base,PIXELS*2);
    for(i=0;i<RECORDS;i++) {
      CHECK(!exact[i] || records[i].keep);render(full,&records[i],&ops[i],data);if(records[i].keep)render(pruned,&records[i],&ops[i],data);
    }
    CHECK(!memcmp(full,pruned,PIXELS*2));
  }
  free(exact);free(pruned);free(full);free(base);free(bits);free(data);free(ops);free(records);
  puts("4096 mixed RGB5/STP/mask/upload/copy operations match full replay and pixel dependency oracle");
}
static unsigned char *load_file(const char *path,size_t *size) {
  FILE *f=fopen(path,"rb");long length;unsigned char *data;
  CHECK(f);CHECK(!fseek(f,0,SEEK_END));length=ftell(f);CHECK(length>=0);rewind(f);*size=(size_t)length;data=malloc(*size?*size:1);CHECK(data);CHECK(fread(data,1,*size,f)==*size);CHECK(!fclose(f));return data;
}
static void captured(const char *record_path,const char *data_path) {
  size_t bytes,data_bytes;unsigned char *raw=load_file(record_path,&bytes),*data=load_file(data_path,&data_bytes),*exact;
  struct rsx_journal_record *records;uint32_t *bits=calloc(RSX_JOURNAL_WORDS,sizeof(*bits));unsigned i,j,count,n,total=0;
  CHECK(bytes%80==0);count=(unsigned)(bytes/80);records=malloc(bytes);exact=malloc(count);CHECK(records && exact && bits);
  for(i=0;i<count;i++)for(j=0;j<20;j++) {unsigned at=i*80+j*4;uint32_t value=((uint32_t)raw[at]<<24)|((uint32_t)raw[at+1]<<16)|((uint32_t)raw[at+2]<<8)|raw[at+3];memcpy((unsigned char *)&records[i]+j*4,&value,4);}
  for(i=0;i<count;i++)CHECK(records[i].offset+records[i].bytes<=data_bytes);
  n=selected(records,count,(const uint32_t *)data,bits);pixel_select(records,count,(const uint32_t *)data,(struct rsx_rect){0,0,1024,512},exact);
  for(i=0;i<count;i++)CHECK(!exact[i] || records[i].keep);
  for(i=0;i<count;i++)total+=!!exact[i];
  printf("captured journal: %u/%u retained; pixel oracle %u\n",n,count,total);
  free(bits);free(exact);free(records);free(data);free(raw);
}
int main(int argc,char **argv) {
  boundaries();retention();dependencies();equivalence();
  if(argc==3)captured(argv[1],argv[2]);else CHECK(argc==1);
  return 0;
}

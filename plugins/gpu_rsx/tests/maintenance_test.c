/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef environ
#undef environ
#endif
#define RSX_DIAGNOSTICS
#include "../renderer.c"

#define CHECK(v) do { if(!(v)) { fprintf(stderr,"line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
static retro_time_t ticks;
static unsigned draws;
static retro_time_t clock_usec(void) { return ticks+=100; }
void SysPrintf(const char *fmt,...) { (void)fmt; }
void rsx_hw_suspend_output(void) { }
void rsx_hw_destroy(void) { }
void rsx_hw_flush(void) { }
void rsx_hw_set_dither(int mode) { (void)mode; }
int vout_init(void) { return 0; }
int vout_finish(void) { return 0; }
int vout_update(struct psx_gpu *gpu_,int x,int y) { (void)gpu_; (void)x; (void)y; return 0; }
void vout_blank(struct psx_gpu *gpu_) { (void)gpu_; }
void vout_set_config(const struct rearmed_cbs *cbs) { (void)cbs; }
int rsx_hw_create(const struct retro_hw_render_interface_rsx *iface) { (void)iface; return 1; }
int rsx_hw_draw(const struct rsx_primitive *p,const uint32_t *regs) { (void)p; (void)regs; draws++; return 1; }
int rsx_hw_upload(int x,int y,int w,int h,const uint16_t *vram) { (void)x; (void)y; (void)w; (void)h; (void)vram; return 1; }
int rsx_hw_copy(const uint32_t *p,const uint32_t *regs) { (void)p; (void)regs; return 1; }
int rsx_hw_output(int a,int b,int c,int d,int e,int f,int g,int h,int i) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; return 1; }
int rsx_hw_present(retro_video_refresh_t cb) { (void)cb; return 1; }

static void setup(void) {
  renderer.base=calloc(1,VRAM_BYTES);
  renderer.scratch=calloc(1,VRAM_BYTES);
  renderer.upload=calloc(1,VRAM_BYTES);
  renderer.data=malloc(JOURNAL_BYTES);
  renderer.records=calloc(JOURNAL_RECORDS,sizeof(*renderer.records));
  gpu.vram=calloc(1,VRAM_BYTES*2);
  CHECK(renderer.base && renderer.scratch && renderer.upload && renderer.data && renderer.records && gpu.vram);
  peops_renderer_init();
  renderer.active=renderer.initialized=1;
  renderer.get_time_usec=clock_usec;
  gpu.ex_regs[3]=0;
  gpu.ex_regs[4]=1023|(511u<<10);
  gpu.ex_regs[5]=0;
  gpu.state.frame_count=NULL;
}
static void teardown(void) {
  renderer.count=renderer.used=0;
  context_destroy();
  free(gpu.vram); gpu.vram=NULL; gpu.state.frame_count=NULL;
}
static void append(uint32_t *words,unsigned count) {
  struct rsx_primitive p;
  struct rsx_journal_record *r;
  CHECK(rsx_decode(&p,words,count,gpu.ex_regs));
  r=record(count*4); CHECK(r);
  r->type=0; r->rect=p.bounds; r->replace=p.replace;
  r->mode=p.mode; r->tpage=p.tpage; r->clut=p.clut;
  memcpy(renderer.data+r->offset,words,count*4);
}
static void compare(const uint16_t *expected) {
  struct rsx_rect all={0,0,1024,512};
  replay(all); CHECK(!memcmp(expected,renderer.scratch,VRAM_BYTES));
}
static void mixed(void) {
  uint16_t *expected=malloc(VRAM_BYTES);
  unsigned i,remaining;
  uint32_t packet[4];
  CHECK(expected); setup();
  for(i=0;i<1536;i++) {
    packet[0]=HTOLE32(0x60000000|((i*32423)&0xffffff));
    packet[1]=HTOLE32((i*19%200)|((i*13%200)<<16));
    packet[2]=HTOLE32(16|(12u<<16));
    append(packet,3);
    if(!(i%17)) {
      struct rsx_journal_record *r=record(32);
      uint16_t *pixels;
      CHECK(r); r->type=1; r->replace=1;
      r->rect=(struct rsx_rect){1020,511,8,2};
      pixels=(uint16_t *)(renderer.data+r->offset);
      for(unsigned j=0;j<16;j++) pixels[j]=HTOLE16(i+j);
    }
    if(!(i%23)) {
      struct rsx_journal_record *r=record(16);
      CHECK(r); r->type=2; r->replace=1;
      r->rect=(struct rsx_rect){300,300,16,16};
      packet[0]=HTOLE32(0x80000000); packet[1]=0;
      packet[2]=HTOLE32(300|(300u<<16)); packet[3]=HTOLE32(16|(16u<<16));
      memcpy(renderer.data+r->offset,packet,sizeof(packet));
    }
  }
  replay((struct rsx_rect){0,0,1024,512}); memcpy(expected,renderer.scratch,VRAM_BYTES);
  advance_base(1,0); compare(expected);
  advance_base(63,0); compare(expected);
  compact(); compare(expected);
  remaining=renderer.count;
  ticks=0; advance_base(remaining,500);
  CHECK(renderer.count==remaining-4); compare(expected);
  while(renderer.count) { advance_base(37,0); compare(expected); }
  CHECK(!renderer.used && !memcmp(expected,renderer.base,VRAM_BYTES));
  teardown(); free(expected);
  puts("incremental uploads, copies, wrapping, pruning and deadlines match full software replay");
}
static void capacity(void) {
  uint32_t packet[4]={HTOLE32(0x660000ff),HTOLE32(16|(16u<<16)),0,HTOLE32(4|(4u<<16))};
  uint16_t *expected=malloc(VRAM_BYTES);
  unsigned count;
  uint64_t checkpoints,compactions;
  CHECK(expected); setup();
  {
    uint32_t fill[3]={HTOLE32(0x600000ff),0,HTOLE32(4|(4u<<16))};
    append(fill,3);
  }
  for(unsigned i=1;i<JOURNAL_RECORDS;i++) append(packet,4);
  CHECK(renderer.count==JOURNAL_RECORDS);
  replay((struct rsx_rect){0,0,1024,512}); memcpy(expected,renderer.scratch,VRAM_BYTES);
  checkpoints=gpu_rsx_diagnostics[1]; compactions=gpu_rsx_diagnostics[2];
  append(packet,4);
  CHECK(renderer.count==JOURNAL_RECORDS-JOURNAL_SLICE+1);
  CHECK(gpu_rsx_diagnostics[1]==checkpoints && gpu_rsx_diagnostics[2]==compactions+1);
  for(unsigned i=0;i<JOURNAL_SLICE*3;i++) append(packet,4);
  CHECK(gpu_rsx_diagnostics[1]==checkpoints && gpu_rsx_diagnostics[2]==compactions+1);
  compare(expected);
  count=renderer.count;
  ticks=100000; renderer.frame_start=1;
  maintain_journal(); CHECK(renderer.count==count);
  gpu_rsx_begin_frame(); maintain_journal();
  CHECK(renderer.count<count && renderer.count>=count-60);
  compare(expected);
  {
    unsigned fields=UINT32_MAX;
    gpu.state.frame_count=&fields;
    gpu_rsx_begin_frame(); fields+=2; ticks+=13000;
    count=renderer.count; maintain_journal(); CHECK(renderer.count<count);
    compare(expected);
    gpu.state.frame_count=NULL;
  }
  renderer.get_time_usec=NULL;
  count=renderer.count; maintain_journal(); CHECK(renderer.count==count-JOURNAL_SLICE);
  compare(expected);
  for(unsigned i=0;i<JOURNAL_RECORDS*2;i++) {
    if(!(i%64)) {
      uint32_t fill[3]={HTOLE32(0x600000ff),0,HTOLE32(4|(4u<<16))};
      append(fill,3);
    } else append(packet,4);
  }
  renderer.prune_after=renderer.serial; compact(); compare(expected);
  CHECK(renderer.count<=JOURNAL_RECORDS && renderer.used<=JOURNAL_BYTES);
  teardown(); free(expected);
  puts("full journals retire a bounded prefix and frame maintenance respects the CPU budget");
}
static unsigned char *load(const char *path,size_t *bytes) {
  FILE *file=fopen(path,"rb"); long size; unsigned char *data;
  CHECK(file && !fseek(file,0,SEEK_END)); size=ftell(file); CHECK(size>=0); rewind(file);
  *bytes=(size_t)size; data=malloc(*bytes?*bytes:1); CHECK(data);
  CHECK(fread(data,1,*bytes,file)==*bytes); CHECK(!fclose(file)); return data;
}
static void byte_capacity(void) {
  uint16_t *expected=malloc(VRAM_BYTES);
  struct rsx_journal_record *r;
  unsigned i,j;
  CHECK(expected); setup();
  for(i=0;i<JOURNAL_BYTES/VRAM_BYTES;i++) {
    r=record(VRAM_BYTES); CHECK(r);
    r->type=1; r->rect=(struct rsx_rect){0,0,1024,512};
    for(j=0;j<VRAM_BYTES/2;j++) ((uint16_t *)(renderer.data+r->offset))[j]=HTOLE16(i+1);
  }
  CHECK(renderer.used==JOURNAL_BYTES);
  for(j=0;j<VRAM_BYTES/2;j++) expected[j]=HTOLE16(i);
  r=record(2); CHECK(r && renderer.count==1 && renderer.used==4);
  r->type=1; r->rect=(struct rsx_rect){5,5,1,1};
  *(uint16_t *)(renderer.data+r->offset)=HTOLE16(0x1234); expected[5*1024+5]=HTOLE16(0x1234);
  r=record(6); CHECK(r && renderer.used==12);
  r->type=1; r->rect=(struct rsx_rect){1023,511,3,1};
  for(j=0;j<3;j++) {
    ((uint16_t *)(renderer.data+r->offset))[j]=HTOLE16(j+40);
    expected[511*1024+((1023+j)&1023)]=HTOLE16(j+40);
  }
  compare(expected); advance_base(1,0);
  CHECK(renderer.count==1 && renderer.used==8 && journal_record(0)->offset==4);
  compare(expected); CHECK(!record(JOURNAL_BYTES+1)); compare(expected);
  advance_base(1,0); CHECK(!memcmp(expected,renderer.base,VRAM_BYTES));
  teardown(); free(expected);
  puts("byte-capacity pressure, halfword alignment and oversized records preserve VRAM and bounds");
}
static void wrapping(void) {
  uint16_t *expected=calloc(1,VRAM_BYTES);
  unsigned i,j;
  CHECK(expected); setup();
  for(i=0;i<6000;i++) {
    struct rsx_journal_record *r=record(65538);
    unsigned x=(i*19)&1023,y=(i/1024)&511;
    CHECK(r && r->offset+r->bytes<=JOURNAL_BYTES && renderer.used<=JOURNAL_BYTES);
    memset(renderer.data+r->offset,(unsigned char)i,r->bytes);
    r->type=1; r->rect=(struct rsx_rect){x,y,3,1};
    for(j=0;j<3;j++) {
      ((uint16_t *)(renderer.data+r->offset))[j]=HTOLE16(i+j);
      expected[y*1024+((x+j)&1023)]=HTOLE16(i+j);
    }
    if(renderer.count>11) advance_base(3,0);
    if(!(i%31)) compare(expected);
  }
  while(renderer.count) advance_base(5,0);
  CHECK(!memcmp(expected,renderer.base,VRAM_BYTES));
  CHECK(!renderer.used && !renderer.first && !renderer.data_head && !renderer.data_tail);
  teardown(); free(expected);
  puts("6000 aligned and wrapped ring uploads match canonical VRAM without moving live records");
}
static void captured(const char *record_path,const char *data_path,const char *base_path) {
  unsigned char *raw,*data,*base;
  size_t bytes,data_bytes,base_bytes;
  uint16_t *expected=malloc(VRAM_BYTES);
  unsigned i,j;
  raw=load(record_path,&bytes); data=load(data_path,&data_bytes); base=load(base_path,&base_bytes);
  CHECK(expected && bytes%80==0 && bytes/80<=JOURNAL_RECORDS && data_bytes<=JOURNAL_BYTES && base_bytes==VRAM_BYTES);
  setup(); renderer.count=(unsigned)(bytes/80); renderer.used=(unsigned)data_bytes;
  renderer.data_tail=renderer.used%JOURNAL_BYTES;
  memcpy(renderer.data,data,data_bytes); memcpy(renderer.base,base,VRAM_BYTES);
  for(i=0;i<renderer.count;i++) for(j=0;j<20;j++) {
    unsigned at=i*80+j*4;
    uint32_t word=((uint32_t)raw[at]<<24)|((uint32_t)raw[at+1]<<16)|((uint32_t)raw[at+2]<<8)|raw[at+3];
    memcpy((unsigned char *)&renderer.records[i]+j*4,&word,4);
  }
  for(i=0;i<renderer.count;i++) CHECK(renderer.records[i].offset+renderer.records[i].bytes<=renderer.used);
  replay((struct rsx_rect){0,0,1024,512}); memcpy(expected,renderer.scratch,VRAM_BYTES);
  advance_base(13,0); compare(expected);
  advance_base(257,0); compare(expected);
  compact(); compare(expected);
  while(renderer.count) advance_base(113,0);
  CHECK(!memcmp(expected,renderer.base,VRAM_BYTES));
  teardown(); free(expected); free(base); free(data); free(raw);
  puts("captured Warped polygons produce identical VRAM after incremental prefix retirement");
}
int main(int argc,char **argv) {
  mixed(); capacity(); byte_capacity(); wrapping();
  if(argc==4) captured(argv[1],argv[2],argv[3]); else CHECK(argc==1);
  return 0;
}

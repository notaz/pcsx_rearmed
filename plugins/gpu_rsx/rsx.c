/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <rsx/rsx.h>
#include <rsx/mm.h>
#include <rsx/nv40.h>
#include "../gpulib/gpu.h"
#include "rsx.h"
#include "batch.h"
#include "blend.h"
#include "generated/shaders.inc"
extern s32 rsxContextCallback(gcmContextData *, u32);
#define PITCH 4096
#define IMAGE_BYTES (PITCH * 512)
#define PAGE_BYTES (2 * 1024 * 1024)
#define MAX_IO_BLOCKS 7
#define BLOCKS (MAX_IO_BLOCKS + 16)
#define FENCE_LABEL 201
#define BARRIER_LABEL 200
struct stream_block {
  unsigned char *ptr;
  uint32_t offset, used, fence;
  unsigned location;
};
/* The frontend keeps this mapping across content changes. */
static struct {
  void *mapping;
  struct stream_block b[BLOCKS];
  unsigned current, io_blocks, block_count;
  uint32_t serial, barrier;
} stream;
static struct {
  const struct retro_hw_render_interface_rsx *iface;
  gcmContextData *ctx;
  void *images[4], *fragment[RSX_FP_COUNT];
  uint32_t offsets[4], fp_offsets[RSX_FP_COUNT], dirty[64];
  rsxVertexProgram *vp;
  void *vp_code;
  uint32_t attributes[3], constants[4];
  unsigned output, width, height;
  int ready, output_dirty, state_dirty, bound_target, bound_fragment, vertex_bound, texture_bound, constants_valid;
  uint32_t vertex_base, batch_first, batch_count, batch_type;
  int scissor[4];
  float params[16];
  struct rsx_primitive_batch primitives;
  int failed, blend;
} hw;
static const unsigned char *const fragments[] = {
  rsx_fragment0, rsx_fragment1, rsx_fragment2, rsx_fragment3,
  rsx_copy, rsx_display24,
  rsx_opaque0_0, rsx_opaque0_1, rsx_opaque1_0, rsx_opaque1_1,
  rsx_opaque2_0, rsx_opaque2_1, rsx_opaque3_0, rsx_opaque3_1, rsx_upload,
  rsx_native0, rsx_native1, rsx_native2, rsx_native3, rsx_display15
};
#ifdef RSX_DIAGNOSTICS
uint64_t gpu_rsx_batch_diagnostics[8];
#define BATCH_COUNT(i,n) (gpu_rsx_batch_diagnostics[i]+=(n))
#else
#define BATCH_COUNT(i,n) ((void)0)
#endif
static int flush_primitives(void);
static void flush_draws(void) {
  if(!hw.batch_count) return;
  __sync_synchronize();
  rsxInvalidateVertexCache(hw.ctx);
  rsxDrawVertexArray(hw.ctx,hw.batch_type,hw.batch_first,hw.batch_count);
  hw.batch_count=0;
}
static int dither_mode;
void rsx_hw_set_dither(int mode) {
  if(dither_mode!=mode) { flush_primitives(); dither_mode=mode; }
}
static void invalidate_state(void) {
  hw.bound_target=hw.bound_fragment=hw.blend=-1;
  hw.vertex_bound=hw.texture_bound=hw.constants_valid=0;
  hw.vertex_base=UINT32_MAX;
  hw.scissor[0]=hw.scissor[1]=hw.scissor[2]=hw.scissor[3]=-1;
}
static uint32_t offset_of(void *p) {
  uint32_t result=0;
  gcmAddressToOffset(p,&result);
  return result;
}
static void barrier(void) {
  flush_draws();
  ++stream.barrier;
  rsxSetWriteBackendLabel(hw.ctx,BARRIER_LABEL,stream.barrier);
  rsxSetWaitLabel(hw.ctx,BARRIER_LABEL,stream.barrier);
}
static void seal_block(void) {
  struct stream_block *b=&stream.b[stream.current];
  if (!b->ptr || !b->used) return;
  flush_draws();
  b->fence=++stream.serial;
  rsxSetWaitForIdle(hw.ctx);
  rsxSetWriteBackendLabel(hw.ctx,FENCE_LABEL,b->fence);
  __sync_synchronize();
  rsxFlushBuffer(hw.ctx);
}
static void *allocate_stream(unsigned size,uint32_t *offset,unsigned *location,unsigned alignment) {
  struct stream_block *b=&stream.b[stream.current];
  unsigned at=(b->used+alignment-1)&~(alignment-1), i;
  uint32_t done;
  if (size>PAGE_BYTES) return NULL;
  if (at+size>PAGE_BYTES) {
    seal_block();
    done=*(volatile uint32_t *)gcmGetLabelAddress(FENCE_LABEL);
    b=NULL;
    /* Reuse completed blocks before growing the local overflow pool. */
    for (i=1;i<=stream.block_count;i++) {
      unsigned n=(stream.current+i)%stream.block_count;
      struct stream_block *candidate=&stream.b[n];
      if (candidate->ptr && (!candidate->used || (int32_t)(done-candidate->fence)>=0)) {
        b=candidate; stream.current=n; b->used=0; break;
      }
    }
    if(!b) for (i=stream.io_blocks;i<stream.block_count;i++) {
      struct stream_block *candidate=&stream.b[i];
      if(candidate->ptr) continue;
      candidate->ptr=rsxMemalign(128,PAGE_BYTES);
      if(!candidate->ptr) continue;
      RSX_COUNT(12,1);
      candidate->offset=offset_of(candidate->ptr);
      candidate->location=GCM_LOCATION_RSX;
      b=candidate; stream.current=i; break;
    }
    if (!b) return NULL;
    at=0;
  }
  *offset=b->offset+at; *location=b->location;
  b->used=at+size;
  return b->ptr+at;
}
static int reserve(unsigned count) {
  return hw.ctx->current+count<=hw.ctx->end || rsxContextCallback(hw.ctx,count)==0;
}
static void target(unsigned);
static void texture(unsigned);
static void scissor(int,int,int,int);
static int vertices(const struct rsx_vertex *,unsigned,unsigned,const float *,unsigned);
static void quad(struct rsx_vertex *,int,int,int,int,float,float,float,float);
static void mark_dirty(int x,int y,int w,int h) {
  int xx,yy;
  if (w<=0 || h<=0) return;
  for (yy=0;yy<h;yy+=16) {
    int ty=((y+yy)&511)/16;
    for (xx=0;xx<w;xx+=16) {
      int tx=((x+xx)&1023)/16, bit=ty*64+tx;
      hw.dirty[bit/32]|=1u<<(bit%32);
      /* Include the final partial tile of an unaligned rectangle. */
      if (((x+xx)&15) && xx+16>=w) {
        bit=ty*64+((tx+1)&63); hw.dirty[bit/32]|=1u<<(bit%32);
      }
    }
    if ((y&15) && yy+16>=h) {
      int ty2=(ty+1)&31;
      for (xx=0;xx<w;xx++) {
        int bit=ty2*64+(((x+xx)&1023)/16);
        hw.dirty[bit/32]|=1u<<(bit%32);
      }
    }
  }
}
static int refresh_region(int x,int y,int w,int h,const uint32_t *wanted) {
  int tx,ty,first=1;
  unsigned char rows[32];
  int x0=x/16,x1=(x+w+15)/16,y0=y/16,y1=(y+h+15)/16;
  if (w<=0 || h<=0) return 1;
  /* Whole tiles, including wrapped texture pages and palettes. */
  memset(rows,0,sizeof(rows));
  for (ty=y0;ty<y1;ty++) {
    int row=ty&31;
    if (rows[row]) continue;
    rows[row]=1;
    if(wanted && !((wanted[row*2]&hw.dirty[row*2])|(wanted[row*2+1]&hw.dirty[row*2+1]))) continue;
    for (tx=x0;tx<x1;) {
      int bit=row*64+(tx&63),start=tx,n=0;
      if (!(hw.dirty[bit/32]&(1u<<(bit%32))) || (wanted && !(wanted[bit/32]&(1u<<(bit%32))))) { tx++; continue; }
      do {
        n++; tx++;
        if(tx>=x1 || !(tx&63)) break;
        bit=row*64+(tx&63);
      } while((hw.dirty[bit/32]&(1u<<(bit%32))) && (!wanted || (wanted[bit/32]&(1u<<(bit%32)))));
      {
        struct rsx_vertex v[6];
        float params[16]={0};
        int px=(start&63)*16,py=row*16,i;
        RSX_COUNT(9,n);
        if(first) {
          BATCH_COUNT(6,1);
          barrier();
          rsxInvalidateTextureCache(hw.ctx,GCM_INVALIDATE_TEXTURE);
          first=0;
        }
        target(1); scissor(0,0,1024,512); texture(0);
        quad(v,px,py,n*16,16,px,py,n*16,16);
        if(!vertices(v,6,4,params,0)) return 0;
        for(i=0;i<n;i++) {
          bit=row*64+((start+i)&63);
          hw.dirty[bit/32]&=~(1u<<(bit%32));
        }
      }
    }
  }

  if (!first) {
    barrier();
    rsxSetWaitForIdle(hw.ctx);
    rsxInvalidateTextureCache(hw.ctx,GCM_INVALIDATE_TEXTURE);
  }
  return 1;
}
static int refresh(int x,int y,int w,int h) {
  return refresh_region(x,y,w,h,NULL);
}
static int refresh_texture(int x,int y,int w,int h) {
  int first=1024-x;
  if(w<=first) return refresh(x,y,w,h);
  return refresh(x,y,first,h) && refresh(0,y+1,w-first,h);
}
static void target(unsigned image) {
  gcmSurface s;
  float scale[4]={512,256,0.5f,0}, offset[4]={512,256,0.5f,0};
  unsigned i;
  hw.state_dirty=1;
  if(hw.bound_target==(int)image) return;
  flush_draws();
  /* Every core target has the same format, pitch and dimensions. */
  if(hw.bound_target>=0 && reserve(2)) {
    hw.ctx->current[0]=(1u<<18)|NV40TCL_COLOR0_OFFSET;
    hw.ctx->current[1]=hw.offsets[image];
    hw.ctx->current+=2;
    hw.bound_target=image;
    return;
  }
  hw.bound_target=image;
  hw.scissor[0]=hw.scissor[1]=0; hw.scissor[2]=1024; hw.scissor[3]=512;
  memset(&s,0,sizeof(s));
  s.colorFormat=GCM_SURFACE_A8R8G8B8; s.colorTarget=GCM_SURFACE_TARGET_0;
  s.colorLocation[0]=GCM_LOCATION_RSX; s.colorOffset[0]=hw.offsets[image]; s.colorPitch[0]=PITCH;
  for(i=1;i<4;i++) { s.colorLocation[i]=GCM_LOCATION_RSX; s.colorPitch[i]=64; }
  s.depthFormat=GCM_SURFACE_ZETA_Z24S8; s.depthLocation=GCM_LOCATION_RSX; s.depthPitch=64;
  s.type=GCM_SURFACE_TYPE_LINEAR; s.antiAlias=GCM_SURFACE_CENTER_1;
  s.width=1024; s.height=512;
  rsxSetSurface(hw.ctx,&s);
  for(i=0;i<8;i++) rsxSetViewportClip(hw.ctx,i,1024,512);
  rsxSetViewport(hw.ctx,0,0,1024,512,0,1,scale,offset);
  rsxSetScissor(hw.ctx,0,0,1024,512);
  rsxSetDepthTestEnable(hw.ctx,GCM_FALSE); rsxSetDepthWriteEnable(hw.ctx,GCM_FALSE);
  rsxSetStencilTestEnable(hw.ctx,GCM_FALSE); rsxSetAlphaTestEnable(hw.ctx,GCM_FALSE);
  rsxSetBlendEnable(hw.ctx,GCM_FALSE); hw.blend=0; rsxSetCullFaceEnable(hw.ctx,GCM_FALSE);
  rsxSetLogicOpEnable(hw.ctx,GCM_FALSE); rsxSetDitherEnable(hw.ctx,GCM_FALSE);
  rsxSetColorMask(hw.ctx,GCM_COLOR_MASK_R|GCM_COLOR_MASK_G|GCM_COLOR_MASK_B|GCM_COLOR_MASK_A);
  rsxSetShadeModel(hw.ctx,GCM_SHADE_MODEL_SMOOTH);
  if(reserve(2)) {
    hw.ctx->current[0]=(1u<<18)|NV40TCL_LINE_WIDTH;
    hw.ctx->current[1]=8;
    hw.ctx->current+=2;
  }
  rsxSetFrontPolygonMode(hw.ctx,GCM_POLYGON_MODE_FILL);
  rsxSetBackPolygonMode(hw.ctx,GCM_POLYGON_MODE_FILL);
  rsxSetZMinMaxControl(hw.ctx,GCM_FALSE,GCM_TRUE,GCM_FALSE);
  rsxSetColorMaskMrt(hw.ctx,0);
  rsxSetUserClipPlaneControl(hw.ctx,0,0,0,0,0,0);
}
static void scissor(int x,int y,int w,int h) {
  if(hw.scissor[0]==x && hw.scissor[1]==y && hw.scissor[2]==w && hw.scissor[3]==h) return;
  flush_draws();
  hw.scissor[0]=x; hw.scissor[1]=y; hw.scissor[2]=w; hw.scissor[3]=h;
  rsxSetScissor(hw.ctx,x,y,w,h);
}
static void texture(unsigned image) {
  gcmTexture t;
  if(hw.texture_bound==(int)image+1) return;
  flush_draws(); hw.texture_bound=image+1;
  memset(&t,0,sizeof(t));
  t.format=GCM_TEXTURE_FORMAT_A8R8G8B8|GCM_TEXTURE_FORMAT_LIN;
  t.mipmap=1; t.dimension=GCM_TEXTURE_DIMS_2D;
  t.remap=(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_B_SHIFT)
    |(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_G_SHIFT)
    |(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_R_SHIFT)
    |(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_A_SHIFT)
    |(GCM_TEXTURE_REMAP_COLOR_B<<GCM_TEXTURE_REMAP_COLOR_B_SHIFT)
    |(GCM_TEXTURE_REMAP_COLOR_G<<GCM_TEXTURE_REMAP_COLOR_G_SHIFT)
    |(GCM_TEXTURE_REMAP_COLOR_R<<GCM_TEXTURE_REMAP_COLOR_R_SHIFT)
    |(GCM_TEXTURE_REMAP_COLOR_A<<GCM_TEXTURE_REMAP_COLOR_A_SHIFT);
  t.width=1024; t.height=512; t.depth=1; t.pitch=PITCH;
  t.location=GCM_LOCATION_RSX; t.offset=hw.offsets[image];
  rsxLoadTexture(hw.ctx,0,&t);
  rsxTextureControl(hw.ctx,0,GCM_TRUE,0,12<<8,GCM_TEXTURE_MAX_ANISO_1);
  rsxTextureFilter(hw.ctx,0,0,GCM_TEXTURE_NEAREST,GCM_TEXTURE_NEAREST,GCM_TEXTURE_CONVOLUTION_QUINCUNX);
  rsxTextureWrapMode(hw.ctx,0,GCM_TEXTURE_CLAMP_TO_EDGE,GCM_TEXTURE_CLAMP_TO_EDGE,
                    GCM_TEXTURE_CLAMP_TO_EDGE,0,GCM_TEXTURE_ZFUNC_LESS,0);
}
static int vertices(const struct rsx_vertex *v,unsigned count,unsigned fp,const float params[16],unsigned blend) {
  struct rsx_vertex *data;
  uint32_t offset;
  unsigned location,i,type=count==2?GCM_TYPE_LINES:GCM_TYPE_TRIANGLES;
  data=allocate_stream(count*sizeof(*data),&offset,&location,32);
  if (!data) return 0;
  memcpy(data,v,count*sizeof(*data));
  if(hw.blend!=(int)blend) {
    flush_draws();
    rsxSetBlendEnable(hw.ctx,blend?GCM_TRUE:GCM_FALSE);
    if(blend) {
      rsxSetBlendFunc(hw.ctx,GCM_ONE,blend==3?GCM_SRC_ALPHA:GCM_ONE,GCM_ONE,GCM_ZERO);
      rsxSetBlendEquation(hw.ctx,blend==2?GCM_FUNC_REVERSE_SUBTRACT:GCM_FUNC_ADD,GCM_FUNC_ADD);
    }
    hw.blend=blend;
  }
  for(i=0;i<count;i++) { data[i].x=(data[i].x+0.5f)*(2.0f/1024)-1; data[i].y=(data[i].y+0.5f)*(2.0f/512)-1; }
  if(hw.batch_count && (hw.batch_type!=type || hw.bound_fragment!=(int)fp ||
      hw.vertex_base!=stream.b[stream.current].offset ||
      offset!=hw.vertex_base+(hw.batch_first+hw.batch_count)*sizeof(*data) ||
      memcmp(hw.params,params,sizeof(hw.params)))) flush_draws();
  if(!hw.vertex_bound) {
    rsxLoadVertexProgram(hw.ctx,hw.vp,hw.vp_code);
    hw.vertex_bound=1;
  }
  for(i=0;i<4;i++) if(!hw.constants_valid || memcmp(hw.params+i*4,params+i*4,16)) {
    rsxLoadVertexProgramParameterBlock(hw.ctx,hw.constants[i],1,params+i*4);
    memcpy(hw.params+i*4,params+i*4,16);
  }
  hw.constants_valid=1;
  if(hw.bound_fragment!=(int)fp) {
    rsxLoadFragmentProgramLocation(hw.ctx,(rsxFragmentProgram *)fragments[fp],hw.fp_offsets[fp],GCM_LOCATION_RSX);
    hw.bound_fragment=fp;
  }
  if(hw.vertex_base!=stream.b[stream.current].offset) {
    hw.vertex_base=stream.b[stream.current].offset;
    rsxBindVertexArrayAttrib(hw.ctx,hw.attributes[0],0,hw.vertex_base,sizeof(*data),4,GCM_VERTEX_DATA_TYPE_F32,location);
    rsxBindVertexArrayAttrib(hw.ctx,hw.attributes[1],0,hw.vertex_base+offsetof(struct rsx_vertex,color),sizeof(*data),4,GCM_VERTEX_DATA_TYPE_U8,location);
    rsxBindVertexArrayAttrib(hw.ctx,hw.attributes[2],0,hw.vertex_base+offsetof(struct rsx_vertex,u),sizeof(*data),2,GCM_VERTEX_DATA_TYPE_F32,location);
  }
  if(!hw.batch_count) hw.batch_first=(offset-hw.vertex_base)/sizeof(*data);
  hw.batch_type=type; hw.batch_count+=count;
  return 1;
}
static void quad(struct rsx_vertex v[6],int x,int y,int w,int h,float u,float vv,float uw,float vh) {
  unsigned i;
  memset(v,0,sizeof(*v)*6);
  for(i=0;i<6;i++) { v[i].w=1; v[i].color=0xffffffff; }
  v[0].x=x; v[0].y=y; v[0].u=u; v[0].v=vv;
  v[1]=v[0]; v[1].x+=w; v[1].u+=uw;
  v[2]=v[0]; v[2].y+=h; v[2].v+=vh;
  v[3]=v[1]; v[4]=v[1]; v[4].y+=h; v[4].v+=vh; v[5]=v[2];
}
static int draw_primitive(const struct rsx_primitive *p,const uint32_t *regs,int prepared) {
  float params[16]={0};
  int px=(p->tpage&15)*64, py=((p->tpage>>4)&1)*256;
  int cx=(p->clut&63)*16, cy=(p->clut>>6)&511;
  int x=p->bounds.x,y=p->bounds.y,w=p->bounds.w,h=p->bounds.h;
  unsigned mw=regs[2]&31,mh=(regs[2]>>5)&31,fp=p->mode;
  int native=rsx_native_blend(p,regs);
  if (w<=0 || h<=0) return 1;
  if (p->fill) {
    uint32_t rgba=p->v[0].color;
    unsigned r=(rgba>>24)>>3,g=((rgba>>16)&255)>>3,b=((rgba>>8)&255)>>3;
    unsigned color=(r*8<<16)|(g*8<<8)|(b*8);
    int yy,xx;
    flush_draws(); target(0); rsxSetClearColor(hw.ctx,color);
    for(yy=0;yy<h;) {
      int y0=(y+yy)&511, nh=512-y0; if(nh>h-yy) nh=h-yy;
      for(xx=0;xx<w;) {
        int x0=(x+xx)&1023,nw=1024-x0; if(nw>w-xx) nw=w-xx;
        scissor(x0,y0,nw,nh);
        rsxClearSurface(hw.ctx,GCM_CLEAR_R|GCM_CLEAR_G|GCM_CLEAR_B|GCM_CLEAR_A);
        xx+=nw;
      }
      yy+=nh;
    }
    mark_dirty(x,y,w,h); return 1;
  }
  if (!p->count) return 1;
  if (!prepared && p->mode) {
    if(!refresh_texture(px,py,64u<<(p->mode-1),256)) return 0;
    if(p->mode<3 && !refresh_texture(cx,cy,p->mode==1?16:256,1)) return 0;
  }
  if (!prepared && !native && (p->semi || (regs[6]&2)) && !refresh(x,y,w,h)) return 0;
  params[0]=px; params[1]=py; params[2]=cx; params[3]=cy;
  params[4]=mw; params[5]=mh;
  params[6]=(((regs[2]>>10)&31)&mw)*8; params[7]=(((regs[2]>>15)&31)&mh)*8;
  params[8]=p->semi?((p->tpage>>5)&3)+1:0;
  params[9]=regs[6]&1; params[10]=(regs[6]>>1)&1;
  params[11]=p->dither && (dither_mode==2 || (dither_mode==1 && (regs[1]&0x200))); params[12]=p->raw;
  target(0);
  scissor(regs[3]&1023,(regs[3]>>10)&511,
          (regs[4]&1023)-(regs[3]&1023)+1,((regs[4]>>10)&511)-((regs[3]>>10)&511)+1);
  texture(1);
  if(!p->semi && !(regs[6]&2) &&
      (!p->mode || ((!mw || (mw|(mw-1))==31) && (!mh || (mh|(mh-1))==31)))) {
    fp=6+p->mode*2+(params[11]!=0);
    params[4]=(32-mw)*8; params[5]=(32-mh)*8;
    params[13]=1.0f/params[4]; params[14]=1.0f/params[5];
    RSX_COUNT(10,1);
  } else RSX_COUNT(11,1);
  if(native) {
    unsigned blend=((p->tpage>>5)&3)==2?2:1;
    fp=15+p->mode;
    if(p->mode && (blend==2 || (regs[6]&1))) {
      params[15]=1;
      if(!vertices(p->v,p->count,fp,params,0)) return 0;
      params[15]=2;
    } else if(p->mode) blend=3;
    if(!vertices(p->v,p->count,fp,params,blend)) return 0;
  } else if(!vertices(p->v,p->count,fp,params,0)) return 0;
  if(p->count==2) {
    struct rsx_vertex dot[6]; unsigned i;
    quad(dot,(int)p->v[1].x,(int)p->v[1].y,1,1,0,0,0,0);
    for(i=0;i<6;i++) dot[i].color=p->v[1].color;
    if(!vertices(dot,6,fp,params,0)) return 0;
  }
  mark_dirty(x,y,w,h);
  return 1;
}
static int flush_primitives(void) {
  uint32_t wanted[64];
  unsigned i,count=hw.primitives.count;
  if(hw.failed) return 0;
  if(!count) return 1;
  rsx_batch_snapshot_tiles(&hw.primitives,wanted);
  hw.primitives.count=0;
  BATCH_COUNT(1,1);
  if(count>1) BATCH_COUNT(2,count);
#ifdef RSX_DIAGNOSTICS
  if(gpu_rsx_batch_diagnostics[3]<count) gpu_rsx_batch_diagnostics[3]=count;
#endif
  if(!refresh_region(0,0,1024,512,wanted)) goto fail;
  for(i=0;i<count;i++) {
    const struct rsx_batch_item *item=&hw.primitives.items[i];
    if(!draw_primitive(&item->primitive,item->regs,1)) goto fail;
  }
  return 1;
fail:
  hw.failed=1; BATCH_COUNT(7,1);
  return 0;
}
int rsx_hw_draw(const struct rsx_primitive *p,const uint32_t *regs) {
  struct rsx_batch_item *item;
  if(hw.failed) return 0;
  if(p->bounds.w<=0 || p->bounds.h<=0 || (!p->fill && !p->count)) return 1;
  if(p->semi && !rsx_native_blend(p,regs) && !p->fill && p->count>=2 && p->bounds.w>0 && p->bounds.h>0) {
    if(!rsx_batch_can_add(&hw.primitives,p,regs)) {
      if(hw.primitives.count==RSX_BATCH_CAPACITY) BATCH_COUNT(5,1);
      else BATCH_COUNT(4,1);
      if(!flush_primitives()) return 0;
    }
    item=&hw.primitives.items[hw.primitives.count++];
    item->primitive=*p; memcpy(item->regs,regs,sizeof(item->regs));
    BATCH_COUNT(0,1);
    return 1;
  }
  if(!flush_primitives()) return 0;
  return draw_primitive(p,regs,0);
}
int rsx_hw_upload(int x,int y,int w,int h,const uint16_t *vram) {
  int row;
  if(!flush_primitives()) return 0;
  RSX_COUNT(7,w*h);
  if(w<=0 || h<=0) return 1;
  for(row=0;row<h;) {
    unsigned pitch=(w*4+63)&~63u,location;
    int rows=h-row,yy,xx,dy,dx;
    uint32_t ofs,*pixels;
    gcmTexture t;
    float params[16]={0};
    if(rows>128) rows=128;
    /* Reserve the image and its vertices together, before sealing a page. */
    pixels=allocate_stream(pitch*rows+4*6*sizeof(struct rsx_vertex),&ofs,&location,128);
    if(!pixels) return 0;
    stream.b[stream.current].used-=4*6*sizeof(struct rsx_vertex);
    for(yy=0;yy<rows;yy++) {
      uint32_t *line=(uint32_t *)((unsigned char *)pixels+pitch*yy);
      for(xx=0;xx<w;xx++) {
        uint16_t c=LE16TOH(vram[((y+row+yy)&511)*1024+((x+xx)&1023)]);
        unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
        line[xx]=((c&0x8000)?0xff000000:0)|(r*8<<16)|(g*8<<8)|(b*8);
      }
    }
    __sync_synchronize();
    flush_draws(); target(0); scissor(0,0,1024,512);
    texture(0);
    memset(&t,0,sizeof(t));
    t.format=GCM_TEXTURE_FORMAT_A8R8G8B8|GCM_TEXTURE_FORMAT_LIN;
    t.mipmap=1; t.dimension=GCM_TEXTURE_DIMS_2D;
    t.remap=(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_B_SHIFT)
      |(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_G_SHIFT)
      |(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_R_SHIFT)
      |(GCM_TEXTURE_REMAP_TYPE_REMAP<<GCM_TEXTURE_REMAP_TYPE_A_SHIFT)
      |(GCM_TEXTURE_REMAP_COLOR_B<<GCM_TEXTURE_REMAP_COLOR_B_SHIFT)
      |(GCM_TEXTURE_REMAP_COLOR_G<<GCM_TEXTURE_REMAP_COLOR_G_SHIFT)
      |(GCM_TEXTURE_REMAP_COLOR_R<<GCM_TEXTURE_REMAP_COLOR_R_SHIFT)
      |(GCM_TEXTURE_REMAP_COLOR_A<<GCM_TEXTURE_REMAP_COLOR_A_SHIFT);
    t.width=w; t.height=rows; t.depth=1; t.pitch=pitch;
    t.location=location; t.offset=ofs;
    rsxLoadTexture(hw.ctx,0,&t); hw.texture_bound=0;
    rsxInvalidateTextureCache(hw.ctx,GCM_INVALIDATE_TEXTURE);
    params[0]=1.0f/w; params[1]=1.0f/rows;
    for(yy=0;yy<rows;) {
      int nh; dy=(y+row+yy)&511; nh=512-dy;
      if(nh>rows-yy) nh=rows-yy;
      for(xx=0;xx<w;) {
        struct rsx_vertex v[6];
        int nw; dx=(x+xx)&1023; nw=1024-dx;
        if(nw>w-xx) nw=w-xx;
        quad(v,dx,dy,nw,nh,xx,yy,nw,nh);
        if(!vertices(v,6,14,params,0)) return 0;
        xx+=nw;
      }
      yy+=nh;
    }
    flush_draws(); row+=rows;
  }
  mark_dirty(x,y,w,h); return 1;
}
int rsx_hw_copy(const uint32_t *data,const uint32_t *regs) {
  int sx=rsx_word(data+1)&1023,sy=(rsx_word(data+1)>>16)&511;
  int dx=rsx_word(data+2)&1023,dy=(rsx_word(data+2)>>16)&511;
  int w=((rsx_word(data+3)-1)&1023)+1,h=(((rsx_word(data+3)>>16)-1)&511)+1;
  float params[16]={0};
  struct rsx_vertex v[6];
  if(!flush_primitives()) return 0;
  if (sx+w>1024 || dx+w>1024 || sy+h>512 || dy+h>512) return 0;
  if (sx<dx+w && dx<sx+w && sy<dy+h && dy<sy+h) return 0;
  if(!refresh(sx,sy,w,h)) return 0;
  if((regs[6]&2) && !refresh(dx,dy,w,h)) return 0;
  quad(v,dx,dy,w,h,sx,sy,w,h);
  params[9]=regs[6]&1; params[10]=(regs[6]>>1)&1;
  target(0); scissor(0,0,1024,512); texture(1);
  if(!vertices(v,6,4,params,0)) return 0;
  mark_dirty(dx,dy,w,h); return 1;
}
int rsx_hw_output(int blank,int ofs,int bgr24,int x,int y,int w,int h,int outw,int outh) {
  RSX_COUNT(8,1);
  int stride=1;
  struct rsx_vertex v[6];
  float params[16]={0};
  unsigned image=2+(hw.output^1);
  if(!flush_primitives()) return 0;
  if (outw<=0 || outw>1024 || outh<=0 || outh>512) return 0;
  if(h>=outh*3/2) { stride=2; h/=2; }
  if(h>outh-y) h=outh-y;
  if(w>outw-x) w=outw-x;
  if(!blank && !refresh(0,0,1024,512)) return 0;
  target(image); scissor(0,0,1024,512);
  rsxSetClearColor(hw.ctx,0xff000000);
  rsxClearSurface(hw.ctx,GCM_CLEAR_R|GCM_CLEAR_G|GCM_CLEAR_B|GCM_CLEAR_A);
  if(!blank && w>0 && h>0) {
    texture(1);
    if(!bgr24) {
      quad(v,x,y,w,h,(ofs&2047)/2,(ofs>>11)&511,w,h*stride);
      if(!vertices(v,6,19,params,0)) return 0;
    } else {
      /* Byte origins need not align to RGB triplets. */
      params[12]=ofs&2047;
      quad(v,x,y,w,h,0,(ofs>>11)&511,w,h*stride);
      if(!vertices(v,6,5,params,0)) return 0;
    }
  }
  hw.output^=1; hw.width=outw; hw.height=outh; hw.ready=hw.output_dirty=1;
  rsx_hw_flush();
  return 1;
}
void rsx_hw_suspend_output(void) {
  rsx_hw_flush();
  hw.ready=0;
  if(hw.iface) hw.iface->set_image(hw.iface->handle,NULL);
}
int rsx_hw_present(retro_video_refresh_t cb) {
  struct retro_rsx_image image;
  if(!flush_primitives()) return 0;
  if(!hw.ready) return 0;
  if(!hw.output_dirty && !hw.state_dirty) {
    rsx_hw_flush(); cb(NULL,hw.width,hw.height,0); invalidate_state(); return 1;
  }
  hw.output_dirty=hw.state_dirty=0;
  image.offset=hw.offsets[2+hw.output]; image.pitch=PITCH;
  image.width=hw.width; image.height=hw.height;
  if(!hw.iface->set_image(hw.iface->handle,&image)) return 0;
  rsx_hw_flush(); cb(RETRO_HW_FRAME_BUFFER_VALID,image.width,image.height,0);
  invalidate_state();
  return 1;
}
void rsx_hw_flush(void) { if(!flush_primitives()) return; flush_draws(); __sync_synchronize(); rsxFlushBuffer(hw.ctx); }
int rsx_hw_create(const struct retro_hw_render_interface_rsx *iface) {
  unsigned i;
  uint32_t size;
  static const char *const attr_names[]={"pos","color","uv"};
  static const char *const const_names[]={"page","window","effects","extra"};
  memset(&hw,0,sizeof(hw)); hw.iface=iface; hw.ctx=iface->context;
  invalidate_state();
  if (iface->io_size-iface->io_used<3*PAGE_BYTES) goto fail;
  if (stream.mapping!=iface->io_address) {
    memset(&stream,0,sizeof(stream)); stream.mapping=iface->io_address;
    *(volatile uint32_t *)gcmGetLabelAddress(FENCE_LABEL)=0;
    *(volatile uint32_t *)gcmGetLabelAddress(BARRIER_LABEL)=0;
    stream.io_blocks=(iface->io_size-iface->io_used)/PAGE_BYTES;
    if(stream.io_blocks>MAX_IO_BLOCKS) stream.io_blocks=MAX_IO_BLOCKS;
    stream.block_count=stream.io_blocks+16;
    for(i=0;i<stream.io_blocks;i++) {
      stream.b[i].ptr=(unsigned char *)stream.mapping+iface->io_used+i*PAGE_BYTES;
      stream.b[i].offset=offset_of(stream.b[i].ptr); stream.b[i].location=GCM_LOCATION_CELL;
    }
  }
  for(i=0;i<4;i++) {
    hw.images[i]=rsxMemalign(128,IMAGE_BYTES);
    if(!hw.images[i]) goto fail;
    hw.offsets[i]=offset_of(hw.images[i]);
  }
  hw.vp=(rsxVertexProgram *)rsx_vertex;
  rsxVertexProgramGetUCode(hw.vp,&hw.vp_code,&size);
  if(!hw.vp_code || !size) goto fail;
  for(i=0;i<3;i++) {
    rsxProgramAttrib *a=rsxVertexProgramGetAttrib(hw.vp,attr_names[i]);
    if(!a) goto fail;
    hw.attributes[i]=a->index;
  }
  for(i=0;i<4;i++) {
    rsxProgramConst *c=rsxVertexProgramGetConst(hw.vp,const_names[i]);
    if(!c) goto fail;
    hw.constants[i]=c->index;
  }
  for(i=0;i<RSX_FP_COUNT;i++) {
    void *ucode=NULL;
    rsxFragmentProgramGetUCode((rsxFragmentProgram *)fragments[i],&ucode,&size);
    if(!ucode || !size) goto fail;
    hw.fragment[i]=rsxMemalign(64,size);
    if(!hw.fragment[i]) goto fail;
    memcpy(hw.fragment[i],ucode,size); hw.fp_offsets[i]=offset_of(hw.fragment[i]);
  }
  memset(hw.dirty,255,sizeof(hw.dirty));
  if(!rsx_hw_upload(0,0,1024,512,gpu.vram)) goto fail;
  return 1;
fail:
  rsx_hw_destroy(); return 0;
}
void rsx_hw_destroy(void) {
  unsigned i;
  if(!hw.iface) return;
  hw.iface->set_image(hw.iface->handle,NULL);
  flush_primitives();
  seal_block();
  for(i=0;i<4;i++) if(hw.images[i]) hw.iface->retire_memory(hw.iface->handle,hw.images[i]);
  for(i=0;i<RSX_FP_COUNT;i++) if(hw.fragment[i]) hw.iface->retire_memory(hw.iface->handle,hw.fragment[i]);
  for(i=stream.io_blocks;i<stream.block_count;i++) {
    if(stream.b[i].ptr) hw.iface->retire_memory(hw.iface->handle,stream.b[i].ptr);
    memset(&stream.b[i],0,sizeof(stream.b[i]));
  }
  if(stream.current>=stream.io_blocks) stream.current=0;
  memset(&hw,0,sizeof(hw));
}

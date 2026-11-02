/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../blend.h"
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
static struct rsx_primitive triangle(void) {
  struct rsx_primitive p;
  memset(&p,0,sizeof(p));
  p.count=3;p.semi=1;p.tpage=1<<5;p.bounds=(struct rsx_rect){0,0,4,4};
  p.v[1].x=4;p.v[2].y=4;
  return p;
}
static struct rsx_primitive rectangle(void) {
  struct rsx_primitive p=triangle();
  p.count=6;p.v[3]=p.v[1];p.v[5]=p.v[2];p.v[4].x=p.v[4].y=4;
  return p;
}
static void eligibility(void) {
  struct rsx_primitive p=triangle();uint32_t regs[8]={0};unsigned i;
  CHECK(rsx_native_blend(&p,regs));
  for(i=0;i<4;i++) {p.tpage=i<<5;CHECK(rsx_native_blend(&p,regs)==(i!=0));}
  p=triangle();p.semi=0;CHECK(!rsx_native_blend(&p,regs));
  p=triangle();p.fill=1;CHECK(!rsx_native_blend(&p,regs));
  p=triangle();p.bounds.w=0;CHECK(!rsx_native_blend(&p,regs));
  p=triangle();p.bounds.h=-1;CHECK(!rsx_native_blend(&p,regs));
  p=triangle();
  for(i=0;i<4;i++) {regs[6]=i;CHECK(rsx_native_blend(&p,regs)==!(i&2));}
  regs[6]=0;
  for(i=0;i<4;i++) {p.mode=i;CHECK(rsx_native_blend(&p,regs));}
  for(i=0;i<9;i++) {p=rectangle();p.count=i;CHECK(rsx_native_blend(&p,regs)==(i==3 || i==6));}
  p=rectangle();CHECK(rsx_native_blend(&p,regs));
  for(i=0;i<6;i++) p.v[i].x=-p.v[i].x;
  CHECK(rsx_native_blend(&p,regs));
  p=rectangle();p.v[4].x=p.v[4].y=1;CHECK(!rsx_native_blend(&p,regs));
  p=rectangle();p.v[4].x=p.v[4].y=-1;CHECK(!rsx_native_blend(&p,regs));
  p=rectangle();p.v[0].x=p.v[0].y=2;CHECK(rsx_native_blend(&p,regs));
  p=rectangle();p.v[4].x=p.v[4].y=2;CHECK(rsx_native_blend(&p,regs));
  p=rectangle();p.v[0].x=p.v[0].y=p.v[4].x=p.v[4].y=2;CHECK(rsx_native_blend(&p,regs));
  p=rectangle();p.v[3].x++;CHECK(!rsx_native_blend(&p,regs));
  p=rectangle();p.v[5].y++;CHECK(!rsx_native_blend(&p,regs));
  puts("native eligibility: flags, counts, winding, overlapping and degenerate quads passed");
}
static unsigned reference(unsigned dst,unsigned src,unsigned mode) {
  unsigned sum;
  if(mode==2) return dst>src?dst-src:0;
  if(mode==3) src/=4;
  sum=dst+src;
  return sum>31?31:sum;
}
static unsigned byte_blend(unsigned dst,unsigned src,unsigned mode) {
  unsigned contribution=8*(mode==3?src/4:src),sum;
  if(mode==2) return dst>contribution?dst-contribution:0;
  sum=dst+contribution;
  return sum>255?255:sum;
}
static void exhaustive(void) {
  unsigned dst,src,mode;
  for(mode=1;mode<=3;mode++) for(dst=0;dst<256;dst++) for(src=0;src<32;src++)
    CHECK((byte_blend(dst,src,mode)>>3)==reference(dst>>3,src,mode));
  CHECK((byte_blend(byte_blend(240,31,1),30,2)>>3)==1);
  puts("all 256 destination bytes x 32 RGB5 sources x 3 modes preserve the RGB5 result");
}
static uint32_t random_state=0xa231f04d;
static uint32_t random_word(void) {
  random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;
  return random_state;
}
static void sequences(void) {
  unsigned bytes[3]={7,159,255},colors[3]={0,19,31},i,j;
  for(i=0;i<100000;i++) {
    unsigned mode=1+random_word()%3;
    for(j=0;j<3;j++) {
      unsigned src=random_word()&31;
      bytes[j]=byte_blend(bytes[j],src,mode);colors[j]=reference(colors[j],src,mode);
      CHECK((bytes[j]>>3)==colors[j]);
    }
  }
  puts("100000 ordered RGB operations preserve saturation, subtraction and residual bits");
}
int main(void) {eligibility();exhaustive();sequences();return 0;}

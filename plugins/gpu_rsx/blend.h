/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PCSX_RSX_BLEND_H
#define PCSX_RSX_BLEND_H
#include "rsx.h"
static int rsx_native_blend(const struct rsx_primitive *p,const uint32_t *regs) {
  float ex,ey,a,b;
  if(!p->semi || p->fill || p->bounds.w<=0 || p->bounds.h<=0 ||
      (regs[6]&2) || !((p->tpage>>5)&3)) return 0;
  if(p->count==3) return 1;
  if(p->count!=6 || p->v[3].x!=p->v[1].x || p->v[3].y!=p->v[1].y ||
      p->v[5].x!=p->v[2].x || p->v[5].y!=p->v[2].y) return 0;
  /* Shared-edge triangles must not cover the same pixels. */
  ex=p->v[2].x-p->v[1].x; ey=p->v[2].y-p->v[1].y;
  a=ex*(p->v[0].y-p->v[1].y)-ey*(p->v[0].x-p->v[1].x);
  b=ex*(p->v[4].y-p->v[1].y)-ey*(p->v[4].x-p->v[1].x);
  return (a<=0 && b>=0) || (a>=0 && b<=0);
}
#endif

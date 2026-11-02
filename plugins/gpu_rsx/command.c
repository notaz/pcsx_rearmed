/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <string.h>
#include "rsx.h"
static int signed11(unsigned v) { return (int)((v & 2047) ^ 1024) - 1024; }
static void vertex(struct rsx_vertex *v, uint32_t xy, uint32_t col,
                   uint32_t uv, int ox, int oy) {
  v->x = signed11(xy) + ox;
  v->y = signed11(xy >> 16) + oy;
  v->z = 0; v->w = 1;
  v->u = uv & 255; v->v = (uv >> 8) & 255;
  v->color = (col & 255) << 24 | ((col >> 8) & 255) << 16
           | ((col >> 16) & 255) << 8 | 255;
  v->pad = 0;
}
int rsx_decode(struct rsx_primitive *p, const uint32_t *data, unsigned words,
               const uint32_t *regs) {
  struct rsx_vertex v[4];
  unsigned cmd = rsx_word(data) >> 24, textured = 0, gouraud = 0;
  unsigned i, n = 0, at = 1, col = rsx_word(data), uv = 0;
  int ox = signed11(regs[5]), oy = signed11(regs[5] >> 11);
  int x0 = 1024, y0 = 512, x1 = 0, y1 = 0;
  int clipx0 = regs[3] & 1023, clipy0 = (regs[3] >> 10) & 511;
  int clipx1 = (regs[4] & 1023) + 1, clipy1 = ((regs[4] >> 10) & 511) + 1;
  memset(p, 0, sizeof(*p));
  p->tpage = regs[1] & 511;
  p->semi = !!(cmd & 2); p->raw = !!(cmd & 1);
  if (cmd == 2) {
    uint32_t xy = rsx_word(data + 1), wh = rsx_word(data + 2);
    int w = ((wh & 1023) + 15) & ~15, h = (wh >> 16) & 511;
    x0 = xy & 1008; y0 = (xy >> 16) & 511;
    p->bounds = (struct rsx_rect){x0,y0,w,h};
    p->replace = p->fill = 1;
    p->semi = p->raw = 0;
    p->count = 0;
    p->v[0].color = (col & 255) << 24 | ((col >> 8) & 255) << 16
                  | ((col >> 16) & 255) << 8 | 255;
    return 1;
  }
  if (cmd >= 0x20 && cmd < 0x40) {
    n = (cmd & 8) ? 4 : 3; textured = !!(cmd & 4); gouraud = !!(cmd & 16);
    for (i = 0; i < n; i++) {
      uint32_t xy;
      if (i && gouraud) { if (at >= words) return 0; col = rsx_word(data + at++); }
      if (at >= words) return 0;
      xy = rsx_word(data + at++);
      if (textured) {
        if (at >= words) return 0;
        uv = rsx_word(data + at++);
        if (i == 0) p->clut = uv >> 16;
        if (i == 1) p->tpage = (uv >> 16) & 511;
      }
      vertex(v+i, xy, col, uv, ox, oy);
    }
  } else if (cmd >= 0x40 && cmd < 0x60) {
    uint32_t xy0=rsx_word(data+1), xy1;
    gouraud=!!(cmd&16);
    xy1=rsx_word(data+(gouraud?3:2));
    vertex(v,xy0,col,0,ox,oy);
    if(gouraud) col=rsx_word(data+2);
    vertex(v+1,xy1,col,0,ox,oy);
    n=2;
  } else if (cmd >= 0x60 && cmd < 0x80) {
    uint32_t xy = rsx_word(data + at++), wh;
    int w, h;
    textured = !!(cmd & 4);
    if (textured) { uv = rsx_word(data + at++); p->clut = uv >> 16; }
    switch ((cmd >> 3) & 3) {
      case 1: w = h = 1; break;
      case 2: w = h = 8; break;
      case 3: w = h = 16; break;
      default:
        if (at >= words) return 0;
        wh = rsx_word(data+at); w = wh & 1023; h = (wh >> 16) & 511;
    }
    vertex(v, xy, col, uv, ox, oy);
    v[1] = v[2] = v[3] = v[0];
    v[1].x += w; v[2].y += h; v[3].x += w; v[3].y += h;
    v[1].u += (regs[1] & 0x1000) ? -w : w;
    v[3].u = v[1].u;
    v[2].v += (regs[1] & 0x2000) ? -h : h;
    v[3].v = v[2].v;
    n = 4;
    p->replace = !textured && !p->semi && !(regs[6] & 2);
  } else {
    return 0;
  }
  p->dither = cmd < 0x40 && (gouraud || (textured && !p->raw));
  p->mode = textured ? ((p->tpage >> 7) & 3) + 1 : 0;
  if (p->mode > 3) p->mode = 3;
  for (i = 0; i < n; i++) {
    if (v[i].x < x0) x0 = (int)v[i].x;
    if (v[i].y < y0) y0 = (int)v[i].y;
    if (v[i].x > x1) x1 = (int)v[i].x;
    if (v[i].y > y1) y1 = (int)v[i].y;
  }
  if(n==2) { x1++; y1++; }
  if (x1-x0 >= 1024 || y1-y0 >= 512) { p->count = 0; return 1; }
  if (n == 4 && !textured && !p->semi && !(regs[6] & 2)
      && v[0].x == v[2].x && v[1].x == v[3].x
      && v[0].y == v[1].y && v[2].y == v[3].y)
    p->replace = 1;
  if (x0 < clipx0) x0 = clipx0;
  if (y0 < clipy0) y0 = clipy0;
  if (x1 > clipx1) x1 = clipx1;
  if (y1 > clipy1) y1 = clipy1;
  p->bounds = (struct rsx_rect){x0,y0,x1-x0,y1-y0};
  if (p->bounds.w <= 0 || p->bounds.h <= 0) return 1;
  p->v[0] = v[0]; p->v[1] = v[1];
  if(n==2) {
    p->count=2;
    if(v[0].x==v[1].x && v[0].y==v[1].y) {
      p->v[1]=p->v[2]=p->v[3]=p->v[4]=p->v[5]=v[0];
      p->v[1].x++; p->v[2].y++;
      p->v[3]=p->v[1]; p->v[4]=p->v[1]; p->v[4].y++; p->v[5]=p->v[2];
      p->count=6;
    }
    return 1;
  }
  p->v[2] = v[2]; p->count = 3;
  if (n == 4) { p->v[3] = v[1]; p->v[4] = v[3]; p->v[5] = v[2]; p->count = 6; }
  return 1;
}

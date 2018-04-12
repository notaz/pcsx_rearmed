/*
 * (C) Gražvydas "notaz" Ignotas, 2011,2012
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include "cspace.h"

/*
 * note: these are intended for testing and should be avoided
 * in favor of NEON version or platform-specific conversion
 */

#ifndef __arm__

void bgr555_to_rgb565(void *dst_, const void *src_, int bytes)
{
	const unsigned int *src = src_;
	unsigned int *dst = dst_;
	unsigned int p;
	int x;

	for (x = 0; x < bytes / 4; x++) {
		p = src[x];
		p = ((p & 0x7c007c00) >> 10) | ((p & 0x03e003e0) << 1)
			| ((p & 0x001f001f) << 11);
		dst[x] = p;
	}
}

#endif

#ifdef __arm64__

void bgr888_to_rgb565(void *dst_, const void *src_, int bytes)
{
    const unsigned char *src = src_;
    unsigned int *dst = dst_;
    unsigned int r1, g1, b1, r2, g2, b2;

    for (; bytes >= 6; bytes -= 6, src += 6, dst++) {
        r1 = src[0] & 0xf8;
        g1 = src[1] & 0xfc;
        b1 = src[2] & 0xf8;
        r2 = src[3] & 0xf8;
        g2 = src[4] & 0xfc;
        b2 = src[5] & 0xf8;
        *dst = (r2 << 24) | (g2 << 19) | (b2 << 13) |
               (r1 << 8) | (g1 << 3) | (b1 >> 3);
    }
}

#endif

#ifndef __ARM_NEON__

void bgr888_to_rgb565(void *dst_, const void *src_, int bytes)
{
    const unsigned char *src = src_;
    unsigned int *dst = dst_;
    unsigned int r1, g1, b1, r2, g2, b2;

    for (; bytes >= 6; bytes -= 6, src += 6, dst++) {
        r1 = src[0] & 0xf8;
        g1 = src[1] & 0xfc;
        b1 = src[2] & 0xf8;
        r2 = src[3] & 0xf8;
        g2 = src[4] & 0xfc;
        b2 = src[5] & 0xf8;
        *dst = (r2 << 24) | (g2 << 19) | (b2 << 13) |
               (r1 << 8) | (g1 << 3) | (b1 >> 3);
    }
}

// TODO?
void rgb888_to_rgb565(void *dst, const void *src, int bytes) {}
void bgr888_to_rgb888(void *dst, const void *src, int bytes) {}

#endif // __ARM_NEON__

/* YUV stuff */
static int yuv_ry[32], yuv_gy[32], yuv_by[32];
static unsigned char yuv_u[32 * 2], yuv_v[32 * 2];

void bgr_to_uyvy_init(void)
{
  int i, v;

  /* init yuv converter:
    y0 = (int)((0.299f * r0) + (0.587f * g0) + (0.114f * b0));
    y1 = (int)((0.299f * r1) + (0.587f * g1) + (0.114f * b1));
    u = (int)(8 * 0.565f * (b0 - y0)) + 128;
    v = (int)(8 * 0.713f * (r0 - y0)) + 128;
  */
  for (i = 0; i < 32; i++) {
    yuv_ry[i] = (int)(0.299f * i * 65536.0f + 0.5f);
    yuv_gy[i] = (int)(0.587f * i * 65536.0f + 0.5f);
    yuv_by[i] = (int)(0.114f * i * 65536.0f + 0.5f);
  }
  for (i = -32; i < 32; i++) {
    v = (int)(8 * 0.565f * i) + 128;
    if (v < 0)
      v = 0;
    if (v > 255)
      v = 255;
    yuv_u[i + 32] = v;
    v = (int)(8 * 0.713f * i) + 128;
    if (v < 0)
      v = 0;
    if (v > 255)
      v = 255;
    yuv_v[i + 32] = v;
  }
}

void rgb565_to_uyvy(void *d, const void *s, int pixels)
{
  unsigned int *dst = d;
  const unsigned short *src = s;
  const unsigned char *yu = yuv_u + 32;
  const unsigned char *yv = yuv_v + 32;
  int r0, g0, b0, r1, g1, b1;
  int y0, y1, u, v;

  for (; pixels > 0; src += 2, dst++, pixels -= 2)
  {
    r0 = (src[0] >> 11) & 0x1f;
    g0 = (src[0] >> 6) & 0x1f;
    b0 =  src[0] & 0x1f;
    r1 = (src[1] >> 11) & 0x1f;
    g1 = (src[1] >> 6) & 0x1f;
    b1 =  src[1] & 0x1f;
    y0 = (yuv_ry[r0] + yuv_gy[g0] + yuv_by[b0]) >> 16;
    y1 = (yuv_ry[r1] + yuv_gy[g1] + yuv_by[b1]) >> 16;
    u = yu[b0 - y0];
    v = yv[r0 - y0];
    // valid Y range seems to be 16..235
    y0 = 16 + 219 * y0 / 31;
    y1 = 16 + 219 * y1 / 31;

    *dst = (y1 << 24) | (v << 16) | (y0 << 8) | u;
  }
}

void bgr555_to_uyvy(void *d, const void *s, int pixels)
{
  unsigned int *dst = d;
  const unsigned short *src = s;
  const unsigned char *yu = yuv_u + 32;
  const unsigned char *yv = yuv_v + 32;
  int r0, g0, b0, r1, g1, b1;
  int y0, y1, u, v;

  for (; pixels > 0; src += 2, dst++, pixels -= 2)
  {
    b0 = (src[0] >> 10) & 0x1f;
    g0 = (src[0] >> 5) & 0x1f;
    r0 =  src[0] & 0x1f;
    b1 = (src[1] >> 10) & 0x1f;
    g1 = (src[1] >> 5) & 0x1f;
    r1 =  src[1] & 0x1f;
    y0 = (yuv_ry[r0] + yuv_gy[g0] + yuv_by[b0]) >> 16;
    y1 = (yuv_ry[r1] + yuv_gy[g1] + yuv_by[b1]) >> 16;
    u = yu[b0 - y0];
    v = yv[r0 - y0];
    y0 = 16 + 219 * y0 / 31;
    y1 = 16 + 219 * y1 / 31;

    *dst = (y1 << 24) | (v << 16) | (y0 << 8) | u;
  }
}

void bgr888_to_uyvy(void *d, const void *s, int pixels)
{
  unsigned int *dst = d;
  const unsigned char *src8 = s;
  const unsigned char *yu = yuv_u + 32;
  const unsigned char *yv = yuv_v + 32;
  int r0, g0, b0, r1, g1, b1;
  int y0, y1, u, v;

  for (; pixels > 0; src8 += 3*2, dst++, pixels -= 2)
  {
    r0 = src8[0], g0 = src8[1], b0 = src8[2];
    r1 = src8[3], g1 = src8[4], b1 = src8[5];
    y0 = (r0 * 19595 + g0 * 38470 + b0 * 7471) >> 16;
    y1 = (r1 * 19595 + g1 * 38470 + b1 * 7471) >> 16;
    u = yu[(b0 - y0) / 8];
    v = yv[(r0 - y0) / 8];
    y0 = 16 + 219 * y0 / 255;
    y1 = 16 + 219 * y1 / 255;

    *dst = (y1 << 24) | (v << 16) | (y0 << 8) | u;
  }
}

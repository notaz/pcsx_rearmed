/*
 * (C) Gražvydas "notaz" Ignotas, 2011,2012,2022
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdint.h>
#include "cspace.h"

/*
 * note: these are intended for testing and should be avoided
 * in favor of NEON version or platform-specific conversion
 */

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SWAP16(x) __builtin_bswap16(x)
#define LE16TOHx2(x) ((SWAP16((x) >> 16) << 16) | SWAP16(x))
#else
#define LE16TOHx2(x) (x)
#endif

#if defined(HAVE_bgr555_to_rgb565)

/* have bgr555_to_rgb565 somewhere else */

#elif ((defined(__clang_major__) && __clang_major__ >= 4) \
        || (defined(__GNUC__) && __GNUC__ >= 5)) \
       && __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__

#include <assert.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define gsli(d_, s_, n_) d_ = vsliq_n_u16(d_, s_, n_)
#define gsri(d_, s_, n_) d_ = vsriq_n_u16(d_, s_, n_)
#else
#define gsli(d_, s_, n_) d_ |= s_ << n_
#define gsri(d_, s_, n_) d_ |= s_ >> n_
#endif

typedef uint16_t gvu16  __attribute__((vector_size(16),aligned(16)));
typedef uint16_t gvu16u __attribute__((vector_size(16),aligned(2)));
#define gdup(v_) {v_, v_, v_, v_, v_, v_, v_, v_}
#define do_one(s) ({ \
  uint16_t d_ = (s) << 1; d_ = (d_ & 0x07c0) | (d_ << 10) | (d_ >> 11); d_; \
})
#define do_one_simd(d_, s_, c0x07c0_) { \
  gvu16 s1 = s_ << 1; \
  d_ = s1 & c0x07c0_; \
  gsli(d_, s_, 11); \
  gsri(d_, s1, 11); \
}

void bgr555_to_rgb565(void * __restrict__ dst_, const void *  __restrict__ src_, int bytes)
{
	const uint16_t * __restrict__ src = src_;
	uint16_t * __restrict__ dst = dst_;
	gvu16 c0x07c0 = gdup(0x07c0);

	assert(!(((uintptr_t)dst | (uintptr_t)src | bytes) & 1));

	// align the destination
	if ((uintptr_t)dst & 0x0e)
	{
		uintptr_t left = 0x10 - ((uintptr_t)dst & 0x0e);
		gvu16 d, s = *(const gvu16u *)src;
		do_one_simd(d, s, c0x07c0);
		*(gvu16u *)dst = d;
		dst += left / 2;
		src += left / 2;
		bytes -= left;
	}
	// go
	for (; bytes >= 16; dst += 8, src += 8, bytes -= 16)
	{
		gvu16 d, s = *(const gvu16u *)src;
		do_one_simd(d, s, c0x07c0);
		*(gvu16 *)dst = d;
		__builtin_prefetch(src + 128/2);
	}
	// finish it
	for (; bytes > 0; dst++, src++, bytes -= 2)
		*dst = do_one(*src);
}
#undef do_one
#undef do_one_simd

#else

void bgr555_to_rgb565(void *dst_, const void *src_, int bytes)
{
    // source can be misaligned, but it's very rare, so just force
    const unsigned int *src = (const void *)((intptr_t)src_ & ~3);
    unsigned int *dst = dst_;
    unsigned int x, p, r, g, b;

    for (x = 0; x < bytes / 4; x++) {
        p = LE16TOHx2(src[x]);

        r = (p & 0x001f001f) << 11;
        g = (p & 0x03e003e0) << 1;
        b = (p & 0x7c007c00) >> 10;

        dst[x] = r | g | b;
    }
}

#endif

#ifndef HAVE_bgr888_to_x

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
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        *dst = (r1 << 24) | (g1 << 19) | (b1 << 13) |
               (r2 << 8) | (g2 << 3) | (b2 >> 3);
#else
        *dst = (r2 << 24) | (g2 << 19) | (b2 << 13) |
               (r1 << 8) | (g1 << 3) | (b1 >> 3);
#endif
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

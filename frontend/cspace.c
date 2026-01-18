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
#include "compiler_features.h"

/*
 * note: these are intended for testing and should be avoided
 * in favor of NEON version or platform-specific conversion
 */

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SWAP16(x) __builtin_bswap16(x)
#define LE16TOHx2(x) ((SWAP16((x) >> 16) << 16) | SWAP16(x))
#define LE32TOH(x) __builtin_bswap32(x)
#else
#define LE16TOHx2(x) (x)
#define LE32TOH(x) (x)
#endif

static inline uint32_t bgr555_to_rgb565_pair(uint32_t p)
{
	uint32_t r, g, b;
	r = (p & 0x001f001f) << 11;
	g = (p & 0x03e003e0) << 1;
	b = (p & 0x7c007c00) >> 10;
	return r | g | b;
}

static inline uint32_t bgr888_to_rgb565_pair(const uint8_t * __restrict__ src,
	int o0, int o1)
{
	uint32_t r1, g1, b1, r2, g2, b2;
	r1 = src[o0 + 0] & 0xf8;
	g1 = src[o0 + 1] & 0xfc;
	b1 = src[o0 + 2] & 0xf8;
	r2 = src[o1 + 0] & 0xf8;
	g2 = src[o1 + 1] & 0xfc;
	b2 = src[o1 + 2] & 0xf8;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return (r1 << 24) | (g1 << 19) | (b1 << 13) |
	       (r2 << 8)  | (g2 << 3)  | (b2 >> 3);
#else
	return (r2 << 24) | (g2 << 19) | (b2 << 13) |
	       (r1 << 8)  | (g1 << 3)  | (b1 >> 3);
#endif
}

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

void bgr555_to_rgb565(void * __restrict__ dst_, const void *  __restrict__ src_,
	int pixels)
{
	const uint16_t * __restrict__ src = src_;
	uint16_t * __restrict__ dst = dst_;
	gvu16 c0x07c0 = gdup(0x07c0);

	assert(!(((uintptr_t)dst | (uintptr_t)src) & 1));

	// align the destination
	if ((uintptr_t)dst & 0x0e)
	{
		uintptr_t left = 0x10 - ((uintptr_t)dst & 0x0e);
		gvu16 d, s = *(const gvu16u *)src;
		do_one_simd(d, s, c0x07c0);
		*(gvu16u *)dst = d;
		dst += left / 2;
		src += left / 2;
		pixels -= left / 2;
	}
	// go
	for (; pixels >= 8; dst += 8, src += 8, pixels -= 8)
	{
		gvu16 d, s = *(const gvu16u *)src;
		do_one_simd(d, s, c0x07c0);
		*(gvu16 *)dst = d;
		__builtin_prefetch(src + 128/2);
	}
	// finish it
	for (; pixels > 0; dst++, src++, pixels--)
		*dst = do_one(*src);
}
#undef do_one
#undef do_one_simd

#else

void bgr555_to_rgb565(void * __restrict__ dst_, const void * __restrict__ src_,
	int pixels)
{
	// source can be misaligned, but it's very rare, so just force
	const uint32_t * __restrict__ src = (const void *)((intptr_t)src_ & ~3);
	uint32_t x, * __restrict__ dst = dst_;

	for (x = 0; x < pixels / 2; x++)
		dst[x] = bgr555_to_rgb565_pair(LE16TOHx2(src[x]));
}

#endif

static inline void bgr888_to_rgb888_one(uint8_t * __restrict__ dst,
	const uint8_t * __restrict__ src)
{
	dst[0] = src[2];
	dst[1] = src[1];
	dst[2] = src[0];
}

#ifndef HAVE_bgr888_to_x

void attr_weak bgr888_to_rgb565(void * __restrict__ dst_,
		const void * __restrict__ src_, int pixels)
{
	const uint8_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; pixels >= 2; pixels -= 2, src += 3*2, dst++)
		*dst = bgr888_to_rgb565_pair(src, 0, 3);
}

// TODO?
void rgb888_to_rgb565(void *dst, const void *src, int pixels) {}

void bgr888_to_rgb888(void * __restrict__ dst_,
	const void * __restrict__ src_, int pixels)
{
	const uint8_t * __restrict__ src = src_;
	uint8_t * __restrict__ dst = dst_;
	
	for (; pixels >= 1; pixels--, src += 3, dst += 3)
		bgr888_to_rgb888_one(dst, src);
}

#endif // HAVE_bgr888_to_x

static inline uint32_t bgr555_to_xrgb8888_one(uint16_t p)
{
	uint32_t t = ((p << 19) | (p >> 7)) & 0xf800f8;
	t |= (p << 6) & 0xf800;
	return t | ((t >> 5) & 0x070707);
}

static inline uint32_t bgr888_to_xrgb8888_one(const uint8_t * __restrict__ src)
{
	return (src[0] << 16) | (src[1] << 8) | src[2];
}

void bgr555_to_xrgb8888(void * __restrict__ dst_,
	const void * __restrict__ src_, int pixels)
{
	const uint16_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; pixels >= 1; pixels--, src++, dst++)
		*dst = bgr555_to_xrgb8888_one(*src);
}

void bgr888_to_xrgb8888(void * __restrict__ dst_,
	const void * __restrict__ src_, int pixels)
{
	const uint8_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; pixels >= 1; pixels--, src += 3, dst++)
		*dst = bgr888_to_xrgb8888_one(src);
}

/* downscale */
void bgr555_to_rgb565_640_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint16_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; dpixels >= 2; dpixels -= 2, src += 4, dst++) {
		uint32_t p = LE32TOH(src[0] | (src[2] << 16));
		*dst = bgr555_to_rgb565_pair(p);
	}
}

void bgr888_to_rgb565_640_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint8_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; dpixels >= 2; dpixels -= 2, src += 4*3, dst++)
		*dst = bgr888_to_rgb565_pair(src, 0*3, 2*3);
}

void bgr888_to_rgb888_640_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint8_t * __restrict__ src = src_;
	uint8_t * __restrict__ dst = dst_;

	for (; dpixels >= 1; dpixels--, src += 2*3, dst += 3)
		bgr888_to_rgb888_one(dst, src);
}

void bgr555_to_xrgb8888_640_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint16_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; dpixels >= 1; dpixels--, src += 2, dst++)
		*dst = bgr555_to_xrgb8888_one(*src);
}

void bgr888_to_xrgb8888_640_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint8_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; dpixels >= 1; dpixels--, src += 3*2, dst++)
		*dst = (src[0] << 16) | (src[1] << 8) | src[2];
}

void bgr555_to_rgb565_512_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint16_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	// 16 -> 10 to keep dst aligned
	for (; dpixels >= 10; dpixels -= 10, src += 16, dst += 5) {
		// picks a src pixel nearest to the center of the dst pixel
		dst[0] = bgr555_to_rgb565_pair(LE32TOH(src[0]  | (src[2] << 16)));
		dst[1] = bgr555_to_rgb565_pair(LE32TOH(src[4]  | (src[5] << 16)));
		dst[2] = bgr555_to_rgb565_pair(LE32TOH(src[7]  | (src[8] << 16)));
		dst[3] = bgr555_to_rgb565_pair(LE32TOH(src[10] | (src[12] << 16)));
		dst[4] = bgr555_to_rgb565_pair(LE32TOH(src[13] | (src[15] << 16)));
	}
}

void bgr888_to_rgb565_512_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint8_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; dpixels >= 10; dpixels -= 10, src += 16*3, dst += 5) {
		dst[0] = bgr888_to_rgb565_pair(src, 3*0, 3*2);
		dst[1] = bgr888_to_rgb565_pair(src, 3*4, 3*5);
		dst[2] = bgr888_to_rgb565_pair(src, 3*7, 3*8);
		dst[3] = bgr888_to_rgb565_pair(src, 3*10, 3*12);
		dst[4] = bgr888_to_rgb565_pair(src, 3*13, 3*15);
	}
}

void bgr888_to_rgb888_512_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint8_t * __restrict__ src = src_;
	uint8_t * __restrict__ dst = dst_;

	for (; dpixels >= 5; dpixels -= 5, src += 8*3, dst += 5*3) {
		bgr888_to_rgb888_one(dst + 3*0, src + 3*0);
		bgr888_to_rgb888_one(dst + 3*1, src + 3*2);
		bgr888_to_rgb888_one(dst + 3*2, src + 3*4);
		bgr888_to_rgb888_one(dst + 3*3, src + 3*5);
		bgr888_to_rgb888_one(dst + 3*4, src + 3*7);
	}
}

void bgr555_to_xrgb8888_512_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint16_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	// 8 -> 5
	for (; dpixels >= 5; dpixels -= 5, src += 8, dst += 5) {
		dst[0] = bgr555_to_xrgb8888_one(src[0]);
		dst[1] = bgr555_to_xrgb8888_one(src[2]);
		dst[2] = bgr555_to_xrgb8888_one(src[4]);
		dst[3] = bgr555_to_xrgb8888_one(src[5]);
		dst[4] = bgr555_to_xrgb8888_one(src[7]);
	}
}

void bgr888_to_xrgb8888_512_to_320(void * __restrict__ dst_,
	const void * __restrict__ src_, int dpixels)
{
	const uint8_t * __restrict__ src = src_;
	uint32_t * __restrict__ dst = dst_;

	for (; dpixels >= 5; dpixels -= 5, src += 8*3, dst += 5) {
		dst[0] = bgr888_to_xrgb8888_one(src + 0*3);
		dst[1] = bgr888_to_xrgb8888_one(src + 2*3);
		dst[2] = bgr888_to_xrgb8888_one(src + 4*3);
		dst[3] = bgr888_to_xrgb8888_one(src + 5*3);
		dst[4] = bgr888_to_xrgb8888_one(src + 7*3);
	}
}

/* YUV stuff */
static int yuv_ry[32], yuv_gy[32], yuv_by[32];
static unsigned char yuv_u[32 * 2], yuv_v[32 * 2];
static struct uyvy { uint32_t y:8; uint32_t vyu:24; } yuv_uyvy[32768];

void bgr_to_uyvy_init(void)
{
	unsigned char yuv_y[256];
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
	// valid Y range seems to be 16..235
	for (i = 0; i < 256; i++) {
		yuv_y[i] = 16 + 219 * i / 32;
	}
	// everything combined into one large array for speed
	for (i = 0; i < 32768; i++) {
		int r = (i >> 0) & 0x1f, g = (i >> 5) & 0x1f, b = (i >> 10) & 0x1f;
		int y = (yuv_ry[r] + yuv_gy[g] + yuv_by[b]) >> 16;
		yuv_uyvy[i].y = yuv_y[y];
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		yuv_uyvy[i].vyu = (yuv_v[b-y + 32] << 16) | (yuv_y[y] << 8) | yuv_u[r-y + 32];
#else
		yuv_uyvy[i].vyu = (yuv_v[r-y + 32] << 16) | (yuv_y[y] << 8) | yuv_u[b-y + 32];
#endif
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

void bgr555_to_uyvy(void *d, const void *s, int pixels, int x2)
{
	uint32_t *dst = d;
	const uint16_t *src = s;
	int i;

	if (x2) {
		for (i = pixels; i >= 4; src += 4, dst += 4, i -= 4)
		{
			const struct uyvy *uyvy0 = yuv_uyvy + (src[0] & 0x7fff);
			const struct uyvy *uyvy1 = yuv_uyvy + (src[1] & 0x7fff);
			const struct uyvy *uyvy2 = yuv_uyvy + (src[2] & 0x7fff);
			const struct uyvy *uyvy3 = yuv_uyvy + (src[3] & 0x7fff);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			dst[0] = uyvy0->y | (uyvy0->vyu << 8);
			dst[1] = uyvy1->y | (uyvy1->vyu << 8);
			dst[2] = uyvy2->y | (uyvy2->vyu << 8);
			dst[3] = uyvy3->y | (uyvy3->vyu << 8);
#else
			dst[0] = (uyvy0->y << 24) | uyvy0->vyu;
			dst[1] = (uyvy1->y << 24) | uyvy1->vyu;
			dst[2] = (uyvy2->y << 24) | uyvy2->vyu;
			dst[3] = (uyvy3->y << 24) | uyvy3->vyu;
#endif
		}
	} else {
		for (i = pixels; i >= 4; src += 4, dst += 2, i -= 4)
		{
			const struct uyvy *uyvy0 = yuv_uyvy + (src[0] & 0x7fff);
			const struct uyvy *uyvy1 = yuv_uyvy + (src[1] & 0x7fff);
			const struct uyvy *uyvy2 = yuv_uyvy + (src[2] & 0x7fff);
			const struct uyvy *uyvy3 = yuv_uyvy + (src[3] & 0x7fff);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			dst[0] = uyvy1->y | (uyvy0->vyu << 8);
			dst[1] = uyvy3->y | (uyvy2->vyu << 8);
#else
			dst[0] = (uyvy1->y << 24) | uyvy0->vyu;
			dst[1] = (uyvy3->y << 24) | uyvy2->vyu;
#endif
		}
	}
}

void bgr888_to_uyvy(void *d, const void *s, int pixels, int x2)
{
	unsigned int *dst = d;
	const unsigned char *src8 = s;
	const unsigned char *yu = yuv_u + 32;
	const unsigned char *yv = yuv_v + 32;
	int r0, g0, b0, r1, g1, b1;
	int y0, y1, u0, u1, v0, v1;

	if (x2) {
		for (; pixels >= 2; src8 += 3*2, pixels -= 2)
		{
			r0 = src8[0], g0 = src8[1], b0 = src8[2];
			r1 = src8[3], g1 = src8[4], b1 = src8[5];
			y0 = (r0 * 19595 + g0 * 38470 + b0 * 7471) >> 16;
			y1 = (r1 * 19595 + g1 * 38470 + b1 * 7471) >> 16;
			u0 = yu[(b0 - y0) / 8];
			u1 = yu[(b1 - y1) / 8];
			v0 = yv[(r0 - y0) / 8];
			v1 = yv[(r1 - y1) / 8];
			y0 = 16 + 219 * y0 / 255;
			y1 = 16 + 219 * y1 / 255;

			*dst++ = (y0 << 24) | (v0 << 16) | (y0 << 8) | u0;
			*dst++ = (y1 << 24) | (v1 << 16) | (y1 << 8) | u1;
		}
	}
	else {
		for (; pixels >= 2; src8 += 3*2, dst++, pixels -= 2)
		{
			r0 = src8[0], g0 = src8[1], b0 = src8[2];
			r1 = src8[3], g1 = src8[4], b1 = src8[5];
			y0 = (r0 * 19595 + g0 * 38470 + b0 * 7471) >> 16;
			y1 = (r1 * 19595 + g1 * 38470 + b1 * 7471) >> 16;
			u0 = yu[(b0 - y0) / 8];
			v0 = yv[(r0 - y0) / 8];
			y0 = 16 + 219 * y0 / 255;
			y1 = 16 + 219 * y1 / 255;

			*dst = (y1 << 24) | (v0 << 16) | (y0 << 8) | u0;
		}
	}
}

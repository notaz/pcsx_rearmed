#include "cspace.h"

/*
 * note: these are intended for testing and should be avoided
 * in favor of NEON version or platform-specific conversion
 */

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
void bgr888_to_rgb888(void *dst, const void *src, int bytes) {}


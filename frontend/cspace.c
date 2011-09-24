#include "cspace.h"

void bgr555_to_rgb565(void *dst_, const void *src_, int bytes)
{
	unsigned int *src = (unsigned int *)src_;
	unsigned int *dst = (unsigned int *)dst_;
	unsigned int p;
	int x;

	for (x = 0; x < bytes / 4; x++) {
		p = src[x];
		p = ((p & 0x7c007c00) >> 10) | ((p & 0x03e003e0) << 1)
			| ((p & 0x001f001f) << 11);
		dst[x] = p;
	}
}

// TODO?
void bgr888_to_rgb888(void *dst, const void *src, int bytes) {}
void bgr888_to_rgb565(void *dst, const void *src, int bytes) {}


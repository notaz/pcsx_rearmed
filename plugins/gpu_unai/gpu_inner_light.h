/***************************************************************************
*   Copyright (C) 2016 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
***************************************************************************/

#ifndef _OP_LIGHT_H_
#define _OP_LIGHT_H_

//  GPU color operations for lighting calculations

static void SetupLightLUT()
{
	// 1024-entry lookup table that modulates 5-bit texture + 5-bit light value.
	// A light value of 15 does not modify the incoming texture color.
	// LightLUT[32*32] array is initialized to following values:
	//  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	//  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	//  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	//  0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
	//  0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
	//  0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9,
	//  0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 9,10,10,10,11,11,
	//  0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 8, 8, 9, 9,10,10,10,11,11,12,12,13,13,
	//  0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,10,10,11,11,12,12,13,13,14,14,15,15,
	//  0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7, 8, 9, 9,10,10,11,11,12,12,13,14,14,15,15,16,16,17,
	//  0, 0, 1, 1, 2, 3, 3, 4, 5, 5, 6, 6, 7, 8, 8, 9,10,10,11,11,12,13,13,14,15,15,16,16,17,18,18,19,
	//  0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 8, 8, 9,10,11,11,12,13,13,14,15,15,16,17,17,18,19,19,20,21,
	//  0, 0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 9,10,11,12,12,13,14,15,15,16,17,18,18,19,20,21,21,22,23,
	//  0, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 8, 9,10,11,12,13,13,14,15,16,17,17,18,19,20,21,21,22,23,24,25,
	//  0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9,10,11,12,13,14,14,15,16,17,18,19,20,21,21,22,23,24,25,26,27,
	//  0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
	//  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	//  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,31,
	//  0, 1, 2, 3, 4, 5, 6, 7, 9,10,11,12,13,14,15,16,18,19,20,21,22,23,24,25,27,28,29,30,31,31,31,31,
	//  0, 1, 2, 3, 4, 5, 7, 8, 9,10,11,13,14,15,16,17,19,20,21,22,23,24,26,27,28,29,30,31,31,31,31,31,
	//  0, 1, 2, 3, 5, 6, 7, 8,10,11,12,13,15,16,17,18,20,21,22,23,25,26,27,28,30,31,31,31,31,31,31,31,
	//  0, 1, 2, 3, 5, 6, 7, 9,10,11,13,14,15,17,18,19,21,22,23,24,26,27,28,30,31,31,31,31,31,31,31,31,
	//  0, 1, 2, 4, 5, 6, 8, 9,11,12,13,15,16,17,19,20,22,23,24,26,27,28,30,31,31,31,31,31,31,31,31,31,
	//  0, 1, 2, 4, 5, 7, 8,10,11,12,14,15,17,18,20,21,23,24,25,27,28,30,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 4, 6, 7, 9,10,12,13,15,16,18,19,21,22,24,25,27,28,30,31,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 4, 6, 7, 9,10,12,14,15,17,18,20,21,23,25,26,28,29,31,31,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 4, 6, 8, 9,11,13,14,16,17,19,21,22,24,26,27,29,30,31,31,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 5, 6, 8,10,11,13,15,16,18,20,21,23,25,27,28,30,31,31,31,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 5, 7, 8,10,12,14,15,17,19,21,22,24,26,28,29,31,31,31,31,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 5, 7, 9,10,12,14,16,18,19,21,23,25,27,29,30,31,31,31,31,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 5, 7, 9,11,13,15,16,18,20,22,24,26,28,30,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,
	//  0, 1, 3, 5, 7, 9,11,13,15,17,19,21,23,25,27,29,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31

	for (int j=0; j < 32; ++j) {
		for (int i=0; i < 32; ++i) {
			int val = i * j / 16;
			if (val > 31) val = 31;
			gpu_unai.LightLUT[(j*32) + i] = val;
		}
	}
}

// gcc5+ and clang13+ understarnd this on ARM
GPU_INLINE s32 clamp_c(s32 x) {
    if (x < 0) return 0;
    if (x > 31) return 31;
    return x;
}

////////////////////////////////////////////////////////////////////////////////
// Create packed Gouraud fixed-pt 8.8 rgb triplet
//
// INPUT:
// 'r','g','b' are 8.10 fixed-pt color components (r shown here)
//     'r' input:  --------------rrrrrrrrXXXXXXXXXX
//                 ^ bit 31
// RETURNS:
//    gcol_t output:  ccccccccXXXXXXXX for c in [r, g, b]
//                    ^ bit 16
// Where 'r,g,b' are integer bits of colors, 'X' fixed-pt, and '-' don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE gcol_t gpuPackGouraudCol(u32 r, u32 g, u32 b)
{
	return (gcol_t){{
		(u16)(r >> 2),
		(u16)(g >> 2),
		(u16)(b >> 2),
		0
	}};
}

////////////////////////////////////////////////////////////////////////////////
// Create packed increment for Gouraud fixed-pt 8.8 rgb triplet
//
// INPUT:
//  Sign-extended 8.10 fixed-pt r,g,b color increment values (only dr is shown)
//   'dr' input:  ssssssssssssssrrrrrrrrXXXXXXXXXX
//                ^ bit 31
// RETURNS:
//   gcol_t output:  ccccccccXXXXXXXX for c in [r, g, b]
//                   ^ bit 16
// Where 'r,g,b' are integer bits of colors, 'X' fixed-pt, and 's' sign bits
//
// NOTE: The correctness of this code/method has not been fully verified,
//       having been merely factored out from original code in
//       poly-drawing functions. Feel free to check/improve it -senquack
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE gcol_t gpuPackGouraudColInc(s32 dr, s32 dg, s32 db)
{
	return (gcol_t){{
		(u16)((dr >> 2) + (dr < 0)),
		(u16)((dg >> 2) + (dg < 0)),
		(u16)((db >> 2) + (db < 0)),
		0
	}};
}

////////////////////////////////////////////////////////////////////////////////
// Extract bgr555 color from Gouraud u32 fixed-pt 8.8 rgb triplet
//
// INPUT:
//  'gCol' input:  ccccccccXXXXXXXX for c in [r, g, b]
//                 ^ bit 16
// RETURNS:
//    u16 output:  0bbbbbgggggrrrrr
//                 ^ bit 16
// Where 'r,g,b' are integer bits of colors, 'X' fixed-pt, and '0' zero
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE uint_fast16_t gpuLightingRGB(gcol_t gCol)
{
	return (gCol.c.r >> 11) |
		((gCol.c.g >> 6) & 0x3e0) |
		((gCol.c.b >> 1) & 0x7c00);
}

GPU_INLINE uint_fast16_t gpuLightingRGBDither(gcol_t gCol, int_fast16_t dt)
{
	dt <<= 4;
	return  clamp_c(((s32)gCol.c.r + dt) >> 11) |
	       (clamp_c(((s32)gCol.c.g + dt) >> 11) << 5) |
	       (clamp_c(((s32)gCol.c.b + dt) >> 11) << 10);
}

////////////////////////////////////////////////////////////////////////////////
// Apply fast (low-precision) 5-bit lighting to bgr555 texture color:
//
// INPUT:
//        'r8','g8','b8' are unsigned 8-bit color values, value of 127
//          is midpoint that doesn't modify that component of texture
//        'uSrc' input:  mbbbbbgggggrrrrr
//                       ^ bit 16
// RETURNS:
//          u16 output:  mbbbbbgggggrrrrr
// Where 'X' are fixed-pt bits, 'm' is the MSB to preserve
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE uint_fast16_t gpuLightingTXTGeneric(uint_fast16_t uSrc, u32 bgr0888)
{
	// the compiler can move this out of the loop if it wants to
	uint_fast32_t b5 = (bgr0888 >> 19);
	uint_fast32_t g5 = (bgr0888 >> 11) & 0x1f;
	uint_fast32_t r5 = (bgr0888 >>  3) & 0x1f;

	return (gpu_unai.LightLUT[((uSrc&0x7C00)>>5) | b5] << 10) |
	       (gpu_unai.LightLUT[ (uSrc&0x03E0)     | g5] <<  5) |
	       (gpu_unai.LightLUT[((uSrc&0x001F)<<5) | r5]      ) |
	       (uSrc & 0x8000);
}


////////////////////////////////////////////////////////////////////////////////
// Apply fast (low-precision) 5-bit Gouraud lighting to bgr555 texture color:
//
// INPUT:
//  'gCol' is a Gouraud fixed-pt 8.8 rgb triplet
//        'gCol' input:  ccccccccXXXXXXXX for c in [r, g, b]
//                       ^ bit 16
//        'uSrc' input:  mbbbbbgggggrrrrr
//                       ^ bit 16
// RETURNS:
//          u16 output:  mbbbbbgggggrrrrr
// Where 'X' are fixed-pt bits, 'm' is the MSB to preserve
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE uint_fast16_t gpuLightingTXTGouraudGeneric(uint_fast16_t uSrc, gcol_t gCol)
{
	return (gpu_unai.LightLUT[((uSrc&0x7C00)>>5) | (gCol.c.b >> 11)] << 10) |
	       (gpu_unai.LightLUT[ (uSrc&0x03E0)     | (gCol.c.g >> 11)] << 5) |
	       (gpu_unai.LightLUT[((uSrc&0x001F)<<5) | (gCol.c.r >> 11)]) |
	       (uSrc & 0x8000);
}

////////////////////////////////////////////////////////////////////////////////
// Apply high-precision 8-bit lighting to bgr555 texture color,
//
// INPUT:
//        'r','g','b' are unsigned 8-bit color component values, value of
//          127 is midpoint that doesn't modify that component of texture
//
//         uSrc input: mbbbbbgggggrrrrr
//                     ^ bit 16
// RETURNS:
//        u16 output:  mbbbbbgggggrrrrr
// Where 'X' are fixed-pt bits, 'm' is the MSB to preserve
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE uint_fast16_t gpuLightingTXTDitherRGB(uint_fast16_t uSrc,
	uint_fast8_t r, uint_fast8_t g, uint_fast8_t b, int_fast16_t dv)
{
	uint_fast16_t rs = uSrc & 0x001F;
	uint_fast16_t gs = uSrc & 0x03E0;
	uint_fast16_t bs = uSrc & 0x7C00;
	s32 r3 = rs * r +  dv;
	s32 g3 = gs * g + (dv << 5);
	s32 b3 = bs * b + (dv << 10);
	return  clamp_c(r3 >> 7) |
	       (clamp_c(g3 >> 12) << 5) |
	       (clamp_c(b3 >> 17) << 10) |
	       (uSrc & 0x8000);
}

GPU_INLINE uint_fast16_t gpuLightingTXTDither(uint_fast16_t uSrc, u32 bgr0888, int_fast16_t dv)
{
	return gpuLightingTXTDitherRGB(uSrc, bgr0888 & 0xff,
			(bgr0888 >> 8) & 0xff, bgr0888 >> 16, dv);
}

GPU_INLINE uint_fast16_t gpuLightingTXTGouraudDither(uint_fast16_t uSrc, gcol_t gCol, int_fast8_t dv)
{
	return gpuLightingTXTDitherRGB(uSrc, gCol.c.r >> 8, gCol.c.g >> 8, gCol.c.b >> 8, dv);
}

#endif  //_OP_LIGHT_H_

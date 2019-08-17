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


////////////////////////////////////////////////////////////////////////////////
// Create packed Gouraud fixed-pt 8.3:8.3:8.2 rgb triplet
//
// INPUT:
// 'r','g','b' are 8.10 fixed-pt color components (r shown here)
//     'r' input:  --------------rrrrrrrrXXXXXXXXXX
//                 ^ bit 31
// RETURNS:
//    u32 output:  rrrrrrrrXXXggggggggXXXbbbbbbbbXX
//                 ^ bit 31
// Where 'r,g,b' are integer bits of colors, 'X' fixed-pt, and '-' don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u32 gpuPackGouraudCol(u32 r, u32 g, u32 b)
{
	return ((u32)(b>> 8)&(0x03ff    ))
	     | ((u32)(g<< 3)&(0x07ff<<10))
	     | ((u32)(r<<14)&(0x07ff<<21));
}


////////////////////////////////////////////////////////////////////////////////
// Create packed increment for Gouraud fixed-pt 8.3:8.3:8.2 rgb triplet
//
// INPUT:
//  Sign-extended 8.10 fixed-pt r,g,b color increment values (only dr is shown)
//   'dr' input:  ssssssssssssssrrrrrrrrXXXXXXXXXX
//                ^ bit 31
// RETURNS:
//   u32 output:  rrrrrrrrXXXggggggggXXXbbbbbbbbXX
//                ^ bit 31
// Where 'r,g,b' are integer bits of colors, 'X' fixed-pt, and 's' sign bits
//
// NOTE: The correctness of this code/method has not been fully verified,
//       having been merely factored out from original code in
//       poly-drawing functions. Feel free to check/improve it -senquack
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u32 gpuPackGouraudColInc(s32 dr, s32 dg, s32 db)
{
	u32 dr_tmp = (u32)(dr << 14)&(0xffffffff<<21);  if (dr < 0) dr_tmp += 1<<21;
	u32 dg_tmp = (u32)(dg <<  3)&(0xffffffff<<10);  if (dg < 0) dg_tmp += 1<<10;
	u32 db_tmp = (u32)(db >>  8)&(0xffffffff    );  if (db < 0) db_tmp += 1<< 0;
	return db_tmp + dg_tmp + dr_tmp;
}


////////////////////////////////////////////////////////////////////////////////
// Extract bgr555 color from Gouraud u32 fixed-pt 8.3:8.3:8.2 rgb triplet
//
// INPUT:
//  'gCol' input:  rrrrrrrrXXXggggggggXXXbbbbbbbbXX
//                 ^ bit 31
// RETURNS:
//    u16 output:  0bbbbbgggggrrrrr
//                 ^ bit 16
// Where 'r,g,b' are integer bits of colors, 'X' fixed-pt, and '0' zero
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u16 gpuLightingRGB(u32 gCol)
{
	return ((gCol<< 5)&0x7C00) |
	       ((gCol>>11)&0x03E0) |
	        (gCol>>27);
}


////////////////////////////////////////////////////////////////////////////////
// Convert packed Gouraud u32 fixed-pt 8.3:8.3:8.2 rgb triplet in 'gCol'
//  to padded u32 5.4:5.4:5.4 bgr fixed-pt triplet, suitable for use
//  with HQ 24-bit lighting/quantization.
//
// INPUT:
//       'gCol' input:  rrrrrrrrXXXggggggggXXXbbbbbbbbXX
//                      ^ bit 31
// RETURNS:
//         u32 output:  000bbbbbXXXX0gggggXXXX0rrrrrXXXX
//                      ^ bit 31
//  Where 'X' are fixed-pt bits, '0' zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u32 gpuLightingRGB24(u32 gCol)
{
	return ((gCol<<19) & (0x1FF<<20)) |
	       ((gCol>> 2) & (0x1FF<<10)) |
	        (gCol>>23);
}


////////////////////////////////////////////////////////////////////////////////
// Apply fast (low-precision) 5-bit lighting to bgr555 texture color:
//
// INPUT:
//        'r5','g5','b5' are unsigned 5-bit color values, value of 15
//          is midpoint that doesn't modify that component of texture
//        'uSrc' input:  -bbbbbgggggrrrrr
//                       ^ bit 16
// RETURNS:
//          u16 output:  0bbbbbgggggrrrrr
// Where 'X' are fixed-pt bits, '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u16 gpuLightingTXT(u16 uSrc, u8 r5, u8 g5, u8 b5)
{
	return (gpu_unai.LightLUT[((uSrc&0x7C00)>>5) | b5] << 10) |
	       (gpu_unai.LightLUT[ (uSrc&0x03E0)     | g5] <<  5) |
	       (gpu_unai.LightLUT[((uSrc&0x001F)<<5) | r5]      );
}


////////////////////////////////////////////////////////////////////////////////
// Apply fast (low-precision) 5-bit Gouraud lighting to bgr555 texture color:
//
// INPUT:
//  'gCol' is a packed Gouraud u32 fixed-pt 8.3:8.3:8.2 rgb triplet, value of
//     15.0 is midpoint that does not modify color of texture
//         gCol input :  rrrrrXXXXXXgggggXXXXXXbbbbbXXXXX
//                       ^ bit 31
//        'uSrc' input:  -bbbbbgggggrrrrr
//                       ^ bit 16
// RETURNS:
//          u16 output:  0bbbbbgggggrrrrr
// Where 'X' are fixed-pt bits, '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u16 gpuLightingTXTGouraud(u16 uSrc, u32 gCol)
{
	return (gpu_unai.LightLUT[((uSrc&0x7C00)>>5) | ((gCol>> 5)&0x1F)]<<10) |
	       (gpu_unai.LightLUT[ (uSrc&0x03E0)     | ((gCol>>16)&0x1F)]<< 5) |
	       (gpu_unai.LightLUT[((uSrc&0x001F)<<5) |  (gCol>>27)      ]    );
}


////////////////////////////////////////////////////////////////////////////////
// Apply high-precision 8-bit lighting to bgr555 texture color,
//  returning a padded u32 5.4:5.4:5.4 bgr fixed-pt triplet
//  suitable for use with HQ 24-bit lighting/quantization.
//
// INPUT:
//        'r8','g8','b8' are unsigned 8-bit color component values, value of
//          127 is midpoint that doesn't modify that component of texture
//
//         uSrc input: -bbbbbgggggrrrrr
//                     ^ bit 16
// RETURNS:
//         u32 output: 000bbbbbXXXX0gggggXXXX0rrrrrXXXX
//                     ^ bit 31
// Where 'X' are fixed-pt bits, '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u32 gpuLightingTXT24(u16 uSrc, u8 r8, u8 g8, u8 b8)
{
	u16 r1 = uSrc&0x001F;
	u16 g1 = uSrc&0x03E0;
	u16 b1 = uSrc&0x7C00;

	u16 r2 = r8;
	u16 g2 = g8;
	u16 b2 = b8;

	u32 r3 = r1 * r2; if (r3 & 0xFFFFF000) r3 = ~0xFFFFF000;
	u32 g3 = g1 * g2; if (g3 & 0xFFFE0000) g3 = ~0xFFFE0000;
	u32 b3 = b1 * b2; if (b3 & 0xFFC00000) b3 = ~0xFFC00000;

	return ((r3>> 3)    ) |
	       ((g3>> 8)<<10) |
	       ((b3>>13)<<20);
}


////////////////////////////////////////////////////////////////////////////////
// Apply high-precision 8-bit lighting to bgr555 texture color in 'uSrc',
//  returning a padded u32 5.4:5.4:5.4 bgr fixed-pt triplet
//  suitable for use with HQ 24-bit lighting/quantization.
//
// INPUT:
//       'uSrc' input: -bbbbbgggggrrrrr
//                     ^ bit 16
//       'gCol' input: rrrrrrrrXXXggggggggXXXbbbbbbbbXX
//                     ^ bit 31
// RETURNS:
//         u32 output: 000bbbbbXXXX0gggggXXXX0rrrrrXXXX
//                     ^ bit 31
// Where 'X' are fixed-pt bits, '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE u32 gpuLightingTXT24Gouraud(u16 uSrc, u32 gCol)
{
	u16 r1 = uSrc&0x001F;
	u16 g1 = uSrc&0x03E0;
	u16 b1 = uSrc&0x7C00;

	u16 r2 = (gCol>>24) & 0xFF;
	u16 g2 = (gCol>>13) & 0xFF;
	u16 b2 = (gCol>> 2) & 0xFF;

	u32 r3 = r1 * r2; if (r3 & 0xFFFFF000) r3 = ~0xFFFFF000;
	u32 g3 = g1 * g2; if (g3 & 0xFFFE0000) g3 = ~0xFFFE0000;
	u32 b3 = b1 * b2; if (b3 & 0xFFC00000) b3 = ~0xFFC00000;

	return ((r3>> 3)    ) |
	       ((g3>> 8)<<10) |
	       ((b3>>13)<<20);
}

#endif  //_OP_LIGHT_H_

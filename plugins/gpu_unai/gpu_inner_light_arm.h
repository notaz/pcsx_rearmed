#ifndef _OP_LIGHT_ARM_H_
#define _OP_LIGHT_ARM_H_

#include "arm_features.h"

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
// note: outdated, unused
GPU_INLINE uint_fast16_t gpuLightingRGBARM(u32 gCol)
{
	uint_fast16_t out = 0x03E0; // don't need the mask after starting to write output
  	u32 tmp;
  
	asm ("and %[tmp], %[gCol], %[out]\n\t"              // tmp holds 0x000000bbbbb00000
	     "and %[out], %[out],  %[gCol], lsr #0x0B\n\t"  // out holds 0x000000ggggg00000
	     "orr %[tmp], %[out],  %[tmp],  lsl #0x05\n\t"  // tmp holds 0x0bbbbbggggg00000
	     "orr %[out], %[tmp],  %[gCol], lsr #0x1B\n\t"  // out holds 0x0bbbbbgggggrrrrr
	     : [out] "+&r" (out), [tmp] "=&r" (tmp)
	     : [gCol] "r"  (gCol)
	     );

	return out;
}

//#ifdef HAVE_ARMV5E // todo?
#ifdef HAVE_ARMV6

////////////////////////////////////////////////////////////////////////////////
// Apply 8-bit lighting to bgr555 texture color:
//
// INPUT:
//	  'r8','g8','b8' are unsigned 8-bit color values, value of 127
//	    is midpoint that doesn't modify that component of texture
//	  'uSrc' input:	 mbbbbbgggggrrrrr
//			 ^ bit 16
// RETURNS:
//	    u16 output:	 mbbbbbgggggrrrrr
// Where 'X' are fixed-pt bits.
////////////////////////////////////////////////////////////////////////////////
// on v6 we have single-cycle mul and sat which is better than the LightLUT
GPU_INLINE u32 gpuLightingTXTARM(u32 uSrc, u32 bgr0888)
{
	int_fast32_t r, g, b, s_d = uSrc;
	// has to be in a block, otherwise gcc schedules the insns poorly
	asm("and    %[r],  %[s_d], #0x001f\n"
	    "and    %[b],  %[bgr], #0xff\n"
	    "smulbb %[r],  %[r],   %[b]\n"
	    "uxtb   %[b],  %[bgr], ror #8\n"
	    "and    %[g],  %[s_d], #0x03e0\n"
	    "smulbb %[g],  %[g],   %[b]\n"
	    "and    %[b],  %[s_d], #0x7c00\n"
	    "and    %[s_d],%[s_d], #0x8000\n"
	    "smulbt %[b],  %[b],   %[bgr]\n"
	    "usat   %[r],  #5, %[r], asr #7\n"
	    "usat   %[g],  #5, %[g], asr #12\n"
	    "usat   %[b],  #5, %[b], asr #17\n"
	    "orr    %[s_d],%[s_d], %[r]\n"
	    "orr    %[s_d],%[s_d], %[g], lsl #5\n"
	    "orr    %[s_d],%[s_d], %[b], lsl #10\n"
	  : [s_d]"+r"(s_d), [r]"=&r"(r), [g]"=&r"(g), [b]"=&r"(b)
	  : [bgr]"r"(bgr0888));
	return s_d;
}
#define gpuLightingTXT gpuLightingTXTARM

GPU_INLINE u32 gpuLightingTXTGouraudARM(u32 uSrc, gcol_t gCol)
{
	u32 r, g, s_d = uSrc;
	asm("str    %[b],   [sp, #-4]!\n"        // conserve regs for gcc
	    "uxtb16 %[b],   %[b],    ror #8\n"   // b = g_rg >> 8 & 0xff00ff
	    "and    %[r],   %[s_d],  #0x001f\n"
	    "and    %[g],   %[s_d],  #0x03e0\n"
	    "smulbb %[r],   %[r],    %[b]\n"
	    "smulbt %[g],   %[g],    %[b]\n"
	    "uxtb   %[b],   %[g_b],  ror #8\n"
	    "tst    %[s_d],          #0x8000\n"
	    "and    %[s_d], %[s_d],  #0x7c00\n"
	    "smulbb %[b],   %[b],    %[s_d]\n"
	    "usat   %[s_d],#5, %[r], asr #7\n"
	    "usat   %[g],  #5, %[g], asr #12\n"
	    "usat   %[b],  #5, %[b], asr #17\n"
	    "orrne  %[s_d], %[s_d],  #0x8000\n"
	    "orr    %[s_d], %[s_d],  %[g], lsl #5\n"
	    "orr    %[s_d], %[s_d],  %[b], lsl #10\n"
	    "ldr    %[b],   [sp], #4\n"
	  : [s_d]"+r"(s_d), [r]"=&r"(r), [g]"=&r"(g)
	  : [b]"r"(gCol.raw32[0]), [g_b]"r"(gCol.raw32[1])
	  : "cc");
	return s_d;
}
#define gpuLightingTXTGouraud gpuLightingTXTGouraudARM

#endif // HAVE_ARMV6

#endif  //_OP_LIGHT_ARM_H_

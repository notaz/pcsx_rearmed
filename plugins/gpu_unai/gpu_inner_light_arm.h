#ifndef _OP_LIGHT_ARM_H_
#define _OP_LIGHT_ARM_H_

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
GPU_INLINE u16 gpuLightingRGBARM(u32 gCol)
{
  u16 out = 0x03E0; // don't need the mask after starting to write output
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


GPU_INLINE u16 gpuLightingTXTARM(u16 uSrc, u8 r5, u8 g5, u8 b5)
{
  u16 out = 0x03E0;
  u32 db, dg;
  asm ("and    %[dg],  %[out],  %[src]  \n\t"
       "orr    %[dg],  %[dg],   %[g5]   \n\t"
       "and    %[db],  %[out],  %[src], lsr #0x05 \n\t"
       "ldrb   %[dg],  [%[lut], %[dg]]  \n\t" 
       "and    %[out], %[out],  %[src], lsl #0x05 \n\t"
       "orr    %[out], %[out],  %[r5]   \n\t"
       "orr    %[db],  %[db],   %[b5]   \n\t"
       "ldrb   %[out], [%[lut], %[out]] \n\t"
       "ldrb   %[db],  [%[lut], %[db]]  \n\t"
       "orr    %[out], %[out],  %[dg],  lsl #0x05   \n\t"
       "orr    %[out], %[out],  %[db],  lsl #0x0A   \n\t"
    : [out] "=&r" (out), [db] "=&r" (db), [dg] "=&r" (dg)
    : [r5] "r" (r5), [g5] "r" (g5),  [b5] "r" (b5),
      [lut] "r" (gpu_unai.LightLUT), [src] "r" (uSrc), "0" (out)
    : "cc");
  return out;
}

GPU_INLINE u16 gpuLightingTXTGouraudARM(u16 uSrc, u32 gCol)
{
  u16 out = 0x03E0; // don't need the mask after starting to write output
  u32 db,dg,gtmp;
  asm ("and    %[dg],  %[out],  %[src]   \n\t"
       "and    %[gtmp],%[out],  %[gCol], lsr #0x0B \n\t"
       "and    %[db],  %[out],  %[src],  lsr #0x05 \n\t"
       "orr    %[dg],  %[dg],   %[gtmp], lsr #0x05 \n\t"
       "and    %[gtmp],%[out],  %[gCol]  \n\t"
       "ldrb   %[dg],  [%[lut], %[dg]]   \n\t"
       "and    %[out], %[out],  %[src],  lsl #0x05 \n\t"
       "orr    %[out], %[out],  %[gCol], lsr #0x1B \n\t"
       "orr    %[db],  %[db],   %[gtmp], lsr #0x05 \n\t"
       "ldrb   %[out], [%[lut], %[out]]  \n\t"
       "ldrb   %[db],  [%[lut], %[db]]   \n\t"
       "orr    %[out], %[out],  %[dg],   lsl #0x05 \n\t"
       "orr    %[out], %[out],  %[db],   lsl #0x0A \n\t"
       : [out] "=&r" (out), [db] "=&r" (db), [dg] "=&r" (dg),
         [gtmp] "=&r" (gtmp) \
       : [gCol] "r" (gCol), [lut] "r" (gpu_unai.LightLUT), "0" (out), [src] "r" (uSrc)
       : "cc");

  return out;
}

#endif  //_OP_LIGHT_ARM_H_

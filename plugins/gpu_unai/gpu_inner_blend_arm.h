#ifndef _OP_BLEND_ARM_H_
#define _OP_BLEND_ARM_H_

////////////////////////////////////////////////////////////////////////////////
// Blend bgr555 color in 'uSrc' (foreground) with bgr555 color
//  in 'uDst' (background), returning resulting color.
//
// INPUT:
//  'uSrc','uDst' input: -bbbbbgggggrrrrr
//                       ^ bit 16
// OUTPUT:
//           u16 output: 0bbbbbgggggrrrrr
//                       ^ bit 16
// RETURNS:
// Where '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
template <int BLENDMODE, bool SKIP_USRC_MSB_MASK>
GPU_INLINE u16 gpuBlendingARM(u16 uSrc, u16 uDst)
{
	// These use Blargg's bitwise modulo-clamping:
	//  http://blargg.8bitalley.com/info/rgb_mixing.html
	//  http://blargg.8bitalley.com/info/rgb_clamped_add.html
	//  http://blargg.8bitalley.com/info/rgb_clamped_sub.html

  
	u16 mix;

  asm ("bic %[uDst], %[uDst], #0x8000" : [uDst] "+r" (uDst));

  if (BLENDMODE == 3) {
    asm ("and %[uSrc], %[mask], %[uSrc], lsr #0x2" : [uSrc] "+r" (uSrc) : [mask] "r" (0x1ce7));
  } else if (!SKIP_USRC_MSB_MASK) {
    asm ("bic %[uSrc], %[uSrc], #0x8000" : [uSrc] "+r" (uSrc));
  }

  
	// 0.5 x Back + 0.5 x Forward
	if (BLENDMODE==0) {
    // mix = ((uSrc + uDst) - ((uSrc ^ uDst) & 0x0421)) >> 1;
    asm ("eor %[mix], %[uSrc], %[uDst]\n\t"
         "and %[mix], %[mix], %[mask]\n\t"
         "sub %[mix], %[uDst], %[mix]\n\t"
         "add %[mix], %[uSrc], %[mix]\n\t"
         "mov %[mix], %[mix], lsr #0x1\n\t"
         : [mix] "=&r" (mix)
         : [uSrc] "r" (uSrc), [uDst] "r" (uDst), [mask] "r" (0x0421));
  }

  if (BLENDMODE == 1 || BLENDMODE == 3) {
    // u32 sum      = uSrc + uDst;
		// u32 low_bits = (uSrc ^ uDst) & 0x0421;
		// u32 carries  = (sum - low_bits) & 0x8420;
		// u32 modulo   = sum - carries;
		// u32 clamp    = carries - (carries >> 5);
		// mix = modulo | clamp;

    u32 sum;

    asm ("add %[sum], %[uSrc], %[uDst]\n\t"
         "eor %[mix], %[uSrc], %[uDst]\n\t"
         "and %[mix], %[mix], %[mask]\n\t"
         "sub %[mix], %[sum], %[mix]\n\t"
         "and %[mix], %[mix], %[mask], lsl #0x05\n\t"
         "sub %[sum], %[sum], %[mix] \n\t"
         "sub %[mix], %[mix], %[mix], lsr #0x05\n\t"
         "orr %[mix], %[sum], %[mix]"
         : [sum] "=&r" (sum), [mix] "=&r" (mix)
         : [uSrc] "r" (uSrc), [uDst] "r" (uDst), [mask] "r" (0x0421));
  }
    
	// 1.0 x Back - 1.0 x Forward
	if (BLENDMODE==2) {
    u32 diff;
    // u32 diff     = uDst - uSrc + 0x8420;
		// u32 low_bits = (uDst ^ uSrc) & 0x8420;
		// u32 borrows  = (diff - low_bits) & 0x8420;
		// u32 modulo   = diff - borrows;
		// u32 clamp    = borrows - (borrows >> 5);
		// mix = modulo & clamp;
    asm ("sub %[diff], %[uDst], %[uSrc]\n\t"
         "add %[diff], %[diff], %[mask]\n\t"
         "eor %[mix], %[uDst], %[uSrc]\n\t"
         "and %[mix], %[mix], %[mask]\n\t"
         "sub %[mix], %[diff], %[mix]\n\t"
         "and %[mix], %[mix], %[mask]\n\t"
         "sub %[diff], %[diff], %[mix]\n\t"
         "sub %[mix], %[mix], %[mix], lsr #0x05\n\t"
         "and %[mix], %[diff], %[mix]"
         : [diff] "=&r" (diff), [mix] "=&r" (mix)
         : [uSrc] "r" (uSrc), [uDst] "r" (uDst), [mask] "r" (0x8420));
	}
  
	return mix;
}

#endif  //_OP_BLEND_ARM_H_

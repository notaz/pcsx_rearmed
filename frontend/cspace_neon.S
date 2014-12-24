/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"

/* sanity check */
#ifndef __ARM_NEON__
#error Compiling NEON code, but appropriate preprocessor flag is missing
#error This usually means -mfpu=neon or -mfloat-abi= is not correctly specified
#endif

.text
.align 2

FUNCTION(bgr555_to_rgb565): @ dst, src, bytes
    pld         [r1]
    mov         r3, #0x07c0
    vdup.16     q15, r3
    subs        r2, r2, #64
    blt         btr16_end64
0:
    pld         [r1, #64*2]
    vldmia      r1!, {q0-q3}
    vshl.u16    q8,  q0, #11
    vshl.u16    q9,  q1, #11
    vshl.u16    q10, q2, #11
    vshl.u16    q11, q3, #11
    vsri.u16    q8,  q0, #10
    vsri.u16    q9,  q1, #10
    vsri.u16    q10, q2, #10
    vsri.u16    q11, q3, #10
    vshl.u16    q0,  q0, #1
    vshl.u16    q1,  q1, #1
    vshl.u16    q2,  q2, #1
    vshl.u16    q3,  q3, #1
    vbit        q8,  q0, q15
    vbit        q9,  q1, q15
    vbit        q10, q2, q15
    vbit        q11, q3, q15
    vstmia      r0!, {q8-q11}
    subs        r2, r2, #64
    bge         0b

btr16_end64:
    adds        r2, r2, #64
    bxeq        lr
    subs        r2, r2, #16
    blt         btr16_end16

    @ handle the remainder (reasonably rare)
0:
    vld1.16     {q0}, [r1]!
    vshl.u16    q1, q0, #11
    vshl.u16    q2, q0, #1
    vsri.u16    q1, q0, #10
    vbit        q1, q2, q15
    subs        r2, r2, #16
    vst1.16     {q1}, [r0]!
    bge         0b

btr16_end16:
    adds        r2, r2, #16
    bxeq        lr
    subs        r2, r2, #8
    bxlt        lr

    @ very rare
    vld1.16     d0, [r1]!
    vshl.u16    d1, d0, #11
    vshl.u16    d2, d0, #1
    vsri.u16    d1, d0, #10
    vbit        d1, d2, d30
    vst1.16     d1, [r0]!
    bx          lr


FUNCTION(bgr888_to_rgb888): @ dst, src, bytes
    pld         [r1]
    @ r2 /= 48
    mov         r2, r2, lsr #4
    movw        r3, #0x5556
    movt        r3, #0x5555
    umull       r12,r2, r3, r2
0:
    pld         [r1, #48*3]
    vld3.8      {d0-d2}, [r1, :64]!
    vld3.8      {d3-d5}, [r1, :64]!
    vswp        d0, d2
    vswp        d3, d5
    vst3.8      {d0-d2}, [r0, :64]!
    vst3.8      {d3-d5}, [r0, :64]!
    subs        r2, r2, #1
    bne         0b

    bx          lr


FUNCTION(bgr888_to_rgb565): @ dst, src, bytes
    pld         [r1]
    @ r2 /= 48
    mov         r2, r2, lsr #4
    movw        r3, #0x5556
    movt        r3, #0x5555
    umull       r12,r2, r3, r2

    mov         r3, #0x07e0
    vdup.16     q15, r3
0:
    pld         [r1, #48*3]
    vld3.8      {d1-d3}, [r1, :64]!
    vld3.8      {d5-d7}, [r1, :64]!

    vshll.u8    q8, d2, #3      @ g
    vshll.u8    q9, d6, #3
    vshr.u8     d0, d3, #3      @ b
    vshr.u8     d4, d7, #3
    vzip.8      d0, d1          @ rb
    vzip.8      d4, d5
    vbit        q0, q8, q15
    vbit        q2, q9, q15

    vstmia      r0!, {d0,d1}
    vstmia      r0!, {d4,d5}
    subs        r2, r2, #1
    bne         0b

    bx          lr


FUNCTION(rgb888_to_rgb565): @ dst, src, bytes
    pld         [r1]
    @ r2 /= 48
    mov         r2, r2, lsr #4
    movw        r3, #0x5556
    movt        r3, #0x5555
    umull       r12,r2, r3, r2

    mov         r3, #0x07e0
    vdup.16     q15, r3
0:
    pld         [r1, #48*3]
    vld3.8      {d1-d3}, [r1, :64]!
    vld3.8      {d5-d7}, [r1, :64]!

    vshll.u8    q8, d2, #3      @ g
    vshll.u8    q9, d6, #3
    vshr.u8     d2, d1, #3      @ b
    vshr.u8     d6, d5, #3
    vzip.8      d2, d3          @ rb
    vzip.8      d6, d7
    vbit        q1, q8, q15
    vbit        q3, q9, q15

    vstmia      r0!, {d2,d3}
    vstmia      r0!, {d6,d7}
    subs        r2, r2, #1
    bne         0b

    bx          lr


@ vim:filetype=armasm

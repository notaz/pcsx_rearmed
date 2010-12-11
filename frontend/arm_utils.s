/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

.text
.align 2

.global bgr555_to_rgb565
bgr555_to_rgb565:
    mov         r3, #0x07c0
    vdup.16     q15, r3
    sub         r2, r2, #64
0:
    vldmia      r1!, {q0-q3}
    vshl.u16    q4, q0, #11
    vshl.u16    q5, q1, #11
    vshl.u16    q6, q2, #11
    vshl.u16    q7, q3, #11
    vsri.u16    q4, q0, #10
    vsri.u16    q5, q1, #10
    vsri.u16    q6, q2, #10
    vsri.u16    q7, q3, #10
    vshl.u16    q0, q0, #1
    vshl.u16    q1, q1, #1
    vshl.u16    q2, q2, #1
    vshl.u16    q3, q3, #1
    vbit        q4, q0, q15
    vbit        q5, q1, q15
    vbit        q6, q2, q15
    vbit        q7, q3, q15
    vstmia      r0!, {q4-q7}
    subs        r2, r2, #64
    bge         0b

    adds        r2, r2, #64
    bxeq        lr

    @ handle the remainder
0:
    vld1.16     {q0}, [r1, :64]!
    vshl.u16    q1, q0, #11
    vshl.u16    q2, q0, #1
    vsri.u16    q1, q0, #10
    vbit        q1, q2, q15
    subs        r2, r2, #16
    vst1.16     {q1}, [r0, :64]!
    bgt         0b

    bx          lr


.global bgr888_to_rgb888
bgr888_to_rgb888:
    @ r2 /= 48
    mov         r2, r2, lsr #4
    movw        r3, #0x5556
    movt        r3, #0x5555
    umull       r12,r2, r3, r2
0:
    vld3.8      {d0-d2}, [r1, :64]!
    vld3.8      {d3-d5}, [r1, :64]!
    vswp        d0, d2
    vswp        d3, d5
    vst3.8      {d0-d2}, [r0, :64]!
    vst3.8      {d3-d5}, [r0, :64]!
    subs        r2, r2, #1
    bne         0b

    bx          lr


@ vim:filetype=armasm

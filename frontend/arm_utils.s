/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

.text
.align 2

.global bgr555_to_rgb565
bgr555_to_rgb565:
    mov         r3, #0x03e0
    vdup.16     q15, r3
    mov         r2, r2, lsr #6
0:
    vldmia      r1!, {q0-q3}
    vshr.u16    q4, q0, #10
    vshr.u16    q5, q1, #10
    vshr.u16    q6, q2, #10
    vshr.u16    q7, q3, #10
    vshl.u16    q8, q0, #11
    vshl.u16    q9, q1, #11
    vshl.u16    q10, q2, #11
    vshl.u16    q11, q3, #11
    vand        q0, q0, q15
    vand        q1, q1, q15
    vand        q2, q2, q15
    vand        q3, q3, q15
    vshl.u16    q0, q0, #1
    vshl.u16    q1, q1, #1
    vshl.u16    q2, q2, #1
    vshl.u16    q3, q3, #1
    vorr        q0, q0, q4
    vorr        q1, q1, q5
    vorr        q2, q2, q6
    vorr        q3, q3, q7
    vorr        q0, q0, q8
    vorr        q1, q1, q9
    vorr        q2, q2, q10
    vorr        q3, q3, q11
    vstmia      r0!, {q0-q3}
    subs        r2, r2, #1
    bne         0b

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

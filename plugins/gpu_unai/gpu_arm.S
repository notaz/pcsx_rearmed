/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of  GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"

.text
.align 2

@ in: r0=dst, r2=pal, r12=0x1e
@ trashes r6-r8,lr,flags
.macro do_4_pixels rs ibase obase
.if \ibase - 1 < 0
    and     r6, r12, \rs, lsl #1
.else
    and     r6, r12, \rs, lsr #\ibase-1
.endif
    and     r7, r12, \rs, lsr #\ibase+3
    and     r8, r12, \rs, lsr #\ibase+7
    and     lr, r12, \rs, lsr #\ibase+11
    ldrh    r6, [r2, r6]
    ldrh    r7, [r2, r7]
    ldrh    r8, [r2, r8]
    ldrh    lr, [r2, lr]
    tst     r6, r6
    strneh  r6, [r0, #\obase+0]
    tst     r7, r7
    strneh  r7, [r0, #\obase+2]
    tst     r8, r8
    strneh  r8, [r0, #\obase+4]
    tst     lr, lr
    strneh  lr, [r0, #\obase+6]
.endm

.global draw_spr16_full @ (u16 *d, void *s, u16 *pal, int lines)
draw_spr16_full:
    stmfd   sp!, {r4-r8,lr}
    mov     r12, #0x1e             @ empty pixel

0:
    ldmia   r1, {r4,r5}
    do_4_pixels r4, 0,  0
    do_4_pixels r4, 16, 8
    do_4_pixels r5, 0,  16
    do_4_pixels r5, 16, 24
    subs    r3, r3, #1
    add     r0, r0, #2048
    add     r1, r1, #2048
    bgt     0b

    ldmfd   sp!, {r4-r8,pc}

@ vim:filetype=armasm

/*
 * (C) Gražvydas "notaz" Ignotas, 2013
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"

.text
.align 2

@ lr=0x001f001f
@ trashes r11, r12
.macro bgr555_to_rgb565_one rn
    and      r11, lr, \rn
    and      r12, lr, \rn, lsr #5
    and      \rn, lr, \rn, lsr #10
    orr      r12, r11, lsl #5
    orr      \rn, r12, lsl #6
.endm

.macro pld_ reg offs=#0
#ifdef HAVE_ARMV6
    pld      [\reg, \offs]
#endif
.endm

FUNCTION(bgr555_to_rgb565): @ void *dst, const void *src, int bytes
    pld_     r1
    push     {r4-r11,lr}
    mov      lr, #0x001f
    subs     r2, #4*8
    orr      lr, lr, lsl #16
    blt      1f

    @ src can be unaligned, but that's very rare, so just force it.
    @ The manual says unaligned ldm should fault, and it does on
    @ cortex-a78's 32bit mode, but curiously on cortex-a8 it just
    @ works and loads the data correctly.
    bic      r1, r1, #3

0:
    ldmia    r1!, {r3-r10}
    subs     r2, #4*8
    bgr555_to_rgb565_one r3

    pld_     r1, #32*2
    bgr555_to_rgb565_one r4
    bgr555_to_rgb565_one r5
    bgr555_to_rgb565_one r6
    bgr555_to_rgb565_one r7
    bgr555_to_rgb565_one r8
    bgr555_to_rgb565_one r9
    bgr555_to_rgb565_one r10
    stmia    r0!, {r3-r10}
    bge      0b

1:
    adds     r2, #4*8
    popeq    {r4-r11,pc}

2:
    ldr      r3, [r1], #4
    subs     r2, #4
    bgr555_to_rgb565_one r3
    str      r3, [r0], #4
    bgt      2b

    pop      {r4-r11,pc}

/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

/* .equiv HAVE_ARMV7, 1 */

.text
.align 2

.macro sgnxt16 reg
.if HAVE_ARMV7
    sxth        \reg, \reg
.else
    lsl         \reg, \reg, #16
    asr         \reg, \reg, #16
.endif
.endm


.global gteNCLIP_arm @ r0=CP2 (d,c),
gteNCLIP_arm:
    push        {r4-r6,lr}

    add         r1, r0, #4*12
    ldmia       r1, {r1-r3}
    mov         r4, r1, asr #16
    mov         r5, r2, asr #16
    mov         r6, r3, asr #16
    sub         r12, r4, r5       @ 3: gteSY0 - gteSY1
    sub         r5, r5, r6        @ 1: gteSY1 - gteSY2
    sgnxt16     r1
    smull       r1, r5, r1, r5    @ RdLo, RdHi
    sub         r6, r4            @ 2: gteSY2 - gteSY0
    sgnxt16     r2
    smlal       r1, r5, r2, r6
    mov         lr, #0            @ gteFLAG
    sgnxt16     r3
    smlal       r1, r5, r3, r12
    mov         r6, #1<<31
    orr         r6, #1<<15
    movs        r2, r1, lsl #1
    adc         r5, r5
    cmp         r5, #0
.if HAVE_ARMV7
    movtgt      lr, #((1<<31)|(1<<16))>>16
.else
    movgt       lr, #(1<<31)
    orrgt       lr, #(1<<16)
.endif
    mvngt       r1, #1<<31        @ maxint
    cmn         r5, #1
    movmi       r1, #1<<31        @ minint
    orrmi       lr, r6
    str         r1, [r0, #4*24]
    str         lr, [r0, #4*(32+31)] @ gteFLAG

    pop         {r4-r6,pc}
    .size	gteNCLIP_arm, .-gteNCLIP_arm


@ vim:filetype=armasm


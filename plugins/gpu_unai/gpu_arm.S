/*
 * (C) Gražvydas "notaz" Ignotas, 2011,2024
 *
 * This work is licensed under the terms of  GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"

.text
.align 2

.macro pld_ reg offs=#0
#ifdef HAVE_ARMV6
    pld      [\reg, \offs]
#endif
.endm

@ in: r0=dst, r2=pal, r12=0x1e
@ trashes r6-r8,lr,flags
.macro do_4x_4bpp rs ibase obase
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

@ in: r0=dst, r2=pal, r12=0x1fe
@ loads/stores \rs,r6-r8
.macro do_4x_8bpp rs
    and      r6, r12, \rs, lsl #1
    and      r7, r12, \rs, lsr #7
    and      r8, r12, \rs, lsr #15
    and      \rs,r12, \rs, lsr #23
    ldrh     r6, [r2, r6]
    ldrh     r7, [r2, r7]
    ldrh     r8, [r2, r8]
    ldrh     \rs,[r2, \rs]
    tst      r6, r6
    strneh   r6, [r0, #0]
    tst      r7, r7
    strneh   r7, [r0, #2]
    tst      r8, r8
    strneh   r8, [r0, #4]
    tst      \rs,\rs
    strneh   \rs,[r0, #6]
.endm

.global sprite_4bpp_x16_asm @ (u16 *d, void *s, u16 *pal, int lines)
sprite_4bpp_x16_asm_:
    ldr     r2, [r3]               @ pal
    ldr     r3, [r3, #0x1c]        @ lines
sprite_4bpp_x16_asm:
    .cfi_startproc
    stmfd   sp!, {r4-r8,lr}
    .cfi_def_cfa_offset 4*6
    .cfi_rel_offset lr, 4*5
    mov     r12, #0x1e             @ empty pixel

0:
    ldmia   r1, {r4,r5}
    pld_    r1, #2048
    do_4x_4bpp r4, 0,  0
    do_4x_4bpp r4, 16, 8
    do_4x_4bpp r5, 0,  16
    do_4x_4bpp r5, 16, 24
    subs    r3, r3, #1
    add     r0, r0, #2048
    add     r1, r1, #2048
    bgt     0b

    ldmfd   sp!, {r4-r8,pc}
    .cfi_endproc


@
.macro sprite_driver_part1 is8bpp
    stmfd   sp!, {r4-r11,lr}
    .cfi_def_cfa_offset 4*9
    .cfi_rel_offset lr, 4*8
    mov     r12, #0x01e
.if \is8bpp
    orr     r12, r12, #0x1f0   @ mask=0x01fe
.endif
    ldr     r4, [r3, #4]       @ u0
    ldr     r5, [r3, #0x1c]    @ h
    and     r4, r4, #((8 >> \is8bpp) - 1)
    sub     r5, r5, #1
    orr     r5, r4, r5, lsl #8 @ ((h-1) << 8) | u0_fraction
    mov     r9, r2             @ saved_w
    mov     r10, r0            @ saved_dst
    mov     r11, r1            @ saved_src
    ldr     r2, [r3]           @ pal
11: @ line_loop:
    pld_    r11, #2048
    mov     r0, r10
    mov     r1, r11
    mov     r3, r9
    ands    r6, r5, #(7 >> \is8bpp)
    bne     15f @ fractional_u
12:
    subs    r3, r3, #(8 >> \is8bpp) @ w
    bmi     14f @ fractional_w
.endm
.macro sprite_driver_part2 is8bpp
    cmn     r3, #(8 >> \is8bpp)
    bne     14f @ fractional_w
13: @ eol:
    add     r10, r10, #2048
    add     r11, r11, #2048
    subs    r5, r5, #0x100
    bpl     11b @ line_loop
    ldmfd   sp!, {r4-r11,pc}
14: @ fractional_w:
    ldr     r4, [r1], #4    
    add     r8, r3, #(8 >> \is8bpp)
    mov     r3, #0
    mov     r4, r4, lsl #1
    b       16f @ fractional_loop
15: @ fractional_u:
    bic     r1, r1, #3
    rsb     r8, r6, #(8 >> \is8bpp)
    ldr     r4, [r1], #4    
    cmp     r8, r3
    movgt   r8, r3
    mov     r7, r6, lsl #(2 + \is8bpp)
    sub     r3, r3, r8
    sub     r7, r7, #1
    mov     r4, r4, lsr r7
16: @ fractional_loop:
.endm
.macro sprite_driver_part3
    tst     r3, r3
    beq     13b @ sprd4_eol
    b       12b @ return from fractional_u
.endm

.global sprite_driver_4bpp_asm @ (u16 *d, const void *s, int width, spriteDriverArg)
sprite_driver_4bpp_asm:
    .cfi_startproc
    ldr     r12, [r3, #4]      @ u0
    mov     r12, r12, lsl #29
    orr     r12, r12, r2       @ w
    cmp     r12, #16
    beq     sprite_4bpp_x16_asm_ @ use specialized aligned x16 version
    sprite_driver_part1 0
0:
    ldr     r4, [r1], #4
    pld_    r1, #28
    do_4x_4bpp r4, 0,  0
    do_4x_4bpp r4, 16, 8
    add     r0, r0, #16
    subs    r3, r3, #8
    bpl     0b
    sprite_driver_part2 0
0:
    and     r7, r12, r4
    mov     r4, r4, lsr #4
    ldrh    r7, [r2, r7]
    add     r0, r0, #2
    tst     r7, r7
    strneh  r7, [r0, #-2]
    subs    r8, r8, #1
    bgt     0b
    sprite_driver_part3
    .cfi_endproc


.global sprite_driver_8bpp_asm @ (u16 *d, const void *s, int width, spriteDriverArg)
sprite_driver_8bpp_asm:
    .cfi_startproc
    sprite_driver_part1 1
0:
    ldr     r4, [r1], #4
    pld_    r1, #28
    do_4x_8bpp r4
    add     r0, r0, #8
    subs    r3, r3, #4
    bpl     0b
    sprite_driver_part2 1
0:
    and     r7, r12, r4
    mov     r4, r4, lsr #8
    ldrh    r7, [r2, r7]
    add     r0, r0, #2
    tst     r7, r7
    strneh  r7, [r0, #-2]
    subs    r8, r8, #1
    bgt     0b
    sprite_driver_part3
    .cfi_endproc


@ vim:filetype=armasm

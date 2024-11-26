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


.macro poly_4bpp_init v_target need_rgb
    add     r12, r1, #4
    ldmia   r12, {r3, r4, r7, r12, lr} @ clut, u, v, u_msk, v_msk
    ldr     r5, [r1, #0x18]    @ u_inc
.if \need_rgb
    ldr     r10,[r1, #0x24]    @ rbg
.endif
    mov     r6, r12
    ldr     r12,[r1, #0x1c]    @ v_inc
.if \need_rgb
    mov     r10,r10,lsl #7     @ 0bbb bbbb 0ggg gggg 0rrr rrrr r000 0000
    bic     r10,r10,#1<<23
    bic     r10,r10,#1<<15
    mov     r11,r10,lsl #8
.endif
    and     r4, r4, r6
    and     lr, lr, r7         @ v_msk & v
    and     lr, lr, #0xff<<10
    tst     r12,r12
    bne     \v_target
    ldr     r1, [r1]           @ src
    mov     r7, r4, lsr #13
    add     r1, r1, lr, lsl #1
    add     r12,r1, r7, lsl #2
    pld_    r12,#2048
.endm

.global poly_4bpp_asm @ (void *d, const struct gpu_unai_inner_t *inn, int count)
poly_4bpp_asm:
    .cfi_startproc
    stmfd   sp!, {r4-r7,lr}
    .cfi_def_cfa_offset 4*5
    .cfi_rel_offset lr, 4*4
    poly_4bpp_init poly_4bpp_v_asm 0
0:
    ldr     lr, [r1, r7, lsl #2]
    lsr     r12,r4, #8
    and     r12,r12,#0x1c
    sub     r12,r12,#1
    mov     r12,lr, ror r12
    add     r4, r4, r5
    and     r12,r12,#0x1e
    and     r4, r4, r6
    ldrh    r12,[r3, r12]
    add     r0, r0, #2
    mov     r7, r4, lsr #13
    tst     r12,r12
    strneh  r12,[r0, #-2]
    subs    r2, r2, #1
    bgt     0b

    ldmfd   sp!, {r4-r7,pc}

poly_4bpp_v_asm: @ r3=clut, r4=u, r5=u_inc, r6=u_msk, r7=v, lr=v_masked
    stmfd   sp!, {r8-r9}
    .cfi_def_cfa_offset 4*7
    .cfi_rel_offset lr, 4*6
    ldr     r9, [r1, #0x14]    @ v_msk
    ldr     r1, [r1]           @ src
    mov     r8, r12            @ v_inc
    mov     r12,r4, lsr #13
    add     lr, r1, lr, lsl #1
    and     r9, r9, #0xff<<10  @ v_msk_final
0:
    ldr     lr, [lr, r12, lsl #2]
    lsr     r12,r4, #8
    and     r12,r12,#0x1c
    sub     r12,r12,#1
    mov     r12,lr, ror r12
    add     r4, r4, r5
    and     r12,r12,#0x1e
    and     r4, r4, r6
    ldrh    r12,[r3, r12]
    add     r0, r0, #2
    add     r7, r7, r8
    and     lr, r7, r9
    tst     r12,r12
    add     lr, r1, lr, lsl #1
    strneh  r12,[r0, #-2]
    mov     r12,r4, lsr #13
    subs    r2, r2, #1
    bgt     0b

    ldmfd   sp!, {r8-r9}
    ldmfd   sp!, {r4-r7,pc}
    .cfi_endproc


#ifdef HAVE_ARMV6

.macro modulate rp mbr mg t0 t1 t2
    and     \t0, \rp, #0x001f
    and     \t1, \rp, #0x03e0
    and     \t2, \rp, #0x7c00
    smulbb  \t0, \t0, \mbr       @ -> 0000 0000 0000 orrr  rrxx xxxx xxxx xxxx
    smulbt  \t1, \t1, \mg        @ -> 0000 000o gggg gxxx  xxxx xxxx xxx0 0000
    smulbt  \t2, \t2, \mbr       @ -> 00ob bbbb xxxx xxxx  xxxx xx00 0000 0000
    and     \rp, \rp, #0x8000
    usat    \t0, #5, \t0, asr #14
    usat    \t1, #5, \t1, asr #19
    usat    \t2, #5, \t2, asr #24
    orr     \rp, \rp, \t0
    orr     \rp, \rp, \t1, lsl #5
    orr     \rp, \rp, \t2, lsl #10
.endm

.global poly_4bpp_l_asm @ (void *d, const struct gpu_unai_inner_t *inn, int count)
poly_4bpp_l_asm:
    .cfi_startproc
    stmfd   sp!, {r4-r11,lr}
    .cfi_def_cfa_offset 4*9
    .cfi_rel_offset lr, 4*8
    poly_4bpp_init poly_4bpp_lv_asm 1
0:
    mov     r12,r4, lsr #13
    subs    r2, r2, #1
    bmi     1f
    ldr     lr, [r1, r12, lsl #2]
    lsr     r12,r4, #8
    and     r12,r12,#0x1c
    sub     r12,r12,#1
    mov     r12,lr, ror r12
    add     r4, r4, r5
    and     r12,r12,#0x1e
    and     r4, r4, r6
    ldrh    r12,[r3, r12]
    add     r0, r0, #2
    tst     r12,r12
    beq     0b
    modulate r12, r10, r11, r7, r8, lr
    strh    r12,[r0, #-2]
    b       0b
1:
    ldmfd   sp!, {r4-r11,pc}

poly_4bpp_lv_asm: @ r3=clut, r4=u, r5=u_inc, r6=u_msk, r7=v, lr=v_masked
    sub     sp, sp, #4*2
    .cfi_def_cfa_offset 4*(9+2)
    .cfi_rel_offset lr, 4*(8+2)
    ldr     r9, [r1, #0x14]    @ v_msk
    ldr     r1, [r1]           @ src
    mov     r8, r12            @ v_inc
    mov     r12,r4, lsr #13
    and     r9, r9, #0xff<<10  @ v_msk_final
    stmia   sp, {r5,r6}
0:
    and     lr, r7, r9
    mov     r12,r4, lsr #13
    add     lr, r1, lr, lsl #1
    subs    r2, r2, #1
    bmi     1f
    ldr     lr, [lr, r12, lsl #2]
    lsr     r12,r4, #8
    and     r12,r12,#0x1c
    sub     r12,r12,#1
    mov     r12,lr, ror r12
    add     r4, r4, r5
    and     r12,r12,#0x1e
    and     r4, r4, r6
    ldrh    r12,[r3, r12]
    add     r0, r0, #2
    add     r7, r7, r8
    tst     r12,r12
    beq     0b
    modulate r12, r10, r11, r5, r6, lr
    strh    r12,[r0, #-2]
    ldmia   sp, {r5,r6}
    b       0b
1:
    add     sp, sp, #4*2
    ldmfd   sp!, {r4-r11,pc}
    .cfi_endproc

#endif // HAVE_ARMV6

@ vim:filetype=armasm

/*
 * (C) Gražvydas "notaz" Ignotas, 2011,2024
 *
 * This work is licensed under the terms of  GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"

.syntax unified
.text
.align 2

.macro pld_ reg offs=#0
#ifdef HAVE_ARMV6
    pld      [\reg, \offs]
#endif
.endm

#ifdef HAVE_ARMV6

@ mbr: 0bbb bbbb 0ggg gggg 0rrr rrrr r000 0000
@ mg:  0ggg gggg ...
@ dither: 2 if on
@ assumes r0 as dst ptr, dither value at [sp, #8]
@ assumes and must retain flags from input (tst \rp, \rp)
.macro modulate dither rp mbr mg t0 t1 t2
    and     \t0, \rp, #0x001f
    and     \t1, \rp, #0x03e0
    and     \t2, \rp, #0x7c00
    smulbb  \t0, \t0, \mbr       @ -> 0000 0000 0000 orrr  rrxx xxxx xxxx xxxx
    smulbt  \t1, \t1, \mg        @ -> 0000 000o gggg gxxx  xxxx xxxx xxx0 0000
    smulbt  \t2, \t2, \mbr       @ -> 00ob bbbb xxxx xxxx  xxxx xx00 0000 0000
.if \dither == 2
    ldr     \mg, [sp, #8]        @ dither_val
    mov     \rp, #0x18
    and     \rp, \rp, r0, lsl #2
    mov     \rp, \mg, ror \rp
    mov     \mg, \mbr, lsl #8    @ restore mg
    sxtb    \rp, \rp
    add     \t0, \t0, \rp, lsl #7
    add     \t1, \t1, \rp, lsl #12
    add     \t2, \t2, \rp, lsl #17
.endif
    usat    \rp, #5, \t0, asr #14
    usat    \t1, #5, \t1, asr #19
    usat    \t2, #5, \t2, asr #24
    orrmi   \rp, \rp, #0x8000
    orr     \rp, \rp, \t1, lsl #5
    orr     \rp, \rp, \t2, lsl #10
.endm

@ http://www.slack.net/~ant/info/rgb_mixing.html
@ p0 = (p0 + p1) / 2; p1 |= 0x8000
@ msb of input p0 is assumed to be set
.macro semitrans0 p0 p1 t
    eor     \t,  \p0, \p1
    and     \t,  \t,  #0x0420
    sub     \p0, \p0, \t
    orr     \p1, \p1, #0x8000
    uhadd16 \p0, \p0, \p1
.endm

.macro semitrans0p p0 p1 m421 t
    eor     \t,  \p0, \p1
    and     \t,  \t,  \m421
    add     \p0, \p0, \p1
    uhsub16 \p0, \p0, \t           @ sub because of borrow into hi16
.endm

@ p0 - {p1|r,g,b}   // p1* - premasked rgb
.macro semitrans2p p0 p1r p1g p1b m1f t0 t1
    and     \t0, \p0, \m1f
    and     \t1, \p0, \m1f, lsl #5
    and     \p0, \p0, \m1f, lsl #10
    uqsub16 \t0, \t0, \p1r
    uqsub16 \t1, \t1, \p1g
    uqsub16 \p0, \p0, \p1b
    orr     \t0, \t0, \t1
    orr     \p0, \p0, \t0
.endm

#else

@ msb of input p0 is assumed to be set
.macro semitrans0 p0 p1 t
    eor     \t,  \p0, \p1
    and     \t,  \t,  #0x0420
    orr     \p1, \p1, #0x8000
    sub     \p0, \p0, \t
    add     \p0, \p0, \p1
    orr     \p0, \p0, #0x10000
    mov     \p0, \p0, lsr #1
.endm

.macro semitrans0p p0 p1 m421 t
    eor     \t,  \p0, \p1
    and     \t,  \t,  \m421
    add     \p0, \p0, \p1
    sub     \p0, \p0, \t
    mov     \p0, \p0, lsr #1
.endm

#endif // HAVE_ARMV6

.macro semitrans13p p0 p1 m421 t0
    add     \t0, \p0, \p1
    eor     \p0, \p0, \p1
    and     \p0, \p0, \m421          @ low_bits
    sub     \p0, \t0, \p0
    and     \p0, \p0, \m421, lsl #5  @ carries
    sub     \t0, \t0, \p0            @ modulo
    sub     \p0, \p0, \p0, lsr #5    @ clamp
    orr     \p0, \t0, \p0
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
    strhne  r6, [r0, #\obase+0]
    tst     r7, r7
    strhne  r7, [r0, #\obase+2]
    tst     r8, r8
    strhne  r8, [r0, #\obase+4]
    tst     lr, lr
    strhne  lr, [r0, #\obase+6]
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
    strhne   r6, [r0, #0]
    tst      r7, r7
    strhne   r7, [r0, #2]
    tst      r8, r8
    strhne   r8, [r0, #4]
    tst      \rs,\rs
    strhne   \rs,[r0, #6]
.endm


@ everything is aligned by at least 16,
@ gpu_unai doesn't do wrapping so it's missing here too
FUNCTION(gpu_fill_asm): @ (void *d, u32 rgbx2, u32 w, u32 h)
    .cfi_startproc
    push    {r4,r5,r10,r11}
    mov     r10,r1
    mov     r11,r1
    mov     r12,r1
    add     r4, r0, #2048
    mov     r5, r2
0:
    subs    r2, r2, #8
    stmia   r0!, {r1,r10,r11,r12}
    bgt     0b

    subs    r3, r3, #1
    mov     r0, r4
    add     r4, r4, #2048
    mov     r2, r5
    bgt     0b

    pop     {r4,r5,r10,r11}
    bx      lr
    .cfi_endproc


@ (void *d, u16 c, u32 cnt, const struct gpu_unai_inner_t *inn)
@ see also poly_untex_st_m
.macro tile_driver_st_m name semit
FUNCTION(\name):
    .cfi_startproc
    stmfd   sp!, {r4-r9,lr}
    .cfi_def_cfa_offset 4*7
    .cfi_rel_offset lr, 4*6
    ldr     r7, [r3, #0x18]        @ y0
    ldr     r8, [r3, #0x1c]        @ y1
.if \semit != 2
    mov     r4, #0x8000
    orr     r4, r4, r4, lsl #16    @ mask 8000
    mov     r6, #0x420
    orr     r6, r6, #1
    orr     r6, r6, r6, lsl #16    @ mask 0421
.endif
.if \semit == 2
    and     r4, r1, #0x03e0
    and     r5, r1, #0x7c00
    and     r1, r1, #0x001f
    orr     r4, r4, r4, lsl #16    @ premasked g
    orr     r5, r5, r5, lsl #16    @ premasked b
    mov     r6, #0x00001f
    orr     r6, #0x1f0000          @ mask
.elseif \semit == 3
    mov     r1, r1, lsr #2
    bic     r1, r1, #(0x0c60>>2)
.endif
    orr     r1, r1, r1, lsl #16
    sub     r3, r8, r7             @ h
    mov     r7, r2                 @ save w
0:
    ldrh    r8, [r0]
    pld_    r0, #2048
    tst     r0, #2
    beq     1f
    sub     r2, #1
.if \semit == 0
    bic     r8, r8, r4
    semitrans0p  r8, r1, r6, lr
.elseif \semit == 1 || \semit == 3
    bic     r8, r8, r4
    semitrans13p r8, r1, r6, lr
.elseif \semit == 2
    semitrans2p  r8, r1, r4, r5, r6, r9, lr
.endif
    strh    r8, [r0], #2
1:
    ldr     r8, [r0]
    pld_    r0, #32
    subs    r2, r2, #2
.if \semit == 0
    bic     r8, r8, r4
    semitrans0p  r8, r1, r6, lr
.elseif \semit == 1 || \semit == 3
    bic     r8, r8, r4
    semitrans13p r8, r1, r6, lr
.elseif \semit == 2
    semitrans2p  r8, r1, r4, r5, r6, r9, lr
.endif
    strpl   r8, [r0], #4
    bpl     1b
2:
    tst     r2, #1
    strhne  r8, [r0], #2
    subs    r3, r3, #1
    mov     r2, r7                 @ w
    add     r0, r0, #2048
    sub     r0, r0, r7, lsl #1
    bgt     0b

    ldmfd   sp!, {r4-r9,pc}
    .cfi_endproc
.endm


tile_driver_st_m tile_driver_st0_asm, 0
tile_driver_st_m tile_driver_st1_asm, 1
tile_driver_st_m tile_driver_st3_asm, 3
#ifdef HAVE_ARMV6
tile_driver_st_m tile_driver_st2_asm, 2
#endif

@ (u16 *d, void *s, u16 *pal, int lines)
sprite_4bpp_x16_asm_:
    ldr     r12,[r3, #0x18]        @ y0
    ldr     r2, [r3, #0x04]        @ pal
    ldr     r3, [r3, #0x1c]        @ y1
    sub     r3, r3, r12
FUNCTION(sprite_4bpp_x16_asm):
    .cfi_startproc
    stmfd   sp!, {r4-r8,lr}
    .cfi_def_cfa_offset 4*6
    .cfi_rel_offset lr, 4*5
    mov     r12, #0x1e

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
    ldr     r4, [r3, #0x08]    @ u
    ldr     r5, [r3, #0x1c]    @ v1
    ldr     r6, [r3, #0x18]    @ v0
    and     r4, r4, #((8 >> \is8bpp) - 1)
    sub     r5, r5, r6
    sub     r5, r5, #1
    orr     r5, r4, r5, lsl #8 @ ((h-1) << 8) | u0_fraction
    mov     r9, r2             @ saved_w
    mov     r10, r0            @ saved_dst
    mov     r11, r1            @ saved_src
    ldr     r2, [r3, #0x04]    @ pal
11: @ line_loop:
    pld_    r11, #2048
    ands    r6, r5, #(7 >> \is8bpp)
    mov     r0, r10
    mov     r1, r11
    mov     r3, r9
    bne     15f @ fractional_u
12:
    subs    r3, r3, #(8 >> \is8bpp) @ w
    bmi     14f @ fractional_w
.endm
.macro sprite_driver_part2 is8bpp
    cmn     r3, #(8 >> \is8bpp)
    bne     14f @ fractional_w
13: @ eol:
    subs    r5, r5, #0x100
    add     r10, r10, #2048
    add     r11, r11, #2048
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

@ (u16 *d, const void *s, int width, const gpu_unai_inner_t *)
FUNCTION(sprite_driver_4bpp_asm):
    .cfi_startproc
    ldr     r12, [r3, #8]      @ u
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
    subs    r3, r3, #8
    add     r0, r0, #16
    bpl     0b
    sprite_driver_part2 0
0:
    and     r7, r12, r4
    mov     r4, r4, lsr #4
    ldrh    r7, [r2, r7]
    add     r0, r0, #2
    tst     r7, r7
    strhne  r7, [r0, #-2]
    subs    r8, r8, #1
    bgt     0b
    sprite_driver_part3
    .cfi_endproc


@ (u16 *d, const void *s, int width, const gpu_unai_inner_t *)
FUNCTION(sprite_driver_8bpp_asm):
    .cfi_startproc
    sprite_driver_part1 1
0:
    ldr     r4, [r1], #4
    pld_    r1, #28
    do_4x_8bpp r4
    subs    r3, r3, #4
    add     r0, r0, #8
    bpl     0b
    sprite_driver_part2 1
0:
    and     r7, r12, r4
    mov     r4, r4, lsr #8
    ldrh    r7, [r2, r7]
    add     r0, r0, #2
    tst     r7, r7
    strhne  r7, [r0, #-2]
    subs    r8, r8, #1
    bgt     0b
    sprite_driver_part3
    .cfi_endproc


@ (u16 *d, const void *s, int width, const gpu_unai_inner_t *)
.macro sprite_driver_l_st name bpp light semit
FUNCTION(\name):
    .cfi_startproc
    stmfd   sp!, {r4-r11,lr}
    .cfi_def_cfa_offset 4*4
    .cfi_rel_offset lr, 4*3
    ldr     r5, [r3, #0x18]    @ y0
    ldr     r7, [r3, #0x1c]    @ y1
    ldr     r8, [r3, #0x20]    @ rbg5
    mov     r6, r2             @ saved_w
    ldr     r2, [r3, #0x04]    @ pal
    ldr     r10,[r3, #0x08]    @ u
    ldr     r11,[r3, #0x10]    @ mask_v00u
    sub     r5, r7, r5         @ h
    mov     r7, r8, lsl #(8+2) @ 0bbb bb00 0ggg gg00 0rrr rr00 0000 0000
    mov     r8, r8, lsl #(16+2)@ 0ggg gg00 ...
    and     r3, r11,#0xff
    orr     r6, r3, r6, lsl #16 @ (w << 16) | u_mask
    mov     r3, r6
    and     r10,r10,r6

3: @ line_loop:
.if \bpp == 4
    add     r9, r1, r10, lsr #1
.elseif \bpp == 8
    add     r9, r1, r10
    pld_    r9, #2048
.endif
0:
.if \bpp == 4
    ldrb    r4, [r1, r10, lsr #1]
.elseif \bpp == 8
    ldrb    r4, [r1, r10]
.endif
    subs    r3, r3, #1<<16
    bmi     1f
.if \bpp == 4
    tst     r10, #1
    movne   r4, r4, lsr #3
    addeq   r4, r4, r4
    and     r4, r4, #0x1e
.elseif \bpp == 8
    add     r4, r4, r4         @ <<= 1
.endif
    ldrsh   r12,[r2, r4]
    add     r10,r10,#1
    and     r10,r10,r6
    add     r0, r0, #2
    tst     r12,r12
    beq     0b
.if \light && \semit != 1
    modulate 0, r12, r7, r8, r4, r9, lr
.endif
.if \semit == 0
    ldrhmi  lr, [r0, #-2]
    strhpl  r12,[r0, #-2]
    bpl     0b
    semitrans0 r12, lr, r9
.elseif \light && \semit == 1
    and     r4,  r12, #0x001f
    and     r9,  r12, #0x03e0
    and     r12, r12, #0x7c00
    ldrhmi  r11, [r0, #-2]
    smulbb  r4,  r4,  r7       @ -> 0000 0000 0000 orrr  rrxx xxxx xxxx xxxx
    smulbt  r9,  r9,  r8       @ -> 0000 000o gggg gxxx  xxxx xxxx xxx0 0000
    smulbt  r12, r12, r7       @ -> 00ob bbbb xxxx xxxx  xxxx xx00 0000 0000
    and     r8,  r11, #0x001f
    and     lr,  r11, #0x03e0
    and     r11, r11, #0x7c00
    addmi   r4,  r4,  r8,  lsl #14
    addmi   r9,  r9,  lr,  lsl #14
    addmi   r12, r12, r11, lsl #14
    usat    r4,  #5,  r4,  asr #14
    usat    r9,  #5,  r9,  asr #19
    usat    r12, #5,  r12, asr #24
    orrmi   r4,  r4,  #0x8000
    orr     r4,  r4,  r9,  lsl #5
    orr     r12, r4,  r12, lsl #10
    mov     r8,  r7,  lsl #8       @ restore r8
.endif
    strh    r12,[r0, #-2]
    b       0b
1:
    add     r0, r0, #2048
    add     r1, r1, #2048
    sub     r0, r0, r6, lsr #15    @ dst
    sub     r10,r10,r6, lsr #16    @ u
    mov     r3, r6                 @ (w << 16) | u_mask
    and     r10,r6, r10
    subs    r5, r5, #1
    and     r10,r10,#0xff
    bgt     3b @ line_loop

    ldmfd   sp!, {r4-r11,pc}
    .cfi_endproc
.endm

sprite_driver_l_st sprite_driver_4bpp_l0_std_asm, 4, 0, -1
sprite_driver_l_st sprite_driver_4bpp_l0_st0_asm, 4, 0,  0
sprite_driver_l_st sprite_driver_8bpp_l0_std_asm, 8, 0, -1
sprite_driver_l_st sprite_driver_8bpp_l0_st0_asm, 8, 0,  0

#ifdef HAVE_ARMV6

sprite_driver_l_st sprite_driver_4bpp_l1_std_asm, 4, 1, -1
sprite_driver_l_st sprite_driver_4bpp_l1_st0_asm, 4, 1,  0
sprite_driver_l_st sprite_driver_4bpp_l1_st1_asm, 4, 1,  1
sprite_driver_l_st sprite_driver_8bpp_l1_std_asm, 8, 1, -1
sprite_driver_l_st sprite_driver_8bpp_l1_st0_asm, 8, 1,  0
sprite_driver_l_st sprite_driver_8bpp_l1_st1_asm, 8, 1,  1

#endif // HAVE_ARMV6


@ (u16 *d, const void *s, int width, const gpu_unai_inner_t *)
FUNCTION(sprite_driver_16bpp_asm):
    .cfi_startproc
    stmfd   sp!, {r4-r6,lr}
    .cfi_def_cfa_offset 4*4
    .cfi_rel_offset lr, 4*3
    ldr     r4, [r3, #0x1c]    @ v1
    ldr     r5, [r3, #0x18]    @ v0
    mov     r12,      #0x00ff
    orr     r12, r12, #0xff00  @ mask
    mov     r6, r2             @ saved_w
    sub     r5, r4, r5
    sub     r5, r5, #1         @ h-1
3: @ line_loop:
    tst     r1, #2
    pld_    r1, #2048
    mov     r2, r6             @ w
    beq     0f
2: @ 1pix:
    ldrh    lr, [r1], #2
    add     r0, r0, #2
    sub     r2, r2, #1
    tst     lr, lr
    strhne  lr, [r0, #-2]
0:
    subs    r2, r2, #4
    bmi     1f
0:
    ldmia   r1!, {r3,r4}
    add     r0, r0, #2*4
    pld_    r1, #24
    tst     r3, r12
    strhne  r3, [r0, #-8]
    movs    lr, r3, lsr #16
    strhne  lr, [r0, #-6]
    tst     r4, r12
    strhne  r4, [r0, #-4]
    movs    lr, r4, lsr #16
    strhne  lr, [r0, #-2]
    subs    r2, r2, #4
    bpl     0b
1:
    adds    r2, r2, #4
    bne     2b @ 1pix
    add     r0, r0, #2048
    add     r1, r1, #2048
    subs    r5, r5, #1
    sub     r0, r0, r6, lsl #1 @ dst
    sub     r1, r1, r6, lsl #1
    bpl     3b @ line_loop

    ldmfd   sp!, {r4-r6,pc}
    .cfi_endproc

@ -----------------------------------------------------------

@ (void *d, const gpu_unai_inner_t *inn, int count)
@ see also tile_driver_st_m
.macro poly_untex_st_m name semit
FUNCTION(\name):
    .cfi_startproc
    ldrh    r1, [r1, #0x38]        @ rgb
    stmfd   sp!, {r4-r7,lr}
    .cfi_def_cfa_offset 4*5
    .cfi_rel_offset lr, 4*4
.if \semit != 2
    mov     r4, #0x8000
    orr     r4, r4, r4, lsl #16    @ mask 8000
    mov     r6, #0x420
    orr     r6, r6, #1
    orr     r6, r6, r6, lsl #16    @ mask 0421
.endif
.if \semit == 2
    and     r4, r1, #0x03e0
    and     r5, r1, #0x7c00
    and     r1, r1, #0x001f
    orr     r4, r4, r4, lsl #16    @ premasked g
    orr     r5, r5, r5, lsl #16    @ premasked b
    mov     r6, #0x00001f
    orr     r6, #0x1f0000          @ mask
.elseif \semit == 3
    mov     r1, r1, lsr #2
    bic     r1, r1, #(0x0c60>>2)
.endif
    orr     r1, r1, r1, lsl #16
0:
    ldrh    r3, [r0]
    tst     r0, #2
    pld_    r0, #2048
    beq     1f
    sub     r2, #1
.if \semit == 0
    bic     r3, r3, r4
    semitrans0p  r3, r1, r6, lr
.elseif \semit == 1 || \semit == 3
    bic     r3, r3, r4
    semitrans13p r3, r1, r6, lr
.elseif \semit == 2
    semitrans2p  r3, r1, r4, r5, r6, r7, lr
.endif
    strh    r3, [r0], #2
1:
    ldr     r3, [r0]
    pld_    r0, #32
    subs    r2, r2, #2
.if \semit == 0
    bic     r3, r3, r4
    semitrans0p  r3, r1, r6, lr
.elseif \semit == 1 || \semit == 3
    bic     r3, r3, r4
    semitrans13p r3, r1, r6, lr
.elseif \semit == 2
    semitrans2p  r3, r1, r4, r5, r6, r7, lr
.endif
    strpl   r3, [r0], #4
    bpl     1b
2:
    tst     r2, #1
    strhne  r3, [r0], #2

    ldmfd   sp!, {r4-r7,pc}
    .cfi_endproc
.endm

poly_untex_st_m poly_utx_l0d0m0st0_asm, 0
poly_untex_st_m poly_utx_l0d0m0st1_asm, 1
poly_untex_st_m poly_utx_l0d0m0st3_asm, 3
#ifdef HAVE_ARMV6
poly_untex_st_m poly_utx_l0d0m0st2_asm, 2


@ r0: dst          r7 : b16_inc
@ r1: r16          r8 : 0x001f001f
@ r2: count        r9 : scratch
@ r3: g16
@ r4: b16
@ r5: r16_inc      r12: scratch
@ r6: g16_inc      lr : scratch
FUNCTION(poly_utx_g1d0m0std_asm): @ (void *d, const gpu_unai_inner_t *inn, int count)
    .cfi_startproc
    stmfd   sp!, {r4-r9,lr}
    .cfi_def_cfa_offset 4*7
    .cfi_rel_offset lr, 4*6
    ldrh    r3, [r1, #0x2a]       @ g16
    ldrh    r4, [r1, #0x2c]       @ b16
    ldrh    r5, [r1, #0x30]       @ r16_inc
    ldrh    r6, [r1, #0x32]       @ g16_inc
    ldrh    r7, [r1, #0x34]       @ b16_inc
    ldrh    r1, [r1, #0x28]       @ r16
    tst     r0, #2
    mov     r8, #0x1f
    pkhbt   r8, r8, r8, lsl #16   @ 0x001f001f
    beq     0f @ pairs
@ do_one:
    and     r9, r8, r1, lsr #11   @ r
    and     r12,r8, r3, lsr #11   @ g
    and     lr, r8, r4, lsr #11   @ b
    uadd16  r1, r1, r5            @ r += r_inc
    uadd16  r3, r3, r6            @ g += g_inc
    uadd16  r4, r4, r7            @ b += b_inc
    orr     r12,r9, r12,lsl #5
    orr     r12,r12,lr ,lsl #10
    subs    r2, r2, #1
    strh    r12,[r0], #2
    ble     1f @ return
0: @ pairs:
    add     r9, r1, r5            @ r += r_inc
    add     r12,r3, r6
    add     lr, r4, r7
    pkhbt   r1, r1, r9, lsl #16   @ r_next | r
    pkhbt   r3, r3, r12,lsl #16   @ g_next | g
    pkhbt   r4, r4, lr, lsl #16   @ b_next | b
    pkhbt   r5, r5, r5, lsl #16   @ r_inc
    pkhbt   r6, r6, r6, lsl #16
    pkhbt   r7, r7, r7, lsl #16
    uadd16  r5, r5, r5            @ r_inc *= 2
    uadd16  r6, r6, r6
    uadd16  r7, r7, r7
0: @ pairs_loop
    and     r9, r8, r1, lsr #11   @ r
    and     r12,r8, r3, lsr #11   @ g
    and     lr, r8, r4, lsr #11   @ b
    uadd16  r1, r1, r5            @ r += r_inc
    uadd16  r3, r3, r6            @ g += g_inc
    uadd16  r4, r4, r7            @ b += b_inc
    orr     r12,r9, r12,lsl #5
    orr     r12,r12,lr ,lsl #10
    subs    r2, r2, #2
    strpl   r12,[r0], #4
    bgt     0b

    nop
    strhmi  r12,[r0], #2
1: @ return
    ldmfd   sp!, {r4-r9,pc}
    .cfi_endproc


@ r0: dst          r7 : b16_inc
@ r1: r16          r8 : 0x001f001f
@ r2: count        r9 : scratch
@ r3: g16          r10: dither_val0
@ r4: b16          r11: dither_val_current | 0x42104210
@ r5: r16_inc      r12: scratch
@ r6: g16_inc      lr : scratch
@ stack: dither_val_prep0, dither_val_prep1
@ maskmode: same as e6: b0=set, b1=check
.macro poly_utx_g_asm_m name maskmode
FUNCTION(\name): @ (void *d, const gpu_unai_inner_t *inn, int count, u32 dv)
    .cfi_startproc
    stmfd   sp!, {r4-r11,lr}
    .cfi_def_cfa_offset 4*9
    .cfi_rel_offset lr, 4*8
    ldrh    r4, [r1, #0x2c]       @ b16
    ldrh    r5, [r1, #0x30]       @ r16_inc
    ldrh    r6, [r1, #0x32]       @ g16_inc
    ldrh    r7, [r1, #0x34]       @ b16_inc
    mov     r10,r3
    ldrh    r3, [r1, #0x2a]       @ g16
    ldrh    r1, [r1, #0x28]       @ r16
    mov     r8, #0x1f
    movs    r9, r0, lsl #30
    movcs   r11,r10,ror #16       @ setup dither_val
    movcc   r11,r10
    pkhbt   r8, r8, r8, lsl #16   @ 0x001f001f
    movmi   r11,r11,ror #8
    tst     r0, #2
    sub     r2, r2, #1            @ adjust for the short loop
    beq     1f @ maybe_pairs
0: @ do_one:
    sxtb    lr, r11
    mov     r11,r11,ror #8
    add     r9, r1, lr, lsl #4    @ r + dither
    add     r12,r3, lr, lsl #4    @ g + dither
    add     lr, r4, lr, lsl #4    @ b + dither
    usat    r9, #5, r9, asr #11   @ r
    usat    r12,#5, r12,asr #11   @ g
    usat    lr, #5, lr, asr #11   @ b
    uadd16  r1, r1, r5            @ r += r_inc
    uadd16  r3, r3, r6            @ g += g_inc
    uadd16  r4, r4, r7            @ b += b_inc
    orr     r12,r9, r12,lsl #5
    orr     r12,r12,lr ,lsl #10
.if \maskmode & 1
    orr     r12,r12,#0x8000
.endif
    sub     r2, r2, #1
    strh    r12,[r0], #2
1: @ maybe_pairs:
    cmp     r2, #4
    bcc     0b                    @ (16) pair setup is expensive, just do it normally
    adds    r2, r2, #1
    beq     2f @ return
@ pairs:
    sxtb    r9, r10
    sxtb    r12,r10,ror #8
    sxtb    lr, r10,ror #16
    sxtb    r11,r10,ror #24
    mov     r9, r9, lsl #4
    mov     r12,r12,lsl #20
    mov     lr, lr, lsl #4
    mov     r11,r11,lsl #20
    pkhbt   r10,r9, r12           @ dither_val1 | dither_val0
    pkhbt   r11,lr, r11           @ dither_val3 | dither_val2
    eor     r1, r1, #0x8000       @ r convert to signed
    eor     r3, r3, #0x8000
    eor     r4, r4, #0x8000
    add     r9, r1, r5            @ r += r_inc
    add     r12,r3, r6
    add     lr, r4, r7
    pkhbt   r1, r1, r9, lsl #16   @ r_next | r
    pkhbt   r3, r3, r12,lsl #16   @ g_next | g
    pkhbt   r4, r4, lr, lsl #16   @ b_next | b
    pkhbt   r5, r5, r5, lsl #16   @ r_inc
    pkhbt   r6, r6, r6, lsl #16
    pkhbt   r7, r7, r7, lsl #16
    uadd16  r5, r5, r5            @ r_inc *= 2
    uadd16  r6, r6, r6
    uadd16  r7, r7, r7
    push    {r10,r11}
    mov     r9,     0x0010
    orr     r9, r9, 0x4200
    tst     r0, #4
    movne   r10,r11
    pkhbt   r11,r9, r9, lsl #16
0: @ pairs_loop
    qadd16  r9, r1, r10           @ r
    qadd16  r12,r3, r10           @ g
    qadd16  lr, r4, r10           @ b
    tst     r0, #4
    ldreq   r10,[sp, #4]          @ load the next dither values
    ldrne   r10,[sp]
    and     r9, r8, r9, lsr #11   @ r
    and     r12,r8, r12,lsr #11   @ g
    and     lr, r8, lr, lsr #11   @ b
    qadd16  r1, r1, r5            @ r += r_inc
    qadd16  r3, r3, r6            @ g += g_inc
    qadd16  r4, r4, r7            @ b += b_inc
    orr     r12,r9, r12,lsl #5
    orr     r12,r12,lr ,lsl #10
    subs    r2, r2, #2
    eor     r12,r12,r11           @ back to unsigned
.if \maskmode & 1
    orr     r12,r12,#0x80000000
    orr     r12,r12,#0x00008000
.endif
    strpl   r12,[r0], #4
    bgt     0b                    @ (17)

    add     sp, sp, #4*2
    strhmi  r12,[r0], #2
2: @ return
    ldmfd   sp!, {r4-r11,pc}
    .cfi_endproc
.endm

poly_utx_g_asm_m poly_utx_g1d1m0std_asm, 0
poly_utx_g_asm_m poly_utx_g1d1m1std_asm, 1

#endif // HAVE_ARMV6

@ stack: u_inc, mask_v00u, dither_val/unused
@ light:  0 off, 1 on, 2 dither
@ semit: -1 off, 0-3 - modes
.macro poly_4_8bpp_asm_m name bpp light semit
FUNCTION(\name): @ (void *d, const gpu_unai_inner_t *inn, int count[, u32 dv])
    .cfi_startproc
    stmfd   sp!, {r3-r11,lr}
    sub     sp, sp, #4*2
    .cfi_def_cfa_offset 4*12
    .cfi_rel_offset lr, 4*11
    add     r12, r1, #4
    ldmia   r12, {r3, r4, r7, r12} @ clut, u, v, mask_v00u
    ldr     r5, [r1, #0x18]    @ u_inc
.if \light
    ldr     r10,[r1, #0x24]    @ rbg
.endif
    mov     r6, r12            @ mask_v00u
    ldr     r12,[r1, #0x1c]    @ v_inc
    pld_    r3                 @ clut
.if \light
    mov     r10,r10,lsl #7     @ 0bbb bbbb 0ggg gggg 0rrr rrrr r000 0000
    bic     r10,r10,#1<<23
    bic     r10,r10,#1<<15
    mov     r11,r10,lsl #8     @ 0ggg gggg ...
.endif
    tst     r12,r12
    ldr     r1, [r1]           @ src
    sub     r0, r0, #2         @ adjust for loop
    bne     10f @ use_vinc

    @ no_vinc:
    and     lr, r7, r6, lsr #(24-10)
    and     r7, r4, r6, lsl #10
    add     r1, r1, lr, lsl #1
    mov     r7, r7, lsr #(13 - (\bpp / 8 * 3))
    mov     r6, r6, lsl #10
#ifdef HAVE_ARMV6
    add     r12,r1, r7, lsl #(2 - (\bpp / 8 * 2))
    pld_    r12,#2048          @ next line
#endif
0:
.if \light || \semit >= 0  @ else this is done before branching
    subs    r2, r2, #1
    and     r7, r4, r6
    mov     r7, r7, lsr #(13 - (\bpp / 8 * 3))
    bmi     1f
.endif
.if \bpp == 4
    ldr     lr, [r1, r7, lsl #2]
    lsr     r12,r4, #8
    and     r12,r12,#0x1c
    sub     r12,r12,#1
    mov     r12,lr, ror r12
    add     r4, r4, r5
    and     r12,r12,#0x1e
.else
    ldrb    r12,[r1, r7]
    add     r4, r4, r5
    add     r12,r12,r12
.endif
    ldrsh   r12,[r3, r12]
    add     r0, r0, #2
.if !\light && \semit < 0
    and     r7, r4, r6
    mov     r7, r7, lsr #(13 - (\bpp / 8 * 3))
    tst     r12,r12
    strhne  r12,[r0]
    subs    r2, r2, #1
    bgt     0b
    @ end
.else
    tst     r12,r12
    beq     0b
.if \light && \semit != 1
    modulate \light, r12, r10, r11, r7, r8, lr
.endif
.if \semit == 0
    ldrhmi  r7, [r0]
    strhpl  r12,[r0]
    bpl     0b
    pld_    r0, #32
    semitrans0 r12, r7, lr
.endif
    strh    r12,[r0]
    b       0b
.endif                         @ \light || \semit >= 0
1:
    add     sp, sp, #4*3
    ldmfd   sp!, {r4-r11,pc}

@ r0: dst          r7 : v
@ r1: src_texels   r8 : v_inc
@ r2: count        r9 : next_texel_ptr
@ r3: clut         r10: rbg
@ r4: u            r11: g
@ r5: u_inc        r12: scratch
@ r6: mask_v00u    lr : texels | scratch
@ stack: u_inc, mask_v00u, dither_val/unused
10: @ use_vinc:
.if \light || \semit >= 0
    stmia   sp, {r5,r6}     @ save {u_inc, mask} for reload when we are out of regs
.endif
    mov     r8, r12               @ v_inc
    @ load the first texel:
    and     lr, r7, r6, lsr #(24-10) @ l_v & (l_v_msk=mask_v00u>>14)
    and     r12,r4, r6, lsl #10
    add     lr, r1, lr, lsl #1
    mov     r12,r12,lsr #(13 - (\bpp / 8 * 3))
.if \bpp == 4
    ldr     r9, [lr, r12, lsl #2]
.else
    ldrb    r9, [lr, r12]
.endif
    add     r4, r4, r5            @ u_next
    add     r7, r7, r8            @ v_next
0:
.if \light || \semit >= 0
    subs    r2, r2, #1
    bmi     1f
.endif
    @ calculate the next load
    and     lr, r7, r6, lsr #(24-10) @ l_v & (l_v_msk=mask_v00u>>14)
    and     r12,r4, r6, lsl #10
    add     lr, r1, lr, lsl #1
.if \bpp == 4
    mov     r12,r12,lsr #13
    add     lr, lr, r12, lsl #2
.else
    add     lr, lr, r12, lsr #10
.endif
    @pld     [lr]                  @ doesn't help?
.if \bpp == 4
    sub     r12,r4, r5            @ undo u_next
    lsr     r12,r12,#8
    and     r12,r12,#0x1c
    sub     r12,r12,#1
    add     r4, r4, r5            @ u_next_next
    mov     r12,r9, ror r12
    and     r12,r12,#0x1e
.else
    add     r4, r4, r5            @ u_next_next
    add     r12,r9, r9
.endif
    @ load the pixel
    ldrsh   r12,[r3, r12]
    @ the slow next texel load:
.if \bpp == 4
    ldr     r9, [lr]
.else
    ldrb    r9, [lr]
.endif
    add     r0, r0, #2
    add     r7, r7, r8            @ v_next_next
.if !\light && \semit < 0
    tst     r12,r12
    strhne  r12,[r0]
    subs    r2, r2, #1
    bgt     0b
    @ end
.else
    tst     r12,r12
    beq     0b
.if \light && \semit != 1
    modulate \light, r12, r10, r11, r5, r6, lr
.endif
.if \semit == 0
    ldrhmi  r6, [r0]
    strhpl  r12,[r0]
    ldmiapl sp, {r5,r6}
    bpl     0b
    pld_    r0, #32
    semitrans0 r12, r6, lr
.endif
    strh    r12,[r0]
    ldmia   sp, {r5,r6}
    b       0b
.endif                         @ \light || \semit >= 0
1:
    add     sp, sp, #4*3
    ldmfd   sp!, {r4-r11,pc}
    .cfi_endproc
.endm

poly_4_8bpp_asm_m poly_4bp_l0d0m0std_asm, 4, 0, -1
poly_4_8bpp_asm_m poly_4bp_l0d0m0st0_asm, 4, 0,  0
poly_4_8bpp_asm_m poly_8bp_l0d0m0std_asm, 8, 0, -1
poly_4_8bpp_asm_m poly_8bp_l0d0m0st0_asm, 8, 0,  0

#ifdef HAVE_ARMV6

poly_4_8bpp_asm_m poly_4bp_l1d0m0std_asm, 4, 1, -1
poly_4_8bpp_asm_m poly_4bp_l1d0m0st0_asm, 4, 1,  0
poly_4_8bpp_asm_m poly_4bp_l1d1m0std_asm, 4, 2, -1
poly_4_8bpp_asm_m poly_4bp_l1d1m0st0_asm, 4, 2,  0
poly_4_8bpp_asm_m poly_8bp_l1d0m0std_asm, 8, 1, -1
poly_4_8bpp_asm_m poly_8bp_l1d0m0st0_asm, 8, 1,  0
poly_4_8bpp_asm_m poly_8bp_l1d1m0std_asm, 8, 2, -1
poly_4_8bpp_asm_m poly_8bp_l1d1m0st0_asm, 8, 2,  0

@ -----------------------------------------------------------
@ gouraud stuff

@ r0: dst          r7 : next_texel_ptr
@ r1: src_texels   r8 : texels | scratch
@ r2: count|b16    r9 : mask_v00u | scratch
@ r3: clut         r10: u_inc | scratch
@ r4: u            r11: v_inc | scratch
@ r5: v            r12: scratch
@ r6: g16|r16      lr : scratch
@ stack: gr16_inc, b16_inc, mask_v00u, dither_val, u_inc, v_inc, sp_offset, lr
@ semit: -1 off, 0-3 - modes
.macro poly_4bpp_g_asm_m name is_dither semit
FUNCTION(\name): @ (void *d, const gpu_unai_inner_t *inn, int count, u32 dv)
    .cfi_startproc
    mov     r12,sp
    bic     sp, #31               @ align stack
    sub     r12, r12, sp
    stmfd   sp!,{r4-r11}
    sub     sp, #32
.if \is_dither
    str     r3, [sp, #0x0c]       @ save dither_val
.endif
    str     r12,[sp, #0x18]       @ save sp_offset
    str     lr, [sp, #0x1c]
    .cfi_rel_offset lr, 0x1c
    ldr     r3, [r1, 0x04]        @ clut
    ldrd    r4, r5, [r1, 0x08]    @ u, v
    ldrd    r10,r11,[r1, 0x18]    @ u_inc, v_inc
    ldrh    r12,[r1, #0x2c]       @ b16
    ldr     r6, [r1, #0x28]       @ g16|r16
    ldr     r9, [r1, #0x10]       @ mask_v00u
    pkhbt   r2, r12, r2, lsl #16  @ count|b16
    ldr     r7, [r1, #0x30]       @ gr16_inc
    ldr     r8, [r1, #0x34]       @ 0b16_inc
    ldr     r1, [r1]              @ src_texels
    pld_    r3                    @ clut
    usub16  r6, r6, r7            @ g16|r16  adjust for loop
    usub16  r2, r2, r8            @ b16
    stmia   sp, {r7-r9}           @ save gr16_inc, b16_inc, mask_v00u
    @ load the first texel:
    and     lr, r5, r9, lsr #(24-10) @ l_v & (l_v_msk=mask_v00u>>14)
    and     r12,r4, r9, lsl #10
    add     lr, r1, lr, lsl #1
    mov     r12,r12,lsr #13
    add     r4, r4, r10           @ u_next
    add     r5, r5, r11           @ v_next
    ldr     r7, [lr, r12, lsl #2]
.if \is_dither || \semit >= 0
    strd    r10, r11, [sp, #0x10] @ save u_inc, v_inc
.endif
    sub     r0, r0, #2            @ adjust for the loop
0: @ loop:
    ldr     r9, [sp, #0x08]       @ mask_v00u
    subs    r2, r2, #1<<16
    bmi     1f @ done
    @ prepare for the next texel load/preload:
    and     lr, r5, r9, lsr #(24-10) @ l_v & (l_v_msk=mask_v00u>>14)
    and     r12,r4, r9, lsl #10
    add     lr, r1, lr, lsl #1
    mov     r12,r12,lsr #13
    add     r8, lr, r12, lsl #2
    @pld     [r8]                  @ doesn't help?
    @ process texels
    sub     r12,r4, r10           @ undo u_next
    lsr     r12,r12,#8
    and     r12,r12,#0x1c
    sub     r12,r12,#1
    mov     r12,r7, ror r12
    add     r4, r4, r10           @ u_next_next
    and     r12,r12,#0x1e
    add     r5, r5, r11           @ v_next_next
    @ load the pixel
    ldrsh   r12,[r3, r12]
    ldmia   sp, {r9, lr}          @ gr16_inc, b16_inc
    @ the slow next texel load:
    ldr     r7, [r8]
    uadd16  r6, r6, r9            @ g16|r16 current
    uadd16  r2, r2, lr            @ b16 current
    tst     r12,r12
    add     r0, r0, #2
    uxtb16  lr, r6, ror #8        @ g16|r16 >> 8 & 0xff00ff
    beq     0b
    @ modulate/light
.if \semit >= 0
    ldrhmi  r11,[r0]
    movpl   r11,#0
.endif
.if \is_dither
    movs    r10,r0, lsl #30
    ldr     r10, [sp, #0x0c]      @ dither_val
.endif
    and     r9, r12, #0x001f      @ r
    and     r8, r12, #0x03e0      @ g
    smulbb  r9, r9, lr
    smulbt  r8, r8, lr
    uxtb    lr, r2, ror #8
.if \is_dither
    movcs   r10, r10, lsr #16
    sxtbmi  r10, r10, ror #8
    sxtbpl  r10, r10
    tst     r12,r12
.endif
    and     r12,r12, #0x7c00      @ b
    smulbb  lr, lr, r12
.if \is_dither
    add     r9,  r9, r10
    add     r8,  r8, r10, lsl #5
    add     lr,  lr, r10, lsl #10
.endif
.if \semit == 1
    and     r10, r11, #0x001f
    and     r12, r11, #0x03e0
    and     r11, r11, #0x7c00
    add     r9,  r9, r10, lsl #7
    add     r8,  r8, r12, lsl #7
    add     lr,  lr, r11, lsl #7
.endif
.if \semit >= 0
    ldrd    r10, r11, [sp, #0x10] @ restore u_inc, v_inc
.elseif \is_dither
    ldr     r10, [sp, #0x10]      @ restore u_inc
.endif
    usat    r12, #5, r9, asr #7
    usat    r8,  #5, r8, asr #12
    usat    lr,  #5, lr, asr #17
    orrmi   r12, #0x8000
    orr     r12, r12, r8, lsl #5
    orr     r12, r12, lr, lsl #10
    strh    r12, [r0]
    b       0b
1: @ done
    @pld     [sp, #64]
    ldr     r0, [sp, #0x18]       @ sp_offset
    ldr     lr, [sp, #0x1c]
    add     sp, sp, #32
    ldmfd   sp!,{r4-r11}
    add     sp, sp, r0
    bx      lr
    .cfi_endproc
.endm

poly_4bpp_g_asm_m poly_4bp_lgd0m0std_asm, 0, -1
poly_4bpp_g_asm_m poly_4bp_lgd0m0st1_asm, 0,  1
poly_4bpp_g_asm_m poly_4bp_lgd1m0std_asm, 1, -1
poly_4bpp_g_asm_m poly_4bp_lgd1m0st1_asm, 1,  1

#endif // HAVE_ARMV6

@ vim:filetype=armasm

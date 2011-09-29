/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

/* .equiv HAVE_ARMV7, 1 */

.text
.align 2

.macro sgnxt16 rd
.if HAVE_ARMV7
    sxth     \rd, \rd
.else
    lsl      \rd, \rd, #16
    asr      \rd, \rd, #16
.endif
.endm

@ prepare work reg for ssatx
@ in: wr reg, bit to saturate to
.macro ssatx_prep wr bit
.if !HAVE_ARMV7
    mov      \wr, #(1<<(\bit-1))
.endif
.endm

.macro ssatx rd wr bit
.if HAVE_ARMV7
    ssat     \rd, #\bit, \rd
.else
    cmp      \rd, \wr
    subge    \rd, \wr, #1
    cmn      \rd, \wr
    rsblt    \rd, \wr, #0
.endif
.endm

.macro usat16_ rd rs
.if HAVE_ARMV7
    usat     \rd, #16, \rs
.else
    subs     \rd, \rs, #0
    movlt    \rd, #0
    cmp      \rd, #0x10000
    movge    \rd, #0x0ff00
    orrge    \rd, #0x000ff
.endif
.endm

@ unsigned divide rd = rm / rs
@ no div by 0 check
@ in: rm, rs
@ trash: rm rs
.macro udiv rd rm rs
    clz      \rd, \rs
    lsl      \rs, \rs, \rd        @ shift up divisor
    orr      \rd, \rd, #1<<31
    lsr      \rd, \rd, \rd
0:
    cmp      \rm, \rs
    subcs    \rm, \rs
    adcs     \rd, \rd, \rd
    lsr      \rs, #1
    bcc      0b
.endm


@ calculate RTPS/RTPT MAC values
@ in: r0 context, r8,r9 VXYZ
@ out: r10-r12 MAC123
@ trash: r1-r7
.macro do_rtpx_mac
    add      r1, r0, #4*32
    add      r2, r0, #4*(32+5)    @ gteTRX
    ldmia    r1!,{r5-r7}          @ gteR1*,gteR2*
    ldmia    r2, {r10-r12}
    smulbb   r2, r5, r8           @ gteR11 * gteVX0
    smultt   r3, r5, r8           @ gteR12 * gteVY0
    smulbb   r4, r6, r9           @ gteR13 * gteVZ0
    qadd     r2, r2, r3
    asr      r4, r4, #1           @ prevent oflow, lose a bit
    add      r3, r4, r2, asr #1
    add      r10,r10,r3, asr #11  @ gteMAC1
    smultb   r2, r6, r8           @ gteR21 * gteVX0
    smulbt   r3, r7, r8           @ gteR22 * gteVY0
    smultb   r4, r7, r9           @ gteR23 * gteVZ0
    ldmia    r1!,{r5-r6}          @ gteR3*
    qadd     r2, r2, r3
    asr      r4, r4, #1
    add      r3, r4, r2, asr #1
    add      r11,r11,r3, asr #11  @ gteMAC2
    @ be more accurate for gteMAC3, since it's also a divider
    smulbb   r2, r5, r8           @ gteR31 * gteVX0
    smultt   r3, r5, r8           @ gteR32 * gteVY0
    smulbb   r4, r6, r9           @ gteR33 * gteVZ0
    qadd     r2, r2, r3
    asr      r3, r4, #31          @ expand to 64bit
    adds     r1, r2, r4
    adc      r3, r2, asr #31      @ 64bit sum in r3,r1
    add      r12,r12,r3, lsl #20
    add      r12,r12,r1, lsr #12  @ gteMAC3
.endm


.global gteRTPS_nf_arm @ r0=CP2 (d,c),
gteRTPS_nf_arm:
    push     {r4-r11,lr}

    ldmia    r0, {r8,r9}          @ VXYZ(0)
    do_rtpx_mac
    add      r1, r0, #4*25        @ gteMAC1
    add      r2, r0, #4*17        @ gteSZ1
    stmia    r1, {r10-r12}        @ gteMAC123 save
    ldmia    r2, {r3-r5}
    add      r1, r0, #4*16        @ gteSZ0
    add      r2, r0, #4*9         @ gteIR1
    ssatx_prep r6, 16
    usat16_  lr, r12              @ limD
    ssatx    r10,r6, 16
    ssatx    r11,r6, 16
    ssatx    r12,r6, 16
    stmia    r1, {r3-r5,lr}       @ gteSZ*
    ldr      r3, [r0,#4*(32+26)]  @ gteH
    stmia    r2, {r10,r11,r12}    @ gteIR123 save
    cmp      r3, lr, lsl #1       @ gteH < gteSZ3*2 ?
    mov      r9, #1<<30
    bhs      1f
.if 1
    lsl      r3, #16
    udiv     r9, r3, lr
.else
    push     {r0, r12}
    mov      r0, r3
    mov      r1, lr
    bl       DIVIDE
    mov      r9, r0
    pop      {r0, r12}
.endif
1:
    ldrd     r6, [r0,#4*(32+24)]  @ gteOFXY
                                  cmp      r9, #0x20000
    add      r1, r0, #4*12        @ gteSXY0
                                  movhs    r9, #0x20000
    ldmia    r1, {r2-r4}
                   /* quotient */ subhs    r9, #1
    mov      r2, #0
    smlal    r6, r2, r10, r9
    stmia    r1!,{r3,r4}          @ shift gteSXY
    mov      r3, #0
    smlal    r7, r3, r11, r9
    lsr      r6, #16
             /* gteDQA, gteDQB */ ldrd     r10,[r0, #4*(32+27)]
    orr      r6, r2, lsl #16      @ (gteOFX + gteIR1 * q) >> 16
    ssatx_prep r2, 11
    lsr      r7, #16
        /* gteDQB + gteDQA * q */ mla      r4, r10, r9, r11
    orr      r7, r3, lsl #16      @ (gteOFY + gteIR2 * q) >> 16
    ssatx    r6, r2, 11           @ gteSX2
    ssatx    r7, r2, 11           @ gteSY2
    strh     r6, [r1]
    strh     r7, [r1, #2]
    str      r4, [r0,#4*24]       @ gteMAC0
    asrs     r4, #12
    movmi    r4, #0
    cmp      r4, #0x1000          @ limH
    movgt    r4, #0x1000
    str      r4, [r0,#4*8]        @ gteIR0

    pop      {r4-r11,pc}
    .size    gteRTPS_nf_arm, .-gteRTPS_nf_arm


.global gteRTPT_nf_arm @ r0=CP2 (d,c),
gteRTPT_nf_arm:
    ldr      r1, [r0, #4*19]      @ gteSZ3
    push     {r4-r11,lr}
    str      r1, [r0, #4*16]      @ gteSZ0
    mov      lr, #0

rtpt_arm_loop:
    add      r1, r0, lr, lsl #1
    ldrd     r8, [r1]             @ VXYZ(v)
    do_rtpx_mac

    ssatx_prep r6, 16
    usat16_  r2, r12              @ limD
    add      r1, r0, #4*25        @ gteMAC1
    ldr      r3, [r0,#4*(32+26)]  @ gteH
    stmia    r1, {r10-r12}        @ gteMAC123 save
    add      r1, r0, #4*17
    ssatx    r10,r6, 16
    ssatx    r11,r6, 16
    ssatx    r12,r6, 16
    str      r2, [r1, lr]         @ fSZ(v)
    cmp      r3, r2, lsl #1       @ gteH < gteSZ3*2 ?
    mov      r9, #1<<30
    bhs      1f
.if 1
    lsl      r3, #16
    udiv     r9, r3, r2
.else
    push     {r0, r12, lr}
    mov      r0, r3
    mov      r1, r2
    bl       DIVIDE
    mov      r9, r0
    pop      {r0, r12, lr}
.endif
1:
                                  cmp      r9, #0x20000
    add      r1, r0, #4*12
                                  movhs    r9, #0x20000
    ldrd     r6, [r0,#4*(32+24)]  @ gteOFXY
                   /* quotient */ subhs    r9, #1
    mov      r2, #0
    smlal    r6, r2, r10, r9
    mov      r3, #0
    smlal    r7, r3, r11, r9
    lsr      r6, #16
    orr      r6, r2, lsl #16      @ (gteOFX + gteIR1 * q) >> 16
    ssatx_prep r2, 11
    lsr      r7, #16
    orr      r7, r3, lsl #16      @ (gteOFY + gteIR2 * q) >> 16
    ssatx    r6, r2, 11           @ gteSX(v)
    ssatx    r7, r2, 11           @ gteSY(v)
    strh     r6, [r1, lr]!
    add      lr, #4
    strh     r7, [r1, #2]
    cmp      lr, #12
    blt      rtpt_arm_loop

    ldrd     r4, [r0, #4*(32+27)] @ gteDQA, gteDQB
    add      r1, r0, #4*9         @ gteIR1
    mla      r3, r4, r9, r5       @ gteDQB + gteDQA * q
    stmia    r1, {r10,r11,r12}    @ gteIR123 save

    str      r3, [r0,#4*24]       @ gteMAC0
    asrs     r3, #12
    movmi    r3, #0
    cmp      r3, #0x1000          @ limH
    movgt    r3, #0x1000
    str      r3, [r0,#4*8]        @ gteIR0

    pop      {r4-r11,pc}
    .size    gteRTPT_nf_arm, .-gteRTPT_nf_arm


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


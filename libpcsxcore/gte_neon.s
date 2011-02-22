/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */


.bss
.align 6 @ cacheline

scratch:
.rept 8*8*2/4
    .word 0
.endr

.text
.align 2

@ approximate signed gteIR|123 flags
@ in: rl/rh with packed gteIR|123
@ trash: r2,r3,r4
.macro do_irs_flags rl rh
    mov         r4, \rl, ror #16
    adds        r2, \rl, #1<<16
    subvcs      r3, \rl, #1<<16
    orrvs       lr, #(1<<31)|(1<<24) @ IR1/limB1
    adds        r2, r4, #1<<16
    subvcs      r3, r4, #1<<16
    mov         r4, \rh, lsl #16
    orrvs       lr, #(1<<31)
    orrvs       lr, #(1<<23)   @ IR2/limB2
    adds        r2, r4, #1<<16
    subvcs      r3, r4, #1<<16
    orrvs       lr, #(1<<22)   @ IR3/limB3
.endm


/*
 *  q | d | c code / phase 1   phase 2          scratch
 *  0   0   gteR1* [s16]       gteMAC3     =    gteMAC3  \ v=0 *
 *      1   gteR2*             gteIR1-3    =    gteIR1-3 /     *
 *  1   2   gteR3*             gteMAC3     =    gteMAC3  \ v=1
 *      3   *                  gteIR1-3    =    gteIR1-3 /
 *  2   4   gteTRX<<12 [s64]   gteOFX [s64]     gteMAC3  \ v=2
 *      5   gteTRY<<12         gteOFY [s64]     gteIR1-3 / 
 *  3   6   gteTRZ<<12         gteDQA [s64]     min gteMAC|12 v=012
 *      7   0                  gteDQB [s64]     max gteMAC|12
 *  4   8   VXYZ(v)  /    gteMAC1,2 [s32]       min gteIR|123
 *      9   *        /    gteMAC3               max gteIR|123
 *  5  10   gteIR1-3 [s16]     gteIR1-3 v=2     quotients 12
 *     11   0                                   quotient 3
 *  6  12   gteH (adj. for cmp)
 *     13   gteH (float for div)
 * ...      <scratch>
 * 15  30   0
 *     31   0
 */
.global gteRTPT_neon @ r0=CP2 (d,c),
gteRTPT_neon:
    push        {r4-r11,lr}

@    fmrx        r4, fpscr      @ vmrs?
    movw        r1, #:lower16:scratch
    movt        r1, #:upper16:scratch
    mov         r12, #0
    veor        q15, q15

    add         r3, r0, #4*32
    vldmia      r3, {d0-d2}    @ gteR*  [16*9]
    add         r3, r0, #4*(32+5)
    vldmia      r3, {d4-d5}    @ gteTR*
    vshl.i64    d2, d2, #32    @ |
    add         r3, r0, #4*(32+26)
    vld1.32     d11[0], [r3]   @ gteH
    vsri.u64    d2, d1, #32    @ |
    add         r3, r0, #4*19
    vld1.32     d14[0], [r3]   @ gteSZ3
    vshll.s32   q3, d5, #12
    vshll.s32   q2, d4, #12    @ gteTRX
    vshl.i64    d1, d1, #16    @ |
    add         r3, r0, #4*16
    vst1.32     d14[0], [r3]   @ gteSZ0 = gteSZ3
    vmovl.s16   q6, d11        @ gteH
    vsri.u64    d1, d0, #48    @ |

    vmov.i32    d22, #0x7fffffff
    vmov.i32    d23, #0x80000000
    mov         r3, #3         @ counter
    mov         r2, r0         @ VXYZ(0)
0:
    vldmia      r2!, {d8}      @ VXYZ(v)
    vmov.16     d8[3], r12     @ kill unused upper vector

    vmull.s16   q8, d0, d8
    vmull.s16   q9, d1, d8
    vmull.s16   q10, d2, d8
    vpaddl.s32  q8, q8
    vpaddl.s32  q9, q9
    vpaddl.s32  q10, q10
    vadd.s64    d16, d17       @ d16=d0.16[2]*d8.16[2], as
    vadd.s64    d18, d19       @ d8[3]==0, so won't affect
    vadd.s64    d20, d21       @ QC
    vadd.s64    d16, d4
    vadd.s64    d18, d5
    vadd.s64    d20, d6
    vqshrn.s64  d8, q8, #12    @ gteMAC1
    vqshrn.s64  d18, q9, #12   @ gteMAC2
    vqshrn.s64  d9, q10, #12   @ gteMAC3
    vsli.u64    d8, d18, #32   @ gteMAC|12
    vmov.32     d9[1], r12
    vqmovn.s32  d10, q4        @ gteIR1-3; losing 2 cycles?
    vmin.s32    d22, d8        @ min gteMAC|12
    vmax.s32    d23, d8        @ max gteMAC|12
    subs        r3, #1
    vst1.32     {d9,d10}, [r1, :64]!
    bgt         0b

    vst1.32     {d22,d23}, [r1, :64]! @ min/max gteMAC|12 (for flags)

    @ - phase2 -
    sub         r1, r1, #8*2*4
    vldmia      r1, {d0-d3}    @ note: d4,d5 is for gteOF|XY

    vmov        d20, d0        @ gteMAC3 v=0
    vmin.s16    d24, d1, d3    @ | find min IR
    vshr.s32    d22, d12, #1   @ || gteH (adjust for cmp)
    vmax.s16    d25, d1, d3    @ | .. also max, for flag gen
    vsli.u64    d20, d2, #32   @ gteMAC3 v=1
    vmov        d21, d9        @ ... v=2

    vmov.i32    q14, #0xffff   @ 0xffff[32]
    vmax.s32    q10, q15
    vmov.i32    q13, #1
    vdup.32     q11, d22[0]    @ gteH/2
    vmin.u32    q10, q14       @ saturate to 0..0xffff - fSZ(v)
    vmin.s16    d24, d10       @ | find min/max IR
    vmax.s16    d25, d10       @ |

    vclt.u32    q11, q11, q10  @ gteH/2 < fSZ(v)?
    add         r3, r0, #4*17
    vst1.32     d20, [r3]!     @ | writeback fSZ(v)
    vand        q11, q10, q11
    vst1.32     d21[0], [r3]   @ |
    vmax.u32    q10, q11, q13  @ make divisor 1 if not
    add         r3, r1, #8*8
    vstmia      r3, {q12}      @ min/max IR for flags
    vcvt.f32.u32 q10, q10
    vshl.u32    d13, d12, #16  @ | preparing gteH

    @ while NEON's busy we calculate some flags on ARM
    add         r2, r1, #8*2*3
    mov         lr, #0         @ gteFLAG
    ldmia       r2, {r4-r7}    @ min/max gteMAC|12
    subs        r2, r4, #1
    orrvs       lr, #(1<<31)|(1<<27)
    subs        r3, r5, #1
    orrvs       lr, #(1<<31)|(1<<26)
    adds        r2, r6, #1
    orrvs       lr, #(1<<30)
    adds        r3, r7, #1
    orrvs       lr, #(1<<29)
    ldr         r4, [r1, #0]   @ gteMAC3 v=0
    ldr         r5, [r1, #8*2] @ ... v=1
    ldr         r6, [r1, #8*4] @ ... v=2

    add         r3, r0, #4*(32+24)
    vld1.32     d4, [r3]       @ || gteOF|XY
    add         r3, r0, #4*(32+27)
    vld1.32     d6, [r3]       @ || gteDQAB

    @ divide
.if 1
    vrecpe.f32  q11, q10       @ inv
    vmovl.s32   q2, d4         @ || gteOFXY [64]
    vmovl.s32   q3, d6         @ || gteDQAB [64]
    vrecps.f32  q12, q10, q11  @ step
    vcvt.f32.u32 d13, d13      @ | gteH (float for div)
    vmul.f32    q11, q12, q11  @ better inv
    vdup.32     q13, d13[0]    @ |
@   vrecps.f32  q12, q10, q11  @ step
@   vmul.f32    q11, q12, q11  @ better inv
    vmul.f32    q10, q13, q11  @ result
.else
    vmovl.s32   q2, d4         @ || gteOFXY [64]
    vmovl.s32   q3, d6         @ || gteDQAB [64]
    vcvt.f32.u32 d13, d13      @ | gteH (float for div)
    vdup.32     q13, d13[0]    @ |

    vpush       {q0}
    vmov        q0, q10        @ to test against C code
    vdiv.f32    s0, s26, s0
    vdiv.f32    s1, s26, s1
    vdiv.f32    s2, s26, s2
    vmov        q10, q0
    vpop        {q0}
.endif

@ approximate gteMACx flags
@ in: rr 123 as gteMAC 123, *flags
@ trash: r2,r3
.macro do_mac_flags rr1 rr2 rr3 nflags pflags
    subs        r2, \rr1, #1
    subvcs      r3, \rr2, #1
    subvcs      r2, \rr3, #1
    orrvs       lr, #\nflags
    adds        r3, \rr1, #1
    addvcs      r2, \rr2, #1
    addvcs      r3, \rr3, #1
    orrvs       lr, #\pflags
.endm

    do_mac_flags r4, r5, r6, (1<<31)|(1<<25), (1<<27) @ MAC3
    orr         r7, r4, r5
    add         r4, r1, #8*8
    orr         r3, r7, r6
    ldmia       r4, {r7,r8,r10,r11} @ min/max IR

    movs        r3, r3, lsr #16
    orrne       lr, #(1<<31)
    orrne       lr, #(1<<18)   @ fSZ (limD)

@    vadd.f32     q10, q        @ adjust for vcvt rounding mode
    vcvt.u32.f32 q8, q10
    vmovl.s16   q9, d1         @ expand gteIR|12 v=0
    vmovl.s16   q10, d3        @ expand gteIR|12 v=1
    add         r6, r1, #8*10
    vstmia      r6, {q8}       @ wb quotients for flags (pre-limE)
    vqshl.u32   q8, #15
    vmovl.s16   q11, d10       @ expand gteIR|12 v=2
    vshr.u32    q8, #15        @ quotients (limE)
    vdup.32     d24, d16[0]
    vdup.32     d25, d16[1]
    vdup.32     d26, d17[0]    @ quotient (dup)

    mov         r4, r7, ror #16
    mov         r5, r10, ror #16
    subs        r2, r7, #1<<16
    addvcs      r3, r10, #1<<16
    orrvs       lr, #(1<<31)
    orrvs       lr, #(1<<23)   @ IR2/limB2
    subs        r2, r4, #1<<16
    addvcs      r3, r5, #1<<16
    mov         r4, r8, lsl #16
    mov         r5, r11, lsl #16
    orrvs       lr, #(1<<31)|(1<<24) @ IR1/limB1
    subs        r2, r4, #1<<16
    addvcs      r3, r5, #1<<16
    orrvs       lr, #(1<<22)   @ IR3/limB3

    vmull.s32   q9, d18, d24   @ gteIR|12 * quotient v=0
    vmull.s32   q10, d20, d25  @ ... v=1
    vmull.s32   q11, d22, d26  @ ... v=2
    vadd.s64    q9, q2         @ gteOF|XY + gteIR|12 * quotient
    vadd.s64    q10, q2        @ ... v=1
    vadd.s64    q11, q2        @ ... v=2
    vqmovn.s64  d18, q9        @ saturate to 32 v=0
    vqmovn.s64  d19, q10       @ ... v=1
    vqmovn.s64  d20, q11       @ ... v=2
    vmin.s32    d14, d18, d19  @ || find min/max fS|XY(v) [32]
    vmax.s32    d15, d18, d19  @ || for flags
    vmin.s32    d14, d20
    vmax.s32    d15, d20
    vqshl.s32   q11, q9, #5    @ 11bit precision, v=0,1
    vqshl.s32   d24, d20, #5   @ ... v=2
    vmull.s32   q13, d6, d17   @ | gteDQA * quotient v=2
    vpmin.s32   d16, d14, d15  @ || also find min/max in pair
    vpmax.s32   d17, d14, d15  @ ||
    vshr.s32    q11, #16+5     @ can't vqshrn because of insn
    vshr.s32    d24, #16+5     @ encoding doesn't allow 21 :(
    vqshl.s32   q7, #5         @ || min/max pairs shifted
    vsli.u64    d16, d17, #32  @ || pack in-pair min/max
    vadd.s64    d26, d7        @ | gteDQB + gteDQA * quotient
    vmovn.s32   d12, q11       @ fS|XY(v) [s16] v=0,1
    vmovn.s32   d13, q12       @ 3
    vstmia      r1, {d14-d16}  @ || other cacheline than quotients
    add         r3, r0, #4*12
    vst1.32     d12, [r3]!     @ writeback fS|XY v=0,1
    vst1.32     d13[0], [r3]

    vqshrn.s64  d26, q13, #12  @ | gteMAC0
    vmovl.u16   q5, d10        @ expand gteIR|123 v=2

    vmov.i32    d13, #0x1000
    vmax.s32    d12, d26, d30

    add         r3, r0, #4*24
    vst1.32     d26[0], [r3]!  @ gteMAC0
    vst1.32     d8, [r3]!      @ gteMAC123 (last iteration)
    vst1.32     d9[0], [r3]

    vmin.s32    d12, d13       @ | gteIR0

    @ ~6 cycles
    ldmia       r6, {r4-r6}    @ quotients
    orr         r4, r5
    orr         r4, r6
    add         r3, r0, #4*12
    movs        r4, r4, lsr #17
    orrne       lr, #(1<<31)   @ limE
    orrne       lr, #(1<<17)   @ limE

    add         r3, r0, #4*8
    vst1.32     d12[0], [r3]!  @ gteIR0
    vst1.32     d10, [r3]!     @ gteIR12
    vst1.32     d11[0], [r3]   @ ..3

    @ ~19 cycles
    ldmia       r1, {r4-r9}
    subs        r2, r4, #1<<21 @ min fSX
    addvcs      r3, r6, #1<<21 @ max fSX
    orrvs       lr, #(1<<31)   @ limG1
    orrvs       lr, #(1<<14)
    subs        r2, r5, #1<<21 @ min fSY
    addvcs      r3, r7, #1<<21 @ max fSY
    orrvs       lr, #(1<<31)   @ limG2
    orrvs       lr, #(1<<13)
    adds        r2, r9, #1
    orrvs       lr, #(1<<31)   @ F
    orrvs       lr, #(1<<16)
    subs        r3, r8, #1
    orrvs       lr, #(1<<31)   @ F

    ldr         r4, [r0, #4*24] @ gteMAC0
    orrvs       lr, #(1<<15)

    adds        r3, r4, #1
    orrvs       lr, #(1<<16)
    orrvs       lr, #(1<<31)   @ F
    subs        r2, r4, #1
    orrvs       lr, #(1<<15)
    orrvs       lr, #(1<<31)   @ F
    cmp         r4, #0x1000
    orrhi       lr, #(1<<12)

    str         lr, [r0, #4*(32+31)] @ gteFLAG

    pop         {r4-r11,pc}

@ vim:filetype=armasm

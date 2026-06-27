/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"
#include "new_dynarec/linkage_offsets.h"

#ifdef FLAGLESS
#define gteRTPS_sf1lm0_neon gteRTPS_sf1lm0_nf_neon
#define gteRTPT_sf1lm0_neon gteRTPT_sf1lm0_nf_neon
#define F(...)
#else
#define F(code...) code
#endif
#ifdef __MACH__
#define gteRTPS_sf1lm0   ESYM(gteRTPS_sf1lm0)
#define gteRTPT_sf1lm0   ESYM(gteRTPT_sf1lm0)
#endif

.syntax unified
.text
.align 2

@ load gteR*, gteTR* (what's needed for MAC|12)
@ in: r0 - context
@ trash: r3
.macro rtpx_preload
    add         r3, r0, #4*32
    vld1.16     {d0-d1}, [r3, :64] @ gteR
    add         r3, r0, #4*(32+5)
    ldmia       r3, {r1,r2,r12}
    vld1.32     {d4}, [r3]         @ gteTR|XY
    vext.16     d1, d0, d1, #3     @ xx32 -> x321
    vshll.s32   q2, d4, #12        @ gteTR|XY
.endm

.align 3

FUNCTION(gteRTPS_sf1lm0_neon): @ r0=CP2 (d,c),
    vld1.16     {d16}, [r0, :64] @ VXYZ(0)
    rtpx_preload               @ gteTR|XYZ -> q2,q3, ...
    sub         r1, #0x7f000000
    sub         r2, #0x7f000000
    cmp         r1, #0x02000000
    sub         r1, r12, #0x7f000000
    cmphi       r2, #0x02000000
    cmphi       r1, #0x02000000
    bls         gteRTPS_sf1lm0 @ if MAC123 can overflow, don't bother

    push        {r4-r10,lr}
@    fmrx        r4, fpscr      @ vmrs? at least 40 cycle hit
    add         r3, r0, #4*17  @ gteSZ*
    ldr         r4, [r0, #LO_cop2_to_gte_divider_tab_ptr] @ gte_divider_tab
    mov         r2, #0
    @pld         [r4]

    vld1.32     {q12}, [r3]    @ d24,d25 gteSZ|123x
    vmov.16     d16[3], r2     @ kill the unused upper vector
                               ldmia   r0, {r2, lr}
    vmull.s16   q10, d0, d16
                               ldr     r6, [r0, #4*(32+4)] @ R33
    vmull.s16   q11, d1, d16
                               ldr     r5, [r0, #4*(32+3)] @ R31,R32
                               smulbb  r7, lr, r6
                               @pld     [r4, #64]
    add         r3, r0, #4*16  @ | gteSZ*
    vst1.32     {d24}, [r3, :64]! @ gteSZ|12 to |01
    vpadal.s32  d4, d20
    vpadal.s32  d5, d22
                               mov     r8, r7, asr #31
    vst1.32     d25[0], [r3, :32] @ gteSZ|3 to |2
    vpadal.s32  d4, d21
    vpadal.s32  d5, d23
                               smlald  r7, r8, r2, r5
                               @pld     [r4, #64*2]
    vshrn.s64   d16, q2, #12

    add         r3, r0, #4*25
    vst1.32     d16, [r3]      @ save gteMAC|12
                               add     r7, r12, r7, lsr #12
                               add     r8, r7, r8, lsl #(32-12) @ gteMAC3
                               ldrh    r10, [r0, #4*(32+26)] @ gteH
                               mov     lr, #0         @ gteFLAG
                               usat    r9, #16, r8    @ r9=SZ3
    vqmovn.s32  d20, q8        @ gteIR|12
                               clz     r2, r9
                            F( cmp     r9, r8       )
                            F( orrne   lr, #(1<<18) ) @ fSZ (limD)
    add         r3, r0, #4*9
    vmovl.s16   q9, d20        @ expand gteIR|12
                               sub     r2, #16        @ r2=shift
    vst1.32     d18, [r3]      @ save gteIR

    mov         r7, r9, lsl r2
    ssat        r3, #16, r8          @ gteIR3
    bic         r7, #0x8000
    add         r5, r7, #0x40

    mov         r6, #0
    cmp         r10, r9, lsl #1      @ gteH < SZ3*2
    mvnge       r10, #0              @ cause a large unsigned value
 F( orrge       lr, #(1<<17) )

    ldrblt      r6, [r4, r5, lsr #7] @ gte_divider_tab lut
    movw        r5, #0x101
    orr         r7, #0x8000
    add         r6, r5
    mov         r5, #0x80
    mls         r7, r6, r7, r5
    ldr         r1, [r0, #4*(32+24)] @ gteOFX
 F( cmp         r3, r8       )
 F( orrne       lr, #(1<<22) )       @ IR3/limB3
    bic         r7, #0xfe000000
    lsr         r7, #8
    ldr         r12, [r0, #4*(32+25)] @ gteOFY
    mla         r7, r6, r7, r5
    str         r8, [r0, #4*27]      @ gteMAC3
    mov         r4, r10, lsl r2
    lsr         r7, #8
    mov         r5, #0x8000
    mov         r8, #0
    smlal       r5, r8, r7, r4
    str         r3, [r0, #4*11]      @ gteIR3
    movw        r4, #0xffff
    str         r9, [r0, #4*19]      @ SZ3
    movt        r4, #0x0001
    uxth        r5, r5, ror #16
#ifndef FLAGLESS
    add         r3, r0, #4*25
    ldmia       r3, {r6,r7}          @ gteMAC|12
#endif
    ldr         r2, [r0, #4*9]       @ gteIR1
    pkhbt       r5, r5, r8, lsl #16
    ldr         r3, [r0, #4*10]      @ gteIR2
    cmp         r5, r4
    movhs       r5, r4               @ quotient
    mov         r4, r1,  asr #31
    mov         r8, r12, asr #31
    smlal       r1, r4, r2, r5       @ mac0_0 = gteOFX + gteIR1 * quotient @ slow :(
    smlal       r12,r8, r3, r5       @ mac0_1 = gteOFY + gteIR2 * quotient
    ldr         r9, [r0, #4*(32+28)] @ gteDQB
 F( cmp         r2, r6       )
 F( orrne       lr, #(1<<24) )       @ IR1/limB1
 F( cmp         r3, r7       )
 F( orrne       lr, #(1<<23) )       @ IR2/limB2
    ldrsh       r2, [r0, #4*(32+27)] @ gteDQA
    mov         r7, r9, asr #31
    ldr         r3, [r0, #4*13]      @ gteSXY1
    smlal       r9, r7, r2, r5       @ mac0_2 = gteDQB + gteDQA * quotient
    ldr         r5, [r0, #4*14]      @ gteSXY2
    uxth        r1, r1,  ror #16
    uxth        r6, r12, ror #16
    str         r3, [r0, #4*12]      @ gteSXY0
    pkhbt       r1, r1, r4, lsl #16  @ mac0_0 >> 16
    pkhbt       r6, r6, r8, lsl #16
    mov         r2, r9, lsr #12
    str         r5, [r0, #4*13]      @ gteSXY1
    orr         r2, r2, r7, lsl #(32-12)
    str         r9, [r0, #4*24]      @ gteMAC0
#ifndef FLAGLESS
    cmp         r1, #0x8000
    cmplt       r6, #0x8000
    cmplt       r2, #0x80000
    orrge       lr, #(1<<16)         @ mac0 positive overflow
    cmn         r1, #0x8000
    cmnge       r6, #0x8000
    cmnge       r2, #0x80000
    orrlt       lr, #(1<<15)
#endif
    ssat        r3, #11, r1
    ssat        r4, #11, r6
    asr         r5, r2, #31
    pkhbt       r7, r3, r4, lsl #16
 F( cmp         r1, r3       )
 F( orrne       lr, #(1<<14) )       @ limG1
 F( cmp         r4, r6       )
 F( orrne       lr, #(1<<13) )       @ limG2
    str         r7, [r0, #4*14]      @ gteSXY2

    cmp         r2, #0x1000
    bic         r2, r5
    movgt       r2, #0x1000
 F( orrhi       lr, #(1<<12) )       @ limH
    strh        r2, [r0, #4*8]       @ gteIR0

#ifndef FLAGLESS
    tst         lr, #0x01800000
    tsteq       lr, #0x0007e000
    orrne       lr, #(1<<31)
    str         lr, [r0, #4*(32+31)] @ gteFLAG
#endif
    pop         {r4-r10,pc}
    .size       gteRTPS_sf1lm0_neon, .-gteRTPS_sf1lm0_neon

.align 3

FUNCTION(gteRTPT_sf1lm0_neon): @ r0=CP2 (d,c),
    vld1.16     {d16-d18}, [r0, :64] @ VXYZ() 123
    rtpx_preload
    sub         r1, #0x7f000000
    sub         r2, #0x7f000000
    cmp         r1, #0x02000000
    sub         r1, r12, #0x7f000000
    cmphi       r2, #0x02000000
    cmphi       r1, #0x02000000
    bls         gteRTPT_sf1lm0 @ if MAC123 can overflow, don't bother

    push        {r4-r11,lr}
    vmov        q1, q2
    mov         r2, #0
    vmov        q3, q2
    sub         sp, #12        @ ir flags+align

    vmov.16     d16[3], r2     @ kill the unused upper vector
    vmov.16     d17[3], r2
    vmov.16     d18[3], r2
    vmull.s16   q10, d0, d16
    vmull.s16   q11, d1, d16
                               ldmia   r0, {r2-r7}
    vmull.s16   q12, d0, d17
    vmull.s16   q13, d1, d17
                               ldr     r1, [r0, #4*(32+4)] @ R33
    vmull.s16   q14, d0, d18
    vmull.s16   q15, d1, d18
                               ldr     r8, [r0, #4*(32+3)] @ R31,R32
    vpadal.s32  d2, d20
                               smulbb  r9,  r3, r1
    vpadal.s32  d3, d22
                               smulbb  r10, r5, r1
    vpadal.s32  d4, d24
                               smulbb  r11, r7, r1
    vpadal.s32  d5, d26
    vpadal.s32  d6, d28
    vpadal.s32  d7, d30
                               mov     r3, r9, asr #31
    vpadal.s32  d2, d21
                               mov     r5, r10, asr #31
    vpadal.s32  d3, d23
                               mov     r7, r11, asr #31
    vpadal.s32  d4, d25
                               smlald  r9,  r3, r2, r8
    vpadal.s32  d5, d27
                               smlald  r10, r5, r4, r8
    vpadal.s32  d6, d29
                               smlald  r11, r7, r6, r8
    vpadal.s32  d7, d31
                               add     r9,  r12, r9,  lsr #12
    vshrn.s64   d0, q1, #12    @ gteMAC|12 v0
                               add     r10, r12, r10, lsr #12
    vshrn.s64   d1, q2, #12    @ gteMAC|12 v1
                               add     r11, r12, r11, lsr #12
    vshrn.s64   d2, q3, #12    @ gteMAC|12 v2
                               add     r1, r9,  r3, lsl #(32-12) @ mac3 v0
    vqmovn.s32  d4, q0         @ gteIR|12  v0 v1
                               add     r2, r10, r5, lsl #(32-12) @ mac3 v1
    vqmovn.s32  d5, q1         @ gteIR|12  v2
                               add     r3, r11, r7, lsl #(32-12) @ mac3 v2
    add         r12, r0, #4*25
    vst1.32     d2, [r12]      @ save gteMAC|12 v2
                               usat    r4, #16, r1    @ SZ1
    vmovl.s16   q3, d4         @ expand gteIR|12 v0 v1
                               ssat    r7, #16, r1    @ ir3 v0
    vmovl.s16   q4, d5         @ expand gteIR|12 v2
    add         r12, r0, #4*12
    vst1.32     d4, [r12, :64] @ save gteIR|12 v0 v1 to SXY0, arm will fix it later
    vceq.i32    q5, q0, q3
    vceq.i32    d12, d2, d8
    add         r12, r0, #4*9
    vst1.32     d8, [r12]      @ save gteIR|12 v2
    vand        d10, d11
    vand        d10, d12
    vmvn        d10, d10       @ ~0 if gteIR|12 != gteMAC|12
    vst1.32     d10, [sp]

    ssat        r8, #16, r2          @ ir3 v1
    ssat        r10,#16, r3          @ ir3 v2
    usat        r5, #16, r2          @ SZ2
    usat        r6, #16, r3          @ SZ3
 F( cmp         r7, r1       )
    mov         lr, #0               @ gteFLAG
 F( cmpeq       r8, r2       )
 F( cmpeq       r10,r3       )
 F( orrne       lr, #(1<<22) )       @ IR3/limB3
    clz         r7, r4
    clz         r8, r5
 F( cmp         r4, r1       )
    clz         r9, r6
 F( cmpeq       r5, r2       )
 F( cmpeq       r6, r3       )
 F( orrne       lr, #(1<<18) )       @ fSZ (limD)
    sub         r7, #16              @ shift
    sub         r8, #16
    sub         r9, #16
    orr         r12,r7, r8, lsl #8
    mov         r7, r4, lsl r7
    mov         r8, r5, lsl r8
    orr         r12,r12,r9, lsl #16  @ save shifts
    mov         r9, r6, lsl r9
    str         r10, [r0, #4*11]     @ gteIR3
    add         r2, r0, #4*16
    str         r3, [r0, #4*27]      @ gteMAC3
    bic         r7, #0x8000
    ldr         r3, [r0, #4*19]      @ SZ3 old
    bic         r8, #0x8000
    ldrh        r11, [r0, #4*(32+26)] @ gteH
    bic         r9, #0x8000
    ldr         r10, [r0, #LO_cop2_to_gte_divider_tab_ptr] @ gte_divider_tab
    add         r1, r7, #0x40
    stmia       r2, {r3-r6}          @ SZ1-SZ3
    add         r2, r8, #0x40
    add         r3, r9, #0x40
    cmp         r11, r4, lsl #1
    mov         r4, r11
    ldrblt      r1, [r10, r1, lsr #7] @ gte_divider_tab lut
 F( orrge       lr, #(1<<17) )
    mvnge       r4, #0
    movge       r1, #0               @ adjusted gteH v0
    cmp         r11, r5, lsl #1
    mov         r5, r11
    ldrblt      r2, [r10, r2, lsr #7]
 F( orrge       lr, #(1<<17) )
    mvnge       r5, #0
    movge       r2, #0
    cmp         r11, r6, lsl #1
    movw        r6, #0x101
    ldrblt      r3, [r10, r3, lsr #7]
 F( orrge       lr, #(1<<17) )
    mvnge       r11, #0
    movge       r3, #0
    @ from this point it may seem neon could be used for parallelism,
    @ but there's just no way to hide the huge latency on c-a8,
    @ also getting results back for flags is a problem
    mov         r10, #0x080
    orr         r7, #0x8000
    orr         r8, #0x8000
    orr         r9, #0x8000
    add         r1, r6
    add         r2, r6
    add         r3, r6
    mls         r7, r1, r7, r10
    mls         r8, r2, r8, r10
    mls         r9, r3, r9, r10
    uxtb        r6, r12, ror #8
    lsl         r4, r12
    uxtb        r12,r12, ror #16
    lsl         r5, r6
    mov         r6, r11, lsl r12
    bic         r7, #0xfe000000
    bic         r8, #0xfe000000
    bic         r9, #0xfe000000
    lsr         r7, #8
    lsr         r8, #8
    lsr         r9, #8
    mla         r1, r1, r7, r10
    mla         r2, r2, r8, r10
    mla         r3, r3, r9, r10
    mov         r7, #0x8000
    mov         r8, #0x8000
    mov         r9, #0x8000
    mov         r10, #0
    mov         r11, #0
    mov         r12, #0
    lsr         r1, #8
    lsr         r2, #8
    lsr         r3, #8
    smlal       r7, r10, r1, r4
    smlal       r8, r11, r2, r5
    smlal       r9, r12, r3, r6
    movw        r6, #0xffff
    ldrd        r4, r5, [sp]         @ ir flags
    movt        r6, #0x0001
    ldrd        r2, r3, [r0, #4*(32+24)] @ gteOF|XY
    and         r4, #(1<<24)
    and         r5, #(1<<23)
    uxth        r7, r7, ror #16
    uxth        r8, r8, ror #16
    orr         lr, r4
    uxth        r9, r9, ror #16
    orr         lr, r5
    pkhbt       r7, r7, r10, lsl #16
    pkhbt       r8, r8, r11, lsl #16
    pkhbt       r9, r9, r12, lsl #16
    cmp         r7, r6
    movhs       r7, r6               @ quotient
    cmp         r8, r6
    movhs       r8, r6
    cmp         r9, r6
    movhs       r9, r6
    mov         r1, r2, asr #31
    ldr         r10, [r0, #4*12]     @ gteIR|12 v0
    mov         r4, r3, asr #31
    mov         r5, r2
    sxth        r11, r10, ror #16
    sxth        r10, r10
    mov         r6, r1
    mov         r12,r4
    smlal       r2, r1, r10, r7      @ mac0_0 = gteOFX + gteIR1 * quotient @ slow :(
    ldr         r10, [r0, #4*13]     @ gteIR|12 v1
    smlal       r3, r4, r11, r7      @ mac0_1 = gteOFY + gteIR2 * quotient
    ldr         r7, [r0, #4*(32+25)] @ reload gteOFY
    sxth        r11, r10, ror #16
    sxth        r10, r10
    smlal       r5, r6, r10, r8      @ mac1_0
    smlal       r7, r12,r11, r8      @ mac1_1
    ldrd        r10, r11, [r0, #4*9] @ gteIR|12 v2
    uxth        r2, r2, ror #16
    uxth        r3, r3, ror #16
    uxth        r5, r5, ror #16
    uxth        r7, r7, ror #16
    pkhbt       r1, r2, r1, lsl #16  @ mac0_0 >> 16
    pkhbt       r2, r3, r4, lsl #16
    pkhbt       r3, r5, r6, lsl #16
    pkhbt       r4, r7, r12,lsl #16
 F( cmp         r1, #0x8000  )
    ssat        r6, #11, r1
 F( cmplt       r2, #0x8000  )
 F( cmplt       r3, #0x8000  )
 F( cmplt       r4, #0x8000  )
 F( orrge       lr, #(1<<16) )       @ mac0 positive overflow
 F( cmn         r1, #0x8000  )
    ssat        r7, #11, r2
 F( cmnge       r2, #0x8000  )
 F( cmnge       r3, #0x8000  )
 F( cmnge       r4, #0x8000  )
 F( orrlt       lr, #(1<<15) )
    ssat        r8, #11, r3
    ssat        r12,#11, r4
 F( cmp         r1, r6       )
 F( orrne       lr, #(1<<14) )       @ limG1 v0
 F( cmp         r2, r7       )
 F( orrne       lr, #(1<<13) )       @ limG2
 F( cmp         r3, r8       )
    ldrd        r2, r3, [r0, #4*(32+24)] @ reload gteOF|XY
 F( orrne       lr, #(1<<14) )       @ limG1 v1
 F( cmp         r4, r12      )
    ldr         r5, [r0, #4*(32+28)] @ gteDQB
 F( orrne       lr, #(1<<13) )       @ limG2
    pkhbt       r6, r6, r7, lsl #16
    pkhbt       r7, r8, r12,lsl #16
    mov         r1, r2, asr #31
    strd        r6, r7, [r0, #4*12]  @ gteSXY|01
    mov         r4, r3, asr #31
    ldrsh       r6, [r0, #4*(32+27)] @ gteDQA
    smlal       r2, r1, r10, r9      @ mac2_0 = gteOFX + gteIR1 * quotient
    mov         r12,r5, asr #31
    smlal       r3, r4, r11, r9      @ mac2_1 = gteOFY + gteIR2 * quotient
    smlal       r5, r12,r6,  r9      @ mac2_2 = gteDQB + gteDQA * quotient
    uxth        r2, r2, ror #16
    uxth        r3, r3, ror #16
    str         r5, [r0, #4*24]      @ gteMAC0
    mov         r5, r5, lsr #12
    pkhbt       r1, r2, r1, lsl #16
    pkhbt       r2, r3, r4, lsl #16
    orr         r3, r5, r12,lsl #(32-12)
 F( cmp         r1, #0x8000  )
 F( cmplt       r2, #0x8000  )
 F( cmplt       r3, #0x80000 )
 F( orrge       lr, #(1<<16) )
 F( cmn         r1, #0x8000  )
 F( cmnge       r2, #0x8000  )
 F( cmnge       r3, #0x80000 )
 F( orrlt       lr, #(1<<15) )
    ssat        r4, #11, r1
    ssat        r5, #11, r2
    asr         r6, r3, #31
    pkhbt       r7, r4, r5, lsl #16
 F( cmp         r1, r4       )
 F( orrne       lr, #(1<<14) )       @ limG1
 F( cmp         r2, r5       )
 F( orrne       lr, #(1<<13) )       @ limG2
    str         r7, [r0, #4*14]      @ gteSXY2

    cmp         r3, #0x1000
    bic         r3, r6
    movgt       r3, #0x1000
 F( orrhi       lr, #(1<<12) )       @ limH
    add         sp, #12
    strh        r3, [r0, #4*8]       @ gteIR0
#ifndef FLAGLESS
    tst         lr, #0x01800000
    tsteq       lr, #0x0007e000
    orrne       lr, #(1<<31)
    str         lr, [r0, #4*(32+31)] @ gteFLAG
#endif
    pop         {r4-r11,pc}
    .size	gteRTPT_sf1lm0_neon, .-gteRTPT_sf1lm0_neon

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
@ vim:filetype=armasm

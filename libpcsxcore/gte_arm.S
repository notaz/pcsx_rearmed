/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"
#include "new_dynarec/linkage_offsets.h"

#ifdef FLAGLESS
#define gteRTPS_sf1lm0_arm gteRTPS_sf1lm0_nf_arm
#define gteRTPT_sf1lm0_arm gteRTPT_sf1lm0_nf_arm
#define F(...)
#else
#define F(code...) code
#endif
#ifdef HAVE_ARMV6
#define V6(code...) code
#else
#define V6(...)
#endif
#ifdef __MACH__
#define gteRTPS_sf1lm0   ESYM(gteRTPS_sf1lm0)
#define gteRTPT_sf1lm0   ESYM(gteRTPT_sf1lm0)
#define gteMVMVA_generic ESYM(gteMVMVA_generic)
#endif

.syntax unified
.text
.align 2

@ prepare work reg for ssatx
@ in: wr reg, bit to saturate to
.macro ssatx_prep wr bit
#ifndef HAVE_ARMV6
    mov      \wr, #(1<<(\bit-1))
#endif
.endm

.macro ssatx rd rs wr bit
#ifdef HAVE_ARMV6
    ssat     \rd, #\bit, \rs
#else
    cmp      \rs, \wr
.if \rd != \rs
    movlt    \rd, \rs
.endif
    subge    \rd, \wr, #1
    cmn      \rd, \wr
    rsblt    \rd, \wr, #0
#endif
.endm

@ prepare work reg for usatx (sat to 0..2^(bit-1))
@ in: wr reg, bit to saturate to
.macro usatx_prep wr bit
#ifdef HAVE_ARMV6
    mov      \wr, #(1<<(\bit-1))
#endif
.endm

.macro usatx rd rs wr bit
#ifdef HAVE_ARMV6
    usat     \rd, #\bit, \rs
#else
    movs     \rd, \rs
    movmi    \rd, #0
    cmp      \rd, \wr
    subge    \rd, \wr, #1
#endif
.endm

.macro usat16_ rd rs
#ifdef HAVE_ARMV6
    usat     \rd, #16, \rs
#else
    subs     \rd, \rs, #0
    movlt    \rd, #0
    cmp      \rd, #0x10000
    movge    \rd, #0x0ff00
    orrge    \rd, #0x000ff
#endif
.endm

.macro ldrd_ rd1 rd2 rs ofs
#ifdef HAVE_ARMV5
    ldrd     \rd1, \rd2, [\rs, #(\ofs)]
#else
    ldr      \rd1, [\rs, #(\ofs)]
    ldr      \rd2, [\rs, #(\ofs+4)]
#endif
.endm

.macro strd_ rd1 rd2 rs ofs
#ifdef HAVE_ARMV5
    strd     \rd1, \rd2, [\rs, #(\ofs)]
#else
    str      \rd1, [\rs, #(\ofs)]
    str      \rd2, [\rs, #(\ofs+4)]
#endif
.endm

#ifdef HAVE_ARMV5

FUNCTION(gteRTPS_sf1lm0_arm):     @ r0=CP2 (d,c),
    push     {r4-r11,lr}
    ldr      r1, [r0, #4*(32+5)]  @ gteTR|XYZ
    ldrd_    r2, r3, r0, 4*(32+6)
    sub      r4, r1, #0x7f000000
    sub      r5, r2, #0x7f000000
    sub      r6, r3, #0x7f000000
    cmp      r4, #0x02000000
    cmphi    r5, #0x02000000
    cmphi    r6, #0x02000000
    bls      call_gteRTPS_sf1lm0  @ if MAC123 can overflow, don't bother

    add      r12,r0, #4*32        @ gteR*
    asr      r6, r1, #20
#ifdef HAVE_ARMV6
    ldmia    r12, {r9-r12}        @ MX*
#else
    ldmia    r12, {r9-r12,r14}    @ MX*
#endif
    asr      r7, r2, #20
    asr      r8, r3, #20
    ldrd_    r4, r5, r0, 0        @ gteV0|XYZ
    lsl      r1, #12              @ expand to 64bit
    lsl      r2, #12
#ifdef HAVE_ARMV6
    mov      r14, r10, lsr #16
    lsl      r3, #12
    orr      r14, r14, r11, lsl #16
    smlald   r1, r6, r9,  r4      @ r09l=MX11 * vx + r09h=MX12 * vy
    smlald   r2, r7, r14, r4      @ r10h=MX21 * vx + r11l=MX22 * vy
    smlald   r3, r8, r12, r4      @ r12l=MX31 * vx + r12h=MX32 * vy
    ldr      r14, [r0, #4*(32+4)] @ gteR33
#else
    lsl      r3, #12
    smlalbb  r1, r6, r9,  r4      @ r09l=MX11 * vx
    smlaltb  r2, r7, r10, r4      @ r10h=MX21 * vx
    smlalbb  r3, r8, r12, r4      @ r12l=MX31 * vx
    smlaltt  r1, r6, r9,  r4      @ r09h=MX12 * vy
    smlalbt  r2, r7, r11, r4      @ r11l=MX22 * vy
    smlaltt  r3, r8, r12, r4      @ r12h=MX32 * vy
#endif
    smlalbb  r1, r6, r10, r5      @ r10l=MX13 * vz
    smlaltb  r2, r7, r11, r5      @ r11h=MX23 * vz
    smlalbb  r3, r8, r14, r5      @ r14l=MX33 * vz
    mov      lr, #0               @ gteFLAG
    ssatx_prep r12, 16
    lsr      r1, #12
    lsr      r2, #12
    lsr      r3, #12
    orr      r1, r1, r6, lsl #20  @ gteMAC1
    orr      r2, r2, r7, lsl #20  @ gteMAC2
    orr      r3, r3, r8, lsl #20  @ gteMAC3
    ssatx    r10,r1, r12, 16      @ gteIR1
    usat16_  r9, r3               @ r9=SZ3
    ssatx    r11,r2, r12, 16      @ gteIR2
    ssatx    r6, r3, r12, 16      @ gteIR3
    strd_    r2, r3, r0, 4*26     @ gteMAC|23
    ldr      r12,[r0, #LO_cop2_to_gte_divider_tab_ptr] @ gte_divider_tab
    clz      r8, r9
    str      r1, [r0, #4*25]      @ gteMAC1
 F( cmp      r9, r3       )
 F( orrne    lr, #(1<<18) )       @ fSZ (limD)
 F( cmp      r6, r3       )
    ldrh     r3, [r0, #4*(32+26)] @ gteH
 F( orrne    lr, #(1<<22) )       @ IR3/limB3
    sub      r8, #16              @ r8=shift
    strd_    r10,r11,r0, 4*9      @ gteIR|12
    mov      r7, r9, lsl r8
    str      r6, [r0, #4*11]      @ gteIR3
    bic      r7, #0x8000
    add      r5, r7, #0x40

    mov         r6, #0
    cmp         r3, r9, lsl #1       @ gteH < SZ3*2
    mvnge       r3, #0               @ cause a large unsigned value
 F( orrge       lr, #(1<<17) )
    ldrblt      r6, [r12, r5, lsr #7] @ gte_divider_tab lut
    mov         r5, #0x80
    orr         r7, #0x8000
    add         r6, #0x100
    rsb         r7, r7, #0
    add         r6, #0x001
    mla         r7, r6, r7, r5
 F( cmp         r10,r1       )
 F( orrne       lr, #(1<<24) )       @ IR1/limB1
    ldr         r1, [r0, #4*(32+24)] @ gteOFX
    bic         r7, #0xfe000000
    lsr         r7, #8
    ldr         r12,[r0, #4*(32+25)] @ gteOFY
    mla         r7, r6, r7, r5
    ldr         r6, [r0, #4*17]      @ old SZ1
 F( cmp         r11,r2       )
 F( orrne       lr, #(1<<23) )       @ IR2/limB2
    mov         r4, r3, lsl r8
    str         r6, [r0, #4*16]
    lsr         r7, #8
    mov         r5, #0x8000
    mov         r8, #0
    smlal       r5, r8, r7, r4
    ldrd_       r2, r3, r0, 4*18     @ old SZ2,SZ3
    mov         r4, #0x20000
    str         r9, [r0, #4*19]      @ SZ3
    strd_       r2, r3, r0, 4*17
    mov         r5, r5, lsr #16
    orr         r5, r5, r8, lsl #16
    cmp         r5, r4
    subhs       r5, r4, #1           @ quotient
    mov         r4, r1,  asr #31
    mov         r8, r12, asr #31
    smlal       r1, r4, r10, r5      @ mac0_0 = gteOFX + gteIR1 * quotient @ slow :(
    smlal       r12,r8, r11, r5      @ mac0_1 = gteOFY + gteIR2 * quotient
    ldr         r9, [r0, #4*(32+28)] @ gteDQB
    ldrsh       r2, [r0, #4*(32+27)] @ gteDQA
    mov         r7, r9, asr #31
    ldr         r3, [r0, #4*13]      @ gteSXY1
    smlal       r9, r7, r2, r5       @ mac0_2 = gteDQB + gteDQA * quotient
    ldr         r5, [r0, #4*14]      @ gteSXY2
    mov         r1, r1,  lsr #16
    mov         r6, r12, lsr #16
    str         r3, [r0, #4*12]      @ gteSXY0
    orr         r1, r1, r4, lsl #16  @ mac0_0 >> 16
    orr         r6, r6, r8, lsl #16
    mov         r2, r9, lsr #12
    str         r5, [r0, #4*13]      @ gteSXY1
    orr         r2, r2, r7, lsl #(32-12)
    str         r9, [r0, #4*24]      @ gteMAC0
 F( cmp         r1, #0x8000  )
V6( ssat        r3, #11, r1  )
 F( cmplt       r6, #0x8000  )
 F( cmplt       r2, #0x80000 )
 F( orrge       lr, #(1<<16) )       @ mac0 positive overflow
 F( cmn         r1, #0x8000  )
V6( ssat        r4, #11, r6  )
    ssatx_prep  r12, 11
 F( cmnge       r6, #0x8000  )
 F( cmnge       r2, #0x80000 )
 F( orrlt       lr, #(1<<15) )
#ifdef HAVE_ARMV6
    asr         r5, r2, #31
    pkhbt       r7, r3, r4, lsl #16
#else
    ssatx       r3, r1, r12, 11
    ssatx       r4, r6, r12, 11
    mov         r5, r3, lsl #16
    mov         r7, r4, lsl #16
    orr         r7, r7, r5, lsr #16
    asr         r5, r2, #31
#endif
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
    pop         {r4-r11,pc}
call_gteRTPS_sf1lm0:
    pop         {r4-r11,lr}
    b           gteRTPS_sf1lm0
    .size       gteRTPS_sf1lm0_arm, .-gteRTPS_sf1lm0_arm


FUNCTION(gteRTPT_sf1lm0_arm):     @ r0=CP2 (d,c),
    push     {r4-r11,lr}
    ldr      r1, [r0, #4*(32+5)]  @ gteTR|XYZ
    ldrd_    r2, r3, r0, 4*(32+6)
    sub      r4, r1, #0x7f000000
    sub      r5, r2, #0x7f000000
    sub      r6, r3, #0x7f000000
    cmp      r4, #0x02000000
    cmphi    r5, #0x02000000
    cmphi    r6, #0x02000000
    bls      call_gteRTPT_sf1lm0  @ if MAC123 can overflow, don't bother

    add      r12,r0, #4*32        @ gteR*
    asr      r6, r1, #20
#ifdef HAVE_ARMV6
    ldmia    r12, {r9-r12}        @ MX*
#else
    ldmia    r12, {r9-r12,r14}    @ MX*
#endif
    sub      sp, #4*(8+3*2)       @ make space for shifted_TR+mac123*v01
    asr      r7, r2, #20
    asr      r8, r3, #20
    ldrd_    r4, r5, r0, 0        @ gteV0|XYZ
    lsl      r1, #12              @ expand to 64bit
    lsl      r2, #12
#ifdef HAVE_ARMV6
    lsl      r3, #12
    mov      r14, r10, lsr #16
    stmia    sp, {r1-r3,r6-r8}
    orr      r14, r14, r11, lsl #16
    smlald   r1, r6, r9,  r4      @ r09l=MX11 * vx + r09h=MX12 * vy
    smlald   r2, r7, r14, r4      @ r10h=MX21 * vx + r11l=MX22 * vy
    str      r14, [sp, #4*6]
    smlald   r3, r8, r12, r4      @ r12l=MX31 * vx + r12h=MX32 * vy
    ldr      r14, [r0, #4*(32+4)] @ gteR33
#else
    lsl      r3, #12
    stmia    sp, {r1-r3,r6-r8}
    smlalbb  r1, r6, r9,  r4      @ r09l=MX11 * vx
    smlaltb  r2, r7, r10, r4      @ r10h=MX21 * vx
    smlalbb  r3, r8, r12, r4      @ r12l=MX31 * vx
    smlaltt  r1, r6, r9,  r4      @ r09h=MX12 * vy
    smlalbt  r2, r7, r11, r4      @ r11l=MX22 * vy
    smlaltt  r3, r8, r12, r4      @ r12h=MX32 * vy
#endif
    smlalbb  r1, r6, r10, r5      @ r10l=MX13 * vz
    smlaltb  r2, r7, r11, r5      @ r11h=MX23 * vz
    smlalbb  r3, r8, r14, r5      @ r14l=MX33 * vz
    ldrd_    r4, r5, r0, 4*2      @ gteV1|XYZ
#ifdef HAVE_ARMV6
    ldr      r14, [sp, #4*6]
#endif
    lsr      r1, #12
    lsr      r2, #12
    lsr      r3, #12
    orr      r1, r1, r6, lsl #20  @ mac1 v0
    add      r6, sp, #4*8
    orr      r2, r2, r7, lsl #20  @ mac2 v0
    orr      r3, r3, r8, lsl #20  @ mac3 v0
    stmia    r6, {r1-r3}          @ save them
    ldmia    sp, {r1-r3,r6-r8}    @ reload shifted gteTR|XYZ
#ifdef HAVE_ARMV6
    smlald   r1, r6, r9,  r4      @ r09l=MX11 * vx + r09h=MX12 * vy
    smlald   r2, r7, r14, r4      @ r10h=MX21 * vx + r11l=MX22 * vy
    smlald   r3, r8, r12, r4      @ r12l=MX31 * vx + r12h=MX32 * vy
    ldr      r14, [r0, #4*(32+4)] @ gteR33
#else
    smlalbb  r1, r6, r9,  r4      @ r09l=MX11 * vx
    smlaltb  r2, r7, r10, r4      @ r10h=MX21 * vx
    smlalbb  r3, r8, r12, r4      @ r12l=MX31 * vx
    smlaltt  r1, r6, r9,  r4      @ r09h=MX12 * vy
    smlalbt  r2, r7, r11, r4      @ r11l=MX22 * vy
    smlaltt  r3, r8, r12, r4      @ r12h=MX32 * vy
#endif
    smlalbb  r1, r6, r10, r5      @ r10l=MX13 * vz
    smlaltb  r2, r7, r11, r5      @ r11h=MX23 * vz
    smlalbb  r3, r8, r14, r5      @ r14l=MX33 * vz
    ldrd_    r4, r5, r0, 4*4      @ gteV2|XYZ
#ifdef HAVE_ARMV6
    ldr      r14, [sp, #4*6]
#endif
    lsr      r1, #12
    lsr      r2, #12
    lsr      r3, #12
    orr      r1, r1, r6, lsl #20  @ mac1 v1
    add      r6, sp, #4*(8+3)
    orr      r2, r2, r7, lsl #20  @ mac2 v1
    orr      r3, r3, r8, lsl #20  @ mac3 v1
    stmia    r6, {r1-r3}          @ save them
    ldmia    sp, {r1-r3,r6-r8}    @ reload shifted gteTR|XYZ

#ifdef HAVE_ARMV6
    smlald   r1, r6, r9,  r4      @ r09l=MX11 * vx + r09h=MX12 * vy
    smlald   r2, r7, r14, r4      @ r10h=MX21 * vx + r11l=MX22 * vy
    smlald   r3, r8, r12, r4      @ r12l=MX31 * vx + r12h=MX32 * vy
    ldr      r14, [r0, #4*(32+4)] @ gteR33
#else
    smlalbb  r1, r6, r9,  r4      @ r09l=MX11 * vx
    smlaltb  r2, r7, r10, r4      @ r10h=MX21 * vx
    smlalbb  r3, r8, r12, r4      @ r12l=MX31 * vx
    smlaltt  r1, r6, r9,  r4      @ r09h=MX12 * vy
    smlalbt  r2, r7, r11, r4      @ r11l=MX22 * vy
    smlaltt  r3, r8, r12, r4      @ r12h=MX32 * vy
#endif
    smlalbb  r1, r6, r10, r5      @ r10l=MX13 * vz
    smlaltb  r2, r7, r11, r5      @ r11h=MX23 * vz
    smlalbb  r3, r8, r14, r5      @ r14l=MX33 * vz
    ldr      r4, [sp, #4*(8+2)]   @ mac3 v0
    lsr      r1, #12
    lsr      r2, #12
    lsr      r3, #12
    orr      r6, r1, r6, lsl #20  @ mac1 v2
    mov      r1, r4
    orr      r7, r2, r7, lsl #20  @ mac2 v2
    ldr      r2, [sp, #4*(8+3+2)] @ mac3 v1
    orr      r3, r3, r8, lsl #20  @ mac3 v2
    strd_    r6, r7, r0, 4*25     @ gteMAC|12

    ssatx_prep  r12, 16
    usat16_     r4, r1            @ SZ1
    ssatx       r7, r1, r12, 16   @ ir3 v0

    ssatx       r8, r2, r12, 16   @ ir3 v1
    ssatx       r10,r3, r12, 16   @ ir3 v2
    usat16_     r5, r2            @ SZ2
    usat16_     r6, r3            @ SZ3
 F( cmp         r7, r1       )
    mov         lr, #0            @ gteFLAG
 F( cmpeq       r8, r2       )
 F( cmpeq       r10,r3       )
 F( orrne       lr, #(1<<22) )    @ IR3/limB3
    clz         r7, r4
    clz         r8, r5
 F( cmp         r4, r1       )
    clz         r9, r6
 F( cmpeq       r5, r2       )
 F( cmpeq       r6, r3       )
 F( orrne       lr, #(1<<18) )    @ fSZ (limD)
    sub         r7, #16           @ shift
    sub         r8, #16
    sub         r9, #16
    orr         r12,r7, r8, lsl #8
    mov         r7, r4, lsl r7
    mov         r8, r5, lsl r8
    orr         r12,r12,r9, lsl #16 @ save shifts
    mov         r9, r6, lsl r9
    str         r10, [r0, #4*11]  @ gteIR3
    add         r2, r0, #4*16
    str         r3, [r0, #4*27]   @ gteMAC3
    bic         r7, #0x8000
    ldr         r3, [r0, #4*19]   @ SZ3 old
    bic         r8, #0x8000
    ldrh        r11, [r0, #4*(32+26)] @ gteH
    bic         r9, #0x8000
    ldr         r10, [r0, #LO_cop2_to_gte_divider_tab_ptr] @ gte_divider_tab
    add         r1, r7, #0x40
    stmia       r2, {r3-r6}       @ r3,r4,r5,r6 SZ1-SZ3
    add         r2, r8, #0x40
    add         r3, r9, #0x40
    cmp         r11, r4, lsl #1
    mov         r4, r11
    ldrblt      r1, [r10, r1, lsr #7] @ gte_divider_tab lut
 F( orrge       lr, #(1<<17) )
    mvnge       r4, #0
    movge       r1, #0            @ adjusted gteH v0
    cmp         r11, r5, lsl #1
    mov         r5, r11
    ldrblt      r2, [r10, r2, lsr #7]
 F( orrge       lr, #(1<<17) )
    mvnge       r5, #0
    movge       r2, #0
    cmp         r11, r6, lsl #1
    mov         r6, #0x001
    ldrblt      r3, [r10, r3, lsr #7]
    orr         r6, #0x100
 F( orrge       lr, #(1<<17) )
    mvnge       r11, #0
    movge       r3, #0
    mov         r10, #0x080
    orr         r7, #0x8000
    orr         r8, #0x8000
    orr         r9, #0x8000
    add         r1, r6
    add         r2, r6
    add         r3, r6
    rsb         r7, r7, #0
    rsb         r8, r8, #0
    rsb         r9, r9, #0
    mla         r7, r1, r7, r10
    mla         r8, r2, r8, r10
    mla         r9, r3, r9, r10
    mov         r6, r12, lsr #8
    lsl         r4, r12
    mov         r12,r12, lsr #16
    lsl         r5, r6
    mov         r6, r11, lsl r12
    bic         r7, #0xfe000000
    bic         r8, #0xfe000000
    bic         r9, #0xfe000000
    lsr         r7, #8
    lsr         r8, #8
    lsr         r9, #8
    mla         r1, r7, r1, r10
    mla         r2, r8, r2, r10
    mla         r3, r9, r3, r10
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
    ldrd_       r2, r3, sp, 4*8   @ mac|12 v0
    ssatx_prep  r6, 16
    ssatx       r4, r2, r6, 16    @ ir1 v0
    ssatx       r5, r3, r6, 16    @ ir2 v0
 F( cmp         r4, r2       )
    mov         r6, #0x20000
 F( orrne       lr, #(1<<24) )    @ IR1/limB1
 F( cmp         r5, r3       )
 F( orrne       lr, #(1<<23) )    @ IR2/limB2
    mov         r7, r7, lsr #16
    mov         r8, r8, lsr #16
    mov         r9, r9, lsr #16
    ldrd        r2, r3, [r0, #4*(32+24)] @ gteOF|XY
    orr         r7, r7, r10, lsl #16
    orr         r8, r8, r11, lsl #16
    orr         r9, r9, r12, lsl #16
    cmp         r7, r6
    subhs       r7, r6, #1        @ quotient
    cmp         r8, r6
    subhs       r8, r6, #1
    cmp         r9, r6
    subhs       r9, r6, #1
    mov         r1, r2, asr #31
    mov         r6, r3, asr #31
    mov         r10,r2
    smlal       r2, r1, r4, r7    @ mac0_0 = gteOFX + gteIR1 * quotient @ slow :(
    mov         r11,r3
    smlal       r3, r6, r5, r7    @ mac0_1 = gteOFY + gteIR2 * quotient
    ldrd_       r4, r5, sp, 4*(8+3) @ mac|12 v1
    ssatx_prep  r7, 16
    ssatx       r12,r4, r7, 16    @ ir1 v1
    mov         r2, r2, lsr #16
    orr         r1, r2, r1, lsl #16 @ mac0_0 >> 16
    ssatx       r2, r5, r7, 16    @ ir2 v1
 F( cmp         r12,r4       )
    mov         r4,  r10, asr #31
 F( orrne       lr, #(1<<24) )    @ IR1/limB1
    mov         r7,  r11, asr #31
    smlal       r10, r4, r12, r8  @ mac1_0
    smlal       r11, r7, r2,  r8  @ mac1_1
 F( cmp         r2, r5       )
 F( orrne       lr, #(1<<23) )    @ IR2/limB2
    mov         r3, r3, lsr #16
    mov         r10,r10,lsr #16
    orr         r2, r3, r6, lsl #16
    mov         r11,r11,lsr #16
    orr         r3, r10,r4, lsl #16
    orr         r4, r11,r7, lsl #16
    ssatx_prep  r5, 11
 F( cmp         r1, #0x8000  )
V6( ssat        r6, #11, r1  )
 F( cmplt       r2, #0x8000  )
 F( cmplt       r3, #0x8000  )
 F( cmplt       r4, #0x8000  )
 F( orrge       lr, #(1<<16) )    @ mac0 positive overflow
 F( cmn         r1, #0x8000  )
V6( ssat        r7, #11, r2  )
 F( cmnge       r2, #0x8000  )
 F( cmnge       r3, #0x8000  )
 F( cmnge       r4, #0x8000  )
 F( orrlt       lr, #(1<<15) )
#ifndef HAVE_ARMV6
    ssatx       r6, r1, r5, 11
    ssatx       r7, r2, r5, 11
#endif
    ssatx       r8, r3, r5, 11
    ssatx       r12,r4, r5, 11
 F( cmp         r1, r6       )
 F( orrne       lr, #(1<<14) )       @ limG1 v0
 F( cmp         r2, r7       )
 F( orrne       lr, #(1<<13) )       @ limG2
 F( cmp         r3, r8       )
    ldrd        r2, r3, [r0, #4*(32+24)] @ reload gteOF|XY
 F( orrne       lr, #(1<<14) )       @ limG1 v1
 F( cmp         r4, r12      )
 F( orrne       lr, #(1<<13) )       @ limG2
#ifdef HAVE_ARMV6
    pkhbt       r6, r6, r7, lsl #16
    pkhbt       r7, r8, r12,lsl #16
#else
    mov         r1, r6, lsl #16
    mov         r4, r8, lsl #16
    mov         r6, r7, lsl #16
    mov         r7, r12,lsl #16
    orr         r6, r6, r1, lsr #16
    orr         r7, r7, r4, lsr #16
#endif
    ssatx_prep  r12,16
    ldrd_       r4, r5, r0, 4*25     @ gteMAC|12
    mov         r1, r2, asr #31
    ssatx       r10,r4, r12, 16      @ ir1 v2
    strd        r6, r7, [r0, #4*12]  @ gteSXY|01
    ssatx       r11,r5, r12, 16      @ ir2 v2
 F( cmp         r10,r4       )
    mov         r4,  r10, asr #31
 F( orrne       lr, #(1<<24) )       @ IR1/limB1
 F( cmp         r11,r5       )
    ldr         r5, [r0, #4*(32+28)] @ gteDQB
 F( orrne       lr, #(1<<23) )       @ IR2/limB2
    mov         r4, r3, asr #31
    ldrsh       r6, [r0, #4*(32+27)] @ gteDQA
    smlal       r2, r1, r10, r9      @ mac2_0 = gteOFX + gteIR1 * quotient
    mov         r12,r5, asr #31
    smlal       r3, r4, r11, r9      @ mac2_1 = gteOFY + gteIR2 * quotient
    smlal       r5, r12,r6,  r9      @ mac2_2 = gteDQB + gteDQA * quotient
    strd        r10,r11, [r0, #4*9]  @ gteIR|12 v2
    mov         r2, r2, lsr #16
    mov         r3, r3, lsr #16
    str         r5, [r0, #4*24]      @ gteMAC0
    mov         r5, r5, lsr #12
    orr         r1, r2, r1, lsl #16
    orr         r2, r3, r4, lsl #16
    orr         r3, r5, r12,lsl #(32-12)
    ssatx_prep  r12,11
 F( cmp         r1, #0x8000  )
V6( ssat        r4, #11, r1  )
 F( cmplt       r2, #0x8000  )
 F( cmplt       r3, #0x80000 )
 F( orrge       lr, #(1<<16) )
 F( cmn         r1, #0x8000  )
V6( ssat        r5, #11, r2  )
 F( cmnge       r2, #0x8000  )
 F( cmnge       r3, #0x80000 )
 F( orrlt       lr, #(1<<15) )
    add         sp, #4*(8+3*2)
#ifdef HAVE_ARMV6
    pkhbt       r7, r4, r5, lsl #16
#else
    ssatx       r4, r1, r12, 11
    ssatx       r5, r2, r12, 11
    mov         r6, r4, lsl #16
    mov         r7, r5, lsl #16
    orr         r7, r7, r6, lsr #16
#endif
    asr         r6, r3, #31
 F( cmp         r1, r4       )
 F( orrne       lr, #(1<<14) )       @ limG1
 F( cmp         r2, r5       )
 F( orrne       lr, #(1<<13) )       @ limG2
    str         r7, [r0, #4*14]      @ gteSXY2

    cmp         r3, #0x1000
    bic         r3, r6
    movgt       r3, #0x1000
 F( orrhi       lr, #(1<<12) )       @ limH
    strh        r3, [r0, #4*8]       @ gteIR0
#ifndef FLAGLESS
    tst         lr, #0x01800000
    tsteq       lr, #0x0007e000
    orrne       lr, #(1<<31)
    str         lr, [r0, #4*(32+31)] @ gteFLAG
#endif
    pop         {r4-r11,pc}
call_gteRTPT_sf1lm0:
    pop         {r4-r11,lr}
    b           gteRTPT_sf1lm0
    .size    gteRTPT_sf1lm0_arm, .-gteRTPT_sf1lm0_arm

#endif // HAVE_ARMV5

// only single copy of the stuff until eof
#ifndef FLAGLESS
#ifdef HAVE_ARMV5

@ note: non-std calling convention used
@ r0 = CP2 (d,c)  (must preserve)
@ r1 = opcode in, flags out
@ r4,r5 = VXYZ(v) packed
@ r6 = &MX11(mx)
@ r7 = &CV1(cv)
@ r8-r10 assumed to be saved by drc (r11 is its ctx ptr)
FUNCTION(gteMVMVA_part_sf1_arm):
    push     {r11,lr}

    ldmia    r7, {r7-r9}          @ CV123
    ldmia    r6, {r6,r10,r11,r12,r14} @ MX*
    sub      r2, r7, #0x7f000000
    sub      r3, r8, #0x7f000000
    cmp      r2,     #0x02000000
    sub      r2, r9, #0x7f000000
    cmphi    r3,     #0x02000000
    cmphi    r2,     #0x02000000
    bls      call_gteMVMVA_generic @ if MAC123 can overflow, don't bother

    asr      r1, r7, #20
    asr      r2, r8, #20
    asr      r3, r9, #20
    lsl      r7, #12              @ expand to 64bit
    lsl      r8, #12
    lsl      r9, #12
#ifdef HAVE_ARMV6
    smlald   r7, r1, r6,  r4      @ r06l=MX11 * vx + r06h=MX12 * vy
    mov      r6, r10, lsr #16
    orr      r6, r6, r11, lsl #16
    smlald   r8, r2, r6,  r4      @ r10h=MX21 * vx + r11l=MX22 * vy
    smlald   r9, r3, r12, r4      @ r12l=MX31 * vx + r12h=MX32 * vy
#else
    smlalbb  r7, r1, r6,  r4      @ r06l=MX11 * vx
    smlaltb  r8, r2, r10, r4      @ r10h=MX21 * vx
    smlalbb  r9, r3, r12, r4      @ r12l=MX31 * vx
    smlaltt  r7, r1, r6,  r4      @ r06h=MX12 * vy
    smlalbt  r8, r2, r11, r4      @ r11l=MX22 * vy
    smlaltt  r9, r3, r12, r4      @ r12h=MX32 * vy
#endif
    smlalbb  r7, r1, r10, r5      @ r10l=MX13 * vz
    smlaltb  r8, r2, r11, r5      @ r11h=MX23 * vz
    smlalbb  r9, r3, r14, r5      @ r14l=MX33 * vz
    pop      {r11,lr}
    add      r12, r0, #4*25
    lsr      r7, #12
    lsr      r8, #12
    lsr      r9, #12
    orr      r7, r7, r1, lsl #20  @ gteMAC1
    orr      r8, r8, r2, lsl #20  @ gteMAC2
    orr      r9, r9, r3, lsl #20  @ gteMAC3
    mov      r1, #0               @ gteFLAG for gteMACtoIR
    stmia    r12, {r7-r9}

    bx       lr
    .size    gteMVMVA_part_sf1_arm, .-gteMVMVA_part_sf1_arm

@ note: non-std calling convention used
@ r0 = CP2 (d,c)  (must preserve)
@ r1 = opcode in, flags out
@ r4,r5 = VXYZ(v) packed
@ r6 = &MX11(mx)
@ r7 = &CV1(cv)
@ r8-r10 assumed to be saved by drc (r11 is its ctx ptr)
FUNCTION(gteMVMVA_part_sf0_arm):
    push     {r11,lr}

    ldmia    r7, {r7-r9}          @ CV123
    sub      r2, r7, #0x7f000000
    sub      r3, r8, #0x7f000000
    cmp      r2,     #0x02000000
    ldmia    r6, {r6,r10,r11,r12,r14} @ MX*
    sub      r2, r9, #0x7f000000
    cmphi    r3,     #0x02000000
    cmphi    r2,     #0x02000000
    bls      call_gteMVMVA_generic @ if MAC123 can overflow, don't bother

    mov      r1, r7, lsl #12
    mov      r2, r8, lsl #12
#ifdef HAVE_ARMV6
    mov      r7, r10, lsr #16
    mov      r3, r9, lsl #12
    orr      r7, r7, r11, lsl #16
    smlad    r1, r6,  r4, r1      @ r06l-MX11 * vx + r06h-MX12 * vy
    smlad    r2, r7,  r4, r2      @ r10h-MX21 * vx + r11l-MX22 * vy
    smlad    r3, r12, r4, r3      @ r12l-MX31 * vx + r12h-MX32 * vy
#else
    mov      r3, r9, lsl #12
    smlabb   r1, r6,  r4, r1      @ r06l=MX11 * vx
    smlatb   r2, r10, r4, r2      @ r10h=MX21 * vx
    smlabb   r3, r12, r4, r3      @ r12l=MX31 * vx
    smlatt   r1, r6,  r4, r1      @ r06h=MX12 * vy
    smlabt   r2, r11, r4, r2      @ r11l=MX22 * vy
    smlatt   r3, r12, r4, r3      @ r12h=MX32 * vy
#endif
    smlabb   r1, r10, r5, r1      @ r10l-MX13 * vz
    smlatb   r2, r11, r5, r2      @ r11h-MX23 * vz
    smlabb   r3, r14, r5, r3      @ r14l-MX33 * vz
    add      r12, r0, #4*25
    pop      {r11,lr}
    stmia    r12, {r1-r3}
    mov      r1, #0               @ gteFLAG for gteMACtoIR
    bx       lr
    .size    gteMVMVA_part_sf0_arm, .-gteMVMVA_part_sf0_arm

.align 4

@ common variant of MVMVA with cv3 (== 0) and shift12,
@ can't overflow so no gteMAC flags needed
@ note: not std calling convention used
@ r0 = CP2 (d,c)  (must preserve)
@ r1 = flags out
@ r4,r5 = VXYZ(v) packed
@ r6 = &MX11(mx)
@ r8-r10 assumed to be saved by drc (r11 is its ctx ptr)
FUNCTION(gteMVMVA_part_sf1cv3_arm):
    push     {r11,lr}
    ldmia    r6, {r6,r10,r11,r12,r14} @ MX*

    smulbb   r7, r6,  r4          @ r06l=MX11 * vx
    smultb   r8, r10, r4          @ r10h=MX21 * vx
    smulbb   r9, r12, r4          @ r12l=MX31 * vx
    asr      r1, r7, #31
    asr      r2, r8, #31
    smlaltt  r7, r1, r6,  r4      @ r06h=MX12 * vy
    asr      r3, r9, #31
    smlalbt  r8, r2, r11, r4      @ r11l=MX22 * vy
    smlaltt  r9, r3, r12, r4      @ r12h=MX32 * vy
    smlalbb  r7, r1, r10, r5      @ r10l=MX13 * vz
    smlaltb  r8, r2, r11, r5      @ r11h=MX23 * vz
    smlalbb  r9, r3, r14, r5      @ r14l=MX33 * vz
    pop      {r11,lr}
    add      r12, r0, #4*25
    lsr      r7, #12
    lsr      r8, #12
    lsr      r9, #12
    orr      r7, r7, r1, lsl #20  @ gteMAC1
    orr      r8, r8, r2, lsl #20  @ gteMAC2
    orr      r9, r9, r3, lsl #20  @ gteMAC3
    mov      r1, #0               @ gteFLAG for gteMACtoIR
    stmia    r12, {r7-r9}
    bx       lr
    .size    gteMVMVA_part_sf1cv3_arm, .-gteMVMVA_part_sf1cv3_arm

@ note: not std calling convention used
@ r0 = CP2 (d,c)  (must preserve)
@ r1 = flags out
@ r4,r5 = VXYZ(v) packed
@ r6 = &MX11(mx)
@ r8-r10 assumed to be saved by drc (r11 is its ctx ptr)
FUNCTION(gteMVMVA_part_sf0cv3_arm):
    push     {r11,lr}
    ldmia    r6, {r6,r10,r11,r12,r14} @ MX*

    smulbb   r7, r6,  r4          @ r06l=MX11 * vx
    smultb   r8, r10, r4          @ r10h=MX21 * vx
    smulbb   r9, r12, r4          @ r12l=MX31 * vx
    smlatt   r7, r6,  r4, r7      @ r06h=MX12 * vy
    smlabt   r8, r11, r4, r8      @ r11l=MX22 * vy
    smlatt   r9, r12, r4, r9      @ r12h=MX32 * vy
    smlabb   r7, r10, r5, r7      @ r10l=MX13 * vz
    smlatb   r8, r11, r5, r8      @ r11h=MX23 * vz
    smlabb   r9, r14, r5, r9      @ r14l=MX33 * vz
    add      r12, r0, #4*25
    pop      {r11,lr}
    mov      r1, #0               @ gteFLAG for gteMACtoIR
    stmia    r12, {r7-r9}
    bx       lr
    .size    gteMVMVA_part_sf0cv3_arm, .-gteMVMVA_part_sf0cv3_arm

call_gteMVMVA_generic:
    push     {r0,r1}
    bl       gteMVMVA_generic
    pop      {r0,r1,r11,lr}
    ldr      r1, [r0, #4*(32+31)] @ gteFLAG for gteMACtoIR
    bx       lr

@ r0 = CP2 (d,c)
@ r1, r2, r3 = vx, vy, vz
@ [sp + 0] = &MX11(mx)
@ [sp + 4] = &CV1(cv)
@ [sp + 8] = shift
FUNCTION(gteMAC123f_arm):
    push     {r0,r3-r11,lr}       @ +r0+r3

    ldrd_    r6, r7, sp, 4*11
#ifdef HAVE_ARMV6
    pkhbt    r4, r1, r2, lsl #16
#else
    mov      r1, r1, lsl #16
    mov      r2, r2, lsl #16
    orr      r4, r2, r1, lsr #16
#endif
    ldmia    r6, {r6,r10,r11,r12,r14} @ MX*
    mov      r0, #0               @ flag
    smulbb   r1, r6,  r4          @ r06l=MX11 * vx
    ldmia    r7, {r7-r9}          @ CV123
    smultb   r2, r10, r4          @ r10h=MX21 * vx
    smulbb   r3, r12, r4          @ r12l=MX31 * vx
    adds     r7, r7, r1, asr #12
    mov      r5, #(1<<30)
    lsrpl    r5, #3
    orrvs    r0, r5
    adds     r8, r8, r2, asr #12
    mov      r5, #(1<<29)
    lsrpl    r5, #3
    orrvs    r0, r5
    adds     r9, r9, r3, asr #12
    mov      r5, #(1<<28)
    lsrpl    r5, #3
    orrvs    r0, r5
    lsl      r1, #20

    smultt   r6, r6,  r4          @ r06h=MX12 * vy
    lsl      r2, #20
    smulbt   r5, r11, r4          @ r11l=MX22 * vy
    lsl      r3, #20
    smultt   r12,r12, r4          @ r12h=MX32 * vy
    adds     r1, r1,  r6, lsl #20
    adcs     r7, r7,  r6, asr #12
    mov      r4, #(1<<30)
    lsrpl    r4, #3
    orrvs    r0, r4
    adds     r2, r2,  r5, lsl #20
    adcs     r8, r8,  r5, asr #12
    ldr      r5, [sp, #4]         @ vz
    mov      r4, #(1<<29)
    lsrpl    r4, #3
    orrvs    r0, r4
    adds     r3, r3, r12, lsl #20
    adcs     r9, r9, r12, asr #12
    mov      r4, #(1<<28)
    lsrpl    r4, #3
    orrvs    r0, r4

    smulbb   r10, r10, r5         @ r10l-MX13 * vz
    smultb   r11, r11, r5         @ r11h-MX23 * vz
    smulbb   r14, r14, r5         @ r14l-MX33 * vz
    adds     r1, r1, r10, lsl #20
    adcs     r7, r7, r10, asr #12
    mov      r4, #(1<<30)
    lsrpl    r4, #3
    orrvs    r0, r4
    adds     r2, r2, r11, lsl #20
    adcs     r8, r8, r11, asr #12
    mov      r4, #(1<<29)
    lsrpl    r4, #3
    orrvs    r0, r4
    adds     r3, r3, r14, lsl #20
    adcs     r9, r9, r14, asr #12
    ldr      r5, [sp, #4*13]      @ shift
    mov      r4, #(1<<28)
    ldr      r6, [sp]             @ regs
    lsrpl    r4, #3
    orrvs    r0, r4
    tst      r5, r5
    add      r12, r6, #4*25
    lsleq    r7, #12
    lsleq    r8, #12
    lsleq    r9, #12
    orreq    r7, r7, r1, lsr #20
    orreq    r8, r8, r2, lsr #20
    orreq    r9, r9, r3, lsr #20
    stmia    r12, {r7-r9}         @ gteMAC
    add      sp, #4*2
    pop      {r4-r11,pc}

#endif /* HAVE_ARMV5 */

FUNCTION(gteNCLIP_arm): @ r0=CP2 (d,c),
    push        {r4-r6,lr}
    ldrsh       r4, [r0, #4*12+2]
    ldrsh       r5, [r0, #4*13+2]
    ldrsh       r6, [r0, #4*14+2]
    ldrsh       lr, [r0, #4*12]
    ldrsh       r2, [r0, #4*13]
    sub         r12, r4, r5       @ 3: gteSY0 - gteSY1
    sub         r5, r5, r6        @ 1: gteSY1 - gteSY2
    smull       r1, r5, lr, r5    @ RdLo, RdHi
    sub         r6, r4            @ 2: gteSY2 - gteSY0
    ldrsh       r3, [r0, #4*14]
    smlal       r1, r5, r2, r6
    mov         lr, #0            @ gteFLAG
    smlal       r1, r5, r3, r12
    mov         r6, #1<<31
    orr         r6, #1<<15
    movs        r2, r1, lsl #1
    adc         r5, r5
    cmp         r5, #0
#ifdef HAVE_ARMV7
    movtgt      lr, #((1<<31)|(1<<16))>>16
#else
    movgt       lr, #(1<<31)
    orrgt       lr, #(1<<16)
#endif
    cmn         r5, #1
    orrmi       lr, r6
    str         r1, [r0, #4*24]
    str         lr, [r0, #4*(32+31)] @ gteFLAG

    pop         {r4-r6,pc}
    .size	gteNCLIP_arm, .-gteNCLIP_arm

#ifdef HAVE_ARMV6

.macro MACtoIRsat lm rd rs
.if \lm
    usat     \rd, #15, \rs
.else
    ssat     \rd, #16, \rs
.endif
.endm

.macro gteMACtoIR lm
    ldrd     r2, r3, [r0, #4*25]  @ gteMAC|12
    ldr      r12,    [r0, #4*27]  @ gteMAC3
    push     {r4-r6}
    MACtoIRsat \lm, r4, r2
    MACtoIRsat \lm, r5, r3
    MACtoIRsat \lm, r6, r12
    strd     r4, r5, [r0, #4*9]   @ gteIR|12
    cmp      r4, r2
    orrne    r1, #(1<<31)|(1<<24)
    cmp      r5, r3
    str      r6,     [r0, #4*11]  @ gteIR3
    orrne    r1, #(1<<23)
    orrne    r1, #(1<<31)
    cmp      r6, r12
    pop      {r4-r6}
    orrne    r1, #(1<<22)         @ doesn't set (1<<31)
    str      r1, [r0, #4*(32+31)] @ gteFLAG
    bx       lr
.endm

#else // !HAVE_ARMV6

@ r0 = CP2 (d,c)
@ r1 = gte_flags
.macro gteMACtoIR lm
    ldr      r2, [r0, #4*25]      @ gteMAC1
    mov      r12, r1
    mov      r1, #1<<15
    cmp      r2, r1
    subge    r2, r1, #1
    orrge    r12, #(1<<31)|(1<<24)
.if \lm
    cmp      r2, #0
    movlt    r2, #0
.else
    cmn      r2, r1
    rsblt    r2, r1, #0
.endif
    str      r2, [r0, #4*9]
    ldrd_    r2, r3, r0, 4*26     @ gteMAC23
    orrlt    r12, #(1<<31)|(1<<24)
    cmp      r2, r1
    subge    r2, r1, #1
    orrge    r12, #1<<23
    orrge    r12, #1<<31
.if \lm
    cmp      r2, #0
    movlt    r2, #0
.else
    cmn      r2, r1
    rsblt    r2, r1, #0
.endif
    orrlt    r12, #1<<23
    orrlt    r12, #1<<31
    cmp      r3, r1
    subge    r3, r1, #1
    orrge    r12, #1<<22
.if \lm
    cmp      r3, #0
    movlt    r3, #0
.else
    cmn      r3, r1
    rsblt    r3, r1, #0
.endif
    orrlt    r12, #1<<22
    strd_    r2, r3, r0, 4*10     @ gteIR23
    str      r12,[r0, #4*(32+31)] @ gteFLAG
    bx       lr
.endm

#endif // !HAVE_ARMV6

FUNCTION(gteMACtoIR_lm0_arm): @ r0=CP2 (d,c)
    gteMACtoIR 0
    .size    gteMACtoIR_lm0_arm, .-gteMACtoIR_lm0_arm

FUNCTION(gteMACtoIR_lm1_arm): @ r0=CP2 (d,c)
    gteMACtoIR 1
    .size    gteMACtoIR_lm1_arm, .-gteMACtoIR_lm1_arm


FUNCTION(gteMACtoIR_lm0_nf_arm): @ r0=CP2 (d,c)
    add      r12, r0, #4*25
    ldmia    r12, {r1-r3}
    ssatx_prep r12, 16
    ssatx    r1, r1, r12, 16
    ssatx    r2, r2, r12, 16
    ssatx    r3, r3, r12, 16
    add      r12, r0, #4*9
    stmia    r12, {r1-r3}
    bx       lr
    .size    gteMACtoIR_lm0_nf_arm, .-gteMACtoIR_lm0_nf_arm


FUNCTION(gteMACtoIR_lm1_nf_arm): @ r0=CP2 (d,c)
    add      r12, r0, #4*25
    ldmia    r12, {r1-r3}
    usatx_prep r12, 16
    usatx    r1, r1, r12, 16
    usatx    r2, r2, r12, 16
    usatx    r3, r3, r12, 16
    add      r12, r0, #4*9
    stmia    r12, {r1-r3}
    bx       lr
    .size    gteMACtoIR_lm1_nf_arm, .-gteMACtoIR_lm1_nf_arm

#endif // FLAGLESS

@ vim:filetype=armasm

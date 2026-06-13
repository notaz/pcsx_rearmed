/*
 * (C) notaz, 2026
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "arm_features.h"
#include "new_dynarec/linkage_offsets.h"

#ifdef FLAGLESS
#define gteRTPS_sf1lm0_arm64 gteRTPS_sf1lm0_nf_arm64
#define F(...)
#else
#define F(code...) code
#endif
#ifdef __MACH__
#define gteRTPS_sf1lm0 ESYM(gteRTPS_sf1lm0)
#endif

.text
.align 2

FUNCTION(gteRTPS_sf1lm0_arm64):   // x0=CP2 (d,c); save: v8-v15
    add      x1, x0, #4*32
    ld1      {v0.8h,v1.8h}, [x1]  // gteRxx, gteTR|XYZ
    ldr      w1, [x0, #4*(32+5)]  // gteTR|XYZ
    mov      w6, #0x7f000000
    ldp      w2, w3, [x0, 4*(32+6)]
    mov      w7, #0x02000000
    ext      v3.16b, v1.16b, v1.16b, #4*1 // gteTR|XYZ...
    ext      v2.16b, v0.16b, v1.16b, #2*6 // gteR31..gteR33...
    ext      v1.16b, v0.16b, v0.16b, #2*3 // gteR21..gteR23...
    sshll2   v4.2d, v3.4s, #12    // gteTR|Z
    sshll    v3.2d, v3.2s, #12    // gteTR|XY

    ld1      {v5.4h}, [x0]        // VXYZ(0)
    sub      w4, w1, w6
    sub      w5, w2, w6
    sub      w6, w3, w6
    ldrh     w10, [x0, #4*(32+26)] // gteH
    cmp      w4, w7
    ccmp     w5, w7, #0, hi
    ccmp     w6, w7, #0, hi
    bls      call_gteRTPS_sf1lm0  // if MAC123 can overflow, don't bother

    add      x1, x0, #4*17        // gteSZ*
    mov      w2, #0

    mov      v5.h[3], w2          // kill the unused upper vector
    ld1      {v16.4s}, [x1]       // gteSZ|123x
    smull    v10.4s, v0.4h, v5.4h
    smull    v11.4s, v1.4h, v5.4h
    smull    v12.4s, v2.4h, v5.4h
    add      x1, x0, #4*16        // gteSZ*
    st1      {v16.2s}, [x1], #8   // gteSZ|12 to |01
    st1      {v16.s}[2], [x1]     // gteSZ|3 to |2
    saddlv   d10, v10.4s
    saddlv   d11, v11.4s
    saddlv   d12, v12.4s
    ldr      x16, [x0, #LO_cop2_to_gte_divider_tab_ptr] // gte_divider_tab
    mov      v10.d[1], v11.d[0]
    add      x1, x0, #4*25
    add      v3.2d, v3.2d, v10.2d
    add      d4,    d4,    d12
    shrn     v5.2s, v3.2d, #12    // gteMAC|12 (overwrites top!)
    shrn2    v5.4s, v4.2d, #12    // gteMAC3
 F( mov      w17, #0      )       // gteFLAG
    st1      {v5.2s}, [x1], #4*2
    sqxtn    v3.4h, v5.4s         // gteIR|123
    st1      {v5.s}[2], [x1]
    sqxtun   v2.4h, v5.4s         // SZ (not uqxtn)
    sxtl     v1.4s, v3.4h         // gteIR|123 expanded
    add      x1, x0, #4*9
 F( mov      w5, v5.s[0] )        // gteMAC
 F( mov      w6, v5.s[1] )
 F( mov      w7, v5.s[2] )
    mov      w14,v1.s[0]          // gteIR
    mov      w15,v1.s[1]
#ifndef FLAGLESS
    mov      w4, v1.s[2]
    cmp      w5, w14
    cset     w5, ne
    cmp      w6, w15
    cset     w6, ne
    orr      w17, w17, w5, lsl #24 // IR1/limB1
    cmp      w7, w4
    cset     w8, ne
    orr      w17, w17, w6, lsl #23 // IR2/limB2
    orr      w17, w17, w8, lsl #22 // IR2/limB3
#endif
    umov     w9, v2.h[2]          // SZ3
    st1      {v1.2s}, [x1], #4*2
    st1      {v1.s}[2], [x1]
 F( cmp      w7, w9 )
 F( cset     w7, ne )
 F( orr      w17, w17, w7, lsl #18 ) // limD

    clz      w2, w9
    sub      w2, w2, #16          // w2=shift

    lslv     w7, w9, w2
    bic      w7, w7, #0x8000
    add      w5, w7, #0x40

    lsr      w5, w5, #7
    mov      w3, #0
    ldrb     w6, [x16, w5, uxtw]  // gte_divider_tab lut
    mov      w1, #-1
    cmp      w10, w9, lsl #1      // gteH < SZ3*2
    csel     w10, w1, w10, ge     // cause a large unsigned value
 F( cset     w1, ge  )
    csel     w6, w3, w6, ge
 F( orr      w17, w17, w1, lsl #17 )

    mov      w5, #0x101
    orr      w7, w7, #0x8000
    add      w6, w6, w5
    mov      w5, #0x80
    msub     w7, w6, w7, w5
    ldpsw    x1, x12, [x0, #4*(32+24)] // gteOF|XY
    bic      w7, w7, #0xfe000000
    lsr      w7, w7, #8
    madd     w7, w6, w7, w5
    lslv     w4, w10, w2
    lsr      w7, w7, #8
    mov      x5, #0x8000
    smaddl   x5, w7, w4, x5
    mov      w4, #0x1ffff
    str      w9, [x0, #4*19]      // SZ3
    lsr      x5, x5, #16
    cmp      w5, w4
    csel     w5, w4, w5, hs       // quotient
    ldrsw    x9, [x0, #4*(32+28)] // gteDQB
    ldrsh    w2, [x0, #4*(32+27)] // gteDQA
    smaddl   x1, w14, w5, x1      // mac0_0 = gteOFX + gteIR1 * quotient
    smaddl   x12,w15, w5, x12     // mac0_1 = gteOFY + gteIR2 * quotient
    smaddl   x9, w2,  w5, x9      // mac0_2 = gteDQB + gteDQA * quotient
    ldp      w3, w5, [x0, #4*13]  // gteSXY|12
    asr      x1, x1,  #16         // mac0_0
    asr      x12,x12, #16         // mac0_1
    stp      w3, w5, [x0, #4*12]  // gteSXY|01
    lsr      x2, x9,  #12
    str      w9, [x0, #4*24]      // gteMAC0
#ifndef FLAGLESS
    mov      w4, #0x8000
    mov      w5, #0x80000
    cmp      w1, w4
    ccmp     w12,w4, #0, lt
    ccmp     w2, w5, #0, lt
    cset     w3, ge
    cmn      w1, w4
    ccmn     w12,w4, #8, ge
    ccmn     w2, w5, #8, ge
    cset     w4, lt
    orr      w17, w17, w3, lsl #16 // mac0 positive overflow
    orr      w17, w17, w4, lsl #15
#endif
    mov      w6, #0x3ff
    mov      w7, #-0x400
    mov      w8, #1
    cmp      w1, w6
    csel     w3, w6, w1, gt
 F( cset     w9, gt    )
    cmp      w12,w6
    csel     w4, w6, w12,gt
 F( cset     w10,gt    )
    cmp      w3, w7
    csel     w3, w7, w3, lt
 F( csel     w9, w8, w9, lt )
    cmp      w4, w7
    csel     w4, w7, w4, lt
 F( csel     w10,w8, w10,lt )
 F( orr      w17, w17, w9, lsl #14 ) // limG1
    bfi      w3, w4, #16, #16
    asr      w5, w2, #31
 F( orr      w17, w17, w10,lsl #13 ) // limG2
    str      w3, [x0, #4*14]      // gteSXY2

    mov      w1, #0x1000
    cmp      w2, #0x1000
    bic      w2, w2, w5
    csel     w2, w1, w2, gt
 F( cset     w1, hi )
 F( orr      w17, w17, w1, lsl #12 ) // limH
    strh     w2, [x0, #4*8]       // gteIR0

#ifndef FLAGLESS
    and      w1, w17, #( 0x0187e000 |  0x01ffe000)
    tst      w1,      #~(~0x0187e000 & 0x01ffe000)
    cset     w1, ne
    orr      w17, w17, w1, lsl #31
    str      w17, [x0, #4*(32+31)] // gteFLAG
#endif
    ret
call_gteRTPS_sf1lm0:
    b        gteRTPS_sf1lm0       // apple can't do cond. branch to an external
    ESIZE(gteRTPS_sf1lm0_arm64, .-gteRTPS_sf1lm0_arm64)


// vim:filetype=armasm

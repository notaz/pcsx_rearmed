/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */


.text
.align 2


.global mix_chan @ (int start, int count, int lv, int rv)
mix_chan:
    vmov.32     d14[0], r2
    vmov.32     d14[1], r3             @ multipliers
    mov         r12, r0
    movw        r0, #:lower16:ChanBuf
    movw        r2, #:lower16:SSumLR
    movt        r0, #:upper16:ChanBuf
    movt        r2, #:upper16:SSumLR
    add         r0, r12, lsl #2
    add         r2, r12, lsl #3
0:
    vldmia      r0!, {d0-d1}
    vldmia      r2, {d2-d5}
    vmul.s32    d10, d14, d0[0]
    vmul.s32    d11, d14, d0[1]
    vmul.s32    d12, d14, d1[0]
    vmul.s32    d13, d14, d1[1]
    vsra.s32    q1, q5, #14
    vsra.s32    q2, q6, #14
    subs        r1, #4
    blt         mc_finish
    vstmia      r2!, {d2-d5}
    bgt         0b
    nop
    bxeq        lr

mc_finish:
    vstmia      r2!, {d2}
    cmp         r1, #-2
    vstmiage    r2!, {d3}
    cmp         r1, #-1
    vstmiage    r2!, {d4}
    bx          lr


.global mix_chan_rvb @ (int start, int count, int lv, int rv)
mix_chan_rvb:
    vmov.32     d14[0], r2
    vmov.32     d14[1], r3             @ multipliers
    mov         r12, r0
    movw        r0, #:lower16:ChanBuf
    movw        r3, #:lower16:sRVBStart
    movw        r2, #:lower16:SSumLR
    movt        r0, #:upper16:ChanBuf
    movt        r3, #:upper16:sRVBStart
    movt        r2, #:upper16:SSumLR
    ldr         r3, [r3]
    add         r0, r12, lsl #2
    add         r2, r12, lsl #3
    add         r3, r12, lsl #3
0:
    vldmia      r0!, {d0-d1}
    vldmia      r2, {d2-d5}
    vldmia      r3, {d6-d9}
    vmul.s32    d10, d14, d0[0]
    vmul.s32    d11, d14, d0[1]
    vmul.s32    d12, d14, d1[0]
    vmul.s32    d13, d14, d1[1]
    vsra.s32    q1, q5, #14
    vsra.s32    q2, q6, #14
    vsra.s32    q3, q5, #14
    vsra.s32    q4, q6, #14
    subs        r1, #4
    blt         mcr_finish
    vstmia      r2!, {d2-d5}
    vstmia      r3!, {d6-d9}
    bgt         0b
    nop
    bxeq        lr

mcr_finish:
    vstmia      r2!, {d2}
    vstmia      r3!, {d6}
    cmp         r1, #-2
    vstmiage    r2!, {d3}
    vstmiage    r3!, {d7}
    cmp         r1, #-1
    vstmiage    r2!, {d4}
    vstmiage    r3!, {d8}
    bx          lr

@ vim:filetype=armasm

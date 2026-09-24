    .set noreorder
    .align 2
    .global printf
    .type printf, @function
printf:
    li    $t2, 0xa0
    jr    $t2
    li    $t1, 0x3f


    .global syscall_a0
    .type syscall_a0, @function
syscall_a0:
    move  $t1, $a0
    move  $a0, $a1
    move  $a1, $a2
    li    $t2, 0xa0
    jr    $t2
    move  $a2, $a3


    .global memset
    .type memset, @function
memset:
    beqz  $a2, 1f
    move  $v0, $a0
0:
    sb    $a1, 0($a0)
    addiu $a2, -1
    bnez  $a2, 0b
    addiu $a0, 1
1:
    jr    $ra
    nop


# u32 out[17]: IR0-3, SXY0-2, SXYP, SZ0-3, MAC0-3, FLAG
.macro gte_store_rtps_regs
    swc2  $8,   0*4($a0)
    swc2  $9,   1*4($a0)
    swc2  $10,  2*4($a0)
    swc2  $11,  3*4($a0)
    swc2  $12,  4*4($a0)
    swc2  $13,  5*4($a0)
    swc2  $14,  6*4($a0)
    swc2  $15,  7*4($a0)
    swc2  $16,  8*4($a0)
    swc2  $17,  9*4($a0)
    swc2  $18, 10*4($a0)
    swc2  $19, 11*4($a0)
    swc2  $24, 12*4($a0)
    swc2  $25, 13*4($a0)
    swc2  $26, 14*4($a0)
    swc2  $27, 15*4($a0)
    cfc2  $t0, $31
    nop
    sw    $t0, 16*4($a0)
.endm

    .global gte_rtps_sf1lm0
    .type gte_rtps_sf1lm0, @function
gte_rtps_sf1lm0:
    cop2  0x0180001    # RTPS sf=1 lm=0
    gte_store_rtps_regs
    jr    $ra
    nop

# same, but FLAG is overwritten by NCLIP, so a flagless RTPS variant may be used
    .global gte_rtps_sf1lm0_nclip
    .type gte_rtps_sf1lm0_nclip, @function
gte_rtps_sf1lm0_nclip:
    cop2  0x0180001    # RTPS sf=1 lm=0
    cop2  0x1400006    # NCLIP
    gte_store_rtps_regs
    jr    $ra
    nop

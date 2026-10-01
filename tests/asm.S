#define FUNCTION(name) \
    .globl name; \
    .type name, @function; \
    name

.set noreorder
.align 2

FUNCTION(printf):
    li    $t2, 0xa0
    jr    $t2
    li    $t1, 0x3f


FUNCTION(syscall_a0):
    move  $t1, $a0
    move  $a0, $a1
    move  $a1, $a2
    li    $t2, 0xa0
    jr    $t2
    move  $a2, $a3


FUNCTION(memset):
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


FUNCTION(cpu_delay0):
    lw    $v1, 0($a0)  # 0
    nop
    lw    $v0, 4($a0)  # 1
    move  $v0, $v1
    jr    $ra
    nop


FUNCTION(cpu_delay1):
    lw    $v0, 4($a0)  # 1
    nop
    lw    $v0, 8($a0)  # 2
    addiu $v0, -1
    jr    $ra
    nop


FUNCTION(cpu_delay_j0):
    lw    $v0, 8($a0)  # 2
    nop
    lw    $v0, 0($a0)  # 0
    bnez  $v0, 0f
    nop
    li    $v0, 1
0: # ok
    jr    $ra
    nop


FUNCTION(cpu_delay_j1):
    lw    $v0, 8($a0)  # 2
    nop
    lw    $v0, 0($a0)  # 0
    bgtz  $v0, 0f
    nop
    li    $v0, 1
0: # ok
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

FUNCTION(gte_rtps_sf1lm0):
    cop2  0x0180001    # RTPS sf=1 lm=0
    gte_store_rtps_regs
    jr    $ra
    nop

# same, but FLAG is overwritten by NCLIP, so a flagless RTPS variant may be used
FUNCTION(gte_rtps_sf1lm0_nclip):
    cop2  0x0180001    # RTPS sf=1 lm=0
    cop2  0x1400006    # NCLIP
    gte_store_rtps_regs
    jr    $ra
    nop

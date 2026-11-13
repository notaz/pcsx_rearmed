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


# self-modifying code / dynarec invalidation tests
# these return 0 on success or the number of the failed step

#define OP_LI_V0 0x24020000 // li $v0, imm16

# the code to be modified
.macro smc_victim name
FUNCTION(\name):
    li    $v0, 1       # patched
    jr    $ra
    nop
.endm

.macro smc_enter
    addiu $sp, -32
    sw    $ra, 28($sp)
    sw    $s0, 24($sp)
    sw    $s1, 20($sp)
.endm

# call func (or reg), fail with the step code if the result is not val
.macro smc_expect func, val, code
    jal   \func
    nop
    li    $t0, \val
    bne   $v0, $t0, smc_fail
    li    $v1, \code
.endm

.macro smc_expect_r reg, val, code
    jalr  \reg
    nop
    li    $t0, \val
    bne   $v0, $t0, smc_fail
    li    $v1, \code
.endm

# real hardware needs this after changing code that runs from cached memory
.macro smc_flush
    jal   smc_flush_cache
    nop
.endm

smc_flush_cache:
    li    $t2, 0xa0
    jr    $t2
    li    $t1, 0x44

smc_pass:
    li    $v1, 0
smc_fail:
    move  $v0, $v1
    lw    $ra, 28($sp)
    lw    $s0, 24($sp)
    lw    $s1, 20($sp)
    jr    $ra
    addiu $sp, 32

# store: something that writes $t0 over the "li" of the victim
.macro smc_test_basic victim, store:vararg
    smc_expect \victim, 1, 1
    li    $t0, OP_LI_V0 + 1
    \store                      # same data, twice
    li    $t0, OP_LI_V0 + 1
    \store
    smc_expect \victim, 1, 2
    li    $t0, OP_LI_V0 + 2
    \store                      # changed
    smc_flush
    smc_expect \victim, 2, 3
    li    $t0, OP_LI_V0 + 2
    \store                      # same again
    smc_expect \victim, 2, 4
    li    $t0, OP_LI_V0 + 1
    \store                      # back to the original
    smc_flush
    smc_expect \victim, 1, 5
    li    $t0, OP_LI_V0 + 2
    \store                      # the 2nd variant again
    smc_flush
    smc_expect \victim, 2, 6
    li    $t0, OP_LI_V0 + 3
    \store                      # changed twice without running it
    li    $t0, OP_LI_V0 + 1
    \store
    smc_flush
    smc_expect \victim, 1, 7
    li    $t0, OP_LI_V0 + 2
    \store                      # changed, then rewritten with the same
    li    $t0, OP_LI_V0 + 2
    \store
    smc_flush
    smc_expect \victim, 2, 8
    li    $t0, OP_LI_V0 + 1
    \store
    smc_flush
    smc_expect \victim, 1, 9
.endm

.macro smc_st_swlr
    swr   $t0, 0($s0)
    swl   $t0, 3($s0)
.endm

.macro smc_st_swc2
    mtc2  $t0, $0
    nop
    swc2  $0, 0($s0)
.endm

# name: test function, ptr_ofs: added to the address in $s0
.macro smc_func_basic name, ptr_ofs, store:vararg
smc_victim \name\()_v
    .data
\name\()_p:
    .word \name\()_v + \ptr_ofs
    .text
FUNCTION(\name):
    smc_enter
    lw    $s0, \name\()_p       # so that it's not a known constant
    smc_test_basic \name\()_v, \store
    b     smc_pass
    nop
.endm

# non-constant address, also through kseg1 and kuseg mirrors
smc_func_basic smc_sw_reg,     0,           sw $t0, 0($s0)
smc_func_basic smc_sw_reg_k1,  0x20000000,  sw $t0, 0($s0)
smc_func_basic smc_sw_reg_ku, -0x80000000,  sw $t0, 0($s0)
# constant address
smc_func_basic smc_sw_c,       0,           sw $t0, smc_sw_c_v
smc_func_basic smc_sw_c_k1,    0,           sw $t0, smc_sw_c_k1_v + 0x20000000
smc_func_basic smc_sw_c_ku,    0,           sw $t0, smc_sw_c_ku_v - 0x80000000
# other kinds of stores
smc_func_basic smc_sh,         0,           sh $t0, 0($s0)
smc_func_basic smc_sb,         0,           sb $t0, 0($s0)
smc_func_basic smc_swlr,       0,           smc_st_swlr
smc_func_basic smc_swc2,       0,           smc_st_swc2


# several nearby stores, like when patching immediates
smc_multi_v:
    li    $v0, 0
    addiu $v0, 0x001   # +0x04, patched
    addiu $v0, 0x010   # +0x08, patched
    nop
    nop
    nop
    nop
    nop
    addiu $v0, 0x100   # +0x20, patched
    jr    $ra
    nop

.macro smc_multi_st a, b, c
    li    $t0, \a
    li    $t1, \b
    li    $t2, \c
    sh    $t0, 0x04($s0)
    sh    $t1, 0x08($s0)
    sh    $t2, 0x20($s0)
.endm

    .data
smc_multi_p:
    .word smc_multi_v
    .text
FUNCTION(smc_multi):
    smc_enter
    lw    $s0, smc_multi_p
    smc_expect smc_multi_v, 0x111, 1
    smc_multi_st 0x001, 0x010, 0x100    # same
    smc_expect smc_multi_v, 0x111, 2
    smc_multi_st 0x002, 0x010, 0x100    # only the first one changes
    smc_flush
    smc_expect smc_multi_v, 0x112, 3
    smc_multi_st 0x002, 0x010, 0x200    # only the last one
    smc_flush
    smc_expect smc_multi_v, 0x212, 4
    smc_multi_st 0x002, 0x020, 0x200    # only the middle one
    smc_flush
    smc_expect smc_multi_v, 0x222, 5
    smc_multi_st 0x002, 0x020, 0x200    # same
    smc_expect smc_multi_v, 0x222, 6
    smc_multi_st 0x001, 0x010, 0x100    # original
    smc_flush
    smc_expect smc_multi_v, 0x111, 7
    b     smc_pass
    nop


# lots of writes of the same data, also to data right after the code
smc_victim smc_many_v
    .word 0            # +0x0c

    .data
smc_many_p:
    .word smc_many_v
    .text
FUNCTION(smc_many):
    smc_enter
    lw    $s0, smc_many_p
    smc_expect smc_many_v, 1, 1
    li    $s1, 300
0:
    li    $t0, OP_LI_V0 + 1
    sw    $t0, 0($s0)
    addiu $s1, -1
    bnez  $s1, 0b
    nop
    smc_expect smc_many_v, 1, 2
    li    $s1, 300
0:  # same, but also run it each time
    li    $t0, OP_LI_V0 + 1
    sw    $t0, 0($s0)
    smc_expect smc_many_v, 1, 3
    addiu $s1, -1
    bnez  $s1, 0b
    nop
    li    $s1, 300
0:  # data nearby
    sw    $s1, 0x0c($s0)
    addiu $s1, -1
    bnez  $s1, 0b
    nop
    smc_expect smc_many_v, 1, 4
    li    $t0, OP_LI_V0 + 2
    sw    $t0, 0($s0)   # a real change must still be noticed
    smc_flush
    smc_expect smc_many_v, 2, 5
    li    $s1, 300
0:
    li    $t0, OP_LI_V0 + 2
    sw    $t0, 0($s0)
    addiu $s1, -1
    bnez  $s1, 0b
    nop
    li    $t0, OP_LI_V0 + 1
    sw    $t0, 0($s0)
    smc_flush
    smc_expect smc_many_v, 1, 6
    b     smc_pass
    nop


# switching between 2 variants of the code all the time
smc_victim smc_toggle_v

    .data
smc_toggle_p:
    .word smc_toggle_v
    .text
FUNCTION(smc_toggle):
    smc_enter
    lw    $s0, smc_toggle_p
    li    $s1, 8
0:
    li    $t0, OP_LI_V0 + 2
    sw    $t0, 0($s0)
    smc_flush
    smc_expect smc_toggle_v, 2, 1
    li    $t0, OP_LI_V0 + 1
    sw    $t0, 0($s0)
    smc_flush
    smc_expect smc_toggle_v, 1, 2
    addiu $s1, -1
    bnez  $s1, 0b
    nop
    b     smc_pass
    nop


# the code runs from kseg1 (so no flushing), writes use different mirrors
smc_victim smc_exec_k1_v

    .data
smc_exec_k1_p:
    .word smc_exec_k1_v
    .text
FUNCTION(smc_exec_k1):
    smc_enter
    lw    $s0, smc_exec_k1_p
    lui   $s1, 0x2000
    or    $s1, $s0
    smc_expect_r $s1, 1, 1
    li    $t0, OP_LI_V0 + 1
    sw    $t0, 0($s0)   # same, kseg0
    smc_expect_r $s1, 1, 2
    li    $t0, OP_LI_V0 + 2
    sw    $t0, 0($s0)   # changed, kseg0
    smc_expect_r $s1, 2, 3
    li    $t0, OP_LI_V0 + 2
    sw    $t0, 0($s1)   # same, kseg1
    smc_expect_r $s1, 2, 4
    li    $t0, OP_LI_V0 + 1
    sw    $t0, 0($s1)   # original, kseg1
    smc_expect_r $s1, 1, 5
    b     smc_pass
    nop


# modifies the code ahead of itself
FUNCTION(smc_self):
    smc_enter
    la    $t0, smc_self_loop + 0x20000000
    jr    $t0          # to kseg1, so that there is no need to flush
    li    $s1, 0
smc_self_loop:
    li    $t0, OP_LI_V0 + 2
    subu  $t0, $s1
    sw    $t0, smc_self_insn + 0x20000000
    nop
    nop
    nop
    nop
smc_self_insn:
    li    $v0, 1       # patched: 2 on the first pass, 1 on the second
    li    $t0, 2
    subu  $t0, $s1
    bne   $v0, $t0, smc_fail
    addiu $v1, $s1, 1
    beqz  $s1, smc_self_loop
    li    $s1, 1
    b     smc_pass
    nop

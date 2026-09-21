    .section .start, "ax", @progbits
    .set noreorder
    .align 2
    .global main
    .global _start
    .type _start, @function

_start:
    la    $t0, __bss_start
    la    $t1, __bss_end

    beq   $t0, $t1, _bss_init_skip
    nop

_bss_init:
    sw    $0, 0($t0)
    addiu $t0, 4
    bne   $t0, $t1, _bss_init
    nop

_bss_init_skip:
    la    $a1, _mainargv
    j     main
    li    $a0, 1


    .section .rodata, "a", @progbits
    .align 2
_mainargv:
    .word _progname
    .word 0
_progname:
    .string "PSX.EXE"

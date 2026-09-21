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


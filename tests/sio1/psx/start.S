/* entry point of the test program, see linktest.c */
	.set noreorder
	.section .text.start, "ax", @progbits
	.global _start
	.ent _start
_start:
	li	$sp, 0x801ffff0
	jal	main
	nop
1:	b	1b
	nop
	.end _start

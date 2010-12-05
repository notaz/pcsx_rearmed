/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - linkage_arm.s                                           *
 *   Copyright (C) 2009-2010 Ari64                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	.cpu arm9tdmi
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 18, 4
	.file	"linkage_arm.s"
	.global	rdram
rdram = 0x80000000
	.global	dynarec_local
	.global	reg
	.global	hi
	.global	lo
	.global reg_cop0
	.global reg_cop2d
	.global reg_cop2c
	.global	FCR0
	.global	FCR31
	.global	next_interupt
	.global	cycle_count
	.global	last_count
	.global	pending_exception
	.global	pcaddr
	.global	stop
	.global	invc_ptr
	.global	address
	.global	readmem_dword
	.global	readmem_word
	.global	dword
	.global	word
	.global	hword
	.global	byte
	.global	branch_target
	.global	PC
	.global	mini_ht
	.global	restore_candidate
	.global	memory_map
	/* psx */
	.global psxRegs
	.global psxHLEt_addr

	.bss
	.align	4
	.type	dynarec_local, %object
	.size	dynarec_local, dynarec_local_end-dynarec_local
dynarec_local:
	.space	dynarec_local_end-dynarec_local /*0x400630*/
next_interupt = dynarec_local + 64
	.type	next_interupt, %object
	.size	next_interupt, 4
cycle_count = next_interupt + 4
	.type	cycle_count, %object
	.size	cycle_count, 4
last_count = cycle_count + 4
	.type	last_count, %object
	.size	last_count, 4
pending_exception = last_count + 4
	.type	pending_exception, %object
	.size	pending_exception, 4
stop = pending_exception + 4
	.type	stop, %object
	.size	stop, 4
invc_ptr = stop + 4
	.type	invc_ptr, %object
	.size	invc_ptr, 4
address = invc_ptr + 4
	.type	address, %object
	.size	address, 4
readmem_dword = address + 4
readmem_word = readmem_dword
	.type	readmem_dword, %object
	.size	readmem_dword, 8
dword = readmem_dword + 8
	.type	dword, %object
	.size	dword, 8
word = dword + 8
	.type	word, %object
	.size	word, 4
hword = word + 4
	.type	hword, %object
	.size	hword, 2
byte = hword + 2
	.type	byte, %object
	.size	byte, 1 /* 1 byte free */
FCR0 = hword + 4
	.type	FCR0, %object
	.size	FCR0, 4
FCR31 = FCR0 + 4
	.type	FCR31, %object
	.size	FCR31, 4
reg = FCR31 + 4

/* psxRegs */
psxRegs = reg
	.type	reg, %object
	.size	reg, 128
	.size	psxRegs, psxRegs_end-psxRegs
lo = reg + 128
	.type	lo, %object
	.size	lo, 4
hi = lo + 4
	.type	hi, %object
	.size	hi, 4
reg_cop0 = hi + 4
	.type	reg_cop0, %object
	.size	reg_cop0, 128
reg_cop2d = reg_cop0 + 128
	.type	reg_cop2d, %object
	.size	reg_cop2d, 128
reg_cop2c = reg_cop2d + 128
	.type	reg_cop2c, %object
	.size	reg_cop2c, 128
PC = reg_cop2c + 128
pcaddr = PC
	.type	PC, %object
	.size	PC, 4
code = PC + 4
	.type	code, %object
	.size	code, 4
cycle = code + 4
	.type	cycle, %object
	.size	cycle, 4
interrupt = cycle + 4
	.type	interrupt, %object
	.size	interrupt, 4
intCycle = interrupt + 4
	.type	intCycle, %object
	.size	intCycle, 128
psxRegs_end = intCycle + 128

psxHLEt_addr = psxRegs_end
	.type	psxHLEt_addr, %object
	.size	psxHLEt_addr, 4
align0 = psxHLEt_addr + 4 /* just for alignment */
	.type	align0, %object
	.size	align0, 4
branch_target = align0 + 4
	.type	branch_target, %object
	.size	branch_target, 4
mini_ht = branch_target + 4
	.type	mini_ht, %object
	.size	mini_ht, 256
restore_candidate = mini_ht + 256
	.type	restore_candidate, %object
	.size	restore_candidate, 512
memory_map = restore_candidate + 512
	.type	memory_map, %object
	.size	memory_map, 4194304
dynarec_local_end = memory_map + 4194304

	.text
	.align	2
	.global	dyna_linker
	.type	dyna_linker, %function
dyna_linker:
	/* r0 = virtual target address */
	/* r1 = instruction to patch */
	mov	r12, r0
	mov	r6, #4096
	mov	r2, #0x80000
	ldr	r3, .jiptr
	sub	r6, r6, #1
	ldr	r7, [r1]
	eor	r2, r2, r12, lsr #12
	and	r6, r6, r12, lsr #12
	cmp	r2, #2048
	add	r12, r7, #2
	orrcs	r2, r6, #2048
	ldr	r5, [r3, r2, lsl #2]
	lsl	r12, r12, #8
	/* jump_in lookup */
.A1:
	movs	r4, r5
	beq	.A3
	ldr	r3, [r5]
	ldr	r5, [r4, #12]
	teq	r3, r0
	bne	.A1
	ldr	r3, [r4, #4]
	ldr	r4, [r4, #8]
	tst	r3, r3
	bne	.A1
.A2:
	mov	r5, r1
	add	r1, r1, r12, asr #6
	teq	r1, r4
	moveq	pc, r4 /* Stale i-cache */
	bl	add_link
	sub	r2, r4, r5
	and	r1, r7, #0xff000000
	lsl	r2, r2, #6
	sub	r1, r1, #2
	add	r1, r1, r2, lsr #8
	str	r1, [r5]
	mov	pc, r4
.A3:
	/* hash_table lookup */
	cmp	r2, #2048
	ldr	r3, .jdptr
	eor	r4, r0, r0, lsl #16
	lslcc	r2, r0, #9
	ldr	r6, .htptr
	lsr	r4, r4, #12
	lsrcc	r2, r2, #21
	bic	r4, r4, #15
	ldr	r5, [r3, r2, lsl #2]
	ldr	r7, [r6, r4]!
	teq	r7, r0
	ldreq	pc, [r6, #4]
	ldr	r7, [r6, #8]
	teq	r7, r0
	ldreq	pc, [r6, #12]
	/* jump_dirty lookup */
.A6:
	movs	r4, r5
	beq	.A8
	ldr	r3, [r5]
	ldr	r5, [r4, #12]
	teq	r3, r0
	bne	.A6
.A7:
	ldr	r1, [r4, #8]
	/* hash_table insert */
	ldr	r2, [r6]
	ldr	r3, [r6, #4]
	str	r0, [r6]
	str	r1, [r6, #4]
	str	r2, [r6, #8]
	str	r3, [r6, #12]
	mov	pc, r1
.A8:
	mov	r4, r0
	mov	r5, r1
	bl	new_recompile_block
	tst	r0, r0
	mov	r0, r4
	mov	r1, r5
	beq	dyna_linker
	/* pagefault */
	mov	r1, r0
	mov	r2, #8
	.size	dyna_linker, .-dyna_linker
	.global	exec_pagefault
	.type	exec_pagefault, %function
exec_pagefault:
	/* r0 = instruction pointer */
	/* r1 = fault address */
	/* r2 = cause */
	ldr	r3, [fp, #reg_cop0+48-dynarec_local] /* Status */
	mvn	r6, #0xF000000F
	ldr	r4, [fp, #reg_cop0+16-dynarec_local] /* Context */
	bic	r6, r6, #0x0F800000
	str	r0, [fp, #reg_cop0+56-dynarec_local] /* EPC */
	orr	r3, r3, #2
	str	r1, [fp, #reg_cop0+32-dynarec_local] /* BadVAddr */
	bic	r4, r4, r6
	str	r3, [fp, #reg_cop0+48-dynarec_local] /* Status */
	and	r5, r6, r1, lsr #9
	str	r2, [fp, #reg_cop0+52-dynarec_local] /* Cause */
	and	r1, r1, r6, lsl #9
	str	r1, [fp, #reg_cop0+40-dynarec_local] /* EntryHi */
	orr	r4, r4, r5
	str	r4, [fp, #reg_cop0+16-dynarec_local] /* Context */
	mov	r0, #0x80000000
	bl	get_addr_ht
	mov	pc, r0
	.size	exec_pagefault, .-exec_pagefault

/* Special dynamic linker for the case where a page fault
   may occur in a branch delay slot */
	.global	dyna_linker_ds
	.type	dyna_linker_ds, %function
dyna_linker_ds:
	/* r0 = virtual target address */
	/* r1 = instruction to patch */
	mov	r12, r0
	mov	r6, #4096
	mov	r2, #0x80000
	ldr	r3, .jiptr
	sub	r6, r6, #1
	ldr	r7, [r1]
	eor	r2, r2, r12, lsr #12
	and	r6, r6, r12, lsr #12
	cmp	r2, #2048
	add	r12, r7, #2
	orrcs	r2, r6, #2048
	ldr	r5, [r3, r2, lsl #2]
	lsl	r12, r12, #8
	/* jump_in lookup */
.B1:
	movs	r4, r5
	beq	.B3
	ldr	r3, [r5]
	ldr	r5, [r4, #12]
	teq	r3, r0
	bne	.B1
	ldr	r3, [r4, #4]
	ldr	r4, [r4, #8]
	tst	r3, r3
	bne	.B1
.B2:
	mov	r5, r1
	add	r1, r1, r12, asr #6
	teq	r1, r4
	moveq	pc, r4 /* Stale i-cache */
	bl	add_link
	sub	r2, r4, r5
	and	r1, r7, #0xff000000
	lsl	r2, r2, #6
	sub	r1, r1, #2
	add	r1, r1, r2, lsr #8
	str	r1, [r5]
	mov	pc, r4
.B3:
	/* hash_table lookup */
	cmp	r2, #2048
	ldr	r3, .jdptr
	eor	r4, r0, r0, lsl #16
	lslcc	r2, r0, #9
	ldr	r6, .htptr
	lsr	r4, r4, #12
	lsrcc	r2, r2, #21
	bic	r4, r4, #15
	ldr	r5, [r3, r2, lsl #2]
	ldr	r7, [r6, r4]!
	teq	r7, r0
	ldreq	pc, [r6, #4]
	ldr	r7, [r6, #8]
	teq	r7, r0
	ldreq	pc, [r6, #12]
	/* jump_dirty lookup */
.B6:
	movs	r4, r5
	beq	.B8
	ldr	r3, [r5]
	ldr	r5, [r4, #12]
	teq	r3, r0
	bne	.B6
.B7:
	ldr	r1, [r4, #8]
	/* hash_table insert */
	ldr	r2, [r6]
	ldr	r3, [r6, #4]
	str	r0, [r6]
	str	r1, [r6, #4]
	str	r2, [r6, #8]
	str	r3, [r6, #12]
	mov	pc, r1
.B8:
	mov	r4, r0
	bic	r0, r0, #7
	mov	r5, r1
	orr	r0, r0, #1
	bl	new_recompile_block
	tst	r0, r0
	mov	r0, r4
	mov	r1, r5
	beq	dyna_linker_ds
	/* pagefault */
	bic	r1, r0, #7
	mov	r2, #0x80000008 /* High bit set indicates pagefault in delay slot */
	sub	r0, r1, #4
	b	exec_pagefault
	.size	dyna_linker_ds, .-dyna_linker_ds
.jiptr:
	.word	jump_in
.jdptr:
	.word	jump_dirty
.htptr:
	.word	hash_table

	.align	2
	.global	jump_vaddr_r0
	.type	jump_vaddr_r0, %function
jump_vaddr_r0:
	eor	r2, r0, r0, lsl #16
	b	jump_vaddr
	.size	jump_vaddr_r0, .-jump_vaddr_r0
	.global	jump_vaddr_r1
	.type	jump_vaddr_r1, %function
jump_vaddr_r1:
	eor	r2, r1, r1, lsl #16
	mov	r0, r1
	b	jump_vaddr
	.size	jump_vaddr_r1, .-jump_vaddr_r1
	.global	jump_vaddr_r2
	.type	jump_vaddr_r2, %function
jump_vaddr_r2:
	mov	r0, r2
	eor	r2, r2, r2, lsl #16
	b	jump_vaddr
	.size	jump_vaddr_r2, .-jump_vaddr_r2
	.global	jump_vaddr_r3
	.type	jump_vaddr_r3, %function
jump_vaddr_r3:
	eor	r2, r3, r3, lsl #16
	mov	r0, r3
	b	jump_vaddr
	.size	jump_vaddr_r3, .-jump_vaddr_r3
	.global	jump_vaddr_r4
	.type	jump_vaddr_r4, %function
jump_vaddr_r4:
	eor	r2, r4, r4, lsl #16
	mov	r0, r4
	b	jump_vaddr
	.size	jump_vaddr_r4, .-jump_vaddr_r4
	.global	jump_vaddr_r5
	.type	jump_vaddr_r5, %function
jump_vaddr_r5:
	eor	r2, r5, r5, lsl #16
	mov	r0, r5
	b	jump_vaddr
	.size	jump_vaddr_r5, .-jump_vaddr_r5
	.global	jump_vaddr_r6
	.type	jump_vaddr_r6, %function
jump_vaddr_r6:
	eor	r2, r6, r6, lsl #16
	mov	r0, r6
	b	jump_vaddr
	.size	jump_vaddr_r6, .-jump_vaddr_r6
	.global	jump_vaddr_r8
	.type	jump_vaddr_r8, %function
jump_vaddr_r8:
	eor	r2, r8, r8, lsl #16
	mov	r0, r8
	b	jump_vaddr
	.size	jump_vaddr_r8, .-jump_vaddr_r8
	.global	jump_vaddr_r9
	.type	jump_vaddr_r9, %function
jump_vaddr_r9:
	eor	r2, r9, r9, lsl #16
	mov	r0, r9
	b	jump_vaddr
	.size	jump_vaddr_r9, .-jump_vaddr_r9
	.global	jump_vaddr_r10
	.type	jump_vaddr_r10, %function
jump_vaddr_r10:
	eor	r2, r10, r10, lsl #16
	mov	r0, r10
	b	jump_vaddr
	.size	jump_vaddr_r10, .-jump_vaddr_r10
	.global	jump_vaddr_r12
	.type	jump_vaddr_r12, %function
jump_vaddr_r12:
	eor	r2, r12, r12, lsl #16
	mov	r0, r12
	b	jump_vaddr
	.size	jump_vaddr_r12, .-jump_vaddr_r12
	.global	jump_vaddr_r7
	.type	jump_vaddr_r7, %function
jump_vaddr_r7:
	eor	r2, r7, r7, lsl #16
	add	r0, r7, #0
	.size	jump_vaddr_r7, .-jump_vaddr_r7
	.global	jump_vaddr
	.type	jump_vaddr, %function
jump_vaddr:
	ldr	r1, .htptr
	mvn	r3, #15
	and	r2, r3, r2, lsr #12
	ldr	r2, [r1, r2]!
	teq	r2, r0
	ldreq	pc, [r1, #4]
	ldr	r2, [r1, #8]
	teq	r2, r0
	ldreq	pc, [r1, #12]
	str	r10, [fp, #cycle_count-dynarec_local]
	bl	get_addr
	ldr	r10, [fp, #cycle_count-dynarec_local]
	mov	pc, r0
	.size	jump_vaddr, .-jump_vaddr

	.align	2
	.global	verify_code_ds
	.type	verify_code_ds, %function
verify_code_ds:
	str	r8, [fp, #branch_target-dynarec_local]
	.size	verify_code_ds, .-verify_code_ds
	.global	verify_code_vm
	.type	verify_code_vm, %function
verify_code_vm:
	.global	verify_code
	.type	verify_code, %function
verify_code:
	/* r1 = source */
	/* r2 = target */
	/* r3 = length */
	tst	r3, #4
	mov	r4, #0
	add	r3, r1, r3
	mov	r5, #0
	ldrne	r4, [r1], #4
	mov	r12, #0
	ldrne	r5, [r2], #4
	teq	r1, r3
	beq	.D3
.D2:
	ldr	r7, [r1], #4
	eor	r9, r4, r5
	ldr	r8, [r2], #4
	orrs	r9, r9, r12
	bne	.D4
	ldr	r4, [r1], #4
	eor	r12, r7, r8
	ldr	r5, [r2], #4
	cmp	r1, r3
	bcc	.D2
	teq	r7, r8
.D3:
	teqeq	r4, r5
.D4:
	ldr	r8, [fp, #branch_target-dynarec_local]
	moveq	pc, lr
.D5:
	bl	get_addr
	mov	pc, r0
	.size	verify_code, .-verify_code
	.size	verify_code_vm, .-verify_code_vm

	.align	2
	.global	cc_interrupt
	.type	cc_interrupt, %function
cc_interrupt:
	ldr	r0, [fp, #last_count-dynarec_local]
	mov	r1, #0
	mov	r2, #0x1fc
	add	r10, r0, r10
	str	r1, [fp, #pending_exception-dynarec_local]
	and	r2, r2, r10, lsr #17
	add	r3, fp, #restore_candidate-dynarec_local
	str	r10, [fp, #cycle-dynarec_local] /* PCSX cycles */
@@	str	r10, [fp, #reg_cop0+36-dynarec_local] /* Count */
	ldr	r4, [r2, r3]
	mov	r10, lr
	tst	r4, r4
	bne	.E4
.E1:
	bl	gen_interupt
	mov	lr, r10
	ldr	r10, [fp, #cycle-dynarec_local]
	ldr	r0, [fp, #next_interupt-dynarec_local]
	ldr	r1, [fp, #pending_exception-dynarec_local]
	ldr	r2, [fp, #stop-dynarec_local]
	str	r0, [fp, #last_count-dynarec_local]
	sub	r10, r10, r0
	tst	r2, r2
	bne	.E3
	tst	r1, r1
	moveq	pc, lr
.E2:
	ldr	r0, [fp, #pcaddr-dynarec_local]
	bl	get_addr_ht
	mov	pc, r0
.E3:
	add	r12, fp, #28
	ldmia	r12, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
.E4:
	/* Move 'dirty' blocks to the 'clean' list */
	lsl	r5, r2, #3
	str	r1, [r2, r3]
.E5:
	lsrs	r4, r4, #1
	mov	r0, r5
	add	r5, r5, #1
	blcs	clean_blocks
	tst	r5, #31
	bne	.E5
	b	.E1
	.size	cc_interrupt, .-cc_interrupt

	.align	2
	.global	do_interrupt
	.type	do_interrupt, %function
do_interrupt:
	ldr	r0, [fp, #pcaddr-dynarec_local]
	bl	get_addr_ht
	ldr	r1, [fp, #next_interupt-dynarec_local]
	ldr	r10, [fp, #cycle-dynarec_local]
	str	r1, [fp, #last_count-dynarec_local]
	sub	r10, r10, r1
	add	r10, r10, #2
	mov	pc, r0
	.size	do_interrupt, .-do_interrupt
	.align	2
	.global	fp_exception
	.type	fp_exception, %function
fp_exception:
	mov	r2, #0x10000000
.E7:
	ldr	r1, [fp, #reg_cop0+48-dynarec_local] /* Status */
	mov	r3, #0x80000000
	str	r0, [fp, #reg_cop0+56-dynarec_local] /* EPC */
	orr	r1, #2
	add	r2, r2, #0x2c
	str	r1, [fp, #reg_cop0+48-dynarec_local] /* Status */
	str	r2, [fp, #reg_cop0+52-dynarec_local] /* Cause */
	add	r0, r3, #0x80
	bl	get_addr_ht
	mov	pc, r0
	.size	fp_exception, .-fp_exception
	.align	2
	.global	fp_exception_ds
	.type	fp_exception_ds, %function
fp_exception_ds:
	mov	r2, #0x90000000 /* Set high bit if delay slot */
	b	.E7
	.size	fp_exception_ds, .-fp_exception_ds

	.align	2
	.global	jump_syscall
	.type	jump_syscall, %function
jump_syscall:
	ldr	r1, [fp, #reg_cop0+48-dynarec_local] /* Status */
	mov	r3, #0x80000000
	str	r0, [fp, #reg_cop0+56-dynarec_local] /* EPC */
	orr	r1, #2
	mov	r2, #0x20
	str	r1, [fp, #reg_cop0+48-dynarec_local] /* Status */
	str	r2, [fp, #reg_cop0+52-dynarec_local] /* Cause */
	add	r0, r3, #0x80
	bl	get_addr_ht
	mov	pc, r0
	.size	jump_syscall, .-jump_syscall
	.align	2

	.align	2
	.global	jump_syscall_hle
	.type	jump_syscall_hle, %function
jump_syscall_hle:
	str	r0, [fp, #pcaddr-dynarec_local] /* PC must be set to EPC for psxException */
	ldr	r2, [fp, #last_count-dynarec_local]
	mov	r1, #0    /* in delay slot */
	add	r2, r2, r10
	mov	r0, #0x20 /* cause */
	str	r2, [fp, #cycle-dynarec_local] /* PCSX cycle counter */
	bl	psxException

	/* note: psxException might do recorsive recompiler call from it's HLE code,
	 * so be ready for this */
pcsx_return:
	ldr	r1, [fp, #next_interupt-dynarec_local]
	ldr	r10, [fp, #cycle-dynarec_local]
	ldr	r0, [fp, #pcaddr-dynarec_local]
	sub	r10, r10, r1
	str	r1, [fp, #last_count-dynarec_local]
	bl	get_addr_ht
	mov	pc, r0
	.size	jump_syscall_hle, .-jump_syscall_hle

	.align	2
	.global	jump_hlecall
	.type	jump_hlecall, %function
jump_hlecall:
	ldr	r2, [fp, #last_count-dynarec_local]
	str	r0, [fp, #pcaddr-dynarec_local]
	and	r1, r1, #7
	add	r2, r2, r10
	ldr	r3, [fp, #psxHLEt_addr-dynarec_local] /* psxHLEt */
	str	r2, [fp, #cycle-dynarec_local] /* PCSX cycle counter */
	adr	lr, pcsx_return
	ldr	pc, [r3, r1, lsl #2] @ FIXME
	.size	jump_hlecall, .-jump_hlecall

new_dyna_leave:
	.align	2
	.global	new_dyna_leave
	.type	new_dyna_leave, %function
	ldr	r0, [fp, #last_count-dynarec_local]
	add	r12, fp, #28
	add	r10, r0, r10
	str	r10, [fp, #cycle-dynarec_local]
	ldmia	r12, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
	.size	new_dyna_leave, .-new_dyna_leave

	/* these are used to call memhandlers */
	.align	2
	.global	indirect_jump_indexed
	.type	indirect_jump_indexed, %function
indirect_jump_indexed:
	ldr	r0, [r0, r1, lsl #2]
	.global	indirect_jump
	.type	indirect_jump, %function
indirect_jump:
	ldr	r12, [fp, #last_count-dynarec_local]
	add	r2, r2, r12 
	str	r2, [fp, #cycle-dynarec_local]
	mov	pc, r0
	.size	indirect_jump, .-indirect_jump
	.size	indirect_jump_indexed, .-indirect_jump_indexed

	.align	2
	.global	jump_eret
	.type	jump_eret, %function
jump_eret:
	ldr	r1, [fp, #reg_cop0+48-dynarec_local] /* Status */
	ldr	r0, [fp, #last_count-dynarec_local]
	bic	r1, r1, #2
	add	r10, r0, r10
	str	r1, [fp, #reg_cop0+48-dynarec_local] /* Status */
	str	r10, [fp, #cycle-dynarec_local]
	bl	check_interupt
	ldr	r1, [fp, #next_interupt-dynarec_local]
	ldr	r0, [fp, #reg_cop0+56-dynarec_local] /* EPC */
	str	r1, [fp, #last_count-dynarec_local]
	subs	r10, r10, r1
	bpl	.E11
.E8:
	bl	get_addr
	mov	pc, r0
.E11:
	str	r0, [fp, #pcaddr-dynarec_local]
	bl	cc_interrupt
	ldr	r0, [fp, #pcaddr-dynarec_local]
	b	.E8
	.size	jump_eret, .-jump_eret

	.align	2
	.global	new_dyna_start
	.type	new_dyna_start, %function
new_dyna_start:
	ldr	r12, .dlptr
	stmia	r12, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
	sub	fp, r12, #28
	ldr	r0, [fp, #pcaddr-dynarec_local]
	/*bl	new_recompile_block*/
	bl	get_addr_ht
	ldr	r1, [fp, #next_interupt-dynarec_local]
	ldr	r10, [fp, #cycle-dynarec_local]
	str	r1, [fp, #last_count-dynarec_local]
	sub	r10, r10, r1
	mov	pc, r0
.dlptr:
	.word	dynarec_local+28
	.size	new_dyna_start, .-new_dyna_start

	.align	2
	.global	write_rdram_new
	.type	write_rdram_new, %function
write_rdram_new:
	ldr	r2, [fp, #address-dynarec_local]
	ldr	r0, [fp, #word-dynarec_local]
	str	r0, [r2]
	b	.E12
	.size	write_rdram_new, .-write_rdram_new
	.align	2
	.global	write_rdramb_new
	.type	write_rdramb_new, %function
write_rdramb_new:
	ldr	r2, [fp, #address-dynarec_local]
	ldrb	r0, [fp, #byte-dynarec_local]
	eor	r2, r2, #3
	strb	r0, [r2]
	b	.E12
	.size	write_rdramb_new, .-write_rdramb_new
	.align	2
	.global	write_rdramh_new
	.type	write_rdramh_new, %function
write_rdramh_new:
	ldr	r2, [fp, #address-dynarec_local]
	ldrh	r0, [fp, #hword-dynarec_local]
	eor	r2, r2, #2
	strh	r0, [r2]
	b	.E12
	.size	write_rdramh_new, .-write_rdramh_new

	.align	2
	.global	do_invalidate
	.type	do_invalidate, %function
do_invalidate:
	ldr	r2, [fp, #address-dynarec_local]
.E12:
	ldr	r1, [fp, #invc_ptr-dynarec_local]
	lsr	r0, r2, #12
	ldrb	r2, [r1, r0]
	tst	r2, r2
	beq	invalidate_block
	mov	pc, lr
	.size	do_invalidate, .-do_invalidate

	.align	2
	.global	read_nomem_new
	.type	read_nomem_new, %function
/*read_nomem_new:*/
read_nomemb_new:
read_nomemh_new:
read_nomemd_new:
	/* should never happen */
	b	read_nomem_new
/*
	ldr	r2, [fp, #address-dynarec_local]
	add	r12, fp, #memory_map-dynarec_local
	lsr	r0, r2, #12
	ldr	r12, [r12, r0, lsl #2]
	mov	r1, #8
	tst	r12, r12
	bmi	tlb_exception
	ldr	r0, [r2, r12, lsl #2]
	str	r0, [fp, #readmem_dword-dynarec_local]
	mov	pc, lr
*/
	.size	read_nomem_new, .-read_nomem_new
/*
	.align	2
	.global	read_nomemb_new
	.type	read_nomemb_new, %function
write_nomem_new:
	str	r3, [fp, #24]
	str	lr, [fp, #28]
	bl	do_invalidate
	ldr	r2, [fp, #address-dynarec_local]
	add	r12, fp, #memory_map-dynarec_local
	ldr	lr, [fp, #28]
	lsr	r0, r2, #12
	ldr	r3, [fp, #24]
	ldr	r12, [r12, r0, lsl #2]
	mov	r1, #0xc
	tst	r12, #0x40000000
	bne	tlb_exception
	ldr	r0, [fp, #word-dynarec_local]
	str	r0, [r2, r12, lsl #2]
	mov	pc, lr
	.size	write_nomem_new, .-write_nomem_new

	.align	2
	.global	write_nomemb_new
	.type	write_nomemb_new, %function
write_nomemb_new:
	str	r3, [fp, #24]
	str	lr, [fp, #28]
	bl	do_invalidate
	ldr	r2, [fp, #address-dynarec_local]
	add	r12, fp, #memory_map-dynarec_local
	ldr	lr, [fp, #28]
	lsr	r0, r2, #12
	ldr	r3, [fp, #24]
	ldr	r12, [r12, r0, lsl #2]
	mov	r1, #0xc
	tst	r12, #0x40000000
	bne	tlb_exception
	eor	r2, r2, #3
	ldrb	r0, [fp, #byte-dynarec_local]
	strb	r0, [r2, r12, lsl #2]
	mov	pc, lr
	.size	write_nomemb_new, .-write_nomemb_new

	.align	2
	.global	write_nomemh_new
	.type	write_nomemh_new, %function
write_nomemh_new:
	str	r3, [fp, #24]
	str	lr, [fp, #28]
	bl	do_invalidate
	ldr	r2, [fp, #address-dynarec_local]
	add	r12, fp, #memory_map-dynarec_local
	ldr	lr, [fp, #28]
	lsr	r0, r2, #12
	ldr	r3, [fp, #24]
	ldr	r12, [r12, r0, lsl #2]
	mov	r1, #0xc
	lsls	r12, #2
	bcs	tlb_exception
	eor	r2, r2, #2
	ldrh	r0, [fp, #hword-dynarec_local]
	strh	r0, [r2, r12]
	mov	pc, lr
	.size	write_nomemh_new, .-write_nomemh_new
*/
	.align	2
	.global	breakpoint
	.type	breakpoint, %function
breakpoint:
	/* Set breakpoint here for debugging */
	mov	pc, lr
	.size	breakpoint, .-breakpoint
	.section	.note.GNU-stack,"",%progbits

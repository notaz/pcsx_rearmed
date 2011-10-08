/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   linkage_arm.s for PCSX                                                *
 *   Copyright (C) 2009-2011 Ari64                                         *
 *   Copyright (C) 2010-2011 Gražvydas "notaz" Ignotas                     *
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

/* .equiv HAVE_ARMV7, 1 */

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
	.global	branch_target
	.global	PC
	.global	mini_ht
	.global	restore_candidate
	/* psx */
	.global psxRegs
	.global mem_rtab
	.global mem_wtab
	.global psxH_ptr
	.global inv_code_start
	.global inv_code_end

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
psxRegs = address + 4

/* psxRegs */
	.type	psxRegs, %object
	.size	psxRegs, psxRegs_end-psxRegs
reg = psxRegs
	.type	reg, %object
	.size	reg, 128
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
	.size	intCycle, 256
psxRegs_end = intCycle + 256

mem_rtab = psxRegs_end
	.type	mem_rtab, %object
	.size	mem_rtab, 4
mem_wtab = mem_rtab + 4
	.type	mem_wtab, %object
	.size	mem_wtab, 4
psxH_ptr = mem_wtab + 4
	.type	psxH_ptr, %object
	.size	psxH_ptr, 4
inv_code_start = psxH_ptr + 4
	.type	inv_code_start, %object
	.size	inv_code_start, 4
inv_code_end = inv_code_start + 4
	.type	inv_code_end, %object
	.size	inv_code_end, 4
branch_target = inv_code_end + 4
	.type	branch_target, %object
	.size	branch_target, 4
align0 = branch_target + 4 /* unused/alignment */
	.type	align0, %object
	.size	align0, 4
mini_ht = align0 + 4
	.type	mini_ht, %object
	.size	mini_ht, 256
restore_candidate = mini_ht + 256
	.type	restore_candidate, %object
	.size	restore_candidate, 512
dynarec_local_end = restore_candidate + 512

/* unused */
FCR0 = align0
	.type	FCR0, %object
	.size	FCR0, 4
FCR31 = align0
	.type	FCR31, %object
	.size	FCR31, 4

.macro load_var_adr reg var
.if HAVE_ARMV7
	movw	\reg, #:lower16:\var
	movt	\reg, #:upper16:\var
.else
	ldr	\reg, =\var
.endif
.endm

.macro dyna_linker_main
	/* r0 = virtual target address */
	/* r1 = instruction to patch */
	ldr	r3, .jiptr
	/* get_page */
	lsr     r2, r0, #12
	mov	r6, #4096
	bic	r2, r2, #0xe0000
	sub	r6, r6, #1
	cmp     r2, #0x1000
	ldr	r7, [r1]
	biclt   r2, #0x0e00
	and	r6, r6, r2
	cmp	r2, #2048
	add	r12, r7, #2
	orrcs	r2, r6, #2048
	ldr	r5, [r3, r2, lsl #2]
	lsl	r12, r12, #8
	add	r6, r1, r12, asr #6
	mov	r8, #0
	/* jump_in lookup */
1:
	movs	r4, r5
	beq	2f
	ldr	r3, [r5]
	ldr	r5, [r4, #12]
	teq	r3, r0
	bne	1b
	ldr	r3, [r4, #4]
	ldr	r4, [r4, #8]
	tst	r3, r3
	bne	1b
	teq	r4, r6
	moveq	pc, r4 /* Stale i-cache */
	mov	r8, r4
	b  	1b     /* jump_in may have dupes, continue search */
2:
	tst     r8, r8
	beq	3f     /* r0 not in jump_in */

	mov	r5, r1
	mov	r1, r6
	bl	add_link
	sub	r2, r8, r5
	and	r1, r7, #0xff000000
	lsl	r2, r2, #6
	sub	r1, r1, #2
	add	r1, r1, r2, lsr #8
	str	r1, [r5]
	mov	pc, r8
3:
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
6:
	movs	r4, r5
	beq	8f
	ldr	r3, [r5]
	ldr	r5, [r4, #12]
	teq	r3, r0
	bne	6b
7:
	ldr	r1, [r4, #8]
	/* hash_table insert */
	ldr	r2, [r6]
	ldr	r3, [r6, #4]
	str	r0, [r6]
	str	r1, [r6, #4]
	str	r2, [r6, #8]
	str	r3, [r6, #12]
	mov	pc, r1
8:
.endm

	.text
	.align	2
	.global	dyna_linker
	.type	dyna_linker, %function
dyna_linker:
	/* r0 = virtual target address */
	/* r1 = instruction to patch */
	dyna_linker_main

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
	dyna_linker_main

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
	ldmnefd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
	tst	r1, r1
	moveq	pc, lr
.E2:
	ldr	r0, [fp, #pcaddr-dynarec_local]
	bl	get_addr_ht
	mov	pc, r0
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
	add	r2, r2, r10
	adr	lr, pcsx_return
	str	r2, [fp, #cycle-dynarec_local] /* PCSX cycle counter */
	bx	r1
	.size	jump_hlecall, .-jump_hlecall

	.align	2
	.global	jump_intcall
	.type	jump_intcall, %function
jump_intcall:
	ldr	r2, [fp, #last_count-dynarec_local]
	str	r0, [fp, #pcaddr-dynarec_local]
	add	r2, r2, r10
	adr	lr, pcsx_return
	str	r2, [fp, #cycle-dynarec_local] /* PCSX cycle counter */
	b	execI
	.size	jump_hlecall, .-jump_hlecall

new_dyna_leave:
	.align	2
	.global	new_dyna_leave
	.type	new_dyna_leave, %function
	ldr	r0, [fp, #last_count-dynarec_local]
	add	r12, fp, #28
	add	r10, r0, r10
	str	r10, [fp, #cycle-dynarec_local]
	ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
	.size	new_dyna_leave, .-new_dyna_leave

	.align	2
	.global	invalidate_addr_r0
	.type	invalidate_addr_r0, %function
invalidate_addr_r0:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	b	invalidate_addr_call
	.size	invalidate_addr_r0, .-invalidate_addr_r0
	.align	2
	.global	invalidate_addr_r1
	.type	invalidate_addr_r1, %function
invalidate_addr_r1:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r1
	b	invalidate_addr_call
	.size	invalidate_addr_r1, .-invalidate_addr_r1
	.align	2
	.global	invalidate_addr_r2
	.type	invalidate_addr_r2, %function
invalidate_addr_r2:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r2
	b	invalidate_addr_call
	.size	invalidate_addr_r2, .-invalidate_addr_r2
	.align	2
	.global	invalidate_addr_r3
	.type	invalidate_addr_r3, %function
invalidate_addr_r3:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r3
	b	invalidate_addr_call
	.size	invalidate_addr_r3, .-invalidate_addr_r3
	.align	2
	.global	invalidate_addr_r4
	.type	invalidate_addr_r4, %function
invalidate_addr_r4:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r4
	b	invalidate_addr_call
	.size	invalidate_addr_r4, .-invalidate_addr_r4
	.align	2
	.global	invalidate_addr_r5
	.type	invalidate_addr_r5, %function
invalidate_addr_r5:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r5
	b	invalidate_addr_call
	.size	invalidate_addr_r5, .-invalidate_addr_r5
	.align	2
	.global	invalidate_addr_r6
	.type	invalidate_addr_r6, %function
invalidate_addr_r6:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r6
	b	invalidate_addr_call
	.size	invalidate_addr_r6, .-invalidate_addr_r6
	.align	2
	.global	invalidate_addr_r7
	.type	invalidate_addr_r7, %function
invalidate_addr_r7:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r7
	b	invalidate_addr_call
	.size	invalidate_addr_r7, .-invalidate_addr_r7
	.align	2
	.global	invalidate_addr_r8
	.type	invalidate_addr_r8, %function
invalidate_addr_r8:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r8
	b	invalidate_addr_call
	.size	invalidate_addr_r8, .-invalidate_addr_r8
	.align	2
	.global	invalidate_addr_r9
	.type	invalidate_addr_r9, %function
invalidate_addr_r9:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r9
	b	invalidate_addr_call
	.size	invalidate_addr_r9, .-invalidate_addr_r9
	.align	2
	.global	invalidate_addr_r10
	.type	invalidate_addr_r10, %function
invalidate_addr_r10:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r10
	b	invalidate_addr_call
	.size	invalidate_addr_r10, .-invalidate_addr_r10
	.align	2
	.global	invalidate_addr_r12
	.type	invalidate_addr_r12, %function
invalidate_addr_r12:
	stmia	fp, {r0, r1, r2, r3, r12, lr}
	mov	r0, r12
	.size	invalidate_addr_r12, .-invalidate_addr_r12
	.align	2
	.global	invalidate_addr_call
	.type	invalidate_addr_call, %function
invalidate_addr_call:
	ldr	r12, [fp, #inv_code_start-dynarec_local]
	ldr	lr, [fp, #inv_code_end-dynarec_local]
	cmp	r0, r12
	cmpcs   lr, r0
	blcc	invalidate_addr
	ldmia	fp, {r0, r1, r2, r3, r12, pc}
	.size	invalidate_addr_call, .-invalidate_addr_call

	.align	2
	.global	new_dyna_start
	.type	new_dyna_start, %function
new_dyna_start:
	/* ip is stored to conform EABI alignment */
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
	load_var_adr fp, dynarec_local
	ldr	r0, [fp, #pcaddr-dynarec_local]
	bl	get_addr_ht
	ldr	r1, [fp, #next_interupt-dynarec_local]
	ldr	r10, [fp, #cycle-dynarec_local]
	str	r1, [fp, #last_count-dynarec_local]
	sub	r10, r10, r1
	mov	pc, r0
	.size	new_dyna_start, .-new_dyna_start

/* --------------------------------------- */

.align	2
.global	jump_handler_read8
.global	jump_handler_read16
.global	jump_handler_read32
.global	jump_handler_write8
.global	jump_handler_write16
.global	jump_handler_write32
.global	jump_handler_write_h
.global jump_handle_swl
.global jump_handle_swr


.macro pcsx_read_mem readop tab_shift
	/* r0 = address, r1 = handler_tab, r2 = cycles */
	lsl	r3, r0, #20
	lsr	r3, #(20+\tab_shift)
	ldr	r12, [fp, #last_count-dynarec_local]
	ldr	r1, [r1, r3, lsl #2]
	add	r2, r2, r12
	lsls	r1, #1
.if \tab_shift == 1
	lsl	r3, #1
	\readop	r0, [r1, r3]
.else
	\readop	r0, [r1, r3, lsl #\tab_shift]
.endif
	movcc	pc, lr
	str	r2, [fp, #cycle-dynarec_local]
	bx	r1
.endm

jump_handler_read8:
	add     r1, #0x1000/4*4 + 0x1000/2*4 @ shift to r8 part
	pcsx_read_mem ldrccb, 0

jump_handler_read16:
	add     r1, #0x1000/4*4              @ shift to r16 part
	pcsx_read_mem ldrcch, 1

jump_handler_read32:
	pcsx_read_mem ldrcc, 2


.macro pcsx_write_mem wrtop tab_shift
	/* r0 = address, r1 = data, r2 = cycles, r3 = handler_tab */
	lsl	r12,r0, #20
	lsr	r12, #(20+\tab_shift)
	ldr	r3, [r3, r12, lsl #2]
	str	r0, [fp, #address-dynarec_local]      @ some handlers still need it..
	lsls	r3, #1
	mov     r0, r2                                @ cycle return in case of direct store
.if \tab_shift == 1
	lsl	r12, #1
	\wrtop	r1, [r3, r12]
.else
	\wrtop	r1, [r3, r12, lsl #\tab_shift]
.endif
	movcc	pc, lr
	ldr	r12, [fp, #last_count-dynarec_local]
	mov     r0, r1
	add	r2, r2, r12
	push	{r2, lr}
	str	r2, [fp, #cycle-dynarec_local]
	blx	r3

	ldr	r0, [fp, #next_interupt-dynarec_local]
	pop	{r2, r3}
	str	r0, [fp, #last_count-dynarec_local]
	sub	r0, r2, r0
	bx	r3
.endm

jump_handler_write8:
	add     r3, #0x1000/4*4 + 0x1000/2*4 @ shift to r8 part
	pcsx_write_mem strccb, 0

jump_handler_write16:
	add     r3, #0x1000/4*4              @ shift to r16 part
	pcsx_write_mem strcch, 1

jump_handler_write32:
	pcsx_write_mem strcc, 2

jump_handler_write_h:
	/* r0 = address, r1 = data, r2 = cycles, r3 = handler */
	ldr	r12, [fp, #last_count-dynarec_local]
	str	r0, [fp, #address-dynarec_local]      @ some handlers still need it..
	add	r2, r2, r12
	mov     r0, r1
	push	{r2, lr}
	str	r2, [fp, #cycle-dynarec_local]
	blx	r3

	ldr	r0, [fp, #next_interupt-dynarec_local]
	pop	{r2, r3}
	str	r0, [fp, #last_count-dynarec_local]
	sub	r0, r2, r0
	bx	r3

jump_handle_swl:
	/* r0 = address, r1 = data, r2 = cycles */
	ldr	r3, [fp, #mem_wtab-dynarec_local]
	mov	r12,r0,lsr #12
	ldr	r3, [r3, r12, lsl #2]
	lsls	r3, #1
	bcs	4f
	add	r3, r0, r3
	mov	r0, r2
	tst	r3, #2
	beq	101f
	tst	r3, #1
	beq	2f
3:
	str	r1, [r3, #-3]
	bx	lr
2:
	lsr	r2, r1, #8
	lsr	r1, #24
	strh	r2, [r3, #-2]
	strb	r1, [r3]
	bx	lr
101:
	tst	r3, #1
	lsrne	r1, #16		@ 1
	lsreq	r12, r1, #24	@ 0
	strneh	r1, [r3, #-1]
	streqb	r12, [r3]
	bx	lr
4:
	mov	r0, r2
@	b	abort
	bx	lr		@ TODO?


jump_handle_swr:
	/* r0 = address, r1 = data, r2 = cycles */
	ldr	r3, [fp, #mem_wtab-dynarec_local]
	mov	r12,r0,lsr #12
	ldr	r3, [r3, r12, lsl #2]
	lsls	r3, #1
	bcs	4f
	add	r3, r0, r3
	and	r12,r3, #3
	mov	r0, r2
	cmp	r12,#2
	strgtb	r1, [r3]	@ 3
	streqh	r1, [r3]	@ 2
	cmp	r12,#1
	strlt	r1, [r3]	@ 0
	bxne	lr
	lsr	r2, r1, #8	@ 1
	strb	r1, [r3]
	strh	r2, [r3, #1]
	bx	lr
4:
	mov	r0, r2
@	b	abort
	bx	lr		@ TODO?


@ vim:filetype=armasm

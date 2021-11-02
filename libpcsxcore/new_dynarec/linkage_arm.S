/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   linkage_arm.s for PCSX                                                *
 *   Copyright (C) 2009-2011 Ari64                                         *
 *   Copyright (C) 2010-2013 Gražvydas "notaz" Ignotas                     *
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

#include "arm_features.h"
#include "new_dynarec_config.h"
#include "linkage_offsets.h"


#ifdef __MACH__
#define dynarec_local		ESYM(dynarec_local)
#define add_link		ESYM(add_link)
#define new_recompile_block	ESYM(new_recompile_block)
#define get_addr		ESYM(get_addr)
#define get_addr_ht		ESYM(get_addr_ht)
#define clean_blocks		ESYM(clean_blocks)
#define gen_interupt		ESYM(gen_interupt)
#define psxException		ESYM(psxException)
#define execI			ESYM(execI)
#define invalidate_addr		ESYM(invalidate_addr)
#endif

	.bss
	.align	4
	.global dynarec_local
	.type	dynarec_local, %object
	.size	dynarec_local, LO_dynarec_local_size
dynarec_local:
	.space	LO_dynarec_local_size

#define DRC_VAR_(name, vname, size_) \
	vname = dynarec_local + LO_##name; \
	.global vname; \
	.type	vname, %object; \
	.size	vname, size_

#define DRC_VAR(name, size_) \
	DRC_VAR_(name, ESYM(name), size_)

DRC_VAR(next_interupt, 4)
DRC_VAR(cycle_count, 4)
DRC_VAR(last_count, 4)
DRC_VAR(pending_exception, 4)
DRC_VAR(stop, 4)
DRC_VAR(invc_ptr, 4)
DRC_VAR(address, 4)
DRC_VAR(psxRegs, LO_psxRegs_end - LO_psxRegs)

/* psxRegs */
DRC_VAR(reg, 128)
DRC_VAR(lo, 4)
DRC_VAR(hi, 4)
DRC_VAR(reg_cop0, 128)
DRC_VAR(reg_cop2d, 128)
DRC_VAR(reg_cop2c, 128)
DRC_VAR(pcaddr, 4)
@DRC_VAR(code, 4)
@DRC_VAR(cycle, 4)
@DRC_VAR(interrupt, 4)
@DRC_VAR(intCycle, 256)

DRC_VAR(rcnts, 7*4*4)
DRC_VAR(mem_rtab, 4)
DRC_VAR(mem_wtab, 4)
DRC_VAR(psxH_ptr, 4)
DRC_VAR(zeromem_ptr, 4)
DRC_VAR(inv_code_start, 4)
DRC_VAR(inv_code_end, 4)
DRC_VAR(branch_target, 4)
DRC_VAR(scratch_buf_ptr, 4)
@DRC_VAR(align0, 12) /* unused/alignment */
DRC_VAR(mini_ht, 256)
DRC_VAR(restore_candidate, 512)


#ifdef TEXRELS_FORBIDDEN
	.data
	.align 2
ptr_jump_in:
	.word	ESYM(jump_in)
ptr_jump_dirty:
	.word	ESYM(jump_dirty)
ptr_hash_table:
	.word	ESYM(hash_table)
#endif


	.syntax unified
	.text
	.align	2

#ifndef HAVE_ARMV5
.macro blx rd
	mov	lr, pc
	bx	\rd
.endm
#endif

.macro load_varadr reg var
#if defined(HAVE_ARMV7) && defined(TEXRELS_FORBIDDEN)
	movw	\reg, #:lower16:(\var-(1678f+8))
	movt	\reg, #:upper16:(\var-(1678f+8))
1678:
	add	\reg, pc
#elif defined(HAVE_ARMV7) && !defined(__PIC__)
	movw	\reg, #:lower16:\var
	movt	\reg, #:upper16:\var
#else
	ldr	\reg, =\var
#endif
.endm

.macro load_varadr_ext reg var
#if defined(HAVE_ARMV7) && defined(TEXRELS_FORBIDDEN)
	movw	\reg, #:lower16:(ptr_\var-(1678f+8))
	movt	\reg, #:upper16:(ptr_\var-(1678f+8))
1678:
	ldr	\reg, [pc, \reg]
#else
	load_varadr \reg \var
#endif
.endm

.macro mov_16 reg imm
#ifdef HAVE_ARMV7
	movw	\reg, #\imm
#else
	mov	\reg, #(\imm & 0x00ff)
	orr	\reg, #(\imm & 0xff00)
#endif
.endm

.macro mov_24 reg imm
#ifdef HAVE_ARMV7
	movw	\reg, #(\imm & 0xffff)
	movt	\reg, #(\imm >> 16)
#else
	mov	\reg, #(\imm & 0x0000ff)
	orr	\reg, #(\imm & 0x00ff00)
	orr	\reg, #(\imm & 0xff0000)
#endif
.endm

/* r0 = virtual target address */
/* r1 = instruction to patch */
.macro dyna_linker_main
#ifndef NO_WRITE_EXEC
	load_varadr_ext r3, jump_in
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
	ldr	r3, [r5]	 /* ll_entry .vaddr */
	ldrd	r4, r5, [r4, #8] /* ll_entry .next, .addr */
	teq	r3, r0
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
	load_varadr_ext r3, jump_dirty
	eor	r4, r0, r0, lsl #16
	lslcc	r2, r0, #9
	load_varadr_ext r6, hash_table
	lsr	r4, r4, #12
	lsrcc	r2, r2, #21
	bic	r4, r4, #15
	ldr	r5, [r3, r2, lsl #2]
	ldr	r7, [r6, r4]!
	teq	r7, r0
	ldreq	pc, [r6, #8]
	ldr	r7, [r6, #4]
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
	ldr	r3, [r6, #8]
	str	r0, [r6]
	str	r1, [r6, #8]
	str	r2, [r6, #4]
	str	r3, [r6, #12]
	mov	pc, r1
8:
#else
	/* XXX: should be able to do better than this... */
	bl	get_addr_ht
	mov	pc, r0
#endif
.endm


FUNCTION(dyna_linker):
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

FUNCTION(exec_pagefault):
	/* r0 = instruction pointer */
	/* r1 = fault address */
	/* r2 = cause */
	ldr	r3, [fp, #LO_reg_cop0+48] /* Status */
	mvn	r6, #0xF000000F
	ldr	r4, [fp, #LO_reg_cop0+16] /* Context */
	bic	r6, r6, #0x0F800000
	str	r0, [fp, #LO_reg_cop0+56] /* EPC */
	orr	r3, r3, #2
	str	r1, [fp, #LO_reg_cop0+32] /* BadVAddr */
	bic	r4, r4, r6
	str	r3, [fp, #LO_reg_cop0+48] /* Status */
	and	r5, r6, r1, lsr #9
	str	r2, [fp, #LO_reg_cop0+52] /* Cause */
	and	r1, r1, r6, lsl #9
	str	r1, [fp, #LO_reg_cop0+40] /* EntryHi */
	orr	r4, r4, r5
	str	r4, [fp, #LO_reg_cop0+16] /* Context */
	mov	r0, #0x80000000
	bl	get_addr_ht
	mov	pc, r0
	.size	exec_pagefault, .-exec_pagefault

/* Special dynamic linker for the case where a page fault
   may occur in a branch delay slot */
FUNCTION(dyna_linker_ds):
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

	.align	2

FUNCTION(jump_vaddr_r0):
	eor	r2, r0, r0, lsl #16
	b	jump_vaddr
	.size	jump_vaddr_r0, .-jump_vaddr_r0
FUNCTION(jump_vaddr_r1):
	eor	r2, r1, r1, lsl #16
	mov	r0, r1
	b	jump_vaddr
	.size	jump_vaddr_r1, .-jump_vaddr_r1
FUNCTION(jump_vaddr_r2):
	mov	r0, r2
	eor	r2, r2, r2, lsl #16
	b	jump_vaddr
	.size	jump_vaddr_r2, .-jump_vaddr_r2
FUNCTION(jump_vaddr_r3):
	eor	r2, r3, r3, lsl #16
	mov	r0, r3
	b	jump_vaddr
	.size	jump_vaddr_r3, .-jump_vaddr_r3
FUNCTION(jump_vaddr_r4):
	eor	r2, r4, r4, lsl #16
	mov	r0, r4
	b	jump_vaddr
	.size	jump_vaddr_r4, .-jump_vaddr_r4
FUNCTION(jump_vaddr_r5):
	eor	r2, r5, r5, lsl #16
	mov	r0, r5
	b	jump_vaddr
	.size	jump_vaddr_r5, .-jump_vaddr_r5
FUNCTION(jump_vaddr_r6):
	eor	r2, r6, r6, lsl #16
	mov	r0, r6
	b	jump_vaddr
	.size	jump_vaddr_r6, .-jump_vaddr_r6
FUNCTION(jump_vaddr_r8):
	eor	r2, r8, r8, lsl #16
	mov	r0, r8
	b	jump_vaddr
	.size	jump_vaddr_r8, .-jump_vaddr_r8
FUNCTION(jump_vaddr_r9):
	eor	r2, r9, r9, lsl #16
	mov	r0, r9
	b	jump_vaddr
	.size	jump_vaddr_r9, .-jump_vaddr_r9
FUNCTION(jump_vaddr_r10):
	eor	r2, r10, r10, lsl #16
	mov	r0, r10
	b	jump_vaddr
	.size	jump_vaddr_r10, .-jump_vaddr_r10
FUNCTION(jump_vaddr_r12):
	eor	r2, r12, r12, lsl #16
	mov	r0, r12
	b	jump_vaddr
	.size	jump_vaddr_r12, .-jump_vaddr_r12
FUNCTION(jump_vaddr_r7):
	eor	r2, r7, r7, lsl #16
	add	r0, r7, #0
	.size	jump_vaddr_r7, .-jump_vaddr_r7
FUNCTION(jump_vaddr):
	load_varadr_ext r1, hash_table
	mvn	r3, #15
	and	r2, r3, r2, lsr #12
	ldr	r2, [r1, r2]!
	teq	r2, r0
	ldreq	pc, [r1, #8]
	ldr	r2, [r1, #4]
	teq	r2, r0
	ldreq	pc, [r1, #12]
	str	r10, [fp, #LO_cycle_count]
	bl	get_addr
	ldr	r10, [fp, #LO_cycle_count]
	mov	pc, r0
	.size	jump_vaddr, .-jump_vaddr

	.align	2

FUNCTION(verify_code_ds):
	str	r8, [fp, #LO_branch_target]
FUNCTION(verify_code_vm):
FUNCTION(verify_code):
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
	ldr	r8, [fp, #LO_branch_target]
	moveq	pc, lr
.D5:
	bl	get_addr
	mov	pc, r0
	.size	verify_code, .-verify_code
	.size	verify_code_vm, .-verify_code_vm

	.align	2
FUNCTION(cc_interrupt):
	ldr	r0, [fp, #LO_last_count]
	mov	r1, #0
	mov	r2, #0x1fc
	add	r10, r0, r10
	str	r1, [fp, #LO_pending_exception]
	and	r2, r2, r10, lsr #17
	add	r3, fp, #LO_restore_candidate
	str	r10, [fp, #LO_cycle]		/* PCSX cycles */
@@	str	r10, [fp, #LO_reg_cop0+36]	/* Count */
	ldr	r4, [r2, r3]
	mov	r10, lr
	tst	r4, r4
	bne	.E4
.E1:
	bl	gen_interupt
	mov	lr, r10
	ldr	r10, [fp, #LO_cycle]
	ldr	r0, [fp, #LO_next_interupt]
	ldr	r1, [fp, #LO_pending_exception]
	ldr	r2, [fp, #LO_stop]
	str	r0, [fp, #LO_last_count]
	sub	r10, r10, r0
	tst	r2, r2
	ldmfdne	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
	tst	r1, r1
	moveq	pc, lr
.E2:
	ldr	r0, [fp, #LO_pcaddr]
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
FUNCTION(do_interrupt):
	ldr	r0, [fp, #LO_pcaddr]
	bl	get_addr_ht
	add	r10, r10, #2
	mov	pc, r0
	.size	do_interrupt, .-do_interrupt

	.align	2
FUNCTION(fp_exception):
	mov	r2, #0x10000000
.E7:
	ldr	r1, [fp, #LO_reg_cop0+48] /* Status */
	mov	r3, #0x80000000
	str	r0, [fp, #LO_reg_cop0+56] /* EPC */
	orr	r1, #2
	add	r2, r2, #0x2c
	str	r1, [fp, #LO_reg_cop0+48] /* Status */
	str	r2, [fp, #LO_reg_cop0+52] /* Cause */
	add	r0, r3, #0x80
	bl	get_addr_ht
	mov	pc, r0
	.size	fp_exception, .-fp_exception
	.align	2
FUNCTION(fp_exception_ds):
	mov	r2, #0x90000000 /* Set high bit if delay slot */
	b	.E7
	.size	fp_exception_ds, .-fp_exception_ds

	.align	2
FUNCTION(jump_syscall):
	ldr	r1, [fp, #LO_reg_cop0+48] /* Status */
	mov	r3, #0x80000000
	str	r0, [fp, #LO_reg_cop0+56] /* EPC */
	orr	r1, #2
	mov	r2, #0x20
	str	r1, [fp, #LO_reg_cop0+48] /* Status */
	str	r2, [fp, #LO_reg_cop0+52] /* Cause */
	add	r0, r3, #0x80
	bl	get_addr_ht
	mov	pc, r0
	.size	jump_syscall, .-jump_syscall
	.align	2

	.align	2
FUNCTION(jump_syscall_hle):
	str	r0, [fp, #LO_pcaddr] /* PC must be set to EPC for psxException */
	ldr	r2, [fp, #LO_last_count]
	mov	r1, #0    /* in delay slot */
	add	r2, r2, r10
	mov	r0, #0x20 /* cause */
	str	r2, [fp, #LO_cycle] /* PCSX cycle counter */
	bl	psxException

	/* note: psxException might do recursive recompiler call from it's HLE code,
	 * so be ready for this */
pcsx_return:
	ldr	r1, [fp, #LO_next_interupt]
	ldr	r10, [fp, #LO_cycle]
	ldr	r0, [fp, #LO_pcaddr]
	sub	r10, r10, r1
	str	r1, [fp, #LO_last_count]
	bl	get_addr_ht
	mov	pc, r0
	.size	jump_syscall_hle, .-jump_syscall_hle

	.align	2
FUNCTION(jump_hlecall):
	ldr	r2, [fp, #LO_last_count]
	str	r0, [fp, #LO_pcaddr]
	add	r2, r2, r10
	adr	lr, pcsx_return
	str	r2, [fp, #LO_cycle] /* PCSX cycle counter */
	bx	r1
	.size	jump_hlecall, .-jump_hlecall

	.align	2
FUNCTION(jump_intcall):
	ldr	r2, [fp, #LO_last_count]
	str	r0, [fp, #LO_pcaddr]
	add	r2, r2, r10
	adr	lr, pcsx_return
	str	r2, [fp, #LO_cycle] /* PCSX cycle counter */
	b	execI
	.size	jump_hlecall, .-jump_hlecall

	.align	2
FUNCTION(new_dyna_leave):
	ldr	r0, [fp, #LO_last_count]
	add	r12, fp, #28
	add	r10, r0, r10
	str	r10, [fp, #LO_cycle]
	ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
	.size	new_dyna_leave, .-new_dyna_leave

	.align	2
FUNCTION(invalidate_addr_r0):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	b	invalidate_addr_call
	.size	invalidate_addr_r0, .-invalidate_addr_r0
	.align	2
FUNCTION(invalidate_addr_r1):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r1
	b	invalidate_addr_call
	.size	invalidate_addr_r1, .-invalidate_addr_r1
	.align	2
FUNCTION(invalidate_addr_r2):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r2
	b	invalidate_addr_call
	.size	invalidate_addr_r2, .-invalidate_addr_r2
	.align	2
FUNCTION(invalidate_addr_r3):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r3
	b	invalidate_addr_call
	.size	invalidate_addr_r3, .-invalidate_addr_r3
	.align	2
FUNCTION(invalidate_addr_r4):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r4
	b	invalidate_addr_call
	.size	invalidate_addr_r4, .-invalidate_addr_r4
	.align	2
FUNCTION(invalidate_addr_r5):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r5
	b	invalidate_addr_call
	.size	invalidate_addr_r5, .-invalidate_addr_r5
	.align	2
FUNCTION(invalidate_addr_r6):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r6
	b	invalidate_addr_call
	.size	invalidate_addr_r6, .-invalidate_addr_r6
	.align	2
FUNCTION(invalidate_addr_r7):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r7
	b	invalidate_addr_call
	.size	invalidate_addr_r7, .-invalidate_addr_r7
	.align	2
FUNCTION(invalidate_addr_r8):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r8
	b	invalidate_addr_call
	.size	invalidate_addr_r8, .-invalidate_addr_r8
	.align	2
FUNCTION(invalidate_addr_r9):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r9
	b	invalidate_addr_call
	.size	invalidate_addr_r9, .-invalidate_addr_r9
	.align	2
FUNCTION(invalidate_addr_r10):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r10
	b	invalidate_addr_call
	.size	invalidate_addr_r10, .-invalidate_addr_r10
	.align	2
FUNCTION(invalidate_addr_r12):
	stmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, lr}
	mov	r0, r12
	.size	invalidate_addr_r12, .-invalidate_addr_r12
	.align	2
invalidate_addr_call:
	ldr	r12, [fp, #LO_inv_code_start]
	ldr	lr, [fp, #LO_inv_code_end]
	cmp	r0, r12
	cmpcs   lr, r0
	blcc	invalidate_addr
	ldmia	fp, {r0, r1, r2, r3, EXTRA_UNSAVED_REGS r12, pc}
	.size	invalidate_addr_call, .-invalidate_addr_call

	.align	2
FUNCTION(new_dyna_start):
	/* ip is stored to conform EABI alignment */
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
	mov	fp, r0 /* dynarec_local */
	ldr	r0, [fp, #LO_pcaddr]
	bl	get_addr_ht
	ldr	r1, [fp, #LO_next_interupt]
	ldr	r10, [fp, #LO_cycle]
	str	r1, [fp, #LO_last_count]
	sub	r10, r10, r1
	mov	pc, r0
	.size	new_dyna_start, .-new_dyna_start

/* --------------------------------------- */

.align	2

.macro pcsx_read_mem readop tab_shift
	/* r0 = address, r1 = handler_tab, r2 = cycles */
	lsl	r3, r0, #20
	lsr	r3, #(20+\tab_shift)
	ldr	r12, [fp, #LO_last_count]
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
	str	r2, [fp, #LO_cycle]
	bx	r1
.endm

FUNCTION(jump_handler_read8):
	add     r1, #0x1000/4*4 + 0x1000/2*4 @ shift to r8 part
	pcsx_read_mem ldrbcc, 0

FUNCTION(jump_handler_read16):
	add     r1, #0x1000/4*4              @ shift to r16 part
	pcsx_read_mem ldrhcc, 1

FUNCTION(jump_handler_read32):
	pcsx_read_mem ldrcc, 2


.macro pcsx_write_mem wrtop tab_shift
	/* r0 = address, r1 = data, r2 = cycles, r3 = handler_tab */
	lsl	r12,r0, #20
	lsr	r12, #(20+\tab_shift)
	ldr	r3, [r3, r12, lsl #2]
	str	r0, [fp, #LO_address]      @ some handlers still need it..
	lsls	r3, #1
	mov     r0, r2                                @ cycle return in case of direct store
.if \tab_shift == 1
	lsl	r12, #1
	\wrtop	r1, [r3, r12]
.else
	\wrtop	r1, [r3, r12, lsl #\tab_shift]
.endif
	movcc	pc, lr
	ldr	r12, [fp, #LO_last_count]
	mov     r0, r1
	add	r2, r2, r12
	push	{r2, lr}
	str	r2, [fp, #LO_cycle]
	blx	r3

	ldr	r0, [fp, #LO_next_interupt]
	pop	{r2, r3}
	str	r0, [fp, #LO_last_count]
	sub	r0, r2, r0
	bx	r3
.endm

FUNCTION(jump_handler_write8):
	add     r3, #0x1000/4*4 + 0x1000/2*4 @ shift to r8 part
	pcsx_write_mem strbcc, 0

FUNCTION(jump_handler_write16):
	add     r3, #0x1000/4*4              @ shift to r16 part
	pcsx_write_mem strhcc, 1

FUNCTION(jump_handler_write32):
	pcsx_write_mem strcc, 2

FUNCTION(jump_handler_write_h):
	/* r0 = address, r1 = data, r2 = cycles, r3 = handler */
	ldr	r12, [fp, #LO_last_count]
	str	r0, [fp, #LO_address]      @ some handlers still need it..
	add	r2, r2, r12
	mov     r0, r1
	push	{r2, lr}
	str	r2, [fp, #LO_cycle]
	blx	r3

	ldr	r0, [fp, #LO_next_interupt]
	pop	{r2, r3}
	str	r0, [fp, #LO_last_count]
	sub	r0, r2, r0
	bx	r3

FUNCTION(jump_handle_swl):
	/* r0 = address, r1 = data, r2 = cycles */
	ldr	r3, [fp, #LO_mem_wtab]
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
	strhne	r1, [r3, #-1]
	strbeq	r12, [r3]
	bx	lr
4:
	mov	r0, r2
@	b	abort
	bx	lr		@ TODO?


FUNCTION(jump_handle_swr):
	/* r0 = address, r1 = data, r2 = cycles */
	ldr	r3, [fp, #LO_mem_wtab]
	mov	r12,r0,lsr #12
	ldr	r3, [r3, r12, lsl #2]
	lsls	r3, #1
	bcs	4f
	add	r3, r0, r3
	and	r12,r3, #3
	mov	r0, r2
	cmp	r12,#2
	strbgt	r1, [r3]	@ 3
	strheq	r1, [r3]	@ 2
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


.macro rcntx_read_mode0 num
	/* r0 = address, r2 = cycles */
	ldr	r3, [fp, #LO_rcnts+6*4+7*4*\num] @ cycleStart
	mov	r0, r2, lsl #16
	sub	r0, r0, r3, lsl #16
	lsr	r0, #16
	bx	lr
.endm

FUNCTION(rcnt0_read_count_m0):
	rcntx_read_mode0 0

FUNCTION(rcnt1_read_count_m0):
	rcntx_read_mode0 1

FUNCTION(rcnt2_read_count_m0):
	rcntx_read_mode0 2

FUNCTION(rcnt0_read_count_m1):
	/* r0 = address, r2 = cycles */
	ldr	r3, [fp, #LO_rcnts+6*4+7*4*0] @ cycleStart
	mov_16	r1, 0x3334
	sub	r2, r2, r3
	mul	r0, r1, r2		@ /= 5
	lsr	r0, #16
	bx	lr

FUNCTION(rcnt1_read_count_m1):
	/* r0 = address, r2 = cycles */
	ldr	r3, [fp, #LO_rcnts+6*4+7*4*1]
	mov_24	r1, 0x1e6cde
	sub	r2, r2, r3
	umull	r3, r0, r1, r2		@ ~ /= hsync_cycles, max ~0x1e6cdd
	bx	lr

FUNCTION(rcnt2_read_count_m1):
	/* r0 = address, r2 = cycles */
	ldr	r3, [fp, #LO_rcnts+6*4+7*4*2]
	mov	r0, r2, lsl #16-3
	sub	r0, r0, r3, lsl #16-3
	lsr	r0, #16			@ /= 8
	bx	lr

@ vim:filetype=armasm

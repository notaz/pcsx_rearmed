/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   linkage_arm.s for PCSX                                                *
 *   Copyright (C) 2009-2011 Ari64                                         *
 *   Copyright (C) 2021 notaz                                              *
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
#include "assem_arm64.h"
#include "linkage_offsets.h"

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
DRC_VAR(branch_target, 4)
DRC_VAR(address, 4)
#DRC_VAR(align0, 16) /* unused/alignment */
DRC_VAR(psxRegs, LO_psxRegs_end - LO_psxRegs)

/* psxRegs */
#DRC_VAR(reg, 128)
DRC_VAR(lo, 4)
DRC_VAR(hi, 4)
DRC_VAR(reg_cop0, 128)
DRC_VAR(reg_cop2d, 128)
DRC_VAR(reg_cop2c, 128)
DRC_VAR(pcaddr, 4)
#DRC_VAR(code, 4)
#DRC_VAR(cycle, 4)
#DRC_VAR(interrupt, 4)
#DRC_VAR(intCycle, 256)

DRC_VAR(rcnts, 7*4*4)
DRC_VAR(inv_code_start, 4)
DRC_VAR(inv_code_end, 4)
DRC_VAR(mem_rtab, 8)
DRC_VAR(mem_wtab, 8)
DRC_VAR(psxH_ptr, 8)
DRC_VAR(invc_ptr, 8)
DRC_VAR(zeromem_ptr, 8)
DRC_VAR(scratch_buf_ptr, 8)
#DRC_VAR(align1, 16) /* unused/alignment */
DRC_VAR(mini_ht, 256)
DRC_VAR(restore_candidate, 512)


	.text
	.align	2

/* r0 = virtual target address */
/* r1 = instruction to patch */
.macro dyna_linker_main
	/* XXX TODO: should be able to do better than this... */
	bl	get_addr_ht
	br	x0
.endm


FUNCTION(dyna_linker):
	/* r0 = virtual target address */
	/* r1 = instruction to patch */
	dyna_linker_main
	.size	dyna_linker, .-dyna_linker

FUNCTION(exec_pagefault):
	/* r0 = instruction pointer */
	/* r1 = fault address */
	/* r2 = cause */
	bl	abort
	.size	exec_pagefault, .-exec_pagefault

/* Special dynamic linker for the case where a page fault
   may occur in a branch delay slot */
FUNCTION(dyna_linker_ds):
	/* r0 = virtual target address */
	/* r1 = instruction to patch */
	dyna_linker_main
	.size	dyna_linker_ds, .-dyna_linker_ds

	.align	2
FUNCTION(cc_interrupt):
	ldr	w0, [rFP, #LO_last_count]
	mov	w2, #0x1fc
	add	rCC, w0, rCC
	str	wzr, [rFP, #LO_pending_exception]
	and	w2, w2, rCC, lsr #17
	add	x3, rFP, #LO_restore_candidate
	str	rCC, [rFP, #LO_cycle]		/* PCSX cycles */
#	str	rCC, [rFP, #LO_reg_cop0+36]	/* Count */
	ldr	w19, [x3, w2, uxtw]
	mov	x21, lr
	cbnz	w19, 4f
1:
	bl	gen_interupt
	mov	lr, x21
	ldr	rCC, [rFP, #LO_cycle]
	ldr	w0, [rFP, #LO_next_interupt]
	ldr	w1, [rFP, #LO_pending_exception]
	ldr	w2, [rFP, #LO_stop]
	str	w0, [rFP, #LO_last_count]
	sub	rCC, rCC, w0
	cbnz	w2, new_dyna_leave
	cbnz	w1, 2f
	ret
2:
	ldr	w0, [rFP, #LO_pcaddr]
	bl	get_addr_ht
	br	x0
4:
	/* Move 'dirty' blocks to the 'clean' list */
	lsl	w20, w2, #3
	str	wzr, [x3, w2, uxtw]
5:
	mov	w0, w20
	add	w20, w20, #1
	tbz	w19, #0, 6f
	bl	clean_blocks
6:
	lsr	w19, w19, #1
	tst	w20, #31
	bne	5b
	b	1b
	.size	cc_interrupt, .-cc_interrupt

	.align	2
FUNCTION(fp_exception):
	mov	w2, #0x10000000
0:
	ldr	w1, [fp, #LO_reg_cop0+48] /* Status */
	mov	w3, #0x80000000
	str	w0, [fp, #LO_reg_cop0+56] /* EPC */
	orr	w1, w1, #2
	add	w2, w2, #0x2c
	str	w1, [fp, #LO_reg_cop0+48] /* Status */
	str	w2, [fp, #LO_reg_cop0+52] /* Cause */
	add	w0, w3, #0x80
	bl	get_addr_ht
	br	x0
	.size	fp_exception, .-fp_exception
	.align	2
FUNCTION(fp_exception_ds):
	mov	w2, #0x90000000 /* Set high bit if delay slot */
	b	0b
	.size	fp_exception_ds, .-fp_exception_ds

	.align	2
FUNCTION(jump_syscall):
	ldr	w1, [fp, #LO_reg_cop0+48] /* Status */
	mov	w3, #0x80000000
	str	w0, [fp, #LO_reg_cop0+56] /* EPC */
	orr	w1, w1, #2
	mov	w2, #0x20
	str	w1, [fp, #LO_reg_cop0+48] /* Status */
	str	w2, [fp, #LO_reg_cop0+52] /* Cause */
	add	w0, w3, #0x80
	bl	get_addr_ht
	br	x0
	.size	jump_syscall, .-jump_syscall
	.align	2

	/* note: psxException might do recursive recompiler call from it's HLE code,
	 * so be ready for this */
FUNCTION(jump_to_new_pc):
	ldr	w1, [fp, #LO_next_interupt]
	ldr	rCC, [fp, #LO_cycle]
	ldr	w0, [fp, #LO_pcaddr]
	sub	rCC, rCC, w1
	str	w1, [fp, #LO_last_count]
	bl	get_addr_ht
	br	x0
	.size	jump_to_new_pc, .-jump_to_new_pc

	/* stack must be aligned by 16, and include space for save_regs() use */
	.align	2
FUNCTION(new_dyna_start):
	stp	x29, x30, [sp, #-SSP_ALL]!
	ldr	w1,  [x0, #LO_next_interupt]
	ldr	w2,  [x0, #LO_cycle]
	stp	x19, x20, [sp, #16*1]
	stp	x21, x22, [sp, #16*2]
	stp	x23, x24, [sp, #16*3]
	stp	x25, x26, [sp, #16*4]
	stp	x27, x28, [sp, #16*5]
	mov	rFP, x0
	ldr	w0,  [rFP, #LO_pcaddr]
	str	w1,  [rFP, #LO_last_count]
	sub	rCC, w2, w1
	bl	get_addr_ht
	br	x0
	.size	new_dyna_start, .-new_dyna_start

	.align	2
FUNCTION(new_dyna_leave):
	ldr	w0,  [rFP, #LO_last_count]
	add	rCC, rCC, w0
	str	rCC, [rFP, #LO_cycle]
	ldp	x19, x20, [sp, #16*1]
	ldp	x21, x22, [sp, #16*2]
	ldp	x23, x24, [sp, #16*3]
	ldp	x25, x26, [sp, #16*4]
	ldp	x27, x28, [sp, #16*5]
	ldp	x29, x30, [sp], #SSP_ALL
	ret
	.size	new_dyna_leave, .-new_dyna_leave

/* --------------------------------------- */

.align	2

.macro memhandler_pre
	/* w0 = adddr/data, x1 = rhandler, w2 = cycles, x3 = whandler */
	ldr	w4, [rFP, #LO_last_count]
	add	w4, w4, w2
	str	w4, [rFP, #LO_cycle]
.endm

.macro memhandler_post
	ldr	w2, [rFP, #LO_next_interupt]
	ldr	w1, [rFP, #LO_cycle]
	sub	w0, w1, w2
	str	w2, [rFP, #LO_last_count]
.endm

FUNCTION(do_memhandler_pre):
	memhandler_pre
	ret

FUNCTION(do_memhandler_post):
	memhandler_post
	ret

.macro pcsx_read_mem readop tab_shift
	/* w0 = address, x1 = handler_tab, w2 = cycles */
	ubfm	w4, w0, #\tab_shift, #11
	ldr	x3, [x1, w4, uxtw #3]
	adds	x3, x3, x3
	bcs	0f
	\readop	w0, [x3, w4, uxtw #\tab_shift]
	ret
0:
	stp	xzr, x30, [sp, #-16]!
	memhandler_pre
	blr	x3
.endm

FUNCTION(jump_handler_read8):
	add     x1, x1, #0x1000/4*8 + 0x1000/2*8  /* shift to r8 part */
	pcsx_read_mem ldrb, 0
	b	handler_read_end

FUNCTION(jump_handler_read16):
	add     x1, x1, #0x1000/4*8               /* shift to r16 part */
	pcsx_read_mem ldrh, 1
	b	handler_read_end

FUNCTION(jump_handler_read32):
	pcsx_read_mem ldr, 2

handler_read_end:
	ldp	xzr, x30, [sp], #16
	ret

.macro pcsx_write_mem wrtop movop tab_shift
	/* w0 = address, w1 = data, w2 = cycles, x3 = handler_tab */
	ubfm	w4, w0, #\tab_shift, #11
	ldr	x3, [x3, w4, uxtw #3]
	adds	x3, x3, x3
	bcs	0f
	mov	w0, w2                    /* cycle return */
	\wrtop	w1, [x3, w4, uxtw #\tab_shift]
	ret
0:
	stp	xzr, x30, [sp, #-16]!
	str	w0, [rFP, #LO_address]    /* some handlers still need it... */
	\movop	w0, w1
	memhandler_pre
	blr	x3
.endm

FUNCTION(jump_handler_write8):
	add     x3, x3, #0x1000/4*8 + 0x1000/2*8  /* shift to r8 part */
	pcsx_write_mem strb uxtb 0
	b	handler_write_end

FUNCTION(jump_handler_write16):
	add     x3, x3, #0x1000/4*8               /* shift to r16 part */
	pcsx_write_mem strh uxth 1
	b	handler_write_end

FUNCTION(jump_handler_write32):
	pcsx_write_mem str mov 2

handler_write_end:
	memhandler_post
	ldp	xzr, x30, [sp], #16
	ret

FUNCTION(jump_handle_swl):
	/* w0 = address, w1 = data, w2 = cycles */
	ldr	x3, [fp, #LO_mem_wtab]
	mov	w4, w0, lsr #12
	ldr	x3, [x3, w4, uxtw #3]
	adds	x3, x3, x3
	bcs	4f
	add	x3, x0, x3
	mov	w0, w2
	tbz	x3, #1, 10f	// & 2
	tbz	x3, #0, 2f	// & 1
3:
	stur	w1, [x3, #-3]
	ret
2:
	lsr	w2, w1, #8
	lsr	w1, w1, #24
	sturh	w2, [x3, #-2]
	strb	w1, [x3]
	ret
10:
	tbz	x3, #0, 0f	// & 1
1:
	lsr	w1, w1, #16
	sturh	w1, [x3, #-1]
	ret
0:
	lsr	w2, w1, #24
	strb	w2, [x3]
	ret
4:
	mov	w0, w2		// todo
	bl	abort
	ret

FUNCTION(jump_handle_swr):
	/* w0 = address, w1 = data, w2 = cycles */
	ldr	x3, [fp, #LO_mem_wtab]
	mov	w4, w0, lsr #12
	ldr	x3, [x3, w4, uxtw #3]
	adds	x3, x3, x3
	bcs	4f
	add	x3, x0, x3
	mov	w0, w2
	tbz	x3, #1, 10f	// & 2
	tbz	x3, #0, 2f	// & 1
3:
	strb	w1, [x3]
	ret
2:
	strh	w1, [x3]
	ret
10:
	tbz	x3, #0, 0f	// & 1
1:
	lsr	w2, w1, #8
	strb	w1, [x3]
	sturh	w2, [x3, #1]
	ret
0:
	str	w1, [x3]
	ret
4:
	mov	w0, w2		// todo
	bl	abort
	ret


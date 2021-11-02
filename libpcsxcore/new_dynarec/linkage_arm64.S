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
#DRC_VAR(code, 4)
#DRC_VAR(cycle, 4)
#DRC_VAR(interrupt, 4)
#DRC_VAR(intCycle, 256)

DRC_VAR(rcnts, 7*4*4)
DRC_VAR(mem_rtab, 4)
DRC_VAR(mem_wtab, 4)
DRC_VAR(psxH_ptr, 4)
DRC_VAR(zeromem_ptr, 4)
DRC_VAR(inv_code_start, 4)
DRC_VAR(inv_code_end, 4)
DRC_VAR(branch_target, 4)
DRC_VAR(scratch_buf_ptr, 4)
#DRC_VAR(align0, 12) /* unused/alignment */
DRC_VAR(mini_ht, 256)
DRC_VAR(restore_candidate, 512)


	.text
	.align	2

/* r0 = virtual target address */
/* r1 = instruction to patch */
.macro dyna_linker_main
	/* XXX: should be able to do better than this... */
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

FUNCTION(jump_vaddr):
	bl	abort
	.size	jump_vaddr, .-jump_vaddr

	.align	2

FUNCTION(verify_code_ds):
	bl	abort
FUNCTION(verify_code_vm):
FUNCTION(verify_code):
	/* r1 = source */
	/* r2 = target */
	/* r3 = length */
	bl	abort
	.size	verify_code, .-verify_code
	.size	verify_code_vm, .-verify_code_vm

	.align	2
FUNCTION(cc_interrupt):
	bl	abort
	.size	cc_interrupt, .-cc_interrupt

	.align	2
FUNCTION(do_interrupt):
	bl	abort
	.size	do_interrupt, .-do_interrupt

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

	.align	2
FUNCTION(jump_syscall_hle):
	bl	abort

	/* note: psxException might do recursive recompiler call from it's HLE code,
	 * so be ready for this */
pcsx_return:
	bl	abort // w10
	ldr	w1, [fp, #LO_next_interupt]
	ldr	w10, [fp, #LO_cycle]
	ldr	w0, [fp, #LO_pcaddr]
	sub	w10, w10, w1
	str	w1, [fp, #LO_last_count]
	bl	get_addr_ht
	br	x0
	.size	jump_syscall_hle, .-jump_syscall_hle

	.align	2
FUNCTION(jump_hlecall):
	bl	abort
	.size	jump_hlecall, .-jump_hlecall

	.align	2
FUNCTION(jump_intcall):
	bl	abort
	.size	jump_intcall, .-jump_intcall

	.align	2
FUNCTION(new_dyna_start):
	stp	x29, x30, [sp, #-96]!	// must be aligned by 16
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
	ldp	x29, x30, [sp], #96
	ret
	.size	new_dyna_leave, .-new_dyna_leave

/* --------------------------------------- */

.align	2

FUNCTION(jump_handler_read8):
	bl	abort

FUNCTION(jump_handler_read16):
	bl	abort

FUNCTION(jump_handler_read32):
	bl	abort

FUNCTION(jump_handler_write8):
	bl	abort

FUNCTION(jump_handler_write16):
	bl	abort

FUNCTION(jump_handler_write32):
	bl	abort

FUNCTION(jump_handler_write_h):
	bl	abort

FUNCTION(jump_handle_swl):
	bl	abort

FUNCTION(jump_handle_swr):
	bl	abort


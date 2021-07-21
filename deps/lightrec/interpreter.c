/*
 * Copyright (C) 2019-2020 Paul Cercueil <paul@crapouillou.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include "disassembler.h"
#include "interpreter.h"
#include "lightrec-private.h"
#include "optimizer.h"
#include "regcache.h"

#include <stdbool.h>

struct interpreter;

static u32 int_CP0(struct interpreter *inter);
static u32 int_CP2(struct interpreter *inter);
static u32 int_SPECIAL(struct interpreter *inter);
static u32 int_REGIMM(struct interpreter *inter);
static u32 int_branch(struct interpreter *inter, u32 pc,
		      union code code, bool branch);

typedef u32 (*lightrec_int_func_t)(struct interpreter *inter);

static const lightrec_int_func_t int_standard[64];

struct interpreter {
	struct lightrec_state *state;
	struct block *block;
	struct opcode *op;
	u32 cycles;
	bool delay_slot;
};

static inline u32 execute(lightrec_int_func_t func, struct interpreter *inter)
{
	return (*func)(inter);
}

static inline u32 jump_skip(struct interpreter *inter)
{
	inter->op = inter->op->next;

	return execute(int_standard[inter->op->i.op], inter);
}

static inline u32 jump_next(struct interpreter *inter)
{
	inter->cycles += lightrec_cycles_of_opcode(inter->op->c);

	if (unlikely(inter->delay_slot))
		return 0;

	return jump_skip(inter);
}

static inline u32 jump_after_branch(struct interpreter *inter)
{
	inter->cycles += lightrec_cycles_of_opcode(inter->op->c);

	if (unlikely(inter->delay_slot))
		return 0;

	inter->op = inter->op->next;

	return jump_skip(inter);
}

static void update_cycles_before_branch(struct interpreter *inter)
{
	u32 cycles;

	if (!inter->delay_slot) {
		cycles = lightrec_cycles_of_opcode(inter->op->c);

		if (has_delay_slot(inter->op->c) &&
		    !(inter->op->flags & LIGHTREC_NO_DS))
			cycles += lightrec_cycles_of_opcode(inter->op->next->c);

		inter->cycles += cycles;
		inter->state->current_cycle += inter->cycles;
		inter->cycles = -cycles;
	}
}

static bool is_branch_taken(const u32 *reg_cache, union code op)
{
	switch (op.i.op) {
	case OP_SPECIAL:
		return op.r.op == OP_SPECIAL_JR || op.r.op == OP_SPECIAL_JALR;
	case OP_J:
	case OP_JAL:
		return true;
	case OP_BEQ:
	case OP_META_BEQZ:
		return reg_cache[op.r.rs] == reg_cache[op.r.rt];
	case OP_BNE:
	case OP_META_BNEZ:
		return reg_cache[op.r.rs] != reg_cache[op.r.rt];
	case OP_REGIMM:
		switch (op.r.rt) {
		case OP_REGIMM_BLTZ:
		case OP_REGIMM_BLTZAL:
			return (s32)reg_cache[op.r.rs] < 0;
		case OP_REGIMM_BGEZ:
		case OP_REGIMM_BGEZAL:
			return (s32)reg_cache[op.r.rs] >= 0;
		}
	default:
		break;
	}

	return false;
}

static u32 int_delay_slot(struct interpreter *inter, u32 pc, bool branch)
{
	struct lightrec_state *state = inter->state;
	u32 *reg_cache = state->native_reg_cache;
	struct opcode new_op, *op = inter->op->next;
	union code op_next;
	struct interpreter inter2 = {
		.state = state,
		.cycles = inter->cycles,
		.delay_slot = true,
		.block = NULL,
	};
	bool run_first_op = false, dummy_ld = false, save_rs = false,
	     load_in_ds, branch_in_ds = false, branch_at_addr = false,
	     branch_taken;
	u32 old_rs, new_rs, new_rt;
	u32 next_pc, ds_next_pc;
	u32 cause, epc;

	if (op->i.op == OP_CP0 && op->r.rs == OP_CP0_RFE) {
		/* When an IRQ happens, the PSX exception handlers (when done)
		 * will jump back to the instruction that was executed right
		 * before the IRQ, unless it was a GTE opcode; in that case, it
		 * jumps to the instruction right after.
		 * Since we will never handle the IRQ right after a GTE opcode,
		 * but on branch boundaries, we need to adjust the return
		 * address so that the GTE opcode is effectively executed.
		 */
		cause = (*state->ops.cop0_ops.cfc)(state, op->c.opcode, 13);
		epc = (*state->ops.cop0_ops.cfc)(state, op->c.opcode, 14);

		if (!(cause & 0x7c) && epc == pc - 4)
			pc -= 4;
	}

	if (inter->delay_slot) {
		/* The branch opcode was in a delay slot of another branch
		 * opcode. Just return the target address of the second
		 * branch. */
		return pc;
	}

	/* An opcode located in the delay slot performing a delayed read
	 * requires special handling; we will always resort to using the
	 * interpreter in that case.
	 * Same goes for when we have a branch in a delay slot of another
	 * branch. */
	load_in_ds = load_in_delay_slot(op->c);
	branch_in_ds = has_delay_slot(op->c);

	if (branch) {
		if (load_in_ds || branch_in_ds)
			op_next = lightrec_read_opcode(state, pc);

		if (load_in_ds) {
			/* Verify that the next block actually reads the
			 * destination register of the delay slot opcode. */
			run_first_op = opcode_reads_register(op_next, op->r.rt);
		}

		if (branch_in_ds) {
			run_first_op = true;
			next_pc = pc + 4;
		}

		if (load_in_ds && run_first_op) {
			next_pc = pc + 4;

			/* If the first opcode of the next block writes the
			 * regiser used as the address for the load, we need to
			 * reset to the old value after it has been executed,
			 * then restore the new value after the delay slot
			 * opcode has been executed. */
			save_rs = opcode_reads_register(op->c, op->r.rs) &&
				opcode_writes_register(op_next, op->r.rs);
			if (save_rs)
				old_rs = reg_cache[op->r.rs];

			/* If both the first opcode of the next block and the
			 * delay slot opcode write to the same register, the
			 * value written by the delay slot opcode is
			 * discarded. */
			dummy_ld = opcode_writes_register(op_next, op->r.rt);
		}

		if (!run_first_op) {
			next_pc = pc;
		} else if (has_delay_slot(op_next)) {
			/* The first opcode of the next block is a branch, so we
			 * cannot execute it here, because of the load delay.
			 * Just check whether or not the branch would be taken,
			 * and save that info into the interpreter struct. */
			branch_at_addr = true;
			branch_taken = is_branch_taken(reg_cache, op_next);
			pr_debug("Target of impossible branch is a branch, "
				 "%staken.\n", branch_taken ? "" : "not ");
			inter->cycles += lightrec_cycles_of_opcode(op_next);
			old_rs = reg_cache[op_next.r.rs];
		} else {
			new_op.c = op_next;
			new_op.flags = 0;
			new_op.offset = 0;
			new_op.next = NULL;
			inter2.op = &new_op;

			/* Execute the first opcode of the next block */
			(*int_standard[inter2.op->i.op])(&inter2);

			if (save_rs) {
				new_rs = reg_cache[op->r.rs];
				reg_cache[op->r.rs] = old_rs;
			}

			inter->cycles += lightrec_cycles_of_opcode(op_next);
		}
	} else {
		next_pc = inter->block->pc
			+ (inter->op->offset + 2) * sizeof(u32);
	}

	inter2.block = inter->block;
	inter2.op = op;
	inter2.cycles = inter->cycles;

	if (dummy_ld)
		new_rt = reg_cache[op->r.rt];

	/* Execute delay slot opcode */
	ds_next_pc = (*int_standard[inter2.op->i.op])(&inter2);

	if (branch_at_addr) {
		if (op_next.i.op == OP_SPECIAL)
			/* TODO: Handle JALR setting $ra */
			ds_next_pc = old_rs;
		else if (op_next.i.op == OP_J || op_next.i.op == OP_JAL)
			/* TODO: Handle JAL setting $ra */
			ds_next_pc = (pc & 0xf0000000) | (op_next.j.imm << 2);
		else
			ds_next_pc = pc + 4 + ((s16)op_next.i.imm << 2);
	}

	if (branch_at_addr && !branch_taken) {
		/* If the branch at the target of the branch opcode is not
		 * taken, we jump to its delay slot */
		next_pc = pc + sizeof(u32);
	} else if (branch_at_addr || (!branch && branch_in_ds)) {
		next_pc = ds_next_pc;
	}

	if (save_rs)
		reg_cache[op->r.rs] = new_rs;
	if (dummy_ld)
		reg_cache[op->r.rt] = new_rt;

	inter->cycles += lightrec_cycles_of_opcode(op->c);

	if (branch_at_addr && branch_taken) {
		/* If the branch at the target of the branch opcode is taken,
		 * we execute its delay slot here, and jump to its target
		 * address. */
		op_next = lightrec_read_opcode(state, pc + 4);

		new_op.c = op_next;
		new_op.flags = 0;
		new_op.offset = sizeof(u32);
		new_op.next = NULL;
		inter2.op = &new_op;
		inter2.block = NULL;

		inter->cycles += lightrec_cycles_of_opcode(op_next);

		pr_debug("Running delay slot of branch at target of impossible "
			 "branch\n");
		(*int_standard[inter2.op->i.op])(&inter2);
	}

	return next_pc;
}

static u32 int_unimplemented(struct interpreter *inter)
{
	pr_warn("Unimplemented opcode 0x%08x\n", inter->op->opcode);

	return jump_next(inter);
}

static u32 int_jump(struct interpreter *inter, bool link)
{
	struct lightrec_state *state = inter->state;
	u32 old_pc = inter->block->pc + inter->op->offset * sizeof(u32);
	u32 pc = (old_pc & 0xf0000000) | (inter->op->j.imm << 2);

	if (link)
		state->native_reg_cache[31] = old_pc + 8;

	if (inter->op->flags & LIGHTREC_NO_DS)
		return pc;

	return int_delay_slot(inter, pc, true);
}

static u32 int_J(struct interpreter *inter)
{
	return int_jump(inter, false);
}

static u32 int_JAL(struct interpreter *inter)
{
	return int_jump(inter, true);
}

static u32 int_jumpr(struct interpreter *inter, u8 link_reg)
{
	struct lightrec_state *state = inter->state;
	u32 old_pc, next_pc = state->native_reg_cache[inter->op->r.rs];

	if (link_reg) {
		old_pc = inter->block->pc + inter->op->offset * sizeof(u32);
		state->native_reg_cache[link_reg] = old_pc + 8;
	}

	if (inter->op->flags & LIGHTREC_NO_DS)
		return next_pc;

	return int_delay_slot(inter, next_pc, true);
}

static u32 int_special_JR(struct interpreter *inter)
{
	return int_jumpr(inter, 0);
}

static u32 int_special_JALR(struct interpreter *inter)
{
	return int_jumpr(inter, inter->op->r.rd);
}

static u32 int_do_branch(struct interpreter *inter, u32 old_pc, u32 next_pc)
{
	if (!inter->delay_slot &&
	    (inter->op->flags & LIGHTREC_LOCAL_BRANCH) &&
	    (s16)inter->op->c.i.imm >= 0) {
		next_pc = old_pc + ((1 + (s16)inter->op->c.i.imm) << 2);
		next_pc = lightrec_emulate_block(inter->block, next_pc);
	}

	return next_pc;
}

static u32 int_branch(struct interpreter *inter, u32 pc,
		      union code code, bool branch)
{
	u32 next_pc = pc + 4 + ((s16)code.i.imm << 2);

	update_cycles_before_branch(inter);

	if (inter->op->flags & LIGHTREC_NO_DS) {
		if (branch)
			return int_do_branch(inter, pc, next_pc);
		else
			return jump_next(inter);
	}

	if (!inter->delay_slot)
		next_pc = int_delay_slot(inter, next_pc, branch);

	if (branch)
		return int_do_branch(inter, pc, next_pc);

	if (inter->op->flags & LIGHTREC_EMULATE_BRANCH)
		return pc + 8;
	else
		return jump_after_branch(inter);
}

static u32 int_beq(struct interpreter *inter, bool bne)
{
	u32 rs, rt, old_pc = inter->block->pc + inter->op->offset * sizeof(u32);

	rs = inter->state->native_reg_cache[inter->op->i.rs];
	rt = inter->state->native_reg_cache[inter->op->i.rt];

	return int_branch(inter, old_pc, inter->op->c, (rs == rt) ^ bne);
}

static u32 int_BEQ(struct interpreter *inter)
{
	return int_beq(inter, false);
}

static u32 int_BNE(struct interpreter *inter)
{
	return int_beq(inter, true);
}

static u32 int_bgez(struct interpreter *inter, bool link, bool lt, bool regimm)
{
	u32 old_pc = inter->block->pc + inter->op->offset * sizeof(u32);
	s32 rs;

	if (link)
		inter->state->native_reg_cache[31] = old_pc + 8;

	rs = (s32)inter->state->native_reg_cache[inter->op->i.rs];

	return int_branch(inter, old_pc, inter->op->c,
			  ((regimm && !rs) || rs > 0) ^ lt);
}

static u32 int_regimm_BLTZ(struct interpreter *inter)
{
	return int_bgez(inter, false, true, true);
}

static u32 int_regimm_BGEZ(struct interpreter *inter)
{
	return int_bgez(inter, false, false, true);
}

static u32 int_regimm_BLTZAL(struct interpreter *inter)
{
	return int_bgez(inter, true, true, true);
}

static u32 int_regimm_BGEZAL(struct interpreter *inter)
{
	return int_bgez(inter, true, false, true);
}

static u32 int_BLEZ(struct interpreter *inter)
{
	return int_bgez(inter, false, true, false);
}

static u32 int_BGTZ(struct interpreter *inter)
{
	return int_bgez(inter, false, false, false);
}

static u32 int_cfc(struct interpreter *inter)
{
	struct lightrec_state *state = inter->state;
	const struct opcode *op = inter->op;
	u32 val;

	val = lightrec_mfc(state, op->c);

	if (likely(op->r.rt))
		state->native_reg_cache[op->r.rt] = val;

	return jump_next(inter);
}

static u32 int_ctc(struct interpreter *inter)
{
	struct lightrec_state *state = inter->state;
	const struct opcode *op = inter->op;

	lightrec_mtc(state, op->c, state->native_reg_cache[op->r.rt]);

	/* If we have a MTC0 or CTC0 to CP0 register 12 (Status) or 13 (Cause),
	 * return early so that the emulator will be able to check software
	 * interrupt status. */
	if (!(inter->op->flags & LIGHTREC_NO_DS) &&
	    op->i.op == OP_CP0 && (op->r.rd == 12 || op->r.rd == 13))
		return inter->block->pc + (op->offset + 1) * sizeof(u32);
	else
		return jump_next(inter);
}

static u32 int_cp0_RFE(struct interpreter *inter)
{
	struct lightrec_state *state = inter->state;
	u32 status;

	/* Read CP0 Status register (r12) */
	status = state->ops.cop0_ops.mfc(state, inter->op->c.opcode, 12);

	/* Switch the bits */
	status = ((status & 0x3c) >> 2) | (status & ~0xf);

	/* Write it back */
	state->ops.cop0_ops.ctc(state, inter->op->c.opcode, 12, status);

	return jump_next(inter);
}

static u32 int_CP(struct interpreter *inter)
{
	struct lightrec_state *state = inter->state;
	const struct lightrec_cop_ops *ops;
	const struct opcode *op = inter->op;

	if ((op->j.imm >> 25) & 1)
		ops = &state->ops.cop2_ops;
	else
		ops = &state->ops.cop0_ops;

	(*ops->op)(state, (op->j.imm) & ~(1 << 25));

	return jump_next(inter);
}

static u32 int_ADDI(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_i *op = &inter->op->i;

	if (likely(op->rt))
		reg_cache[op->rt] = reg_cache[op->rs] + (s32)(s16)op->imm;

	return jump_next(inter);
}

static u32 int_SLTI(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_i *op = &inter->op->i;

	if (likely(op->rt))
		reg_cache[op->rt] = (s32)reg_cache[op->rs] < (s32)(s16)op->imm;

	return jump_next(inter);
}

static u32 int_SLTIU(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_i *op = &inter->op->i;

	if (likely(op->rt))
		reg_cache[op->rt] = reg_cache[op->rs] < (u32)(s32)(s16)op->imm;

	return jump_next(inter);
}

static u32 int_ANDI(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_i *op = &inter->op->i;

	if (likely(op->rt))
		reg_cache[op->rt] = reg_cache[op->rs] & op->imm;

	return jump_next(inter);
}

static u32 int_ORI(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_i *op = &inter->op->i;

	if (likely(op->rt))
		reg_cache[op->rt] = reg_cache[op->rs] | op->imm;

	return jump_next(inter);
}

static u32 int_XORI(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_i *op = &inter->op->i;

	if (likely(op->rt))
		reg_cache[op->rt] = reg_cache[op->rs] ^ op->imm;

	return jump_next(inter);
}

static u32 int_LUI(struct interpreter *inter)
{
	struct opcode_i *op = &inter->op->i;

	inter->state->native_reg_cache[op->rt] = op->imm << 16;

	return jump_next(inter);
}

static u32 int_io(struct interpreter *inter, bool is_load)
{
	struct opcode_i *op = &inter->op->i;
	u32 *reg_cache = inter->state->native_reg_cache;
	u32 val;

	val = lightrec_rw(inter->state, inter->op->c,
			  reg_cache[op->rs], reg_cache[op->rt],
			  &inter->op->flags);

	if (is_load && op->rt)
		reg_cache[op->rt] = val;

	return jump_next(inter);
}

static u32 int_load(struct interpreter *inter)
{
	return int_io(inter, true);
}

static u32 int_store(struct interpreter *inter)
{
	u32 next_pc;

	if (likely(!(inter->op->flags & LIGHTREC_SMC)))
		return int_io(inter, false);

	lightrec_rw(inter->state, inter->op->c,
		    inter->state->native_reg_cache[inter->op->i.rs],
		    inter->state->native_reg_cache[inter->op->i.rt],
		    &inter->op->flags);

	next_pc = inter->block->pc + (inter->op->offset + 1) * 4;

	/* Invalidate next PC, to force the rest of the block to be rebuilt */
	lightrec_invalidate(inter->state, next_pc, 4);

	return next_pc;
}

static u32 int_LWC2(struct interpreter *inter)
{
	return int_io(inter, false);
}

static u32 int_special_SLL(struct interpreter *inter)
{
	struct opcode *op = inter->op;
	u32 rt;

	if (op->opcode) { /* Handle NOPs */
		rt = inter->state->native_reg_cache[op->r.rt];
		inter->state->native_reg_cache[op->r.rd] = rt << op->r.imm;
	}

	return jump_next(inter);
}

static u32 int_special_SRL(struct interpreter *inter)
{
	struct opcode *op = inter->op;
	u32 rt = inter->state->native_reg_cache[op->r.rt];

	inter->state->native_reg_cache[op->r.rd] = rt >> op->r.imm;

	return jump_next(inter);
}

static u32 int_special_SRA(struct interpreter *inter)
{
	struct opcode *op = inter->op;
	s32 rt = inter->state->native_reg_cache[op->r.rt];

	inter->state->native_reg_cache[op->r.rd] = rt >> op->r.imm;

	return jump_next(inter);
}

static u32 int_special_SLLV(struct interpreter *inter)
{
	struct opcode *op = inter->op;
	u32 rs = inter->state->native_reg_cache[op->r.rs];
	u32 rt = inter->state->native_reg_cache[op->r.rt];

	inter->state->native_reg_cache[op->r.rd] = rt << (rs & 0x1f);

	return jump_next(inter);
}

static u32 int_special_SRLV(struct interpreter *inter)
{
	struct opcode *op = inter->op;
	u32 rs = inter->state->native_reg_cache[op->r.rs];
	u32 rt = inter->state->native_reg_cache[op->r.rt];

	inter->state->native_reg_cache[op->r.rd] = rt >> (rs & 0x1f);

	return jump_next(inter);
}

static u32 int_special_SRAV(struct interpreter *inter)
{
	struct opcode *op = inter->op;
	u32 rs = inter->state->native_reg_cache[op->r.rs];
	s32 rt = inter->state->native_reg_cache[op->r.rt];

	inter->state->native_reg_cache[op->r.rd] = rt >> (rs & 0x1f);

	return jump_next(inter);
}

static u32 int_syscall_break(struct interpreter *inter)
{

	if (inter->op->r.op == OP_SPECIAL_BREAK)
		inter->state->exit_flags |= LIGHTREC_EXIT_BREAK;
	else
		inter->state->exit_flags |= LIGHTREC_EXIT_SYSCALL;

	return inter->block->pc + inter->op->offset * sizeof(u32);
}

static u32 int_special_MFHI(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;

	if (likely(op->rd))
		reg_cache[op->rd] = reg_cache[REG_HI];

	return jump_next(inter);
}

static u32 int_special_MTHI(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;

	reg_cache[REG_HI] = reg_cache[inter->op->r.rs];

	return jump_next(inter);
}

static u32 int_special_MFLO(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;

	if (likely(op->rd))
		reg_cache[op->rd] = reg_cache[REG_LO];

	return jump_next(inter);
}

static u32 int_special_MTLO(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;

	reg_cache[REG_LO] = reg_cache[inter->op->r.rs];

	return jump_next(inter);
}

static u32 int_special_MULT(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	s32 rs = reg_cache[inter->op->r.rs];
	s32 rt = reg_cache[inter->op->r.rt];
	u64 res = (s64)rs * (s64)rt;

	if (!(inter->op->flags & LIGHTREC_MULT32))
		reg_cache[REG_HI] = res >> 32;
	reg_cache[REG_LO] = res;

	return jump_next(inter);
}

static u32 int_special_MULTU(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	u32 rs = reg_cache[inter->op->r.rs];
	u32 rt = reg_cache[inter->op->r.rt];
	u64 res = (u64)rs * (u64)rt;

	if (!(inter->op->flags & LIGHTREC_MULT32))
		reg_cache[REG_HI] = res >> 32;
	reg_cache[REG_LO] = res;

	return jump_next(inter);
}

static u32 int_special_DIV(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	s32 rs = reg_cache[inter->op->r.rs];
	s32 rt = reg_cache[inter->op->r.rt];
	u32 lo, hi;

	if (rt == 0) {
		hi = rs;
		lo = (rs < 0) * 2 - 1;
	} else if ((rs == 0x80000000) && (rt == 0xFFFFFFFF)) {
		lo = rs;
		hi = 0;
	} else {
		lo = rs / rt;
		hi = rs % rt;
	}

	reg_cache[REG_HI] = hi;
	reg_cache[REG_LO] = lo;

	return jump_next(inter);
}

static u32 int_special_DIVU(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	u32 rs = reg_cache[inter->op->r.rs];
	u32 rt = reg_cache[inter->op->r.rt];
	u32 lo, hi;

	if (rt == 0) {
		hi = rs;
		lo = (u32)-1;
	} else {
		lo = rs / rt;
		hi = rs % rt;
	}

	reg_cache[REG_HI] = hi;
	reg_cache[REG_LO] = lo;

	return jump_next(inter);
}

static u32 int_special_ADD(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	s32 rs = reg_cache[op->rs];
	s32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = rs + rt;

	return jump_next(inter);
}

static u32 int_special_SUB(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	u32 rs = reg_cache[op->rs];
	u32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = rs - rt;

	return jump_next(inter);
}

static u32 int_special_AND(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	u32 rs = reg_cache[op->rs];
	u32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = rs & rt;

	return jump_next(inter);
}

static u32 int_special_OR(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	u32 rs = reg_cache[op->rs];
	u32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = rs | rt;

	return jump_next(inter);
}

static u32 int_special_XOR(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	u32 rs = reg_cache[op->rs];
	u32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = rs ^ rt;

	return jump_next(inter);
}

static u32 int_special_NOR(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	u32 rs = reg_cache[op->rs];
	u32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = ~(rs | rt);

	return jump_next(inter);
}

static u32 int_special_SLT(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	s32 rs = reg_cache[op->rs];
	s32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = rs < rt;

	return jump_next(inter);
}

static u32 int_special_SLTU(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;
	u32 rs = reg_cache[op->rs];
	u32 rt = reg_cache[op->rt];

	if (likely(op->rd))
		reg_cache[op->rd] = rs < rt;

	return jump_next(inter);
}

static u32 int_META_SKIP(struct interpreter *inter)
{
	return jump_skip(inter);
}

static u32 int_META_MOV(struct interpreter *inter)
{
	u32 *reg_cache = inter->state->native_reg_cache;
	struct opcode_r *op = &inter->op->r;

	if (likely(op->rd))
		reg_cache[op->rd] = reg_cache[op->rs];

	return jump_next(inter);
}

static u32 int_META_SYNC(struct interpreter *inter)
{
	inter->state->current_cycle += inter->cycles;
	inter->cycles = 0;

	return jump_skip(inter);
}

static const lightrec_int_func_t int_standard[64] = {
	[OP_SPECIAL]		= int_SPECIAL,
	[OP_REGIMM]		= int_REGIMM,
	[OP_J]			= int_J,
	[OP_JAL]		= int_JAL,
	[OP_BEQ]		= int_BEQ,
	[OP_BNE]		= int_BNE,
	[OP_BLEZ]		= int_BLEZ,
	[OP_BGTZ]		= int_BGTZ,
	[OP_ADDI]		= int_ADDI,
	[OP_ADDIU]		= int_ADDI,
	[OP_SLTI]		= int_SLTI,
	[OP_SLTIU]		= int_SLTIU,
	[OP_ANDI]		= int_ANDI,
	[OP_ORI]		= int_ORI,
	[OP_XORI]		= int_XORI,
	[OP_LUI]		= int_LUI,
	[OP_CP0]		= int_CP0,
	[OP_CP2]		= int_CP2,
	[OP_LB]			= int_load,
	[OP_LH]			= int_load,
	[OP_LWL]		= int_load,
	[OP_LW]			= int_load,
	[OP_LBU]		= int_load,
	[OP_LHU]		= int_load,
	[OP_LWR]		= int_load,
	[OP_SB]			= int_store,
	[OP_SH]			= int_store,
	[OP_SWL]		= int_store,
	[OP_SW]			= int_store,
	[OP_SWR]		= int_store,
	[OP_LWC2]		= int_LWC2,
	[OP_SWC2]		= int_store,

	[OP_META_REG_UNLOAD]	= int_META_SKIP,
	[OP_META_BEQZ]		= int_BEQ,
	[OP_META_BNEZ]		= int_BNE,
	[OP_META_MOV]		= int_META_MOV,
	[OP_META_SYNC]		= int_META_SYNC,
};

static const lightrec_int_func_t int_special[64] = {
	[OP_SPECIAL_SLL]	= int_special_SLL,
	[OP_SPECIAL_SRL]	= int_special_SRL,
	[OP_SPECIAL_SRA]	= int_special_SRA,
	[OP_SPECIAL_SLLV]	= int_special_SLLV,
	[OP_SPECIAL_SRLV]	= int_special_SRLV,
	[OP_SPECIAL_SRAV]	= int_special_SRAV,
	[OP_SPECIAL_JR]		= int_special_JR,
	[OP_SPECIAL_JALR]	= int_special_JALR,
	[OP_SPECIAL_SYSCALL]	= int_syscall_break,
	[OP_SPECIAL_BREAK]	= int_syscall_break,
	[OP_SPECIAL_MFHI]	= int_special_MFHI,
	[OP_SPECIAL_MTHI]	= int_special_MTHI,
	[OP_SPECIAL_MFLO]	= int_special_MFLO,
	[OP_SPECIAL_MTLO]	= int_special_MTLO,
	[OP_SPECIAL_MULT]	= int_special_MULT,
	[OP_SPECIAL_MULTU]	= int_special_MULTU,
	[OP_SPECIAL_DIV]	= int_special_DIV,
	[OP_SPECIAL_DIVU]	= int_special_DIVU,
	[OP_SPECIAL_ADD]	= int_special_ADD,
	[OP_SPECIAL_ADDU]	= int_special_ADD,
	[OP_SPECIAL_SUB]	= int_special_SUB,
	[OP_SPECIAL_SUBU]	= int_special_SUB,
	[OP_SPECIAL_AND]	= int_special_AND,
	[OP_SPECIAL_OR]		= int_special_OR,
	[OP_SPECIAL_XOR]	= int_special_XOR,
	[OP_SPECIAL_NOR]	= int_special_NOR,
	[OP_SPECIAL_SLT]	= int_special_SLT,
	[OP_SPECIAL_SLTU]	= int_special_SLTU,
};

static const lightrec_int_func_t int_regimm[64] = {
	[OP_REGIMM_BLTZ]	= int_regimm_BLTZ,
	[OP_REGIMM_BGEZ]	= int_regimm_BGEZ,
	[OP_REGIMM_BLTZAL]	= int_regimm_BLTZAL,
	[OP_REGIMM_BGEZAL]	= int_regimm_BGEZAL,
};

static const lightrec_int_func_t int_cp0[64] = {
	[OP_CP0_MFC0]		= int_cfc,
	[OP_CP0_CFC0]		= int_cfc,
	[OP_CP0_MTC0]		= int_ctc,
	[OP_CP0_CTC0]		= int_ctc,
	[OP_CP0_RFE]		= int_cp0_RFE,
};

static const lightrec_int_func_t int_cp2_basic[64] = {
	[OP_CP2_BASIC_MFC2]	= int_cfc,
	[OP_CP2_BASIC_CFC2]	= int_cfc,
	[OP_CP2_BASIC_MTC2]	= int_ctc,
	[OP_CP2_BASIC_CTC2]	= int_ctc,
};

static u32 int_SPECIAL(struct interpreter *inter)
{
	lightrec_int_func_t f = int_special[inter->op->r.op];
	if (likely(f))
		return execute(f, inter);
	else
		return int_unimplemented(inter);
}

static u32 int_REGIMM(struct interpreter *inter)
{
	lightrec_int_func_t f = int_regimm[inter->op->r.rt];
	if (likely(f))
		return execute(f, inter);
	else
		return int_unimplemented(inter);
}

static u32 int_CP0(struct interpreter *inter)
{
	lightrec_int_func_t f = int_cp0[inter->op->r.rs];
	if (likely(f))
		return execute(f, inter);
	else
		return int_CP(inter);
}

static u32 int_CP2(struct interpreter *inter)
{
	if (inter->op->r.op == OP_CP2_BASIC) {
		lightrec_int_func_t f = int_cp2_basic[inter->op->r.rs];
		if (likely(f))
			return execute(f, inter);
	}

	return int_CP(inter);
}

static u32 lightrec_int_op(struct interpreter *inter)
{
	return execute(int_standard[inter->op->i.op], inter);
}

static u32 lightrec_emulate_block_list(struct block *block, struct opcode *op)
{
	struct interpreter inter;
	u32 pc;

	inter.block = block;
	inter.state = block->state;
	inter.op = op;
	inter.cycles = 0;
	inter.delay_slot = false;

	pc = lightrec_int_op(&inter);

	/* Add the cycles of the last branch */
	inter.cycles += lightrec_cycles_of_opcode(inter.op->c);

	block->state->current_cycle += inter.cycles;

	return pc;
}

u32 lightrec_emulate_block(struct block *block, u32 pc)
{
	u32 offset = (kunseg(pc) - kunseg(block->pc)) >> 2;
	struct opcode *op;

	for (op = block->opcode_list;
	     op && (op->offset < offset); op = op->next);
	if (op)
		return lightrec_emulate_block_list(block, op);

	pr_err("PC 0x%x is outside block at PC 0x%x\n", pc, block->pc);

	return 0;
}

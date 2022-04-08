// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2014-2021 Paul Cercueil <paul@crapouillou.net>
 */

#include "blockcache.h"
#include "debug.h"
#include "disassembler.h"
#include "emitter.h"
#include "interpreter.h"
#include "lightrec-config.h"
#include "lightning-wrapper.h"
#include "lightrec.h"
#include "memmanager.h"
#include "reaper.h"
#include "recompiler.h"
#include "regcache.h"
#include "optimizer.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#if ENABLE_THREADED_COMPILER
#include <stdatomic.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#if ENABLE_TINYMM
#include <tinymm.h>
#endif

#define GENMASK(h, l) \
	(((uintptr_t)-1 << (l)) & ((uintptr_t)-1 >> (__WORDSIZE - 1 - (h))))

static struct block * lightrec_precompile_block(struct lightrec_state *state,
						u32 pc);
static bool lightrec_block_is_fully_tagged(const struct block *block);

static void lightrec_mtc2(struct lightrec_state *state, u8 reg, u32 data);
static u32 lightrec_mfc2(struct lightrec_state *state, u8 reg);

static void lightrec_default_sb(struct lightrec_state *state, u32 opcode,
				void *host, u32 addr, u8 data)
{
	*(u8 *)host = data;

	if (!state->invalidate_from_dma_only)
		lightrec_invalidate(state, addr, 1);
}

static void lightrec_default_sh(struct lightrec_state *state, u32 opcode,
				void *host, u32 addr, u16 data)
{
	*(u16 *)host = HTOLE16(data);

	if (!state->invalidate_from_dma_only)
		lightrec_invalidate(state, addr, 2);
}

static void lightrec_default_sw(struct lightrec_state *state, u32 opcode,
				void *host, u32 addr, u32 data)
{
	*(u32 *)host = HTOLE32(data);

	if (!state->invalidate_from_dma_only)
		lightrec_invalidate(state, addr, 4);
}

static u8 lightrec_default_lb(struct lightrec_state *state,
			      u32 opcode, void *host, u32 addr)
{
	return *(u8 *)host;
}

static u16 lightrec_default_lh(struct lightrec_state *state,
			       u32 opcode, void *host, u32 addr)
{
	return LE16TOH(*(u16 *)host);
}

static u32 lightrec_default_lw(struct lightrec_state *state,
			       u32 opcode, void *host, u32 addr)
{
	return LE32TOH(*(u32 *)host);
}

static const struct lightrec_mem_map_ops lightrec_default_ops = {
	.sb = lightrec_default_sb,
	.sh = lightrec_default_sh,
	.sw = lightrec_default_sw,
	.lb = lightrec_default_lb,
	.lh = lightrec_default_lh,
	.lw = lightrec_default_lw,
};

static void __segfault_cb(struct lightrec_state *state, u32 addr,
			  const struct block *block)
{
	lightrec_set_exit_flags(state, LIGHTREC_EXIT_SEGFAULT);
	pr_err("Segmentation fault in recompiled code: invalid "
	       "load/store at address 0x%08x\n", addr);
	if (block)
		pr_err("Was executing block PC 0x%08x\n", block->pc);
}

static void lightrec_swl(struct lightrec_state *state,
			 const struct lightrec_mem_map_ops *ops,
			 u32 opcode, void *host, u32 addr, u32 data)
{
	unsigned int shift = addr & 0x3;
	unsigned int mask = GENMASK(31, (shift + 1) * 8);
	u32 old_data;

	/* Align to 32 bits */
	addr &= ~3;
	host = (void *)((uintptr_t)host & ~3);

	old_data = ops->lw(state, opcode, host, addr);

	data = (data >> ((3 - shift) * 8)) | (old_data & mask);

	ops->sw(state, opcode, host, addr, data);
}

static void lightrec_swr(struct lightrec_state *state,
			 const struct lightrec_mem_map_ops *ops,
			 u32 opcode, void *host, u32 addr, u32 data)
{
	unsigned int shift = addr & 0x3;
	unsigned int mask = (1 << (shift * 8)) - 1;
	u32 old_data;

	/* Align to 32 bits */
	addr &= ~3;
	host = (void *)((uintptr_t)host & ~3);

	old_data = ops->lw(state, opcode, host, addr);

	data = (data << (shift * 8)) | (old_data & mask);

	ops->sw(state, opcode, host, addr, data);
}

static void lightrec_swc2(struct lightrec_state *state, union code op,
			  const struct lightrec_mem_map_ops *ops,
			  void *host, u32 addr)
{
	u32 data = lightrec_mfc2(state, op.i.rt);

	ops->sw(state, op.opcode, host, addr, data);
}

static u32 lightrec_lwl(struct lightrec_state *state,
			const struct lightrec_mem_map_ops *ops,
			u32 opcode, void *host, u32 addr, u32 data)
{
	unsigned int shift = addr & 0x3;
	unsigned int mask = (1 << (24 - shift * 8)) - 1;
	u32 old_data;

	/* Align to 32 bits */
	addr &= ~3;
	host = (void *)((uintptr_t)host & ~3);

	old_data = ops->lw(state, opcode, host, addr);

	return (data & mask) | (old_data << (24 - shift * 8));
}

static u32 lightrec_lwr(struct lightrec_state *state,
			const struct lightrec_mem_map_ops *ops,
			u32 opcode, void *host, u32 addr, u32 data)
{
	unsigned int shift = addr & 0x3;
	unsigned int mask = GENMASK(31, 32 - shift * 8);
	u32 old_data;

	/* Align to 32 bits */
	addr &= ~3;
	host = (void *)((uintptr_t)host & ~3);

	old_data = ops->lw(state, opcode, host, addr);

	return (data & mask) | (old_data >> (shift * 8));
}

static void lightrec_lwc2(struct lightrec_state *state, union code op,
			  const struct lightrec_mem_map_ops *ops,
			  void *host, u32 addr)
{
	u32 data = ops->lw(state, op.opcode, host, addr);

	lightrec_mtc2(state, op.i.rt, data);
}

static void lightrec_invalidate_map(struct lightrec_state *state,
		const struct lightrec_mem_map *map, u32 addr, u32 len)
{
	if (map == &state->maps[PSX_MAP_KERNEL_USER_RAM]) {
		memset(&state->code_lut[lut_offset(addr)], 0,
		       ((len + 3) / 4) * sizeof(void *));
	}
}

const struct lightrec_mem_map *
lightrec_get_map(struct lightrec_state *state, void **host, u32 kaddr)
{
	const struct lightrec_mem_map *map;
	unsigned int i;
	u32 addr;

	for (i = 0; i < state->nb_maps; i++) {
		const struct lightrec_mem_map *mapi = &state->maps[i];

		if (kaddr >= mapi->pc && kaddr < mapi->pc + mapi->length) {
			map = mapi;
			break;
		}
	}

	if (i == state->nb_maps)
		return NULL;

	addr = kaddr - map->pc;

	while (map->mirror_of)
		map = map->mirror_of;

	if (host)
		*host = map->address + addr;

	return map;
}

u32 lightrec_rw(struct lightrec_state *state, union code op,
		u32 addr, u32 data, u16 *flags, struct block *block)
{
	const struct lightrec_mem_map *map;
	const struct lightrec_mem_map_ops *ops;
	u32 opcode = op.opcode;
	void *host;

	addr += (s16) op.i.imm;

	map = lightrec_get_map(state, &host, kunseg(addr));
	if (!map) {
		__segfault_cb(state, addr, block);
		return 0;
	}

	if (unlikely(map->ops)) {
		if (flags)
			*flags |= LIGHTREC_HW_IO;

		ops = map->ops;
	} else {
		if (flags)
			*flags |= LIGHTREC_DIRECT_IO;

		ops = &lightrec_default_ops;
	}

	switch (op.i.op) {
	case OP_SB:
		ops->sb(state, opcode, host, addr, (u8) data);
		return 0;
	case OP_SH:
		ops->sh(state, opcode, host, addr, (u16) data);
		return 0;
	case OP_SWL:
		lightrec_swl(state, ops, opcode, host, addr, data);
		return 0;
	case OP_SWR:
		lightrec_swr(state, ops, opcode, host, addr, data);
		return 0;
	case OP_SW:
		ops->sw(state, opcode, host, addr, data);
		return 0;
	case OP_SWC2:
		lightrec_swc2(state, op, ops, host, addr);
		return 0;
	case OP_LB:
		return (s32) (s8) ops->lb(state, opcode, host, addr);
	case OP_LBU:
		return ops->lb(state, opcode, host, addr);
	case OP_LH:
		return (s32) (s16) ops->lh(state, opcode, host, addr);
	case OP_LHU:
		return ops->lh(state, opcode, host, addr);
	case OP_LWC2:
		lightrec_lwc2(state, op, ops, host, addr);
		return 0;
	case OP_LWL:
		return lightrec_lwl(state, ops, opcode, host, addr, data);
	case OP_LWR:
		return lightrec_lwr(state, ops, opcode, host, addr, data);
	case OP_LW:
	default:
		return ops->lw(state, opcode, host, addr);
	}
}

static void lightrec_rw_helper(struct lightrec_state *state,
			       union code op, u16 *flags,
			       struct block *block)
{
	u32 ret = lightrec_rw(state, op, state->regs.gpr[op.i.rs],
			      state->regs.gpr[op.i.rt], flags, block);

	switch (op.i.op) {
	case OP_LB:
	case OP_LBU:
	case OP_LH:
	case OP_LHU:
	case OP_LWL:
	case OP_LWR:
	case OP_LW:
		if (op.i.rt)
			state->regs.gpr[op.i.rt] = ret;
	default: /* fall-through */
		break;
	}
}

static void lightrec_rw_cb(struct lightrec_state *state, union code op)
{
	lightrec_rw_helper(state, op, NULL, NULL);
}

static void lightrec_rw_generic_cb(struct lightrec_state *state, u32 arg)
{
	struct block *block;
	struct opcode *op;
	bool was_tagged;
	u16 offset = (u16)arg;

	block = lightrec_find_block_from_lut(state->block_cache,
					     arg >> 16, state->next_pc);
	if (unlikely(!block)) {
		pr_err("rw_generic: No block found in LUT for PC 0x%x offset 0x%x\n",
			 state->next_pc, offset);
		return;
	}

	op = &block->opcode_list[offset];
	was_tagged = op->flags & (LIGHTREC_HW_IO | LIGHTREC_DIRECT_IO);

	lightrec_rw_helper(state, op->c, &op->flags, block);

	if (!was_tagged) {
		pr_debug("Opcode of block at PC 0x%08x has been tagged - flag "
			 "for recompilation\n", block->pc);

		block->flags |= BLOCK_SHOULD_RECOMPILE;
	}
}

static u32 clamp_s32(s32 val, s32 min, s32 max)
{
	return val < min ? min : val > max ? max : val;
}

static u32 lightrec_mfc2(struct lightrec_state *state, u8 reg)
{
	s16 gteir1, gteir2, gteir3;

	switch (reg) {
	case 1:
	case 3:
	case 5:
	case 8:
	case 9:
	case 10:
	case 11:
		return (s32)(s16) state->regs.cp2d[reg];
	case 7:
	case 16:
	case 17:
	case 18:
	case 19:
		return (u16) state->regs.cp2d[reg];
	case 28:
	case 29:
		gteir1 = (s16) state->regs.cp2d[9];
		gteir2 = (s16) state->regs.cp2d[10];
		gteir3 = (s16) state->regs.cp2d[11];

		return clamp_s32(gteir1 >> 7, 0, 0x1f) << 0 |
			clamp_s32(gteir2 >> 7, 0, 0x1f) << 5 |
			clamp_s32(gteir3 >> 7, 0, 0x1f) << 10;
	case 15:
		reg = 14;
	default: /* fall-through */
		return state->regs.cp2d[reg];
	}
}

u32 lightrec_mfc(struct lightrec_state *state, union code op)
{
	if (op.i.op == OP_CP0)
		return state->regs.cp0[op.r.rd];
	else if (op.r.rs == OP_CP2_BASIC_MFC2)
		return lightrec_mfc2(state, op.r.rd);
	else
		return state->regs.cp2c[op.r.rd];
}

static void lightrec_mtc0(struct lightrec_state *state, u8 reg, u32 data)
{
	u32 status, oldstatus, cause;

	switch (reg) {
	case 1:
	case 4:
	case 8:
	case 14:
	case 15:
		/* Those registers are read-only */
		return;
	default:
		break;
	}

	if (reg == 12) {
		status = state->regs.cp0[12];
		oldstatus = status;

		if (status & ~data & BIT(16)) {
			state->ops.enable_ram(state, true);
			lightrec_invalidate_all(state);
		} else if (~status & data & BIT(16)) {
			state->ops.enable_ram(state, false);
		}
	}

	if (reg == 13) {
		state->regs.cp0[13] &= ~0x300;
		state->regs.cp0[13] |= data & 0x300;
	} else {
		state->regs.cp0[reg] = data;
	}

	if (reg == 12 || reg == 13) {
		cause = state->regs.cp0[13];
		status = state->regs.cp0[12];

		/* Handle software interrupts */
		if (!!(status & cause & 0x300) & status)
			lightrec_set_exit_flags(state, LIGHTREC_EXIT_CHECK_INTERRUPT);

		/* Handle hardware interrupts */
		if (reg == 12 && !(~status & 0x401) && (~oldstatus & 0x401))
			lightrec_set_exit_flags(state, LIGHTREC_EXIT_CHECK_INTERRUPT);
	}
}

static u32 count_leading_bits(s32 data)
{
	u32 cnt = 33;

#ifdef __has_builtin
#if __has_builtin(__builtin_clrsb)
	return 1 + __builtin_clrsb(data);
#endif
#endif

	data = (data ^ (data >> 31)) << 1;

	do {
		cnt -= 1;
		data >>= 1;
	} while (data);

	return cnt;
}

static void lightrec_mtc2(struct lightrec_state *state, u8 reg, u32 data)
{
	switch (reg) {
	case 15:
		state->regs.cp2d[12] = state->regs.cp2d[13];
		state->regs.cp2d[13] = state->regs.cp2d[14];
		state->regs.cp2d[14] = data;
		break;
	case 28:
		state->regs.cp2d[9] = (data << 7) & 0xf80;
		state->regs.cp2d[10] = (data << 2) & 0xf80;
		state->regs.cp2d[11] = (data >> 3) & 0xf80;
		break;
	case 31:
		return;
	case 30:
		state->regs.cp2d[31] = count_leading_bits((s32) data);
	default: /* fall-through */
		state->regs.cp2d[reg] = data;
		break;
	}
}

static void lightrec_ctc2(struct lightrec_state *state, u8 reg, u32 data)
{
	switch (reg) {
	case 4:
	case 12:
	case 20:
	case 26:
	case 27:
	case 29:
	case 30:
		data = (s32)(s16) data;
		break;
	case 31:
		data = (data & 0x7ffff000) | !!(data & 0x7f87e000) << 31;
	default: /* fall-through */
		break;
	}

	state->regs.cp2c[reg] = data;
}

void lightrec_mtc(struct lightrec_state *state, union code op, u32 data)
{
	if (op.i.op == OP_CP0)
		lightrec_mtc0(state, op.r.rd, data);
	else if (op.r.rs == OP_CP2_BASIC_CTC2)
		lightrec_ctc2(state, op.r.rd, data);
	else
		lightrec_mtc2(state, op.r.rd, data);
}

static void lightrec_mtc_cb(struct lightrec_state *state, union code op)
{
	lightrec_mtc(state, op, state->regs.gpr[op.r.rt]);
}

void lightrec_rfe(struct lightrec_state *state)
{
	u32 status;

	/* Read CP0 Status register (r12) */
	status = state->regs.cp0[12];

	/* Switch the bits */
	status = ((status & 0x3c) >> 2) | (status & ~0xf);

	/* Write it back */
	lightrec_mtc0(state, 12, status);
}

void lightrec_cp(struct lightrec_state *state, union code op)
{
	if (op.i.op == OP_CP0) {
		pr_err("Invalid CP opcode to coprocessor #0\n");
		return;
	}

	(*state->ops.cop2_op)(state, op.opcode);
}

static void lightrec_syscall_cb(struct lightrec_state *state, union code op)
{
	lightrec_set_exit_flags(state, LIGHTREC_EXIT_SYSCALL);
}

static void lightrec_break_cb(struct lightrec_state *state, union code op)
{
	lightrec_set_exit_flags(state, LIGHTREC_EXIT_BREAK);
}

struct block * lightrec_get_block(struct lightrec_state *state, u32 pc)
{
	struct block *block = lightrec_find_block(state->block_cache, pc);

	if (block && lightrec_block_is_outdated(state, block)) {
		pr_debug("Block at PC 0x%08x is outdated!\n", block->pc);

		/* Make sure the recompiler isn't processing the block we'll
		 * destroy */
		if (ENABLE_THREADED_COMPILER)
			lightrec_recompiler_remove(state->rec, block);

		lightrec_unregister_block(state->block_cache, block);
		remove_from_code_lut(state->block_cache, block);
		lightrec_free_block(state, block);
		block = NULL;
	}

	if (!block) {
		block = lightrec_precompile_block(state, pc);
		if (!block) {
			pr_err("Unable to recompile block at PC 0x%x\n", pc);
			lightrec_set_exit_flags(state, LIGHTREC_EXIT_SEGFAULT);
			return NULL;
		}

		lightrec_register_block(state->block_cache, block);
	}

	return block;
}

static void * get_next_block_func(struct lightrec_state *state, u32 pc)
{
	struct block *block;
	bool should_recompile;
	void *func;

	for (;;) {
		func = state->code_lut[lut_offset(pc)];
		if (func && func != state->get_next_block)
			break;

		block = lightrec_get_block(state, pc);

		if (unlikely(!block))
			break;

		if (OPT_REPLACE_MEMSET && (block->flags & BLOCK_IS_MEMSET)) {
			func = state->memset_func;
			break;
		}

		should_recompile = block->flags & BLOCK_SHOULD_RECOMPILE &&
			!(block->flags & BLOCK_IS_DEAD);

		if (unlikely(should_recompile)) {
			pr_debug("Block at PC 0x%08x should recompile\n", pc);

			lightrec_unregister(MEM_FOR_CODE, block->code_size);

			if (ENABLE_THREADED_COMPILER)
				lightrec_recompiler_add(state->rec, block);
			else
				lightrec_compile_block(state->cstate, block);
		}

		if (ENABLE_THREADED_COMPILER && likely(!should_recompile))
			func = lightrec_recompiler_run_first_pass(state, block, &pc);
		else
			func = block->function;

		if (likely(func))
			break;

		if (unlikely(block->flags & BLOCK_NEVER_COMPILE)) {
			pc = lightrec_emulate_block(state, block, pc);

		} else if (!ENABLE_THREADED_COMPILER) {
			/* Block wasn't compiled yet - run the interpreter */
			if (block->flags & BLOCK_FULLY_TAGGED)
				pr_debug("Block fully tagged, skipping first pass\n");
			else if (ENABLE_FIRST_PASS && likely(!should_recompile))
				pc = lightrec_emulate_block(state, block, pc);

			/* Then compile it using the profiled data */
			lightrec_compile_block(state->cstate, block);
		} else {
			lightrec_recompiler_add(state->rec, block);
		}

		if (state->exit_flags != LIGHTREC_EXIT_NORMAL ||
		    state->current_cycle >= state->target_cycle)
			break;
	}

	state->next_pc = pc;
	return func;
}

static s32 c_function_wrapper(struct lightrec_state *state, s32 cycles_delta,
			      void (*f)(struct lightrec_state *, u32 d),
			      u32 d)
{
	state->current_cycle = state->target_cycle - cycles_delta;

	(*f)(state, d);

	return state->target_cycle - state->current_cycle;
}

static struct block * generate_wrapper(struct lightrec_state *state)
{
	struct block *block;
	jit_state_t *_jit;
	unsigned int i;
	int stack_ptr;
	jit_word_t code_size;
	jit_node_t *to_tramp, *to_fn_epilog;
	jit_node_t *addr[C_WRAPPERS_COUNT - 1];

	block = lightrec_malloc(state, MEM_FOR_IR, sizeof(*block));
	if (!block)
		goto err_no_mem;

	_jit = jit_new_state();
	if (!_jit)
		goto err_free_block;

	jit_name("RW wrapper");
	jit_note(__FILE__, __LINE__);

	/* Wrapper entry point */
	jit_prolog();
	jit_tramp(256);

	/* Add entry points; separate them by opcodes that increment
	 * LIGHTREC_REG_STATE (since we cannot touch other registers).
	 * The difference will then tell us which C function to call. */
	for (i = C_WRAPPERS_COUNT - 1; i > 0; i--) {
		jit_addi(LIGHTREC_REG_STATE, LIGHTREC_REG_STATE, __WORDSIZE / 8);
		addr[i - 1] = jit_indirect();
	}

	jit_epilog();
	jit_prolog();

	stack_ptr = jit_allocai(sizeof(uintptr_t) * NUM_TEMPS);

	/* Save all temporaries on stack */
	for (i = 0; i < NUM_TEMPS; i++)
		jit_stxi(stack_ptr + i * sizeof(uintptr_t), JIT_FP, JIT_R(i));

	/* Jump to the trampoline */
	to_tramp = jit_jmpi();

	/* The trampoline will jump back here */
	to_fn_epilog = jit_label();

	/* Restore temporaries from stack */
	for (i = 0; i < NUM_TEMPS; i++)
		jit_ldxi(JIT_R(i), JIT_FP, stack_ptr + i * sizeof(uintptr_t));

	jit_ret();
	jit_epilog();

	/* Trampoline entry point.
	 * The sole purpose of the trampoline is to cheese Lightning not to
	 * save/restore the callee-saved register LIGHTREC_REG_CYCLE, since we
	 * do want to return to the caller with this register modified. */
	jit_prolog();
	jit_tramp(256);
	jit_patch(to_tramp);

	/* Retrieve the wrapper function */
	jit_ldxi(JIT_R0, LIGHTREC_REG_STATE,
		 offsetof(struct lightrec_state, c_wrappers));

	/* Restore LIGHTREC_REG_STATE to its correct value */
	jit_movi(LIGHTREC_REG_STATE, (uintptr_t) state);

	jit_prepare();
	jit_pushargr(LIGHTREC_REG_STATE);
	jit_pushargr(LIGHTREC_REG_CYCLE);
	jit_pushargr(JIT_R0);
	jit_pushargr(JIT_R1);
	jit_finishi(c_function_wrapper);
	jit_retval_i(LIGHTREC_REG_CYCLE);

	jit_patch_at(jit_jmpi(), to_fn_epilog);
	jit_epilog();

	block->_jit = _jit;
	block->function = jit_emit();
	block->opcode_list = NULL;
	block->flags = 0;
	block->nb_ops = 0;

	state->wrappers_eps[C_WRAPPERS_COUNT - 1] = block->function;

	for (i = 0; i < C_WRAPPERS_COUNT - 1; i++)
		state->wrappers_eps[i] = jit_address(addr[i]);

	jit_get_code(&code_size);
	lightrec_register(MEM_FOR_CODE, code_size);

	block->code_size = code_size;

	if (ENABLE_DISASSEMBLER) {
		pr_debug("Wrapper block:\n");
		jit_disassemble();
	}

	jit_clear_state();
	return block;

err_free_block:
	lightrec_free(state, MEM_FOR_IR, sizeof(*block), block);
err_no_mem:
	pr_err("Unable to compile wrapper: Out of memory\n");
	return NULL;
}

static u32 lightrec_memset(struct lightrec_state *state)
{
	u32 kunseg_pc = kunseg(state->regs.gpr[4]);
	void *host;
	const struct lightrec_mem_map *map = lightrec_get_map(state, &host, kunseg_pc);
	u32 length = state->regs.gpr[5] * 4;

	if (!map) {
		pr_err("Unable to find memory map for memset target address "
		       "0x%x\n", kunseg_pc);
		return 0;
	}

	pr_debug("Calling host memset, PC 0x%x (host address 0x%" PRIxPTR ") for %u bytes\n",
		 kunseg_pc, (uintptr_t)host, length);
	memset(host, 0, length);

	if (!state->invalidate_from_dma_only)
		lightrec_invalidate_map(state, map, kunseg_pc, length);

	/* Rough estimation of the number of cycles consumed */
	return 8 + 5 * (length  + 3 / 4);
}

static struct block * generate_dispatcher(struct lightrec_state *state)
{
	struct block *block;
	jit_state_t *_jit;
	jit_node_t *to_end, *to_c, *loop, *addr, *addr2, *addr3;
	unsigned int i;
	u32 offset, ram_len;
	jit_word_t code_size;

	block = lightrec_malloc(state, MEM_FOR_IR, sizeof(*block));
	if (!block)
		goto err_no_mem;

	_jit = jit_new_state();
	if (!_jit)
		goto err_free_block;

	jit_name("dispatcher");
	jit_note(__FILE__, __LINE__);

	jit_prolog();
	jit_frame(256);

	jit_getarg(JIT_R0, jit_arg());
	jit_getarg_i(LIGHTREC_REG_CYCLE, jit_arg());

	/* Force all callee-saved registers to be pushed on the stack */
	for (i = 0; i < NUM_REGS; i++)
		jit_movr(JIT_V(i), JIT_V(i));

	/* Pass lightrec_state structure to blocks, using the last callee-saved
	 * register that Lightning provides */
	jit_movi(LIGHTREC_REG_STATE, (intptr_t) state);

	loop = jit_label();

	/* Call the block's code */
	jit_jmpr(JIT_R0);

	if (OPT_REPLACE_MEMSET) {
		/* Blocks will jump here when they need to call
		 * lightrec_memset() */
		addr3 = jit_indirect();

		jit_prepare();
		jit_pushargr(LIGHTREC_REG_STATE);
		jit_finishi(lightrec_memset);

		jit_ldxi_ui(JIT_V0, LIGHTREC_REG_STATE,
			    offsetof(struct lightrec_state, regs.gpr[31]));

		jit_retval(JIT_R0);
		jit_subr(LIGHTREC_REG_CYCLE, LIGHTREC_REG_CYCLE, JIT_R0);
	}

	/* The block will jump here, with the number of cycles remaining in
	 * LIGHTREC_REG_CYCLE */
	addr2 = jit_indirect();

	/* Store back the next_pc to the lightrec_state structure */
	offset = offsetof(struct lightrec_state, next_pc);
	jit_stxi_i(offset, LIGHTREC_REG_STATE, JIT_V0);

	/* Jump to end if state->target_cycle < state->current_cycle */
	to_end = jit_blei(LIGHTREC_REG_CYCLE, 0);

	/* Convert next PC to KUNSEG and avoid mirrors */
	ram_len = state->maps[PSX_MAP_KERNEL_USER_RAM].length;
	jit_andi(JIT_R0, JIT_V0, 0x10000000 | (ram_len - 1));
	to_c = jit_bgei(JIT_R0, ram_len);

	/* Fast path: code is running from RAM, use the code LUT */
	if (__WORDSIZE == 64)
		jit_lshi(JIT_R0, JIT_R0, 1);
	jit_addr(JIT_R0, JIT_R0, LIGHTREC_REG_STATE);
	jit_ldxi(JIT_R0, JIT_R0, offsetof(struct lightrec_state, code_lut));

	/* If we get non-NULL, loop */
	jit_patch_at(jit_bnei(JIT_R0, 0), loop);

	/* Slow path: call C function get_next_block_func() */
	jit_patch(to_c);

	if (ENABLE_FIRST_PASS || OPT_DETECT_IMPOSSIBLE_BRANCHES) {
		/* We may call the interpreter - update state->current_cycle */
		jit_ldxi_i(JIT_R2, LIGHTREC_REG_STATE,
			   offsetof(struct lightrec_state, target_cycle));
		jit_subr(JIT_R1, JIT_R2, LIGHTREC_REG_CYCLE);
		jit_stxi_i(offsetof(struct lightrec_state, current_cycle),
			   LIGHTREC_REG_STATE, JIT_R1);
	}

	/* The code LUT will be set to this address when the block at the target
	 * PC has been preprocessed but not yet compiled by the threaded
	 * recompiler */
	addr = jit_indirect();

	/* Get the next block */
	jit_prepare();
	jit_pushargr(LIGHTREC_REG_STATE);
	jit_pushargr(JIT_V0);
	jit_finishi(&get_next_block_func);
	jit_retval(JIT_R0);

	if (ENABLE_FIRST_PASS || OPT_DETECT_IMPOSSIBLE_BRANCHES) {
		/* The interpreter may have updated state->current_cycle and
		 * state->target_cycle - recalc the delta */
		jit_ldxi_i(JIT_R1, LIGHTREC_REG_STATE,
			   offsetof(struct lightrec_state, current_cycle));
		jit_ldxi_i(JIT_R2, LIGHTREC_REG_STATE,
			   offsetof(struct lightrec_state, target_cycle));
		jit_subr(LIGHTREC_REG_CYCLE, JIT_R2, JIT_R1);
	}

	/* If we get non-NULL, loop */
	jit_patch_at(jit_bnei(JIT_R0, 0), loop);

	/* When exiting, the recompiled code will jump to that address */
	jit_note(__FILE__, __LINE__);
	jit_patch(to_end);

	jit_retr(LIGHTREC_REG_CYCLE);
	jit_epilog();

	block->_jit = _jit;
	block->function = jit_emit();
	block->opcode_list = NULL;
	block->flags = 0;
	block->nb_ops = 0;

	jit_get_code(&code_size);
	lightrec_register(MEM_FOR_CODE, code_size);

	block->code_size = code_size;

	state->eob_wrapper_func = jit_address(addr2);
	if (OPT_REPLACE_MEMSET)
		state->memset_func = jit_address(addr3);
	state->get_next_block = jit_address(addr);

	if (ENABLE_DISASSEMBLER) {
		pr_debug("Dispatcher block:\n");
		jit_disassemble();
	}

	/* We're done! */
	jit_clear_state();
	return block;

err_free_block:
	lightrec_free(state, MEM_FOR_IR, sizeof(*block), block);
err_no_mem:
	pr_err("Unable to compile dispatcher: Out of memory\n");
	return NULL;
}

union code lightrec_read_opcode(struct lightrec_state *state, u32 pc)
{
	void *host = NULL;

	lightrec_get_map(state, &host, kunseg(pc));

	const u32 *code = (u32 *)host;
	return (union code) *code;
}

unsigned int lightrec_cycles_of_opcode(union code code)
{
	return 2;
}

void lightrec_free_opcode_list(struct lightrec_state *state, struct block *block)
{
	lightrec_free(state, MEM_FOR_IR,
		      sizeof(*block->opcode_list) * block->nb_ops,
		      block->opcode_list);
}

static unsigned int lightrec_get_mips_block_len(const u32 *src)
{
	unsigned int i;
	union code c;

	for (i = 1; ; i++) {
		c.opcode = LE32TOH(*src++);

		if (is_syscall(c))
			return i;

		if (is_unconditional_jump(c))
			return i + 1;
	}
}

static struct opcode * lightrec_disassemble(struct lightrec_state *state,
					    const u32 *src, unsigned int *len)
{
	struct opcode *list;
	unsigned int i, length;

	length = lightrec_get_mips_block_len(src);

	list = lightrec_malloc(state, MEM_FOR_IR, sizeof(*list) * length);
	if (!list) {
		pr_err("Unable to allocate memory\n");
		return NULL;
	}

	for (i = 0; i < length; i++) {
		list[i].opcode = LE32TOH(src[i]);
		list[i].flags = 0;
	}

	*len = length * sizeof(u32);

	return list;
}

static struct block * lightrec_precompile_block(struct lightrec_state *state,
						u32 pc)
{
	struct opcode *list;
	struct block *block;
	void *host;
	const struct lightrec_mem_map *map = lightrec_get_map(state, &host, kunseg(pc));
	const u32 *code = (u32 *) host;
	unsigned int length;
	bool fully_tagged;

	if (!map)
		return NULL;

	block = lightrec_malloc(state, MEM_FOR_IR, sizeof(*block));
	if (!block) {
		pr_err("Unable to recompile block: Out of memory\n");
		return NULL;
	}

	list = lightrec_disassemble(state, code, &length);
	if (!list) {
		lightrec_free(state, MEM_FOR_IR, sizeof(*block), block);
		return NULL;
	}

	block->pc = pc;
	block->_jit = NULL;
	block->function = NULL;
	block->opcode_list = list;
	block->code = code;
	block->next = NULL;
	block->flags = 0;
	block->code_size = 0;
#if ENABLE_THREADED_COMPILER
	block->op_list_freed = (atomic_flag)ATOMIC_FLAG_INIT;
#endif
	block->nb_ops = length / sizeof(u32);

	lightrec_optimize(state, block);

	length = block->nb_ops * sizeof(u32);

	lightrec_register(MEM_FOR_MIPS_CODE, length);

	if (ENABLE_DISASSEMBLER) {
		pr_debug("Disassembled block at PC: 0x%08x\n", block->pc);
		lightrec_print_disassembly(block, code);
	}

	pr_debug("Block size: %hu opcodes\n", block->nb_ops);

	/* If the first opcode is an 'impossible' branch, never compile the
	 * block */
	if (should_emulate(block->opcode_list))
		block->flags |= BLOCK_NEVER_COMPILE;

	fully_tagged = lightrec_block_is_fully_tagged(block);
	if (fully_tagged)
		block->flags |= BLOCK_FULLY_TAGGED;

	if (OPT_REPLACE_MEMSET && (block->flags & BLOCK_IS_MEMSET))
		state->code_lut[lut_offset(pc)] = state->memset_func;

	block->hash = lightrec_calculate_block_hash(block);

	pr_debug("Recompile count: %u\n", state->nb_precompile++);

	return block;
}

static bool lightrec_block_is_fully_tagged(const struct block *block)
{
	const struct opcode *op;
	unsigned int i;

	for (i = 0; i < block->nb_ops; i++) {
		op = &block->opcode_list[i];

		/* Verify that all load/stores of the opcode list
		 * Check all loads/stores of the opcode list and mark the
		 * block as fully compiled if they all have been tagged. */
		switch (op->c.i.op) {
		case OP_LB:
		case OP_LH:
		case OP_LWL:
		case OP_LW:
		case OP_LBU:
		case OP_LHU:
		case OP_LWR:
		case OP_SB:
		case OP_SH:
		case OP_SWL:
		case OP_SW:
		case OP_SWR:
		case OP_LWC2:
		case OP_SWC2:
			if (!(op->flags & (LIGHTREC_DIRECT_IO |
					   LIGHTREC_HW_IO)))
				return false;
		default: /* fall-through */
			continue;
		}
	}

	return true;
}

static void lightrec_reap_block(struct lightrec_state *state, void *data)
{
	struct block *block = data;

	pr_debug("Reap dead block at PC 0x%08x\n", block->pc);
	lightrec_unregister_block(state->block_cache, block);
	lightrec_free_block(state, block);
}

static void lightrec_reap_jit(struct lightrec_state *state, void *data)
{
	_jit_destroy_state(data);
}

int lightrec_compile_block(struct lightrec_cstate *cstate,
			   struct block *block)
{
	struct lightrec_state *state = cstate->state;
	struct lightrec_branch_target *target;
	bool op_list_freed = false, fully_tagged = false;
	struct block *block2;
	struct opcode *elm;
	jit_state_t *_jit, *oldjit;
	jit_node_t *start_of_block;
	bool skip_next = false;
	jit_word_t code_size;
	unsigned int i, j;
	u32 offset;

	fully_tagged = lightrec_block_is_fully_tagged(block);
	if (fully_tagged)
		block->flags |= BLOCK_FULLY_TAGGED;

	_jit = jit_new_state();
	if (!_jit)
		return -ENOMEM;

	oldjit = block->_jit;
	block->_jit = _jit;

	lightrec_regcache_reset(cstate->reg_cache);
	cstate->cycles = 0;
	cstate->nb_branches = 0;
	cstate->nb_local_branches = 0;
	cstate->nb_targets = 0;

	jit_prolog();
	jit_tramp(256);

	start_of_block = jit_label();

	for (i = 0; i < block->nb_ops; i++) {
		elm = &block->opcode_list[i];

		if (skip_next) {
			skip_next = false;
			continue;
		}

		cstate->cycles += lightrec_cycles_of_opcode(elm->c);

		if (should_emulate(elm)) {
			pr_debug("Branch at offset 0x%x will be emulated\n",
				 i << 2);

			lightrec_emit_eob(cstate, block, i, false);
			skip_next = !(elm->flags & LIGHTREC_NO_DS);
		} else {
			lightrec_rec_opcode(cstate, block, i);
			skip_next = has_delay_slot(elm->c) &&
				!(elm->flags & LIGHTREC_NO_DS);
#if _WIN32
			/* FIXME: GNU Lightning on Windows seems to use our
			 * mapped registers as temporaries. Until the actual bug
			 * is found and fixed, unconditionally mark our
			 * registers as live here. */
			lightrec_regcache_mark_live(cstate->reg_cache, _jit);
#endif
		}
	}

	for (i = 0; i < cstate->nb_branches; i++)
		jit_patch(cstate->branches[i]);

	for (i = 0; i < cstate->nb_local_branches; i++) {
		struct lightrec_branch *branch = &cstate->local_branches[i];

		pr_debug("Patch local branch to offset 0x%x\n",
			 branch->target << 2);

		if (branch->target == 0) {
			jit_patch_at(branch->branch, start_of_block);
			continue;
		}

		for (j = 0; j < cstate->nb_targets; j++) {
			if (cstate->targets[j].offset == branch->target) {
				jit_patch_at(branch->branch,
					     cstate->targets[j].label);
				break;
			}
		}

		if (j == cstate->nb_targets)
			pr_err("Unable to find branch target\n");
	}

	jit_ldxi(JIT_R0, LIGHTREC_REG_STATE,
		 offsetof(struct lightrec_state, eob_wrapper_func));

	jit_jmpr(JIT_R0);

	jit_ret();
	jit_epilog();

	block->function = jit_emit();
	block->flags &= ~BLOCK_SHOULD_RECOMPILE;

	/* Add compiled function to the LUT */
	state->code_lut[lut_offset(block->pc)] = block->function;

	if (ENABLE_THREADED_COMPILER) {
		/* Since we might try to reap the same block multiple times,
		 * we need the reaper to wait until everything has been
		 * submitted, so that the duplicate entries can be dropped. */
		lightrec_reaper_pause(state->reaper);
	}

	/* Detect old blocks that have been covered by the new one */
	for (i = 0; i < cstate->nb_targets; i++) {
		target = &cstate->targets[i];

		if (!target->offset)
			continue;

		offset = block->pc + target->offset * sizeof(u32);
		block2 = lightrec_find_block(state->block_cache, offset);
		if (block2) {
			/* No need to check if block2 is compilable - it must
			 * be, otherwise block wouldn't be compilable either */

			/* Set the "block dead" flag to prevent the dynarec from
			 * recompiling this block */
			block2->flags |= BLOCK_IS_DEAD;

			/* If block2 was pending for compilation, cancel it.
			 * If it's being compiled right now, wait until it
			 * finishes. */
			if (ENABLE_THREADED_COMPILER)
				lightrec_recompiler_remove(state->rec, block2);
		}

		/* We know from now on that block2 (if present) isn't going to
		 * be compiled. We can override the LUT entry with our new
		 * block's entry point. */
		offset = lut_offset(block->pc) + target->offset;
		state->code_lut[offset] = jit_address(target->label);

		if (block2) {
			pr_debug("Reap block 0x%08x as it's covered by block "
				 "0x%08x\n", block2->pc, block->pc);

			/* Finally, reap the block. */
			if (ENABLE_THREADED_COMPILER) {
				lightrec_reaper_add(state->reaper,
						    lightrec_reap_block,
						    block2);
			} else {
				lightrec_unregister_block(state->block_cache, block2);
				lightrec_free_block(state, block2);
			}
		}
	}

	if (ENABLE_THREADED_COMPILER)
		lightrec_reaper_continue(state->reaper);

	jit_get_code(&code_size);
	lightrec_register(MEM_FOR_CODE, code_size);

	block->code_size = code_size;

	if (ENABLE_DISASSEMBLER) {
		pr_debug("Compiling block at PC: 0x%08x\n", block->pc);
		jit_disassemble();
	}

	jit_clear_state();

#if ENABLE_THREADED_COMPILER
	if (fully_tagged)
		op_list_freed = atomic_flag_test_and_set(&block->op_list_freed);
#endif
	if (fully_tagged && !op_list_freed) {
		pr_debug("Block PC 0x%08x is fully tagged"
			 " - free opcode list\n", block->pc);
		lightrec_free_opcode_list(state, block);
		block->opcode_list = NULL;
	}

	if (oldjit) {
		pr_debug("Block 0x%08x recompiled, reaping old jit context.\n",
			 block->pc);

		if (ENABLE_THREADED_COMPILER)
			lightrec_reaper_add(state->reaper,
					    lightrec_reap_jit, oldjit);
		else
			_jit_destroy_state(oldjit);
	}

	return 0;
}

static void lightrec_print_info(struct lightrec_state *state)
{
	if ((state->current_cycle & ~0xfffffff) != state->old_cycle_counter) {
		pr_info("Lightrec RAM usage: IR %u KiB, CODE %u KiB, "
			"MIPS %u KiB, TOTAL %u KiB, avg. IPI %f\n",
			lightrec_get_mem_usage(MEM_FOR_IR) / 1024,
			lightrec_get_mem_usage(MEM_FOR_CODE) / 1024,
			lightrec_get_mem_usage(MEM_FOR_MIPS_CODE) / 1024,
			lightrec_get_total_mem_usage() / 1024,
		       lightrec_get_average_ipi());
		state->old_cycle_counter = state->current_cycle & ~0xfffffff;
	}
}

u32 lightrec_execute(struct lightrec_state *state, u32 pc, u32 target_cycle)
{
	s32 (*func)(void *, s32) = (void *)state->dispatcher->function;
	void *block_trace;
	s32 cycles_delta;

	state->exit_flags = LIGHTREC_EXIT_NORMAL;

	/* Handle the cycle counter overflowing */
	if (unlikely(target_cycle < state->current_cycle))
		target_cycle = UINT_MAX;

	state->target_cycle = target_cycle;
	state->next_pc = pc;

	block_trace = get_next_block_func(state, pc);
	if (block_trace) {
		cycles_delta = state->target_cycle - state->current_cycle;

		cycles_delta = (*func)(block_trace, cycles_delta);

		state->current_cycle = state->target_cycle - cycles_delta;
	}

	if (ENABLE_THREADED_COMPILER)
		lightrec_reaper_reap(state->reaper);

	if (LOG_LEVEL >= INFO_L)
		lightrec_print_info(state);

	return state->next_pc;
}

u32 lightrec_execute_one(struct lightrec_state *state, u32 pc)
{
	return lightrec_execute(state, pc, state->current_cycle);
}

u32 lightrec_run_interpreter(struct lightrec_state *state, u32 pc)
{
	struct block *block = lightrec_get_block(state, pc);
	if (!block)
		return 0;

	state->exit_flags = LIGHTREC_EXIT_NORMAL;

	pc = lightrec_emulate_block(state, block, pc);

	if (LOG_LEVEL >= INFO_L)
		lightrec_print_info(state);

	return pc;
}

void lightrec_free_block(struct lightrec_state *state, struct block *block)
{
	lightrec_unregister(MEM_FOR_MIPS_CODE, block->nb_ops * sizeof(u32));
	if (block->opcode_list)
		lightrec_free_opcode_list(state, block);
	if (block->_jit)
		_jit_destroy_state(block->_jit);
	lightrec_unregister(MEM_FOR_CODE, block->code_size);
	lightrec_free(state, MEM_FOR_IR, sizeof(*block), block);
}

struct lightrec_cstate * lightrec_create_cstate(struct lightrec_state *state)
{
	struct lightrec_cstate *cstate;

	cstate = lightrec_malloc(state, MEM_FOR_LIGHTREC, sizeof(*cstate));
	if (!cstate)
		return NULL;

	cstate->reg_cache = lightrec_regcache_init(state);
	if (!cstate->reg_cache) {
		lightrec_free(state, MEM_FOR_LIGHTREC, sizeof(*cstate), cstate);
		return NULL;
	}

	cstate->state = state;

	return cstate;
}

void lightrec_free_cstate(struct lightrec_cstate *cstate)
{
	lightrec_free_regcache(cstate->reg_cache);
	lightrec_free(cstate->state, MEM_FOR_LIGHTREC, sizeof(*cstate), cstate);
}

struct lightrec_state * lightrec_init(char *argv0,
				      const struct lightrec_mem_map *map,
				      size_t nb,
				      const struct lightrec_ops *ops)
{
	struct lightrec_state *state;

	/* Sanity-check ops */
	if (!ops || !ops->cop2_op || !ops->enable_ram) {
		pr_err("Missing callbacks in lightrec_ops structure\n");
		return NULL;
	}

	init_jit(argv0);

	state = calloc(1, sizeof(*state) +
		       sizeof(*state->code_lut) * CODE_LUT_SIZE);
	if (!state)
		goto err_finish_jit;

	lightrec_register(MEM_FOR_LIGHTREC, sizeof(*state) +
			  sizeof(*state->code_lut) * CODE_LUT_SIZE);

#if ENABLE_TINYMM
	state->tinymm = tinymm_init(malloc, free, 4096);
	if (!state->tinymm)
		goto err_free_state;
#endif

	state->block_cache = lightrec_blockcache_init(state);
	if (!state->block_cache)
		goto err_free_tinymm;

	if (ENABLE_THREADED_COMPILER) {
		state->rec = lightrec_recompiler_init(state);
		if (!state->rec)
			goto err_free_block_cache;

		state->reaper = lightrec_reaper_init(state);
		if (!state->reaper)
			goto err_free_recompiler;
	} else {
		state->cstate = lightrec_create_cstate(state);
		if (!state->cstate)
			goto err_free_block_cache;
	}

	state->nb_maps = nb;
	state->maps = map;

	memcpy(&state->ops, ops, sizeof(*ops));

	state->dispatcher = generate_dispatcher(state);
	if (!state->dispatcher)
		goto err_free_reaper;

	state->c_wrapper_block = generate_wrapper(state);
	if (!state->c_wrapper_block)
		goto err_free_dispatcher;

	state->c_wrappers[C_WRAPPER_RW] = lightrec_rw_cb;
	state->c_wrappers[C_WRAPPER_RW_GENERIC] = lightrec_rw_generic_cb;
	state->c_wrappers[C_WRAPPER_MTC] = lightrec_mtc_cb;
	state->c_wrappers[C_WRAPPER_CP] = lightrec_cp;
	state->c_wrappers[C_WRAPPER_SYSCALL] = lightrec_syscall_cb;
	state->c_wrappers[C_WRAPPER_BREAK] = lightrec_break_cb;

	map = &state->maps[PSX_MAP_BIOS];
	state->offset_bios = (uintptr_t)map->address - map->pc;

	map = &state->maps[PSX_MAP_SCRATCH_PAD];
	state->offset_scratch = (uintptr_t)map->address - map->pc;

	map = &state->maps[PSX_MAP_KERNEL_USER_RAM];
	state->offset_ram = (uintptr_t)map->address - map->pc;

	if (state->maps[PSX_MAP_MIRROR1].address == map->address + 0x200000 &&
	    state->maps[PSX_MAP_MIRROR2].address == map->address + 0x400000 &&
	    state->maps[PSX_MAP_MIRROR3].address == map->address + 0x600000)
		state->mirrors_mapped = true;

	if (state->offset_bios == 0 &&
	    state->offset_scratch == 0 &&
	    state->offset_ram == 0 &&
	    state->mirrors_mapped) {
		pr_info("Memory map is perfect. Emitted code will be best.\n");
	} else {
		pr_info("Memory map is sub-par. Emitted code will be slow.\n");
	}

	return state;

err_free_dispatcher:
	lightrec_free_block(state, state->dispatcher);
err_free_reaper:
	if (ENABLE_THREADED_COMPILER)
		lightrec_reaper_destroy(state->reaper);
err_free_recompiler:
	if (ENABLE_THREADED_COMPILER)
		lightrec_free_recompiler(state->rec);
	else
		lightrec_free_cstate(state->cstate);
err_free_block_cache:
	lightrec_free_block_cache(state->block_cache);
err_free_tinymm:
#if ENABLE_TINYMM
	tinymm_shutdown(state->tinymm);
err_free_state:
#endif
	lightrec_unregister(MEM_FOR_LIGHTREC, sizeof(*state) +
			    sizeof(*state->code_lut) * CODE_LUT_SIZE);
	free(state);
err_finish_jit:
	finish_jit();
	return NULL;
}

void lightrec_destroy(struct lightrec_state *state)
{
	/* Force a print info on destroy*/
	state->current_cycle = ~state->current_cycle;
	lightrec_print_info(state);

	if (ENABLE_THREADED_COMPILER) {
		lightrec_free_recompiler(state->rec);
		lightrec_reaper_destroy(state->reaper);
	} else {
		lightrec_free_cstate(state->cstate);
	}

	lightrec_free_block_cache(state->block_cache);
	lightrec_free_block(state, state->dispatcher);
	lightrec_free_block(state, state->c_wrapper_block);
	finish_jit();

#if ENABLE_TINYMM
	tinymm_shutdown(state->tinymm);
#endif
	lightrec_unregister(MEM_FOR_LIGHTREC, sizeof(*state) +
			    sizeof(*state->code_lut) * CODE_LUT_SIZE);
	free(state);
}

void lightrec_invalidate(struct lightrec_state *state, u32 addr, u32 len)
{
	u32 kaddr = kunseg(addr & ~0x3);
	const struct lightrec_mem_map *map = lightrec_get_map(state, NULL, kaddr);

	if (map) {
		if (map != &state->maps[PSX_MAP_KERNEL_USER_RAM])
			return;

		/* Handle mirrors */
		kaddr &= (state->maps[PSX_MAP_KERNEL_USER_RAM].length - 1);

		lightrec_invalidate_map(state, map, kaddr, len);
	}
}

void lightrec_invalidate_all(struct lightrec_state *state)
{
	memset(state->code_lut, 0, sizeof(*state->code_lut) * CODE_LUT_SIZE);
}

void lightrec_set_invalidate_mode(struct lightrec_state *state, bool dma_only)
{
	if (state->invalidate_from_dma_only != dma_only)
		lightrec_invalidate_all(state);

	state->invalidate_from_dma_only = dma_only;
}

void lightrec_set_exit_flags(struct lightrec_state *state, u32 flags)
{
	if (flags != LIGHTREC_EXIT_NORMAL) {
		state->exit_flags |= flags;
		state->target_cycle = state->current_cycle;
	}
}

u32 lightrec_exit_flags(struct lightrec_state *state)
{
	return state->exit_flags;
}

u32 lightrec_current_cycle_count(const struct lightrec_state *state)
{
	return state->current_cycle;
}

void lightrec_reset_cycle_count(struct lightrec_state *state, u32 cycles)
{
	state->current_cycle = cycles;

	if (state->target_cycle < cycles)
		state->target_cycle = cycles;
}

void lightrec_set_target_cycle_count(struct lightrec_state *state, u32 cycles)
{
	if (state->exit_flags == LIGHTREC_EXIT_NORMAL) {
		if (cycles < state->current_cycle)
			cycles = state->current_cycle;

		state->target_cycle = cycles;
	}
}

struct lightrec_registers * lightrec_get_registers(struct lightrec_state *state)
{
	return &state->regs;
}

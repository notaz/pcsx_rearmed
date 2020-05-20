/*
 * Copyright (C) 2016-2020 Paul Cercueil <paul@crapouillou.net>
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

#ifndef __LIGHTREC_PRIVATE_H__
#define __LIGHTREC_PRIVATE_H__

#include "config.h"
#include "disassembler.h"
#include "lightrec.h"

#if ENABLE_THREADED_COMPILER
#include <stdatomic.h>
#endif

#define ARRAY_SIZE(x) (sizeof(x) ? sizeof(x) / sizeof((x)[0]) : 0)
#define BIT(x) (1 << (x))

#ifdef __GNUC__
#	define likely(x)       __builtin_expect(!!(x),1)
#	define unlikely(x)     __builtin_expect(!!(x),0)
#else
#	define likely(x)       (x)
#	define unlikely(x)     (x)
#endif

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#	define LE32TOH(x)	__builtin_bswap32(x)
#	define HTOLE32(x)	__builtin_bswap32(x)
#	define LE16TOH(x)	__builtin_bswap16(x)
#	define HTOLE16(x)	__builtin_bswap16(x)
#else
#	define LE32TOH(x)	(x)
#	define HTOLE32(x)	(x)
#	define LE16TOH(x)	(x)
#	define HTOLE16(x)	(x)
#endif

/* Flags for (struct block *)->flags */
#define BLOCK_NEVER_COMPILE	BIT(0)
#define BLOCK_SHOULD_RECOMPILE	BIT(1)
#define BLOCK_FULLY_TAGGED	BIT(2)
#define BLOCK_IS_DEAD		BIT(3)

#define RAM_SIZE	0x200000
#define BIOS_SIZE	0x80000

#define CODE_LUT_SIZE	((RAM_SIZE + BIOS_SIZE) >> 2)

/* Definition of jit_state_t (avoids inclusion of <lightning.h>) */
struct jit_node;
struct jit_state;
typedef struct jit_state jit_state_t;

struct blockcache;
struct recompiler;
struct regcache;
struct opcode;
struct tinymm;
struct reaper;

struct block {
	jit_state_t *_jit;
	struct lightrec_state *state;
	struct opcode *opcode_list;
	void (*function)(void);
	u32 pc;
	u32 hash;
#if ENABLE_THREADED_COMPILER
	atomic_flag op_list_freed;
#endif
	unsigned int code_size;
	u16 flags;
	u16 nb_ops;
	const struct lightrec_mem_map *map;
	struct block *next;
};

struct lightrec_branch {
	struct jit_node *branch;
	u32 target;
};

struct lightrec_branch_target {
	struct jit_node *label;
	u32 offset;
};

struct lightrec_state {
	u32 native_reg_cache[34];
	u32 next_pc;
	u32 current_cycle;
	u32 target_cycle;
	u32 exit_flags;
	struct block *dispatcher, *rw_wrapper, *rw_generic_wrapper,
		     *mfc_wrapper, *mtc_wrapper, *rfe_wrapper, *cp_wrapper,
		     *syscall_wrapper, *break_wrapper;
	void *rw_func, *rw_generic_func, *mfc_func, *mtc_func, *rfe_func,
	     *cp_func, *syscall_func, *break_func;
	struct jit_node *branches[512];
	struct lightrec_branch local_branches[512];
	struct lightrec_branch_target targets[512];
	unsigned int nb_branches;
	unsigned int nb_local_branches;
	unsigned int nb_targets;
	struct tinymm *tinymm;
	struct blockcache *block_cache;
	struct regcache *reg_cache;
	struct recompiler *rec;
	struct reaper *reaper;
	void (*eob_wrapper_func)(void);
	void (*get_next_block)(void);
	struct lightrec_ops ops;
	unsigned int nb_precompile;
	unsigned int cycles;
	unsigned int nb_maps;
	const struct lightrec_mem_map *maps;
	uintptr_t offset_ram, offset_bios, offset_scratch;
	_Bool mirrors_mapped;
	_Bool invalidate_from_dma_only;
	void *code_lut[];
};

u32 lightrec_rw(struct lightrec_state *state, union code op,
		u32 addr, u32 data, u16 *flags);

void lightrec_free_block(struct block *block);

void remove_from_code_lut(struct blockcache *cache, struct block *block);

static inline u32 kunseg(u32 addr)
{
	if (unlikely(addr >= 0xa0000000))
		return addr - 0xa0000000;
	else
		return addr &~ 0x80000000;
}

static inline u32 lut_offset(u32 pc)
{
	if (pc & BIT(28))
		return ((pc & (BIOS_SIZE - 1)) + RAM_SIZE) >> 2; // BIOS
	else
		return (pc & (RAM_SIZE - 1)) >> 2; // RAM
}

void lightrec_mtc(struct lightrec_state *state, union code op, u32 data);
u32 lightrec_mfc(struct lightrec_state *state, union code op);

union code lightrec_read_opcode(struct lightrec_state *state, u32 pc);

struct block * lightrec_get_block(struct lightrec_state *state, u32 pc);
int lightrec_compile_block(struct block *block);

#endif /* __LIGHTREC_PRIVATE_H__ */

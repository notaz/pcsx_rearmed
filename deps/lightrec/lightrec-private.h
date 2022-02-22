/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2016-2021 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __LIGHTREC_PRIVATE_H__
#define __LIGHTREC_PRIVATE_H__

#include "lightrec-config.h"
#include "disassembler.h"
#include "lightrec.h"

#if ENABLE_THREADED_COMPILER
#include <stdatomic.h>
#endif

#define ARRAY_SIZE(x) (sizeof(x) ? sizeof(x) / sizeof((x)[0]) : 0)

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

#if HAS_DEFAULT_ELM
#define SET_DEFAULT_ELM(table, value) [0 ... ARRAY_SIZE(table) - 1] = value
#else
#define SET_DEFAULT_ELM(table, value) [0] = NULL
#endif

/* Flags for (struct block *)->flags */
#define BLOCK_NEVER_COMPILE	BIT(0)
#define BLOCK_SHOULD_RECOMPILE	BIT(1)
#define BLOCK_FULLY_TAGGED	BIT(2)
#define BLOCK_IS_DEAD		BIT(3)
#define BLOCK_IS_MEMSET		BIT(4)

#define RAM_SIZE	0x200000
#define BIOS_SIZE	0x80000

#define CODE_LUT_SIZE	((RAM_SIZE + BIOS_SIZE) >> 2)

#define REG_LO 32
#define REG_HI 33

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
	struct opcode *opcode_list;
	void (*function)(void);
	const u32 *code;
	struct block *next;
	u32 pc;
	u32 hash;
	unsigned int code_size;
	u16 nb_ops;
	u8 flags;
#if ENABLE_THREADED_COMPILER
	atomic_flag op_list_freed;
#endif
};

struct lightrec_branch {
	struct jit_node *branch;
	u32 target;
};

struct lightrec_branch_target {
	struct jit_node *label;
	u32 offset;
};

enum c_wrappers {
	C_WRAPPER_RW,
	C_WRAPPER_RW_GENERIC,
	C_WRAPPER_MFC,
	C_WRAPPER_MTC,
	C_WRAPPER_CP,
	C_WRAPPER_SYSCALL,
	C_WRAPPER_BREAK,
	C_WRAPPERS_COUNT,
};

struct lightrec_cstate {
	struct lightrec_state *state;

	struct jit_node *branches[512];
	struct lightrec_branch local_branches[512];
	struct lightrec_branch_target targets[512];
	unsigned int nb_branches;
	unsigned int nb_local_branches;
	unsigned int nb_targets;
	unsigned int cycles;

	struct regcache *reg_cache;
};

struct lightrec_state {
	struct lightrec_registers regs;
	u32 next_pc;
	u32 current_cycle;
	u32 target_cycle;
	u32 exit_flags;
	u32 old_cycle_counter;
	struct block *dispatcher, *c_wrapper_block;
	void *c_wrapper, *c_wrappers[C_WRAPPERS_COUNT];
	struct tinymm *tinymm;
	struct blockcache *block_cache;
	struct recompiler *rec;
	struct lightrec_cstate *cstate;
	struct reaper *reaper;
	void (*eob_wrapper_func)(void);
	void (*memset_func)(void);
	void (*get_next_block)(void);
	struct lightrec_ops ops;
	unsigned int nb_precompile;
	unsigned int nb_maps;
	const struct lightrec_mem_map *maps;
	uintptr_t offset_ram, offset_bios, offset_scratch;
	_Bool mirrors_mapped;
	_Bool invalidate_from_dma_only;
	void *code_lut[];
};

u32 lightrec_rw(struct lightrec_state *state, union code op,
		u32 addr, u32 data, u16 *flags,
		struct block *block);

void lightrec_free_block(struct lightrec_state *state, struct block *block);

void remove_from_code_lut(struct blockcache *cache, struct block *block);

const struct lightrec_mem_map *
lightrec_get_map(struct lightrec_state *state, void **host, u32 kaddr);

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

static inline u32 get_ds_pc(const struct block *block, u16 offset, s16 imm)
{
	u16 flags = block->opcode_list[offset].flags;

	offset += !!(OPT_SWITCH_DELAY_SLOTS && (flags & LIGHTREC_NO_DS));

	return block->pc + (offset + imm << 2);
}

static inline u32 get_branch_pc(const struct block *block, u16 offset, s16 imm)
{
	u16 flags = block->opcode_list[offset].flags;

	offset -= !!(OPT_SWITCH_DELAY_SLOTS && (flags & LIGHTREC_NO_DS));

	return block->pc + (offset + imm << 2);
}

void lightrec_mtc(struct lightrec_state *state, union code op, u32 data);
u32 lightrec_mfc(struct lightrec_state *state, union code op);
void lightrec_rfe(struct lightrec_state *state);
void lightrec_cp(struct lightrec_state *state, union code op);

struct lightrec_cstate * lightrec_create_cstate(struct lightrec_state *state);
void lightrec_free_cstate(struct lightrec_cstate *cstate);

union code lightrec_read_opcode(struct lightrec_state *state, u32 pc);

struct block * lightrec_get_block(struct lightrec_state *state, u32 pc);
int lightrec_compile_block(struct lightrec_cstate *cstate, struct block *block);
void lightrec_free_opcode_list(struct lightrec_state *state, struct block *block);

unsigned int lightrec_cycles_of_opcode(union code code);

static inline u8 get_mult_div_lo(union code c)
{
	return (OPT_FLAG_MULT_DIV && c.r.rd) ? c.r.rd : REG_LO;
}

static inline u8 get_mult_div_hi(union code c)
{
	return (OPT_FLAG_MULT_DIV && c.r.imm) ? c.r.imm : REG_HI;
}

#endif /* __LIGHTREC_PRIVATE_H__ */

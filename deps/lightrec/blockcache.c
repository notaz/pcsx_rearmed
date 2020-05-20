/*
 * Copyright (C) 2015-2020 Paul Cercueil <paul@crapouillou.net>
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

#include "blockcache.h"
#include "debug.h"
#include "lightrec-private.h"
#include "memmanager.h"

#include <stdbool.h>
#include <stdlib.h>

/* Must be power of two */
#define LUT_SIZE 0x4000

struct blockcache {
	struct lightrec_state *state;
	struct block * lut[LUT_SIZE];
};

struct block * lightrec_find_block(struct blockcache *cache, u32 pc)
{
	struct block *block;

	pc = kunseg(pc);

	for (block = cache->lut[(pc >> 2) & (LUT_SIZE - 1)];
	     block; block = block->next)
		if (kunseg(block->pc) == pc)
			return block;

	return NULL;
}

void remove_from_code_lut(struct blockcache *cache, struct block *block)
{
	struct lightrec_state *state = block->state;
	const struct opcode *op;
	u32 offset = lut_offset(block->pc);

	/* Use state->get_next_block in the code LUT, which basically
	 * calls back get_next_block_func(), until the compiler
	 * overrides this. This is required, as a NULL value in the code
	 * LUT means an outdated block. */
	state->code_lut[offset] = state->get_next_block;

	for (op = block->opcode_list; op; op = op->next)
		if (op->c.i.op == OP_META_SYNC)
			state->code_lut[offset + op->offset] = NULL;

}

void lightrec_register_block(struct blockcache *cache, struct block *block)
{
	u32 pc = kunseg(block->pc);
	struct block *old;

	old = cache->lut[(pc >> 2) & (LUT_SIZE - 1)];
	if (old)
		block->next = old;

	cache->lut[(pc >> 2) & (LUT_SIZE - 1)] = block;

	remove_from_code_lut(cache, block);
}

void lightrec_unregister_block(struct blockcache *cache, struct block *block)
{
	u32 pc = kunseg(block->pc);
	struct block *old = cache->lut[(pc >> 2) & (LUT_SIZE - 1)];

	if (old == block) {
		cache->lut[(pc >> 2) & (LUT_SIZE - 1)] = old->next;
		return;
	}

	for (; old; old = old->next) {
		if (old->next == block) {
			old->next = block->next;
			return;
		}
	}

	pr_err("Block at PC 0x%x is not in cache\n", block->pc);
}

void lightrec_free_block_cache(struct blockcache *cache)
{
	struct block *block, *next;
	unsigned int i;

	for (i = 0; i < LUT_SIZE; i++) {
		for (block = cache->lut[i]; block; block = next) {
			next = block->next;
			lightrec_free_block(block);
		}
	}

	lightrec_free(cache->state, MEM_FOR_LIGHTREC, sizeof(*cache), cache);
}

struct blockcache * lightrec_blockcache_init(struct lightrec_state *state)
{
	struct blockcache *cache;

	cache = lightrec_calloc(state, MEM_FOR_LIGHTREC, sizeof(*cache));
	if (!cache)
		return NULL;

	cache->state = state;

	return cache;
}

u32 lightrec_calculate_block_hash(const struct block *block)
{
	const struct lightrec_mem_map *map = block->map;
	u32 pc, hash = 0xffffffff;
	const u32 *code;
	unsigned int i;

	pc = kunseg(block->pc) - map->pc;

	while (map->mirror_of)
		map = map->mirror_of;

	code = map->address + pc;

	/* Jenkins one-at-a-time hash algorithm */
	for (i = 0; i < block->nb_ops; i++) {
		hash += *code++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

bool lightrec_block_is_outdated(struct block *block)
{
	void **lut_entry = &block->state->code_lut[lut_offset(block->pc)];
	bool outdated;

	if (*lut_entry)
		return false;

	outdated = block->hash != lightrec_calculate_block_hash(block);
	if (likely(!outdated)) {
		/* The block was marked as outdated, but the content is still
		 * the same */
		if (block->function)
			*lut_entry = block->function;
		else
			*lut_entry = block->state->get_next_block;
	}

	return outdated;
}

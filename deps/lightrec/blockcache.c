// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2015-2021 Paul Cercueil <paul@crapouillou.net>
 */

#include "blockcache.h"
#include "debug.h"
#include "lightrec-private.h"
#include "memmanager.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Must be power of two */
#define LUT_SIZE 0x4000

struct blockcache {
	struct lightrec_state *state;
	struct block * lut[LUT_SIZE];
};

u16 lightrec_get_lut_entry(const struct block *block)
{
	return (kunseg(block->pc) >> 2) & (LUT_SIZE - 1);
}

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

struct block * lightrec_find_block_from_lut(struct blockcache *cache,
					    u16 lut_entry, u32 addr_in_block)
{
	struct block *block;
	u32 pc;

	addr_in_block = kunseg(addr_in_block);

	for (block = cache->lut[lut_entry]; block; block = block->next) {
		pc = kunseg(block->pc);
		if (addr_in_block >= pc &&
		    addr_in_block < pc + (block->nb_ops << 2))
			return block;
	}

	return NULL;
}

void remove_from_code_lut(struct blockcache *cache, struct block *block)
{
	struct lightrec_state *state = cache->state;
	u32 offset = lut_offset(block->pc);

	if (block->function) {
		memset(&state->code_lut[offset], 0,
		       block->nb_ops * sizeof(*state->code_lut));
	}
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
			lightrec_free_block(cache->state, block);
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
	const u32 *code = block->code;
	u32 hash = 0xffffffff;
	unsigned int i;

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

bool lightrec_block_is_outdated(struct lightrec_state *state, struct block *block)
{
	void **lut_entry = &state->code_lut[lut_offset(block->pc)];
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
			*lut_entry = state->get_next_block;
	}

	return outdated;
}

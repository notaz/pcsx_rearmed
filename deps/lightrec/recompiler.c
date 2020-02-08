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

#include "debug.h"
#include "interpreter.h"
#include "lightrec-private.h"
#include "memmanager.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

struct block_rec {
	struct block *block;
	struct block_rec *next;
};

struct recompiler {
	struct lightrec_state *state;
	pthread_t thd;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	bool stop;
	struct block *current_block;
	struct block_rec *list;
};

static void slist_remove(struct recompiler *rec, struct block_rec *elm)
{
	struct block_rec *prev;

	if (rec->list == elm) {
		rec->list = elm->next;
	} else {
		for (prev = rec->list; prev && prev->next != elm; )
			prev = prev->next;
		if (prev)
			prev->next = elm->next;
	}
}

static void lightrec_compile_list(struct recompiler *rec)
{
	struct block_rec *next;
	struct block *block;
	int ret;

	while (!!(next = rec->list)) {
		block = next->block;
		rec->current_block = block;

		pthread_mutex_unlock(&rec->mutex);

		ret = lightrec_compile_block(block);
		if (ret) {
			pr_err("Unable to compile block at PC 0x%x: %d\n",
			       block->pc, ret);
		}

		pthread_mutex_lock(&rec->mutex);

		slist_remove(rec, next);
		lightrec_free(rec->state, MEM_FOR_LIGHTREC,
			      sizeof(*next), next);
		pthread_cond_signal(&rec->cond);
	}

	rec->current_block = NULL;
}

static void * lightrec_recompiler_thd(void *d)
{
	struct recompiler *rec = d;

	pthread_mutex_lock(&rec->mutex);

	for (;;) {
		do {
			pthread_cond_wait(&rec->cond, &rec->mutex);

			if (rec->stop) {
				pthread_mutex_unlock(&rec->mutex);
				return NULL;
			}

		} while (!rec->list);

		lightrec_compile_list(rec);
	}
}

struct recompiler *lightrec_recompiler_init(struct lightrec_state *state)
{
	struct recompiler *rec;
	int ret;

	rec = lightrec_malloc(state, MEM_FOR_LIGHTREC, sizeof(*rec));
	if (!rec) {
		pr_err("Cannot create recompiler: Out of memory\n");
		return NULL;
	}

	rec->state = state;
	rec->stop = false;
	rec->current_block = NULL;
	rec->list = NULL;

	ret = pthread_cond_init(&rec->cond, NULL);
	if (ret) {
		pr_err("Cannot init cond variable: %d\n", ret);
		goto err_free_rec;
	}

	ret = pthread_mutex_init(&rec->mutex, NULL);
	if (ret) {
		pr_err("Cannot init mutex variable: %d\n", ret);
		goto err_cnd_destroy;
	}

	ret = pthread_create(&rec->thd, NULL, lightrec_recompiler_thd, rec);
	if (ret) {
		pr_err("Cannot create recompiler thread: %d\n", ret);
		goto err_mtx_destroy;
	}

	return rec;

err_mtx_destroy:
	pthread_mutex_destroy(&rec->mutex);
err_cnd_destroy:
	pthread_cond_destroy(&rec->cond);
err_free_rec:
	lightrec_free(state, MEM_FOR_LIGHTREC, sizeof(*rec), rec);
	return NULL;
}

void lightrec_free_recompiler(struct recompiler *rec)
{
	rec->stop = true;

	/* Stop the thread */
	pthread_mutex_lock(&rec->mutex);
	pthread_cond_signal(&rec->cond);
	pthread_mutex_unlock(&rec->mutex);
	pthread_join(rec->thd, NULL);

	pthread_mutex_destroy(&rec->mutex);
	pthread_cond_destroy(&rec->cond);
	lightrec_free(rec->state, MEM_FOR_LIGHTREC, sizeof(*rec), rec);
}

int lightrec_recompiler_add(struct recompiler *rec, struct block *block)
{
	struct block_rec *block_rec, *prev;

	pthread_mutex_lock(&rec->mutex);

	for (block_rec = rec->list, prev = NULL; block_rec;
	     prev = block_rec, block_rec = block_rec->next) {
		if (block_rec->block == block) {
			/* The block to compile is already in the queue - bump
			 * it to the top of the list */
			if (prev) {
				prev->next = block_rec->next;
				block_rec->next = rec->list;
				rec->list = block_rec;
			}

			pthread_mutex_unlock(&rec->mutex);
			return 0;
		}
	}

	/* By the time this function was called, the block has been recompiled
	 * and ins't in the wait list anymore. Just return here. */
	if (block->function) {
		pthread_mutex_unlock(&rec->mutex);
		return 0;
	}

	block_rec = lightrec_malloc(rec->state, MEM_FOR_LIGHTREC,
				    sizeof(*block_rec));
	if (!block_rec) {
		pthread_mutex_unlock(&rec->mutex);
		return -ENOMEM;
	}

	pr_debug("Adding block PC 0x%x to recompiler\n", block->pc);

	block_rec->block = block;
	block_rec->next = rec->list;
	rec->list = block_rec;

	/* Signal the thread */
	pthread_cond_signal(&rec->cond);
	pthread_mutex_unlock(&rec->mutex);

	return 0;
}

void lightrec_recompiler_remove(struct recompiler *rec, struct block *block)
{
	struct block_rec *block_rec;

	pthread_mutex_lock(&rec->mutex);

	for (block_rec = rec->list; block_rec; block_rec = block_rec->next) {
		if (block_rec->block == block) {
			if (block == rec->current_block) {
				/* Block is being recompiled - wait for
				 * completion */
				do {
					pthread_cond_wait(&rec->cond,
							  &rec->mutex);
				} while (block == rec->current_block);
			} else {
				/* Block is not yet being processed - remove it
				 * from the list */
				slist_remove(rec, block_rec);
				lightrec_free(rec->state, MEM_FOR_LIGHTREC,
					      sizeof(*block_rec), block_rec);
			}

			break;
		}
	}

	pthread_mutex_unlock(&rec->mutex);
}

void * lightrec_recompiler_run_first_pass(struct block *block, u32 *pc)
{
	bool freed;

	if (likely(block->function)) {
		if (block->flags & BLOCK_FULLY_TAGGED) {
			freed = atomic_flag_test_and_set(&block->op_list_freed);

			if (!freed) {
				pr_debug("Block PC 0x%08x is fully tagged"
					 " - free opcode list\n", block->pc);

				/* The block was already compiled but the opcode list
				 * didn't get freed yet - do it now */
				lightrec_free_opcode_list(block->state,
							  block->opcode_list);
				block->opcode_list = NULL;
			}
		}

		return block->function;
	}

	/* Mark the opcode list as freed, so that the threaded compiler won't
	 * free it while we're using it in the interpreter. */
	freed = atomic_flag_test_and_set(&block->op_list_freed);

	/* Block wasn't compiled yet - run the interpreter */
	*pc = lightrec_emulate_block(block, *pc);

	if (!freed)
		atomic_flag_clear(&block->op_list_freed);

	/* The block got compiled while the interpreter was running.
	 * We can free the opcode list now. */
	if (block->function && (block->flags & BLOCK_FULLY_TAGGED) &&
	    !atomic_flag_test_and_set(&block->op_list_freed)) {
		pr_debug("Block PC 0x%08x is fully tagged"
			 " - free opcode list\n", block->pc);

		lightrec_free_opcode_list(block->state, block->opcode_list);
		block->opcode_list = NULL;
	}

	return NULL;
}

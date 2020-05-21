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

#ifndef __LIGHTREC_RECOMPILER_H__
#define __LIGHTREC_RECOMPILER_H__

struct block;
struct lightrec_state;
struct recompiler;

struct recompiler *lightrec_recompiler_init(struct lightrec_state *state);
void lightrec_free_recompiler(struct recompiler *rec);
int lightrec_recompiler_add(struct recompiler *rec, struct block *block);
void lightrec_recompiler_remove(struct recompiler *rec, struct block *block);

void * lightrec_recompiler_run_first_pass(struct block *block, u32 *pc);

#endif /* __LIGHTREC_RECOMPILER_H__ */

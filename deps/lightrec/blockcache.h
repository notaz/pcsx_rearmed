/*
 * Copyright (C) 2014-2020 Paul Cercueil <paul@crapouillou.net>
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

#ifndef __BLOCKCACHE_H__
#define __BLOCKCACHE_H__

#include "lightrec.h"

struct blockcache;

struct block * lightrec_find_block(struct blockcache *cache, u32 pc);
void lightrec_register_block(struct blockcache *cache, struct block *block);
void lightrec_unregister_block(struct blockcache *cache, struct block *block);

struct blockcache * lightrec_blockcache_init(struct lightrec_state *state);
void lightrec_free_block_cache(struct blockcache *cache);

u32 lightrec_calculate_block_hash(const struct block *block);
_Bool lightrec_block_is_outdated(struct block *block);

#endif /* __BLOCKCACHE_H__ */

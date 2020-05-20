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

#ifndef __REGCACHE_H__
#define __REGCACHE_H__

#include "lightrec-private.h"

#define NUM_REGS (JIT_V_NUM - 2)
#define NUM_TEMPS (JIT_R_NUM)
#define LIGHTREC_REG_STATE (JIT_V(JIT_V_NUM - 1))
#define LIGHTREC_REG_CYCLE (JIT_V(JIT_V_NUM - 2))

#define REG_LO 32
#define REG_HI 33

struct register_value {
	_Bool known;
	u32 value;
};

struct native_register;
struct regcache;

u8 lightrec_alloc_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg);
u8 lightrec_alloc_reg_temp(struct regcache *cache, jit_state_t *_jit);
u8 lightrec_alloc_reg_out(struct regcache *cache, jit_state_t *_jit, u8 reg);
u8 lightrec_alloc_reg_in(struct regcache *cache, jit_state_t *_jit, u8 reg);
u8 lightrec_alloc_reg_out_ext(struct regcache *cache,
			      jit_state_t *_jit, u8 reg);
u8 lightrec_alloc_reg_in_ext(struct regcache *cache, jit_state_t *_jit, u8 reg);

u8 lightrec_request_reg_in(struct regcache *cache, jit_state_t *_jit,
			   u8 reg, u8 jit_reg);

void lightrec_regcache_reset(struct regcache *cache);

void lightrec_lock_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg);
void lightrec_free_reg(struct regcache *cache, u8 jit_reg);
void lightrec_free_regs(struct regcache *cache);
void lightrec_clean_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg);
void lightrec_clean_regs(struct regcache *cache, jit_state_t *_jit);
void lightrec_unload_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg);
void lightrec_storeback_regs(struct regcache *cache, jit_state_t *_jit);

void lightrec_clean_reg_if_loaded(struct regcache *cache, jit_state_t *_jit,
				  u8 reg, _Bool unload);

u8 lightrec_alloc_reg_in_address(struct regcache *cache,
		jit_state_t *_jit, u8 reg, s16 offset);

struct native_register * lightrec_regcache_enter_branch(struct regcache *cache);
void lightrec_regcache_leave_branch(struct regcache *cache,
			struct native_register *regs);

struct regcache * lightrec_regcache_init(struct lightrec_state *state);
void lightrec_free_regcache(struct regcache *cache);

const char * lightrec_reg_name(u8 reg);

void lightrec_regcache_mark_live(struct regcache *cache, jit_state_t *_jit);

#endif /* __REGCACHE_H__ */

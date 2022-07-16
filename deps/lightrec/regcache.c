// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2014-2021 Paul Cercueil <paul@crapouillou.net>
 */

#include "debug.h"
#include "memmanager.h"
#include "lightning-wrapper.h"
#include "regcache.h"

#include <stdbool.h>
#include <stddef.h>

struct native_register {
	bool used, loaded, dirty, output, extend, extended,
	     zero_extend, zero_extended, locked;
	s8 emulated_register;
};

struct regcache {
	struct lightrec_state *state;
	struct native_register lightrec_regs[NUM_REGS + NUM_TEMPS];
};

static const char * mips_regs[] = {
	"zero",
	"at",
	"v0", "v1",
	"a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9",
	"k0", "k1",
	"gp", "sp", "fp", "ra",
	"lo", "hi",
};

const char * lightrec_reg_name(u8 reg)
{
	return mips_regs[reg];
}

static inline bool lightrec_reg_is_zero(u8 jit_reg)
{
#if defined(__mips__) || defined(__alpha__) || defined(__riscv)
	if (jit_reg == _ZERO)
		return true;
#endif
	return false;
}

static inline s8 lightrec_get_hardwired_reg(u8 reg)
{
#if defined(__mips__) || defined(__alpha__) || defined(__riscv)
	if (reg == 0)
		return _ZERO;
#endif
	return -1;
}

static inline u8 lightrec_reg_number(const struct regcache *cache,
		const struct native_register *nreg)
{
	return (u8) (((uintptr_t) nreg - (uintptr_t) cache->lightrec_regs)
			/ sizeof(*nreg));
}

static inline u8 lightrec_reg_to_lightning(const struct regcache *cache,
		const struct native_register *nreg)
{
	u8 offset = lightrec_reg_number(cache, nreg);
	return offset < NUM_REGS ? JIT_V(offset) : JIT_R(offset - NUM_REGS);
}

static inline struct native_register * lightning_reg_to_lightrec(
		struct regcache *cache, u8 reg)
{
	if ((JIT_V0 > JIT_R0 && reg >= JIT_V0) ||
			(JIT_V0 < JIT_R0 && reg < JIT_R0)) {
		if (JIT_V1 > JIT_V0)
			return &cache->lightrec_regs[reg - JIT_V0];
		else
			return &cache->lightrec_regs[JIT_V0 - reg];
	} else {
		if (JIT_R1 > JIT_R0)
			return &cache->lightrec_regs[NUM_REGS + reg - JIT_R0];
		else
			return &cache->lightrec_regs[NUM_REGS + JIT_R0 - reg];
	}
}

u8 lightrec_get_reg_in_flags(struct regcache *cache, u8 jit_reg)
{
	struct native_register *reg;
	u8 flags = 0;

	if (lightrec_reg_is_zero(jit_reg))
		return REG_EXT | REG_ZEXT;

	reg = lightning_reg_to_lightrec(cache, jit_reg);
	if (reg->extended)
		flags |= REG_EXT;
	if (reg->zero_extended)
		flags |= REG_ZEXT;

	return flags;
}

void lightrec_set_reg_out_flags(struct regcache *cache, u8 jit_reg, u8 flags)
{
	struct native_register *reg;

	if (!lightrec_reg_is_zero(jit_reg)) {
		reg = lightning_reg_to_lightrec(cache, jit_reg);
		reg->extend = flags & REG_EXT;
		reg->zero_extend = flags & REG_ZEXT;
	}
}

static struct native_register * alloc_temp(struct regcache *cache)
{
	unsigned int i;

	/* We search the register list in reverse order. As temporaries are
	 * meant to be used only in the emitter functions, they can be mapped to
	 * caller-saved registers, as they won't have to be saved back to
	 * memory. */
	for (i = ARRAY_SIZE(cache->lightrec_regs); i; i--) {
		struct native_register *nreg = &cache->lightrec_regs[i - 1];
		if (!nreg->used && !nreg->loaded && !nreg->dirty)
			return nreg;
	}

	for (i = ARRAY_SIZE(cache->lightrec_regs); i; i--) {
		struct native_register *nreg = &cache->lightrec_regs[i - 1];
		if (!nreg->used)
			return nreg;
	}

	return NULL;
}

static struct native_register * find_mapped_reg(struct regcache *cache,
						u8 reg, bool out)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cache->lightrec_regs); i++) {
		struct native_register *nreg = &cache->lightrec_regs[i];
		if ((!reg || nreg->loaded || nreg->dirty) &&
				nreg->emulated_register == reg &&
				(!out || !nreg->locked))
			return nreg;
	}

	return NULL;
}

static struct native_register * alloc_in_out(struct regcache *cache,
					     u8 reg, bool out)
{
	struct native_register *nreg;
	unsigned int i;

	/* Try to find if the register is already mapped somewhere */
	nreg = find_mapped_reg(cache, reg, out);
	if (nreg)
		return nreg;

	/* Try to allocate a non-dirty, non-loaded register.
	 * Loaded registers may be re-used later, so it's better to avoid
	 * re-using one if possible. */
	for (i = 0; i < ARRAY_SIZE(cache->lightrec_regs); i++) {
		nreg = &cache->lightrec_regs[i];
		if (!nreg->used && !nreg->dirty && !nreg->loaded)
			return nreg;
	}

	/* Try to allocate a non-dirty register */
	for (i = 0; i < ARRAY_SIZE(cache->lightrec_regs); i++) {
		nreg = &cache->lightrec_regs[i];
		if (!nreg->used && !nreg->dirty)
			return nreg;
	}

	for (i = 0; i < ARRAY_SIZE(cache->lightrec_regs); i++) {
		nreg = &cache->lightrec_regs[i];
		if (!nreg->used)
			return nreg;
	}

	return NULL;
}

static void lightrec_discard_nreg(struct native_register *nreg)
{
	nreg->extended = false;
	nreg->zero_extended = false;
	nreg->loaded = false;
	nreg->output = false;
	nreg->dirty = false;
	nreg->used = false;
	nreg->locked = false;
	nreg->emulated_register = -1;
}

static void lightrec_unload_nreg(struct regcache *cache, jit_state_t *_jit,
		struct native_register *nreg, u8 jit_reg)
{
	/* If we get a dirty register, store back the old value */
	if (nreg->dirty) {
		s16 offset = offsetof(struct lightrec_state, regs.gpr)
			+ (nreg->emulated_register << 2);

		jit_stxi_i(offset, LIGHTREC_REG_STATE, jit_reg);
	}

	lightrec_discard_nreg(nreg);
}

void lightrec_unload_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg)
{
	if (lightrec_reg_is_zero(jit_reg))
		return;

	lightrec_unload_nreg(cache, _jit,
			lightning_reg_to_lightrec(cache, jit_reg), jit_reg);
}

/* lightrec_lock_reg: the register will be cleaned if dirty, then locked.
 * A locked register cannot only be used as input, not output. */
void lightrec_lock_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg)
{
	struct native_register *reg;

	if (lightrec_reg_is_zero(jit_reg))
		return;

	reg = lightning_reg_to_lightrec(cache, jit_reg);
	lightrec_clean_reg(cache, _jit, jit_reg);

	reg->locked = true;
}

u8 lightrec_alloc_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg)
{
	struct native_register *reg;

	if (lightrec_reg_is_zero(jit_reg))
		return jit_reg;

	reg = lightning_reg_to_lightrec(cache, jit_reg);
	lightrec_unload_nreg(cache, _jit, reg, jit_reg);

	reg->used = true;
	return jit_reg;
}

u8 lightrec_alloc_reg_temp(struct regcache *cache, jit_state_t *_jit)
{
	u8 jit_reg;
	struct native_register *nreg = alloc_temp(cache);
	if (!nreg) {
		/* No free register, no dirty register to free. */
		pr_err("No more registers! Abandon ship!\n");
		return 0;
	}

	jit_reg = lightrec_reg_to_lightning(cache, nreg);
	lightrec_unload_nreg(cache, _jit, nreg, jit_reg);

	nreg->used = true;
	return jit_reg;
}

u8 lightrec_alloc_reg_out(struct regcache *cache, jit_state_t *_jit,
			  u8 reg, u8 flags)
{
	struct native_register *nreg;
	u8 jit_reg;
	s8 hw_reg;

	hw_reg = lightrec_get_hardwired_reg(reg);
	if (hw_reg >= 0)
		return (u8) hw_reg;

	nreg = alloc_in_out(cache, reg, true);
	if (!nreg) {
		/* No free register, no dirty register to free. */
		pr_err("No more registers! Abandon ship!\n");
		return 0;
	}

	jit_reg = lightrec_reg_to_lightning(cache, nreg);

	/* If we get a dirty register that doesn't correspond to the one
	 * we're requesting, store back the old value */
	if (nreg->emulated_register != reg)
		lightrec_unload_nreg(cache, _jit, nreg, jit_reg);

	nreg->used = true;
	nreg->output = true;
	nreg->emulated_register = reg;
	nreg->extend = flags & REG_EXT;
	nreg->zero_extend = flags & REG_ZEXT;
	return jit_reg;
}

u8 lightrec_alloc_reg_in(struct regcache *cache, jit_state_t *_jit,
			 u8 reg, u8 flags)
{
	struct native_register *nreg;
	u8 jit_reg;
	bool reg_changed;
	s8 hw_reg;

	hw_reg = lightrec_get_hardwired_reg(reg);
	if (hw_reg >= 0)
		return (u8) hw_reg;

	nreg = alloc_in_out(cache, reg, false);
	if (!nreg) {
		/* No free register, no dirty register to free. */
		pr_err("No more registers! Abandon ship!\n");
		return 0;
	}

	jit_reg = lightrec_reg_to_lightning(cache, nreg);

	/* If we get a dirty register that doesn't correspond to the one
	 * we're requesting, store back the old value */
	reg_changed = nreg->emulated_register != reg;
	if (reg_changed)
		lightrec_unload_nreg(cache, _jit, nreg, jit_reg);

	if (!nreg->loaded && !nreg->dirty && reg != 0) {
		s16 offset = offsetof(struct lightrec_state, regs.gpr)
			+ (reg << 2);

		nreg->zero_extended = flags & REG_ZEXT;
		nreg->extended = !nreg->zero_extended;

		/* Load previous value from register cache */
		if (nreg->zero_extended)
			jit_ldxi_ui(jit_reg, LIGHTREC_REG_STATE, offset);
		else
			jit_ldxi_i(jit_reg, LIGHTREC_REG_STATE, offset);

		nreg->loaded = true;
	}

	/* Clear register r0 before use */
	if (reg == 0 && (!nreg->loaded || nreg->dirty)) {
		jit_movi(jit_reg, 0);
		nreg->extended = true;
		nreg->zero_extended = true;
		nreg->loaded = true;
	}

	nreg->used = true;
	nreg->output = false;
	nreg->emulated_register = reg;

	if ((flags & REG_EXT) && !nreg->extended &&
	    (!nreg->zero_extended || !(flags & REG_ZEXT))) {
		nreg->extended = true;
		nreg->zero_extended = false;
		jit_extr_i(jit_reg, jit_reg);
	} else if (!(flags & REG_EXT) && (flags & REG_ZEXT) &&
		   !nreg->zero_extended) {
		nreg->zero_extended = true;
		nreg->extended = false;
		jit_extr_ui(jit_reg, jit_reg);
	}

	return jit_reg;
}

u8 lightrec_request_reg_in(struct regcache *cache, jit_state_t *_jit,
			   u8 reg, u8 jit_reg)
{
	struct native_register *nreg;
	u16 offset;

	nreg = find_mapped_reg(cache, reg, false);
	if (nreg) {
		jit_reg = lightrec_reg_to_lightning(cache, nreg);
		nreg->used = true;
		return jit_reg;
	}

	nreg = lightning_reg_to_lightrec(cache, jit_reg);
	lightrec_unload_nreg(cache, _jit, nreg, jit_reg);

	/* Load previous value from register cache */
	offset = offsetof(struct lightrec_state, regs.gpr) + (reg << 2);
	jit_ldxi_i(jit_reg, LIGHTREC_REG_STATE, offset);

	nreg->extended = true;
	nreg->zero_extended = false;
	nreg->used = true;
	nreg->loaded = true;
	nreg->emulated_register = reg;

	return jit_reg;
}

static void free_reg(struct native_register *nreg)
{
	/* Set output registers as dirty */
	if (nreg->used && nreg->output && nreg->emulated_register > 0)
		nreg->dirty = true;
	if (nreg->output) {
		nreg->extended = nreg->extend;
		nreg->zero_extended = nreg->zero_extend;
	}
	nreg->used = false;
}

void lightrec_free_reg(struct regcache *cache, u8 jit_reg)
{
	if (!lightrec_reg_is_zero(jit_reg))
		free_reg(lightning_reg_to_lightrec(cache, jit_reg));
}

void lightrec_free_regs(struct regcache *cache)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cache->lightrec_regs); i++)
		free_reg(&cache->lightrec_regs[i]);
}

static void clean_reg(jit_state_t *_jit,
		struct native_register *nreg, u8 jit_reg, bool clean)
{
	if (nreg->dirty) {
		s16 offset = offsetof(struct lightrec_state, regs.gpr)
			+ (nreg->emulated_register << 2);

		jit_stxi_i(offset, LIGHTREC_REG_STATE, jit_reg);
		nreg->loaded |= nreg->dirty;
		nreg->dirty ^= clean;
	}
}

static void clean_regs(struct regcache *cache, jit_state_t *_jit, bool clean)
{
	unsigned int i;

	for (i = 0; i < NUM_REGS; i++)
		clean_reg(_jit, &cache->lightrec_regs[i], JIT_V(i), clean);
	for (i = 0; i < NUM_TEMPS; i++) {
		clean_reg(_jit, &cache->lightrec_regs[i + NUM_REGS],
				JIT_R(i), clean);
	}
}

void lightrec_storeback_regs(struct regcache *cache, jit_state_t *_jit)
{
	clean_regs(cache, _jit, false);
}

void lightrec_clean_regs(struct regcache *cache, jit_state_t *_jit)
{
	clean_regs(cache, _jit, true);
}

void lightrec_clean_reg(struct regcache *cache, jit_state_t *_jit, u8 jit_reg)
{
	struct native_register *reg;

	if (!lightrec_reg_is_zero(jit_reg)) {
		reg = lightning_reg_to_lightrec(cache, jit_reg);
		clean_reg(_jit, reg, jit_reg, true);
	}
}

void lightrec_clean_reg_if_loaded(struct regcache *cache, jit_state_t *_jit,
				  u8 reg, bool unload)
{
	struct native_register *nreg;
	u8 jit_reg;

	nreg = find_mapped_reg(cache, reg, false);
	if (nreg) {
		jit_reg = lightrec_reg_to_lightning(cache, nreg);

		if (unload)
			lightrec_unload_nreg(cache, _jit, nreg, jit_reg);
		else
			clean_reg(_jit, nreg, jit_reg, true);
	}
}

void lightrec_discard_reg_if_loaded(struct regcache *cache, u8 reg)
{
	struct native_register *nreg;

	nreg = find_mapped_reg(cache, reg, false);
	if (nreg)
		lightrec_discard_nreg(nreg);
}

struct native_register * lightrec_regcache_enter_branch(struct regcache *cache)
{
	struct native_register *backup;

	backup = lightrec_malloc(cache->state, MEM_FOR_LIGHTREC,
				 sizeof(cache->lightrec_regs));
	memcpy(backup, &cache->lightrec_regs, sizeof(cache->lightrec_regs));

	return backup;
}

void lightrec_regcache_leave_branch(struct regcache *cache,
			struct native_register *regs)
{
	memcpy(&cache->lightrec_regs, regs, sizeof(cache->lightrec_regs));
	lightrec_free(cache->state, MEM_FOR_LIGHTREC,
		      sizeof(cache->lightrec_regs), regs);
}

void lightrec_regcache_reset(struct regcache *cache)
{
	memset(&cache->lightrec_regs, 0, sizeof(cache->lightrec_regs));
}

struct regcache * lightrec_regcache_init(struct lightrec_state *state)
{
	struct regcache *cache;

	cache = lightrec_calloc(state, MEM_FOR_LIGHTREC, sizeof(*cache));
	if (!cache)
		return NULL;

	cache->state = state;

	return cache;
}

void lightrec_free_regcache(struct regcache *cache)
{
	return lightrec_free(cache->state, MEM_FOR_LIGHTREC,
			     sizeof(*cache), cache);
}

void lightrec_regcache_mark_live(struct regcache *cache, jit_state_t *_jit)
{
	struct native_register *nreg;
	unsigned int i;

#ifdef _WIN32
	/* FIXME: GNU Lightning on Windows seems to use our mapped registers as
	 * temporaries. Until the actual bug is found and fixed, unconditionally
	 * mark our registers as live here. */
	for (i = 0; i < NUM_REGS; i++) {
		nreg = &cache->lightrec_regs[i];

		if (nreg->used || nreg->loaded || nreg->dirty)
			jit_live(JIT_V(i));
	}
#endif

	for (i = 0; i < NUM_TEMPS; i++) {
		nreg = &cache->lightrec_regs[NUM_REGS + i];

		if (nreg->used || nreg->loaded || nreg->dirty)
			jit_live(JIT_R(i));
	}
}

/*
 * Copyright (C) 2016 Paul Cercueil <paul@crapouillou.net>
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

#ifndef __LIGHTREC_H__
#define __LIGHTREC_H__

#ifdef __cplusplus
#define _Bool bool
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#   ifdef lightrec_EXPORTS
#	define __api __declspec(dllexport)
#   elif !defined(LIGHTREC_STATIC)
#	define __api __declspec(dllimport)
#   else
#	define __api
#   endif
#elif __GNUC__ >= 4
#   define __api __attribute__((visibility ("default")))
#else
#   define __api
#endif

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t  s8;

struct lightrec_state;
struct lightrec_mem_map;

/* Exit flags */
#define LIGHTREC_EXIT_NORMAL	(0)
#define LIGHTREC_EXIT_SYSCALL	(1 << 0)
#define LIGHTREC_EXIT_BREAK	(1 << 1)
#define LIGHTREC_EXIT_CHECK_INTERRUPT	(1 << 2)
#define LIGHTREC_EXIT_SEGFAULT	(1 << 3)

enum psx_map {
	PSX_MAP_KERNEL_USER_RAM,
	PSX_MAP_BIOS,
	PSX_MAP_SCRATCH_PAD,
	PSX_MAP_PARALLEL_PORT,
	PSX_MAP_HW_REGISTERS,
	PSX_MAP_CACHE_CONTROL,
	PSX_MAP_MIRROR1,
	PSX_MAP_MIRROR2,
	PSX_MAP_MIRROR3,
};

enum mem_type {
	MEM_FOR_CODE,
	MEM_FOR_MIPS_CODE,
	MEM_FOR_IR,
	MEM_FOR_LIGHTREC,
	MEM_TYPE_END,
};

struct lightrec_mem_map_ops {
	void (*sb)(struct lightrec_state *, u32 addr, u8 data);
	void (*sh)(struct lightrec_state *, u32 addr, u16 data);
	void (*sw)(struct lightrec_state *, u32 addr, u32 data);
	u8 (*lb)(struct lightrec_state *, u32 addr);
	u16 (*lh)(struct lightrec_state *, u32 addr);
	u32 (*lw)(struct lightrec_state *, u32 addr);
};

struct lightrec_mem_map {
	u32 pc;
	u32 length;
	void *address;
	const struct lightrec_mem_map_ops *ops;
	const struct lightrec_mem_map *mirror_of;
};

struct lightrec_cop_ops {
	u32 (*mfc)(struct lightrec_state *state, u8 reg);
	u32 (*cfc)(struct lightrec_state *state, u8 reg);
	void (*mtc)(struct lightrec_state *state, u8 reg, u32 value);
	void (*ctc)(struct lightrec_state *state, u8 reg, u32 value);
	void (*op)(struct lightrec_state *state, u32 opcode);
};

struct lightrec_ops {
	struct lightrec_cop_ops cop0_ops;
	struct lightrec_cop_ops cop2_ops;
};

__api struct lightrec_state *lightrec_init(char *argv0,
					   const struct lightrec_mem_map *map,
					   size_t nb,
					   const struct lightrec_ops *ops);

__api void lightrec_destroy(struct lightrec_state *state);

__api u32 lightrec_execute(struct lightrec_state *state,
			   u32 pc, u32 target_cycle);
__api u32 lightrec_execute_one(struct lightrec_state *state, u32 pc);
__api u32 lightrec_run_interpreter(struct lightrec_state *state, u32 pc);

__api void lightrec_invalidate(struct lightrec_state *state, u32 addr, u32 len);
__api void lightrec_invalidate_all(struct lightrec_state *state);
__api void lightrec_set_invalidate_mode(struct lightrec_state *state,
					_Bool dma_only);

__api void lightrec_set_exit_flags(struct lightrec_state *state, u32 flags);
__api u32 lightrec_exit_flags(struct lightrec_state *state);

__api void lightrec_dump_registers(struct lightrec_state *state, u32 regs[34]);
__api void lightrec_restore_registers(struct lightrec_state *state,
				      u32 regs[34]);

__api u32 lightrec_current_cycle_count(const struct lightrec_state *state);
__api void lightrec_reset_cycle_count(struct lightrec_state *state, u32 cycles);
__api void lightrec_set_target_cycle_count(struct lightrec_state *state,
					   u32 cycles);

__api unsigned int lightrec_get_mem_usage(enum mem_type type);
__api unsigned int lightrec_get_total_mem_usage(void);
__api float lightrec_get_average_ipi(void);

#ifdef __cplusplus
};
#endif

#endif /* __LIGHTREC_H__ */

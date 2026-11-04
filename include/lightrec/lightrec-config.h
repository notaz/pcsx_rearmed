/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2019-2021 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __LIGHTREC_CONFIG_H__
#define __LIGHTREC_CONFIG_H__

#if defined(_WIN32)
#include <malloc.h>
#ifndef LIGHTREC_ALIGNED_ALLOC
#define LIGHTREC_ALIGNED_ALLOC(alignment, size) _aligned_malloc(size, alignment)
#endif
#ifndef LIGHTREC_ALIGNED_FREE
#define LIGHTREC_ALIGNED_FREE _aligned_free
#endif
#elif defined(__PSL1GHT__)
#include <malloc.h>
#ifndef LIGHTREC_ALIGNED_ALLOC
#define LIGHTREC_ALIGNED_ALLOC memalign
#endif
#ifndef LIGHTREC_ALIGNED_FREE
#define LIGHTREC_ALIGNED_FREE free
#endif
#endif

#define ENABLE_THREADED_COMPILER LIGHTREC_ENABLE_THREADED_COMPILER
#define ENABLE_FIRST_PASS 1
#define ENABLE_DISASSEMBLER LIGHTREC_ENABLE_DISASSEMBLER
#define ENABLE_CODE_BUFFER 1

#define HAS_DEFAULT_ELM 1

#define OPT_REMOVE_DIV_BY_ZERO_SEQ 1
#define OPT_REPLACE_MEMSET LIGHTREC_NO_DEBUG
#define OPT_DETECT_IMPOSSIBLE_BRANCHES 1
#define OPT_HANDLE_LOAD_DELAYS 1
#define OPT_TRANSFORM_OPS 1
#define OPT_LOCAL_BRANCHES 1
#define OPT_SWITCH_DELAY_SLOTS 1
#define OPT_FLAG_IO 1
#define OPT_FLAG_MULT_DIV LIGHTREC_NO_DEBUG
#define OPT_EARLY_UNLOAD 1
#define OPT_PRELOAD_PC 1
#define OPT_DETECT_IDLE 1

#define OPT_SH4_USE_GBR 0

#endif /* __LIGHTREC_CONFIG_H__ */


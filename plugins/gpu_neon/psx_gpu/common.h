#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#ifdef NEON_PC
typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;
#else
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long int s64;
typedef unsigned long long int u64;
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "vector_ops.h"
#include "psx_gpu.h"

#define unlikely(x) __builtin_expect((x), 0)

#endif


CFLAGS += -ggdb -Wall -O2 -DNDEBUG

include ../../config.mak

SRC += psx_gpu_if.c

CFLAGS += -I../../include
CFLAGS += -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
CFLAGS += -DGPU_NEON -DNEON_BUILD
ifeq "$(HAVE_NEON_ASM)" "1"
SRC += psx_gpu/psx_gpu_arm_neon.S
else
SRC += psx_gpu/psx_gpu_simd.c
CFLAGS += -DSIMD_BUILD
endif

BIN_GPULIB = gpu_neon.so
include ../gpulib/gpulib.mak

CFLAGS += -ggdb -Wall -O2 -DNDEBUG

include ../../config.mak

SRC += psx_gpu_if.c

CFLAGS += -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
ifeq "$(HAVE_NEON)" "1"
SRC += psx_gpu/psx_gpu_arm_neon.S
CFLAGS += -DNEON_BUILD
else
CFLAGS += -fno-strict-aliasing
endif

BIN_GPULIB = gpu_neon.so
include ../gpulib/gpulib.mak

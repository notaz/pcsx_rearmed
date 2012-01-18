CC = $(CROSS_COMPILE)gcc

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

HAVE_NEON = $(shell $(CC) -E -dD $(CFLAGS_) ../gpulib/gpu.h | grep -q '__ARM_NEON__ 1' && echo 1)

# FIXME: rework NEON detection to get rid of this
ifeq "$(ARCH)" "arm"
 ARM_CORTEXA8 ?= 1
 ifeq "$(ARM_CORTEXA8)" "1"
  CFLAGS_ += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
 endif
endif

CFLAGS += -ggdb -Wall -fPIC -O2

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

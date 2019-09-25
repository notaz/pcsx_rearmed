CFLAGS += -ggdb -Wall -O3 -ffast-math
CFLAGS += -DREARMED
CFLAGS += -I../../include
#CFLAGS += -DINLINE="static __inline__"
#CFLAGS += -Dasm="__asm__ __volatile__"
CFLAGS += -DUSE_GPULIB=1

include ../../config.mak

SRC_STANDALONE += gpu.cpp
SRC_GPULIB += gpulib_if.cpp

ifeq "$(ARCH)" "arm"
SRC += gpu_arm.S
endif

#BIN_STANDALONE = gpuPCSX4ALL.so
BIN_GPULIB = gpu_unai.so
include ../gpulib/gpulib.mak

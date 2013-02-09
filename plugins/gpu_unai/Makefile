CFLAGS += -ggdb -Wall -O3 -ffast-math
CFLAGS += -DREARMED
CFLAGS += -I../../include

include ../../config.mak

SRC_STANDALONE += gpu.cpp
SRC_GPULIB += gpulib_if.cpp

ifeq "$(ARCH)" "arm"
SRC += gpu_arm.s
endif

#BIN_STANDALONE = gpuPCSX4ALL.so
BIN_GPULIB = gpu_unai.so
include ../gpulib/gpulib.mak

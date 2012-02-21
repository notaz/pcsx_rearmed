CC = $(CROSS_COMPILE)g++

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -DREARMED
CFLAGS += -ggdb -Wall -O3 -ffast-math

SRC_STANDALONE += gpu.cpp
SRC_GPULIB += gpulib_if.cpp

ifeq "$(ARCH)" "arm"
SRC += gpu_arm.s
endif

BIN_STANDLALONE = gpuPCSX4ALL.so
BIN_GPULIB = gpu_unai.so
include ../gpulib/gpulib.mak

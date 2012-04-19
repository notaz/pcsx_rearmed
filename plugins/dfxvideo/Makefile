CFLAGS += -Wall -ggdb -O2
CFLAGS += -fno-strict-aliasing -ffast-math
# ugh
CFLAGS += -I../../libpcsxcore

include ../../config.mak

SRC_STANDALONE += gpu.c
SRC_GPULIB += gpulib_if.c

ifeq "$(ARCH)" "arm"
SRC_STANDALONE += draw_fb.c
else
SRC_STANDALONE += draw.c
LDLIBS_STANDALONE += -lX11 -lXv
endif

BIN_STANDLALONE = gpuPEOPS.so
BIN_GPULIB = gpu_peops.so
include ../gpulib/gpulib.mak

CFLAGS += -Wall -ggdb -O2
CFLAGS += -fno-strict-aliasing -ffast-math
CFLAGS += -I../../include

include ../../config.mak

SRC_STANDALONE += gpu.c
SRC_GPULIB += gpulib_if.c

#ifeq "$(ARCH)" "arm"
SRC_STANDALONE += draw_pl.c
#else
#SRC_STANDALONE += draw.c
#LDLIBS_STANDALONE += -lX11 -lXv -lXext
#endif

#BIN_STANDALONE = gpuPEOPS.so
BIN_GPULIB = gpu_peops.so
include ../gpulib/gpulib.mak

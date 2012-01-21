CC = $(CROSS_COMPILE)gcc

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -ggdb -fPIC -O2 # -Wall

SRC_STANDALONE += gpuDraw.c gpuFps.c gpuPlugin.c gpuPrim.c gpuTexture.c
SRC_GPULIB += gpulib_if.c

CFLAGS += -I$(PREFIX)include
LDLIBS += -L$(PREFIX)lib
ifeq "$(PLATFORM)" "caanoo"
CFLAGS += -DFAKE_WINDOW
LDLIBS += -lopengles_lite -lstdc++
else
LDLIBS += -lGLES_CM
endif

BIN_STANDLALONE = gpuGLES.so
BIN_GPULIB = gpu_gles.so
include ../gpulib/gpulib.mak

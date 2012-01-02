CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
CC_ = $(CC)

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -ggdb -Wall -fPIC -O2
CFLAGS += -fno-strict-aliasing
ifdef MAEMO
CFLAGS += -DMAEMO
endif

SRC += gpu.c
ifeq "$(ARCH)" "arm"
ARM_CORTEXA8 ?= 1
ifeq "$(ARM_CORTEXA8)" "1"
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
SRC += ../../frontend/cspace_neon.s
else
CFLAGS += -mcpu=arm926ej-s -mtune=arm926ej-s
SRC += ../../frontend/cspace.c
endif
SRC += vout_fb.c
EXT = so
endif
ifeq "$(ARCH)" "x86_64"
CFLAGS += `sdl-config --cflags` -m32
LDFLAGS += `sdl-config --libs`
SRC += vout_sdl.c
EXT = so.x86
endif

HAVE_NEON = $(shell $(CC_) -E -dD $(CFLAGS) gpu.h | grep -q '__ARM_NEON__ 1' && echo 1)

TARGETS = gpu_neon.$(EXT) gpu_peops.$(EXT) gpu_unai.$(EXT)

gpu_neon.$(EXT): SRC += psx_gpu_if.c
gpu_neon.$(EXT): CFLAGS += -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
ifeq "$(HAVE_NEON)" "1"
gpu_neon.$(EXT): SRC += psx_gpu/psx_gpu_arm_neon.S
gpu_neon.$(EXT): CFLAGS += -DNEON_BUILD
else
gpu_neon.$(EXT): CFLAGS += -fno-strict-aliasing
endif
gpu_peops.$(EXT): SRC += peops_if.c
gpu_peops.$(EXT): CFLAGS += -fno-strict-aliasing
gpu_unai.$(EXT): SRC += unai_if.cpp
gpu_unai.$(EXT): CC_ = $(CXX)
ifeq "$(ARCH)" "arm"
gpu_unai.$(EXT): SRC += ../gpu_unai/gpu_arm.s
endif

all: $(TARGETS)

$(TARGETS): $(SRC)
	$(CC_) -o $@ $(SRC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@

clean:
	$(RM) $(TARGETS)

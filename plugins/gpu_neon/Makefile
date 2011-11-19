CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
CC_ = $(CC)

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -ggdb -Wall -fPIC -O2
CFLAGS += -fno-strict-aliasing

SRC += gpu.c
ifeq "$(ARCH)" "arm"
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
SRC += vout_fb.c ../../frontend/cspace_neon.s
EXT = so
endif
ifeq "$(ARCH)" "x86_64"
CFLAGS += `sdl-config --cflags` -m32
LDFLAGS += `sdl-config --libs`
SRC += vout_sdl.c
EXT = so.x86
endif

TARGETS = gpu_neon.$(EXT) gpu_peops.$(EXT) gpu_unai.$(EXT)

gpu_neon.$(EXT): SRC += psx_gpu_if.c
gpu_neon.$(EXT): CFLAGS += -fno-strict-aliasing
gpu_peops.$(EXT): SRC += peops_if.c
gpu_peops.$(EXT): CFLAGS += -fno-strict-aliasing
gpu_unai.$(EXT): SRC += unai_if.cpp
gpu_unai.$(EXT): CC_ = $(CXX)

all: $(TARGETS)

$(TARGETS): $(SRC)
	$(CC_) -o $@ $(SRC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@

clean:
	$(RM) $(TARGETS)

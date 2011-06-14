CC = $(CROSS_COMPILE)gcc

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -ggdb -Wall -fPIC -O2

TARGET = gpu_neon.so
LDFLAGS += -shared -Wl,-soname,$(TARGET)
SRC += gpu.c
ifeq "$(ARCH)" "arm"
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
SRC += vout_fb.c ../../frontend/arm_utils.s
else
CFLAGS += `sdl-config --cflags` -m32
LDFLAGS += `sdl-config --libs`
SRC += vout_sdl.c
endif
PEOPS=1
ifndef PEOPS
else
SRC += peops.c
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) $(TARGET)

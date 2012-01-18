CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
AR = $(CROSS_COMPILE)ar

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -ggdb -Wall -fPIC -O2

OBJS += gpu.o

ifeq "$(ARCH)" "arm"
ARM_CORTEXA8 ?= 1
ifeq "$(ARM_CORTEXA8)" "1"
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
ASFLAGS += -mcpu=cortex-a8 -mfpu=neon
OBJS += cspace_neon.o
else
CFLAGS += -mcpu=arm926ej-s -mtune=arm926ej-s
ASFLAGS += -mcpu=arm926ej-s -mfloat-abi=softfp
OBJS += cspace.o
endif
OBJS += vout_fb.o
EXT = a
else
CFLAGS += `sdl-config --cflags` -m32
OBJS += vout_sdl.o
EXT = x86.a
endif

TARGET = gpulib.$(EXT)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) crs $@ $^

clean:
	$(RM) $(TARGET) $(OBJS)

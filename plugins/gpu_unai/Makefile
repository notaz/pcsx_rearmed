CC = $(CROSS_COMPILE)gcc

CFLAGS += -ggdb -fPIC -Wall -DREARMED
ifndef DEBUG
CFLAGS += -O2 -ffast-math -fomit-frame-pointer
endif
ifdef MAEMO
CFLAGS += -DMAEMO
endif
ARM_CORTEXA8 ?= 1
ifeq "$(ARM_CORTEXA8)" "1"
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
SRC += ../../frontend/cspace_neon.s
else
CFLAGS += -mcpu=arm926ej-s -mtune=arm926ej-s
SRC += ../../frontend/cspace.c
endif

SRC += gpu.cpp gpu_arm.s

TARGET = gpuPCSX4ALL.so
LDFLAGS += -shared -Wl,-soname,$(TARGET)

-include Makefile.local

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

# $(TARGET): *.h

clean:
	$(RM) $(TARGET) $(OBJ)

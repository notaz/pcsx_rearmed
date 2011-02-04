CC = $(CROSS_COMPILE)gcc

CFLAGS += -ggdb -fPIC -Wall -DREARMED
ifndef DEBUG
CFLAGS += -O2 -ffast-math -fomit-frame-pointer
endif
ifdef MAEMO
CFLAGS += -DMAEMO
endif
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
# -fschedule-insns (from -O2+) causes bugs, probably bad asm() statements
CFLAGS += -fno-schedule-insns -fno-schedule-insns2

TARGET = gpuPCSX4ALL.so
LDFLAGS += -shared -Wl,-soname,$(TARGET)

-include Makefile.local

all: $(TARGET)

$(TARGET): gpu.cpp ../../frontend/arm_utils.s
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

# $(TARGET): *.h

clean:
	$(RM) $(TARGET) $(OBJ)

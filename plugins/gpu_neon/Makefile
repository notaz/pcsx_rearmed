CC = $(CROSS_COMPILE)gcc
CFLAGS += -ggdb -Wall -fPIC
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp

TARGET = gpu_neon.so
LDFLAGS += -shared -Wl,-soname,$(TARGET)
SRC += gpu.c ../../frontend/arm_utils.s

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) $(TARGET)

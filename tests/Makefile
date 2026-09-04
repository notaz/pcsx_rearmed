CROSS_COMPILE ?= mipsel-linux-gnu-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as

CFLAGS += -march=mips1 -mabi=32 -EL -fno-pic -mno-shared -mno-abicalls -mfp32 -mno-llsc
CFLAGS += -fno-stack-protector -nostdlib -ffreestanding
CFLAGS += -mno-gpopt -fomit-frame-pointer
CFLAGS += -fno-builtin
CFLAGS += -fno-strict-aliasing
CFLAGS += -g -Wall -O2
LDFLAGS += -Wl,-Map=$(TARGET).map -nostdlib -Tps-exe.ld -static -Wl,--gc-sections
LDFLAGS += $(ARCHFLAGS) -Wl,--oformat=elf32-tradlittlemips

TARGET = test.exe

OBJS += crt0.o asm.o main.o

all: $(TARGET)

$(TARGET): $(TARGET).elf
	$(CROSS_COMPILE)objcopy -O binary $< $@

$(TARGET).elf: $(OBJS) ps-exe.ld
	$(CC) -g -o $@ $(filter %.o,$^) $(LDFLAGS) $(LDLIBS)

%.o: %.s
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.S
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	$(RM) *.o $(TARGET).elf $(TARGET).map $(TARGET)

.PHONY: clean all

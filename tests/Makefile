CROSS_COMPILE ?= mipsel-linux-gnu-
# gcc-mipsel-linux-gnu is gone from newer distros, but binutils remain.
# The external (GNU) assembler is used as LLVM's one can't handle GTE ops.
CC = clang --target=$(CROSS_COMPILE:-=) -fno-integrated-as
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS += -march=mips1 -mabi=32 -EL -fno-pic -mno-abicalls -mfp32
#CFLAGS += mno-shared -mno-llsc
CFLAGS += -fno-stack-protector -nostdlib -ffreestanding
CFLAGS += -mno-gpopt -fomit-frame-pointer
CFLAGS += -fno-builtin
CFLAGS += -fno-strict-aliasing
CFLAGS += -g -Wall -O2
ASFLAGS += -march=mips1 -mabi=32 -EL -fno-pic -mno-abicalls -g
LDFLAGS += -Wl,-Map=$(TARGET).map -nostdlib -Tps-exe.ld -static -Wl,--gc-sections
LDFLAGS += $(ARCHFLAGS) -Wl,--oformat=elf32-tradlittlemips

TARGET = test.exe

OBJS += crt0.o asm.o main.o

all: $(TARGET)

$(TARGET): $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).elf: $(OBJS) ps-exe.ld
	$(CC) -g -o $@ $(filter %.o,$^) $(LDFLAGS) $(LDLIBS)

%.o: %.s
	$(CC) -c $(ASFLAGS) -o $@ $<

%.o: %.S
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	$(RM) *.o $(TARGET).elf $(TARGET).map $(TARGET)

testcd.cue:
	../tools/mkm2cd testcd 750
	test -f $@

run: $(TARGET) ../pcsx testcd.cue
	cd .. && $(QEMU) ./pcsx -stdout -stdin -novid -nothread $(TEST_ARGS) \
		-cdfile tests/testcd.cue tests/$(TARGET)

.PHONY: clean all run

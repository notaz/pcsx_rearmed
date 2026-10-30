CC ?= cc
CFLAGS += -std=gnu99 -O2 -Imemory_include -D__PS3__ -D__PSL1GHT__
EXE_EXT = $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),.exe)
TARGET = memory-psl1ght-test$(EXE_EXT)
all: $(TARGET)
$(TARGET): memory_test.c ../../libpcsxcore/lightrec/ps3.c ../../libpcsxcore/lightrec/ps3.h $(wildcard memory_include/sys/*.h) memory_include/ppu-lv2.h
	$(CC) $(CFLAGS) $< -o $@
run: all
	./$(TARGET)
clean:
	$(RM) $(TARGET)
.PHONY: all run clean

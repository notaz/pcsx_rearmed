CC ?= cc
CFLAGS += -O2 -std=gnu99 -fno-strict-aliasing -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections
EXE_EXT = $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),.exe)
TARGET = dither-test$(EXE_EXT)
RENDERER = ../../plugins/dfxvideo/gpulib_if.c ../../plugins/dfxvideo/soft.c
RENDERER += ../../plugins/dfxvideo/prim.c ../../plugins/gpulib/gpu.h

all: $(TARGET)

$(TARGET): dither_test.c $(RENDERER)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

run: all
	./$(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: all run clean

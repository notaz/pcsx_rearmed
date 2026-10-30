CC ?= cc
CFLAGS += -O2 -std=gnu99 -fno-strict-aliasing -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections
EXE_EXT = $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),.exe)
TARGETS = dither-test$(EXE_EXT) memory-test$(EXE_EXT)
RENDERER = ../../plugins/dfxvideo/gpulib_if.c ../../plugins/dfxvideo/soft.c
RENDERER += ../../plugins/dfxvideo/prim.c ../../plugins/gpulib/gpu.h

all: $(TARGETS)

dither-test$(EXE_EXT): dither_test.c $(RENDERER)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

memory-test$(EXE_EXT): memory_test.c $(RENDERER)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

run: all
	./dither-test$(EXE_EXT)
	./memory-test$(EXE_EXT)

clean:
	$(RM) $(TARGETS)

.PHONY: all run clean

CC ?= cc
CFLAGS += -std=gnu99 -O2 -fno-strict-aliasing
CFLAGS += -I../../include/lightning -I../../deps/lightning/include
CFLAGS += -I../../include/lightrec -I../../deps/lightrec -DLIGHTREC_STATIC
CFLAGS += -DLIGHTREC_ENABLE_THREADED_COMPILER=0 -DLIGHTREC_ENABLE_DISASSEMBLER=0
CFLAGS += -DLIGHTREC_NO_DEBUG=1 -DHAVE_MMAP=1
LDLIBS += -lm -lpthread

JIT_NAMES = jit_disasm jit_memory jit_names jit_note jit_print jit_size lightning
LIGHTREC_NAMES = blockcache constprop disassembler emitter interpreter lightrec
LIGHTREC_NAMES += memmanager optimizer regcache
OBJS = $(addprefix jit-,$(addsuffix .o,$(JIT_NAMES)))
OBJS += $(addprefix lightrec-,$(addsuffix .o,$(LIGHTREC_NAMES))) lightrec-tlsf.o

ifneq (,$(findstring mingw,$(shell $(CC) -dumpmachine)))
EXE_EXT = .exe
CFLAGS += -I../../include/mman -I../../deps/mman -Dmprotect=_mprotect
OBJS += mman.o
endif

TESTS = $(addsuffix $(EXE_EXT),startup-quiet-test startup-log-test)
TESTS += $(addsuffix $(EXE_EXT),startup-selftest-quiet-test startup-selftest-log-test)
TESTS += psl1ght-log-path-test$(EXE_EXT)
TESTS += startup-psl1ght-selftest-test$(EXE_EXT)
TESTS += startup-platform-selftest-test$(EXE_EXT)
TESTS += memory-psl1ght-test$(EXE_EXT)
MEMORY_DEPS = memory_test.c ../../libpcsxcore/lightrec/ps3.c ../../libpcsxcore/lightrec/ps3.h
MEMORY_DEPS += $(wildcard memory_include/sys/*.h) memory_include/ppu-lv2.h

all: $(TESTS)

memory-psl1ght-test$(EXE_EXT): $(MEMORY_DEPS)
	$(CC) $(CFLAGS) -Imemory_include -D__PSL1GHT__ $< -o $@

psl1ght-log-path-test$(EXE_EXT): psl1ght_log_test.c ../../libpcsxcore/lightrec/ps3_log.c
	$(CC) $(CFLAGS) -Iinclude -D__PS3__ -D__PSL1GHT__ -DPS3_LIGHTREC_LOG=1 -Dmkdir=ps3_test_mkdir -Dfopen=ps3_test_fopen $^ -o $@

startup-quiet-test$(EXE_EXT): startup_test.c ../../libpcsxcore/lightrec/ps3_init.c
	$(CC) $(CFLAGS) -DPS3_LIGHTREC_LOG=0 -DPS3_LIGHTREC_SELFTEST=0 $^ -o $@

startup-log-test$(EXE_EXT): startup_test.c ../../libpcsxcore/lightrec/ps3_init.c
	$(CC) $(CFLAGS) -DPS3_LIGHTREC_LOG=1 -DPS3_LIGHTREC_SELFTEST=0 $^ -o $@

startup-selftest-quiet-test$(EXE_EXT): startup_test.c ../../libpcsxcore/lightrec/ps3_init.c ../../libpcsxcore/lightrec/ps3_test.c $(OBJS)
	$(CC) $(CFLAGS) -D__PS3__ -D__PSL1GHT__ -DPS3_LIGHTREC_CORE -DPS3_LIGHTREC_LOG=0 -DPS3_LIGHTREC_SELFTEST=1 $^ $(LDLIBS) -o $@

startup-selftest-log-test$(EXE_EXT): startup_test.c ../../libpcsxcore/lightrec/ps3_init.c ../../libpcsxcore/lightrec/ps3_test.c $(OBJS)
	$(CC) $(CFLAGS) -D__PS3__ -D__PSL1GHT__ -DPS3_LIGHTREC_CORE -DPS3_LIGHTREC_LOG=1 -DPS3_LIGHTREC_SELFTEST=1 $^ $(LDLIBS) -o $@

startup-psl1ght-selftest-test$(EXE_EXT): startup_test.c ../../libpcsxcore/lightrec/ps3_init.c ../../libpcsxcore/lightrec/ps3_test.c $(OBJS)
	$(CC) $(CFLAGS) -D__PS3__ -D__PSL1GHT__ -DPS3_LIGHTREC_CORE -DPS3_LIGHTREC_LOG=0 -DPS3_LIGHTREC_SELFTEST=1 $^ $(LDLIBS) -o $@

startup-platform-selftest-test$(EXE_EXT): startup_test.c ../../libpcsxcore/lightrec/ps3_init.c ../../libpcsxcore/lightrec/ps3_test.c $(OBJS)
	$(CC) $(CFLAGS) -D__PS3__ -DPS3_LIGHTREC_CORE -DPS3_LIGHTREC_LOG=0 -DPS3_LIGHTREC_SELFTEST=1 $^ $(LDLIBS) -o $@

jit-%.o: ../../deps/lightning/lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

lightrec-%.o: ../../deps/lightrec/%.c
	$(CC) $(CFLAGS) -c $< -o $@

lightrec-tlsf.o: ../../deps/lightrec/tlsf/tlsf.c
	$(CC) $(CFLAGS) -c $< -o $@

mman.o: ../../deps/mman/mman.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./memory-psl1ght-test$(EXE_EXT)
	./psl1ght-log-path-test$(EXE_EXT)
	./startup-quiet-test$(EXE_EXT)
	./startup-log-test$(EXE_EXT)
	./startup-selftest-quiet-test$(EXE_EXT)
	./startup-selftest-log-test$(EXE_EXT)
	./startup-psl1ght-selftest-test$(EXE_EXT)
	./startup-platform-selftest-test$(EXE_EXT)

clean:
	$(RM) $(OBJS) $(TESTS)

.PHONY: all run clean

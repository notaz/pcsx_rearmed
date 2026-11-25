# Tests for the SIO1 (link cable) emulation, see README.md.
#
#   make check        port model and two process link, no emulator needed
#   make check-emu    two instances of the libretro core running psx/linktest
#                     (needs CORE=<pcsx_rearmed_libretro.so> and a MIPS gcc)

CC ?= gcc
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu99 -Wall -Wextra
CORE_DIR = ../../libpcsxcore
MODEL = -DSIO1_MODEL_ONLY
SEEDS ?= 1 2 3 5 8
CYCLES ?= 33868800

# any gcc that can make MIPS I code will do
MIPS_CC ?= mipsel-linux-gnu-gcc
MIPS_OBJCOPY ?= mipsel-linux-gnu-objcopy
MIPS_CFLAGS = -march=mips1 -mabi=32 -EL -msoft-float -mno-abicalls -fno-pic \
	-G0 -ffreestanding -fno-builtin -nostdlib -O2 -Wall -Wl,--build-id=none

CORE ?= ../../pcsx_rearmed_libretro.so

all: test_model test_link

test_model: test_model.c $(CORE_DIR)/sio1.c $(CORE_DIR)/sio1.h
	$(CC) $(CFLAGS) $(MODEL) -fsanitize=address,undefined \
		test_model.c $(CORE_DIR)/sio1.c -o $@

test_link: test_link.c $(CORE_DIR)/sio1.c $(CORE_DIR)/sio1_link.c \
		$(CORE_DIR)/sio1.h $(CORE_DIR)/sio1_link.h
	$(CC) $(CFLAGS) $(MODEL) -DSIO1_LINK_SHM test_link.c \
		$(CORE_DIR)/sio1.c $(CORE_DIR)/sio1_link.c -o $@ -lpthread

retro_host: retro_host.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE -I../../deps/libretro-common/include \
		retro_host.c -o $@ -ldl

psx/linktest.exe: psx/linktest.c psx/start.S psx/link.ld psx/mkexe.py
	$(MIPS_CC) $(MIPS_CFLAGS) -T psx/link.ld psx/start.S psx/linktest.c \
		-o psx/linktest.elf
	$(MIPS_OBJCOPY) -O binary psx/linktest.elf psx/linktest.bin
	python3 psx/mkexe.py psx/linktest.bin $@

check: test_model test_link
	./test_model
	@for delay in 0 512; do for seed in $(SEEDS); do \
		echo "link test: seed $$seed, line delay $$delay"; \
		./run_link_test.sh $$seed $(CYCLES) 2 $$delay | grep "^run" || exit 1; \
		./run_link_test.sh $$seed $(CYCLES) 2 $$delay > /dev/null || exit 1; \
	done; done

check-emu: retro_host psx/linktest.exe
	@for timing in exact fast; do for drc in disabled enabled; do \
		echo "two linked instances: timing $$timing, dynarec $$drc"; \
		./run_emu_test.sh $(CORE) psx/linktest.exe $$timing $$drc || exit 1; \
	done; done

clean:
	$(RM) test_model test_link retro_host psx/linktest.elf \
		psx/linktest.bin psx/linktest.exe

.PHONY: all check check-emu clean

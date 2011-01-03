#CROSS_COMPILE=
AS = $(CROSS_COMPILE)as
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld

ARCH = $(shell $(CC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -ggdb -Ifrontend
LDFLAGS += -lz -lpthread -ldl -lpng
ifeq "$(ARCH)" "arm"
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=softfp -ffast-math
ASFLAGS += -mcpu=cortex-a8 -mfpu=neon
endif
ifndef DEBUG
CFLAGS += -O2 # -DNDEBUG
endif
#DRC_DBG = 1
#PCNT = 1
TARGET = pcsx

all: $(TARGET)

# core
OBJS += libpcsxcore/cdriso.o libpcsxcore/cdrom.o libpcsxcore/cheat.o libpcsxcore/debug.o \
	libpcsxcore/decode_xa.o libpcsxcore/disr3000a.o libpcsxcore/gte.o libpcsxcore/mdec.o \
	libpcsxcore/misc.o libpcsxcore/plugins.o libpcsxcore/ppf.o libpcsxcore/psxbios.o \
	libpcsxcore/psxcommon.o libpcsxcore/psxcounters.o libpcsxcore/psxdma.o libpcsxcore/psxhle.o \
	libpcsxcore/psxhw.o libpcsxcore/psxinterpreter.o libpcsxcore/psxmem.o libpcsxcore/r3000a.o \
	libpcsxcore/sio.o libpcsxcore/socket.o libpcsxcore/spu.o
# dynarec
ifndef NO_NEW_DRC
OBJS += libpcsxcore/new_dynarec/new_dynarec.o libpcsxcore/new_dynarec/linkage_arm.o
OBJS += libpcsxcore/new_dynarec/pcsxmem.o
endif
OBJS += libpcsxcore/new_dynarec/emu_if.o
libpcsxcore/new_dynarec/new_dynarec.o: libpcsxcore/new_dynarec/assem_arm.c
ifdef DRC_DBG
libpcsxcore/new_dynarec/emu_if.o: CFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -DDRC_DBG
endif

# spu
OBJS += plugins/dfsound/adsr.o plugins/dfsound/dma.o plugins/dfsound/oss.o plugins/dfsound/reverb.o \
	plugins/dfsound/xa.o plugins/dfsound/freeze.o plugins/dfsound/cfg.o plugins/dfsound/registers.o \
	plugins/dfsound/spu.o
# gpu
plugins/dfxvideo/%.o: CFLAGS += -Wall
OBJS += plugins/dfxvideo/gpu.o
ifdef X11
LDFLAGS += -lX11 -lXv
OBJS += plugins/dfxvideo/draw.o
else
OBJS += plugins/dfxvideo/draw_fb.o
endif
# cdrcimg
plugins/cdrcimg/%.o: CFLAGS += -Wall
OBJS += plugins/cdrcimg/cdrcimg.o

# gui
OBJS += frontend/main.o frontend/plugin.o frontend/plugin_lib.o
OBJS += frontend/menu.o
OBJS += frontend/linux/fbdev.o frontend/linux/in_evdev.o
OBJS += frontend/linux/plat.o frontend/linux/oshide.o
OBJS += frontend/common/fonts.o frontend/common/input.o frontend/common/readpng.o
ifeq "$(ARCH)" "arm"
OBJS += frontend/arm_utils.o
OBJS += frontend/plat_omap.o
else
OBJS += frontend/plat_dummy.o
endif
ifdef PCNT
CFLAGS += -DPCNT
endif
frontend/%.o: CFLAGS += -Wall -DIN_EVDEV
frontend/menu.o: frontend/revision.h

frontend/revision.h: FORCE
	@(git describe || echo) | sed -e 's/.*/#define REV "\0"/' > $@_
	@diff -q $@_ $@ > /dev/null 2>&1 || cp $@_ $@
	@rm $@_
.PHONY: FORCE


$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,-Map=$@.map

spunull.so: plugins/spunull/spunull.c
	$(CC) $(CFLAGS) -shared -fPIC -ggdb -O2 -o $@ $^

plugins/gpu-gles/gpuGLES.so:
	make -C plugins/gpu-gles/

clean:
	$(RM) $(TARGET) $(OBJS)

# ----------- release -----------

PND_MAKE ?= $(HOME)/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

VER ?= $(shell git describe --abbrev=0 master)

rel: pcsx spunull.so plugins/gpu-gles/gpuGLES.so \
		pandora/pcsx.sh pandora/pcsx.pxml pandora/pcsx.png \
		pandora/picorestore pandora/readme.txt skin COPYING
	rm -rf out
	mkdir -p out/plugins
	cp -r $^ out/
	mv out/*.so out/plugins/
	$(PND_MAKE) -p pcsx_rearmed_$(VER).pnd -d out -x pandora/pcsx.pxml -i pandora/pcsx.png -c

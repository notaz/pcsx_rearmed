# Makefile for PCSX ReARMed

# default stuff goes here, so that config can override
TARGET = pcsx
CFLAGS += -Wall -ggdb -Ifrontend -ffast-math
LDLIBS += -lpthread -ldl -lpng -lz -lm
ifndef DEBUG
CFLAGS += -O2 -DNDEBUG
endif
CXXFLAGS += $(CFLAGS)
#DRC_DBG = 1
#PCNT = 1

all: config.mak target_ plugins_

ifneq ($(wildcard config.mak),)
config.mak: ./configure
	@echo $@ is out-of-date, running configure
	@sed -n "/.*Configured with/s/[^:]*: //p" $@ | sh
include config.mak
else
config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1
endif
-include Makefile.local

CC_LINK = $(CC)

# core
OBJS += libpcsxcore/cdriso.o libpcsxcore/cdrom.o libpcsxcore/cheat.o libpcsxcore/debug.o \
	libpcsxcore/decode_xa.o libpcsxcore/disr3000a.o libpcsxcore/mdec.o \
	libpcsxcore/misc.o libpcsxcore/plugins.o libpcsxcore/ppf.o libpcsxcore/psxbios.o \
	libpcsxcore/psxcommon.o libpcsxcore/psxcounters.o libpcsxcore/psxdma.o libpcsxcore/psxhle.o \
	libpcsxcore/psxhw.o libpcsxcore/psxinterpreter.o libpcsxcore/psxmem.o libpcsxcore/r3000a.o \
	libpcsxcore/sio.o libpcsxcore/socket.o libpcsxcore/spu.o
OBJS += libpcsxcore/gte.o libpcsxcore/gte_nf.o libpcsxcore/gte_divider.o
ifeq "$(ARCH)" "arm"
OBJS += libpcsxcore/gte_arm.o
endif
ifeq "$(HAVE_NEON)" "1"
OBJS += libpcsxcore/gte_neon.o
endif
libpcsxcore/gte.o libpcsxcore/gte_nf.o: CFLAGS += -fno-strict-aliasing
libpcsxcore/cdrom.o libpcsxcore/misc.o: CFLAGS += -Wno-pointer-sign
libpcsxcore/misc.o libpcsxcore/psxbios.o: CFLAGS += -Wno-nonnull

# dynarec
ifeq "$(USE_DYNAREC)" "1"
OBJS += libpcsxcore/new_dynarec/new_dynarec.o libpcsxcore/new_dynarec/linkage_arm.o
OBJS += libpcsxcore/new_dynarec/pcsxmem.o
endif
OBJS += libpcsxcore/new_dynarec/emu_if.o
libpcsxcore/new_dynarec/new_dynarec.o: libpcsxcore/new_dynarec/assem_arm.c \
	libpcsxcore/new_dynarec/pcsxmem_inline.c
libpcsxcore/new_dynarec/new_dynarec.o: CFLAGS += -Wno-all -Wno-pointer-sign
ifdef DRC_DBG
libpcsxcore/new_dynarec/emu_if.o: CFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -DDRC_DBG
endif
ifeq "$(DRC_CACHE_BASE)" "1"
libpcsxcore/new_dynarec/%.o: CFLAGS += -DBASE_ADDR_FIXED=1
endif

# spu
OBJS += plugins/dfsound/dma.o plugins/dfsound/freeze.o \
	plugins/dfsound/registers.o plugins/dfsound/spu.o
plugins/dfsound/spu.o: plugins/dfsound/adsr.c plugins/dfsound/reverb.c \
	plugins/dfsound/xa.c
ifeq "$(ARCH)" "arm"
OBJS += plugins/dfsound/arm_utils.o
endif
ifeq "$(SOUND_DRIVER)" "oss"
plugins/dfsound/%.o: CFLAGS += -DUSEOSS
OBJS += plugins/dfsound/oss.o
endif
ifeq "$(SOUND_DRIVER)" "alsa"
plugins/dfsound/%.o: CFLAGS += -DUSEALSA
OBJS += plugins/dfsound/alsa.o
LDLIBS += -lasound
endif
ifeq "$(SOUND_DRIVER)" "none"
OBJS += plugins/dfsound/nullsnd.o
endif

# builtin gpu
OBJS += plugins/gpulib/gpu.o plugins/gpulib/vout_pl.o
ifeq "$(HAVE_NEON)" "1"
OBJS += plugins/gpulib/cspace_neon.o
else
OBJS += plugins/gpulib/cspace.o
endif
ifeq "$(BUILTIN_GPU)" "neon"
OBJS += plugins/gpu_neon/psx_gpu_if.o plugins/gpu_neon/psx_gpu/psx_gpu_arm_neon.o
plugins/gpu_neon/psx_gpu_if.o: CFLAGS += -DNEON_BUILD -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
plugins/gpu_neon/psx_gpu_if.o: plugins/gpu_neon/psx_gpu/*.c
endif
ifeq "$(BUILTIN_GPU)" "peops"
# note: code is not safe for strict-aliasing? (Castlevania problems)
plugins/dfxvideo/gpulib_if.o: CFLAGS += -fno-strict-aliasing
plugins/dfxvideo/gpulib_if.o: plugins/dfxvideo/prim.c plugins/dfxvideo/soft.c
OBJS += plugins/dfxvideo/gpulib_if.o
endif
ifeq "$(BUILTIN_GPU)" "unai"
OBJS += plugins/gpulib/cspace.o
OBJS += plugins/gpu_unai/gpulib_if.o
ifeq "$(ARCH)" "arm"
OBJS += plugins/gpu_unai/gpu_arm.o
endif
plugins/gpu_unai/gpulib_if.o: CFLAGS += -DREARMED
CC_LINK = $(CXX)
endif

# cdrcimg
OBJS += plugins/cdrcimg/cdrcimg.o

# dfinput
OBJS += plugins/dfinput/main.o plugins/dfinput/pad.o plugins/dfinput/guncon.o

# gui
OBJS += frontend/main.o frontend/plugin.o
OBJS += frontend/common/readpng.o frontend/common/fonts.o
OBJS += frontend/linux/plat.o

ifeq "$(PLATFORM)" "generic"
OBJS += frontend/plat_sdl.o frontend/common/in_sdl.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "pandora"
OBJS += frontend/linux/fbdev.o
OBJS += frontend/plat_omap.o frontend/linux/xenv.o
OBJS += frontend/plat_pandora.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "caanoo"
OBJS += frontend/plat_pollux.o frontend/in_tsbutton.o frontend/blit320.o
OBJS += frontend/gp2x/in_gp2x.o frontend/warm/warm.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "maemo"
OBJS += maemo/hildon.o maemo/main.o
maemo/%.o: maemo/%.c
OBJS += frontend/plugin_lib.o
endif
ifeq "$(PLATFORM)" "libretro"
OBJS += frontend/libretro.o
endif
ifeq "$(USE_FRONTEND)" "1"
OBJS += frontend/menu.o frontend/linux/in_evdev.o
OBJS += frontend/common/input.o
OBJS += frontend/plugin_lib.o
ifeq "$(HAVE_TSLIB)" "1"
frontend/%.o: CFLAGS += -DHAVE_TSLIB
OBJS += frontend/pl_gun_ts.o
endif
else
CFLAGS += -DNO_FRONTEND
endif

ifdef X11
frontend/%.o: CFLAGS += -DX11
OBJS += frontend/xkb.o
endif
ifdef PCNT
CFLAGS += -DPCNT
endif
frontend/%.o: CFLAGS += -DIN_EVDEV
frontend/menu.o frontend/main.o frontend/plat_sdl.o: frontend/revision.h

libpcsxcore/gte_nf.o: libpcsxcore/gte.c
	$(CC) -c -o $@ $^ $(CFLAGS) -DFLAGLESS

frontend/revision.h: FORCE
	@(git describe || echo) | sed -e 's/.*/#define REV "\0"/' > $@_
	@diff -q $@_ $@ > /dev/null 2>&1 || cp $@_ $@
	@rm $@_
.PHONY: FORCE

%.o: %.S
	$(CC) $(CFLAGS) -c $^ -o $@

target_: $(TARGET)

$(TARGET): $(OBJS)
	$(CC_LINK) -o $@ $^ $(LDFLAGS) $(LDLIBS) -Wl,-Map=$@.map

clean: $(PLAT_CLEAN) clean_plugins
	$(RM) $(TARGET) $(OBJS) $(TARGET).map

ifneq ($(PLUGINS),)
plugins_: $(PLUGINS)

$(PLUGINS):
	make -C $(dir $@)

clean_plugins:
	make -C plugins/gpulib/ clean
	for dir in $(PLUGINS) ; do \
		$(MAKE) -C $$(dirname $$dir) clean; done
else
plugins_:
clean_plugins:
endif

# ----------- release -----------

VER ?= $(shell git describe master)

ifeq "$(PLATFORM)" "generic"
OUT = pcsx_rearmed_$(VER)

rel: pcsx $(PLUGINS) \
		frontend/pandora/skin readme.txt COPYING
	rm -rf $(OUT)
	mkdir -p $(OUT)/plugins
	mkdir -p $(OUT)/bios
	cp -r $^ $(OUT)/
	mv $(OUT)/*.so* $(OUT)/plugins/
	zip -9 -r $(OUT).zip $(OUT)
endif

ifeq "$(PLATFORM)" "pandora"
PND_MAKE ?= $(HOME)/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

rel: pcsx $(PLUGINS) \
		frontend/pandora/pcsx.sh frontend/pandora/pcsx.pxml.templ frontend/pandora/pcsx.png \
		frontend/pandora/picorestore frontend/pandora/skin readme.txt COPYING
	rm -rf out
	mkdir -p out/plugins
	cp -r $^ out/
	sed -e 's/%PR%/$(VER)/g' out/pcsx.pxml.templ > out/pcsx.pxml
	rm out/pcsx.pxml.templ
	mv out/*.so out/plugins/
	mv out/plugins/gpu_unai.so out/plugins/gpuPCSX4ALL.so
	mv out/plugins/gpu_gles.so out/plugins/gpuGLES.so
	mv out/plugins/gpu_peops.so out/plugins/gpuPEOPS.so
	$(PND_MAKE) -p pcsx_rearmed_$(VER).pnd -d out -x out/pcsx.pxml -i frontend/pandora/pcsx.png -c
endif

ifeq "$(PLATFORM)" "caanoo"
PLAT_CLEAN = caanoo_clean

caanoo_clean:
	$(RM) frontend/320240/pollux_set

rel: pcsx $(PLUGINS) \
		frontend/320240/caanoo.gpe frontend/320240/pcsx26.png \
		frontend/320240/pcsxb.png frontend/320240/skin \
		frontend/warm/bin/warm_2.6.24.ko frontend/320240/pollux_set \
		frontend/320240/pcsx_rearmed.ini frontend/320240/haptic_w.cfg \
		frontend/320240/haptic_s.cfg \
		readme.txt COPYING
	rm -rf out
	mkdir -p out/pcsx_rearmed/plugins
	cp -r $^ out/pcsx_rearmed/
	mv out/pcsx_rearmed/gpu_unai.so out/pcsx_rearmed/gpuPCSX4ALL.so
	mv out/pcsx_rearmed/gpu_gles.so out/pcsx_rearmed/gpuGLES.so
	mv out/pcsx_rearmed/*.so out/pcsx_rearmed/plugins/
	mv out/pcsx_rearmed/caanoo.gpe out/pcsx_rearmed/pcsx.gpe
	mv out/pcsx_rearmed/pcsx_rearmed.ini out/
	mkdir out/pcsx_rearmed/lib/
	cp ./lib/libbz2.so.1 out/pcsx_rearmed/lib/
	mkdir out/pcsx_rearmed/bios/
	cd out && zip -9 -r ../pcsx_rearmed_$(VER)_caanoo.zip *
endif

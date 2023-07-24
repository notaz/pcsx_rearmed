# Makefile for PCSX ReARMed

# default stuff goes here, so that config can override
TARGET ?= pcsx
CFLAGS += -Wall -Iinclude -ffast-math
ifeq ($(DEBUG), 1)
CFLAGS += -O0 -ggdb
else
ifeq ($(platform), $(filter $(platform), vita ctr))
CFLAGS += -O3 -DNDEBUG
else
CFLAGS += -O2 -DNDEBUG
endif
endif
CFLAGS += -DHAVE_MMAP=$(if $(NO_MMAP),0,1) \
	  -DHAVE_PTHREAD=$(if $(NO_PTHREAD),0,1) \
	  -DHAVE_POSIX_MEMALIGN=$(if $(NO_POSIX_MEMALIGN),0,1)
CXXFLAGS += $(CFLAGS)
#DRC_DBG = 1
#PCNT = 1

# Suppress minor warnings for dependencies
deps/%: CFLAGS += -Wno-unused -Wno-unused-function

all: config.mak target_ plugins_

ifndef NO_CONFIG_MAK
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
else # NO_CONFIG_MAK
config.mak:
endif

-include Makefile.local

CC_LINK ?= $(CC)
CC_AS ?= $(CC)
LDFLAGS += $(MAIN_LDFLAGS)
EXTRA_LDFLAGS ?= -Wl,-Map=$@.map
LDLIBS += $(MAIN_LDLIBS)
ifdef PCNT
CFLAGS += -DPCNT
endif

# core
OBJS += libpcsxcore/cdriso.o libpcsxcore/cdrom.o libpcsxcore/cheat.o libpcsxcore/database.o \
	libpcsxcore/decode_xa.o libpcsxcore/mdec.o \
	libpcsxcore/misc.o libpcsxcore/plugins.o libpcsxcore/ppf.o libpcsxcore/psxbios.o \
	libpcsxcore/psxcommon.o libpcsxcore/psxcounters.o libpcsxcore/psxdma.o libpcsxcore/psxhle.o \
	libpcsxcore/psxhw.o libpcsxcore/psxinterpreter.o libpcsxcore/psxmem.o libpcsxcore/r3000a.o \
	libpcsxcore/sio.o libpcsxcore/spu.o
OBJS += libpcsxcore/gte.o libpcsxcore/gte_nf.o libpcsxcore/gte_divider.o

ifeq ($(DEBUG), 1)
#OBJS += libpcsxcore/debug.o	libpcsxcore/socket.o libpcsxcore/disr3000a.o
endif

ifeq ($(WANT_ZLIB),1)
CFLAGS += -Ideps/libchdr/deps/zlib-1.2.11
OBJS += deps/libchdr/deps/zlib-1.2.11/adler32.o \
        deps/libchdr/deps/zlib-1.2.11/compress.o \
        deps/libchdr/deps/zlib-1.2.11/crc32.o \
        deps/libchdr/deps/zlib-1.2.11/deflate.o \
        deps/libchdr/deps/zlib-1.2.11/gzclose.o \
        deps/libchdr/deps/zlib-1.2.11/gzlib.o \
        deps/libchdr/deps/zlib-1.2.11/gzread.o \
        deps/libchdr/deps/zlib-1.2.11/gzwrite.o \
        deps/libchdr/deps/zlib-1.2.11/infback.o \
        deps/libchdr/deps/zlib-1.2.11/inffast.o \
        deps/libchdr/deps/zlib-1.2.11/inflate.o \
        deps/libchdr/deps/zlib-1.2.11/inftrees.o \
        deps/libchdr/deps/zlib-1.2.11/trees.o \
        deps/libchdr/deps/zlib-1.2.11/uncompr.o \
        deps/libchdr/deps/zlib-1.2.11/zutil.o
endif
ifeq "$(ARCH)" "arm"
OBJS += libpcsxcore/gte_arm.o
endif
ifeq "$(HAVE_NEON_ASM)" "1"
OBJS += libpcsxcore/gte_neon.o
endif
libpcsxcore/psxbios.o: CFLAGS += -Wno-nonnull

# dynarec
ifeq "$(DYNAREC)" "lightrec"
CFLAGS += -Ideps/lightning/include -Ideps/lightrec -Iinclude/lightning -Iinclude/lightrec \
		  -DLIGHTREC -DLIGHTREC_STATIC
LIGHTREC_CUSTOM_MAP ?= 0
LIGHTREC_CUSTOM_MAP_OBJ ?= libpcsxcore/lightrec/mem.o
LIGHTREC_THREADED_COMPILER ?= 0
CFLAGS += -DLIGHTREC_CUSTOM_MAP=$(LIGHTREC_CUSTOM_MAP) \
	  -DLIGHTREC_ENABLE_THREADED_COMPILER=$(LIGHTREC_THREADED_COMPILER)
ifeq ($(LIGHTREC_CUSTOM_MAP),1)
LDLIBS += -lrt
OBJS += $(LIGHTREC_CUSTOM_MAP_OBJ)
endif
ifeq ($(LIGHTREC_THREADED_COMPILER),1)
OBJS += deps/lightrec/recompiler.o \
	deps/lightrec/reaper.o
endif
OBJS += deps/lightrec/tlsf/tlsf.o
OBJS += libpcsxcore/lightrec/plugin.o
OBJS += deps/lightning/lib/jit_disasm.o \
		deps/lightning/lib/jit_memory.o \
		deps/lightning/lib/jit_names.o \
		deps/lightning/lib/jit_note.o \
		deps/lightning/lib/jit_print.o \
		deps/lightning/lib/jit_size.o \
		deps/lightning/lib/lightning.o \
		deps/lightrec/blockcache.o \
		deps/lightrec/constprop.o \
		deps/lightrec/disassembler.o \
		deps/lightrec/emitter.o \
		deps/lightrec/interpreter.o \
		deps/lightrec/lightrec.o \
		deps/lightrec/memmanager.o \
		deps/lightrec/optimizer.o \
		deps/lightrec/regcache.o
libpcsxcore/lightrec/mem.o: CFLAGS += -D_GNU_SOURCE
ifeq ($(MMAP_WIN32),1)
CFLAGS += -Iinclude/mman -I deps/mman
OBJS += deps/mman/mman.o
endif
else ifeq "$(DYNAREC)" "ari64"
OBJS += libpcsxcore/new_dynarec/new_dynarec.o
OBJS += libpcsxcore/new_dynarec/pcsxmem.o
 ifeq "$(ARCH)" "arm"
 OBJS += libpcsxcore/new_dynarec/linkage_arm.o
 libpcsxcore/new_dynarec/new_dynarec.o: libpcsxcore/new_dynarec/assem_arm.c
 else ifneq (,$(findstring $(ARCH),aarch64 arm64))
 OBJS += libpcsxcore/new_dynarec/linkage_arm64.o
 libpcsxcore/new_dynarec/new_dynarec.o: libpcsxcore/new_dynarec/assem_arm64.c
 else
 $(error no dynarec support for architecture $(ARCH))
 endif
else
CFLAGS += -DDRC_DISABLE
endif
OBJS += libpcsxcore/new_dynarec/emu_if.o libpcsxcore/new_dynarec/events.o
libpcsxcore/new_dynarec/new_dynarec.o: libpcsxcore/new_dynarec/pcsxmem_inline.c
ifdef DRC_DBG
libpcsxcore/new_dynarec/emu_if.o: CFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -DDRC_DBG
endif
ifeq "$(BASE_ADDR_DYNAMIC)" "1"
libpcsxcore/new_dynarec/%.o: CFLAGS += -DBASE_ADDR_DYNAMIC=1
endif

# spu
OBJS += plugins/dfsound/dma.o plugins/dfsound/freeze.o \
	plugins/dfsound/registers.o plugins/dfsound/spu.o \
	plugins/dfsound/out.o plugins/dfsound/nullsnd.o
plugins/dfsound/spu.o: plugins/dfsound/adsr.c plugins/dfsound/reverb.c \
	plugins/dfsound/xa.c
ifeq "$(ARCH)" "arm"
OBJS += plugins/dfsound/arm_utils.o
endif
ifeq "$(HAVE_C64_TOOLS)" "1"
plugins/dfsound/spu.o: CFLAGS += -DC64X_DSP
plugins/dfsound/spu.o: plugins/dfsound/spu_c64x.c
frontend/menu.o: CFLAGS += -DC64X_DSP
endif
ifneq ($(findstring oss,$(SOUND_DRIVERS)),)
plugins/dfsound/out.o: CFLAGS += -DHAVE_OSS
OBJS += plugins/dfsound/oss.o
endif
ifneq ($(findstring alsa,$(SOUND_DRIVERS)),)
plugins/dfsound/out.o: CFLAGS += -DHAVE_ALSA
OBJS += plugins/dfsound/alsa.o
LDLIBS += -lasound
endif
ifneq ($(findstring sdl,$(SOUND_DRIVERS)),)
plugins/dfsound/out.o: CFLAGS += -DHAVE_SDL
OBJS += plugins/dfsound/sdl.o
endif
ifneq ($(findstring pulseaudio,$(SOUND_DRIVERS)),)
plugins/dfsound/out.o: CFLAGS += -DHAVE_PULSE
OBJS += plugins/dfsound/pulseaudio.o
endif
ifneq ($(findstring libretro,$(SOUND_DRIVERS)),)
plugins/dfsound/out.o: CFLAGS += -DHAVE_LIBRETRO
endif

# builtin gpu
OBJS += plugins/gpulib/gpu.o plugins/gpulib/vout_pl.o
ifeq "$(BUILTIN_GPU)" "neon"
CFLAGS += -DGPU_NEON
OBJS += plugins/gpu_neon/psx_gpu_if.o
plugins/gpu_neon/psx_gpu_if.o: CFLAGS += -DNEON_BUILD -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
plugins/gpu_neon/psx_gpu_if.o: plugins/gpu_neon/psx_gpu/*.c
frontend/menu.o frontend/plugin_lib.o: CFLAGS += -DBUILTIN_GPU_NEON
 ifeq "$(HAVE_NEON_ASM)" "1"
 OBJS += plugins/gpu_neon/psx_gpu/psx_gpu_arm_neon.o
 else
 OBJS += plugins/gpu_neon/psx_gpu/psx_gpu_simd.o
 plugins/gpu_neon/psx_gpu_if.o: CFLAGS += -DSIMD_BUILD
 plugins/gpu_neon/psx_gpu/psx_gpu_simd.o: CFLAGS += -DSIMD_BUILD
 endif
endif
ifeq "$(BUILTIN_GPU)" "peops"
CFLAGS += -DGPU_PEOPS
# note: code is not safe for strict-aliasing? (Castlevania problems)
plugins/dfxvideo/gpulib_if.o: CFLAGS += -fno-strict-aliasing
plugins/dfxvideo/gpulib_if.o: plugins/dfxvideo/prim.c plugins/dfxvideo/soft.c
OBJS += plugins/dfxvideo/gpulib_if.o
ifeq "$(THREAD_RENDERING)" "1"
CFLAGS += -DTHREAD_RENDERING
OBJS += plugins/gpulib/gpulib_thread_if.o
endif
endif
ifeq "$(BUILTIN_GPU)" "unai"
CFLAGS += -DGPU_UNAI
CFLAGS += -DUSE_GPULIB=1
#CFLAGS += -DINLINE="static __inline__"
#CFLAGS += -Dasm="__asm__ __volatile__"
OBJS += plugins/gpu_unai/gpulib_if.o
ifeq "$(ARCH)" "arm"
OBJS += plugins/gpu_unai/gpu_arm.o
endif
ifeq "$(THREAD_RENDERING)" "1"
CFLAGS += -DTHREAD_RENDERING
OBJS += plugins/gpulib/gpulib_thread_if.o
endif
plugins/gpu_unai/gpulib_if.o: CFLAGS += -DREARMED -O3 
CC_LINK = $(CXX)
endif

# cdrcimg
OBJS += plugins/cdrcimg/cdrcimg.o

# libchdr
ifeq "$(HAVE_CHD)" "1"
CFLAGS += -Ideps/libchdr/include
CFLAGS += -Ideps/libchdr/include/libchdr
OBJS += deps/libchdr/deps/lzma-19.00/src/Alloc.o
OBJS += deps/libchdr/deps/lzma-19.00/src/Bra86.o
OBJS += deps/libchdr/deps/lzma-19.00/src/BraIA64.o
OBJS += deps/libchdr/deps/lzma-19.00/src/CpuArch.o
OBJS += deps/libchdr/deps/lzma-19.00/src/Delta.o
OBJS += deps/libchdr/deps/lzma-19.00/src/LzFind.o
OBJS += deps/libchdr/deps/lzma-19.00/src/Lzma86Dec.o
OBJS += deps/libchdr/deps/lzma-19.00/src/LzmaDec.o
OBJS += deps/libchdr/deps/lzma-19.00/src/LzmaEnc.o
OBJS += deps/libchdr/deps/lzma-19.00/src/Sort.o
OBJS += deps/libchdr/src/libchdr_bitstream.o
OBJS += deps/libchdr/src/libchdr_cdrom.o
OBJS += deps/libchdr/src/libchdr_chd.o
OBJS += deps/libchdr/src/libchdr_flac.o
OBJS += deps/libchdr/src/libchdr_huffman.o
CFLAGS += -Ideps/libchdr/deps/lzma-19.00/include
CFLAGS += -DHAVE_CHD -D_7ZIP_ST
LDFLAGS += -lm
endif

# dfinput
ifneq "$(PLATFORM)" "libretro"
OBJS += plugins/dfinput/main.o plugins/dfinput/pad.o plugins/dfinput/guncon.o
endif

# frontend/gui
OBJS += frontend/cspace.o
ifeq "$(HAVE_NEON_ASM)" "1"
OBJS += frontend/cspace_neon.o
frontend/cspace.o: CFLAGS += -DHAVE_bgr555_to_rgb565 -DHAVE_bgr888_to_x
else
ifeq "$(ARCH)" "arm"
OBJS += frontend/cspace_arm.o
frontend/cspace.o: CFLAGS += -DHAVE_bgr555_to_rgb565
endif
endif

ifeq "$(PLATFORM)" "generic"
OBJS += frontend/libpicofe/in_sdl.o
OBJS += frontend/libpicofe/plat_sdl.o
OBJS += frontend/libpicofe/plat_dummy.o
OBJS += frontend/libpicofe/linux/in_evdev.o
OBJS += frontend/plat_sdl.o
ifeq "$(HAVE_GLES)" "1"
OBJS += frontend/libpicofe/gl.o frontend/libpicofe/gl_platform.o
LDLIBS += $(LDLIBS_GLES)
frontend/libpicofe/plat_sdl.o: CFLAGS += -DHAVE_GLES $(CFLAGS_GLES)
frontend/libpicofe/gl_platform.o: CFLAGS += -DHAVE_GLES $(CFLAGS_GLES)
frontend/libpicofe/gl.o: CFLAGS += -DHAVE_GLES $(CFLAGS_GLES)
frontend/plat_sdl.o: CFLAGS += -DHAVE_GLES $(CFLAGS_GLES)
endif
USE_PLUGIN_LIB = 1
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "pandora"
OBJS += frontend/libpicofe/pandora/plat.o
OBJS += frontend/libpicofe/linux/fbdev.o frontend/libpicofe/linux/xenv.o
OBJS += frontend/libpicofe/linux/in_evdev.o
OBJS += frontend/plat_pandora.o frontend/plat_omap.o
frontend/main.o frontend/menu.o: CFLAGS += -include frontend/pandora/ui_feat.h
frontend/libpicofe/linux/plat.o: CFLAGS += -DPANDORA
USE_PLUGIN_LIB = 1
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "caanoo"
OBJS += frontend/libpicofe/gp2x/in_gp2x.o frontend/warm/warm.o
OBJS += frontend/libpicofe/gp2x/soc_pollux.o
OBJS += frontend/libpicofe/linux/in_evdev.o
OBJS += frontend/plat_pollux.o frontend/in_tsbutton.o frontend/blit320.o
frontend/main.o frontend/menu.o: CFLAGS += -include frontend/320240/ui_gp2x.h
USE_PLUGIN_LIB = 1
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "maemo"
OBJS += maemo/hildon.o maemo/main.o maemo/maemo_xkb.o frontend/pl_gun_ts.o
maemo/%.o: maemo/%.c
USE_PLUGIN_LIB = 1
LDFLAGS += $(shell pkg-config --libs hildon-1 libpulse)
CFLAGS += $(shell pkg-config --cflags hildon-1) -DHAVE_TSLIB
CFLAGS += `pkg-config --cflags glib-2.0 libosso dbus-1 hildon-fm-2`
LDFLAGS += `pkg-config --libs glib-2.0 libosso dbus-1 hildon-fm-2`
endif
ifeq "$(PLATFORM)" "libretro"
ifeq "$(USE_LIBRETRO_VFS)" "1"
OBJS += deps/libretro-common/compat/compat_posix_string.o
OBJS += deps/libretro-common/compat/fopen_utf8.o
OBJS += deps/libretro-common/encodings/compat_strl.o
OBJS += deps/libretro-common/encodings/encoding_utf.o
OBJS += deps/libretro-common/file/file_path.o
OBJS += deps/libretro-common/streams/file_stream.o
OBJS += deps/libretro-common/streams/file_stream_transforms.o
OBJS += deps/libretro-common/string/stdstring.o
OBJS += deps/libretro-common/time/rtime.o
OBJS += deps/libretro-common/vfs/vfs_implementation.o
CFLAGS += -DUSE_LIBRETRO_VFS
endif
OBJS += frontend/libretro.o
CFLAGS += -Ideps/libretro-common/include
CFLAGS += -DFRONTEND_SUPPORTS_RGB565
CFLAGS += -DHAVE_LIBRETRO

ifneq ($(DYNAREC),lightrec)
ifeq ($(MMAP_WIN32),1)
OBJS += libpcsxcore/memmap_win32.o
endif
endif
endif

ifeq "$(USE_PLUGIN_LIB)" "1"
OBJS += frontend/plugin_lib.o
OBJS += frontend/libpicofe/linux/plat.o
OBJS += frontend/libpicofe/readpng.o frontend/libpicofe/fonts.o
frontend/libpicofe/linux/plat.o: CFLAGS += -DNO_HOME_DIR
ifeq "$(HAVE_NEON_ASM)" "1"
OBJS += frontend/libpicofe/arm/neon_scale2x.o
OBJS += frontend/libpicofe/arm/neon_eagle2x.o
frontend/libpicofe/arm/neon_scale2x.o: CFLAGS += -DDO_BGR_TO_RGB
frontend/libpicofe/arm/neon_eagle2x.o: CFLAGS += -DDO_BGR_TO_RGB
endif
endif
ifeq "$(USE_FRONTEND)" "1"
OBJS += frontend/menu.o
OBJS += frontend/libpicofe/input.o
frontend/menu.o: frontend/libpicofe/menu.c
ifeq "$(HAVE_TSLIB)" "1"
frontend/%.o: CFLAGS += -DHAVE_TSLIB
OBJS += frontend/pl_gun_ts.o
endif
else
CFLAGS += -DNO_FRONTEND
endif

# misc
OBJS += frontend/main.o frontend/plugin.o


frontend/menu.o frontend/main.o: frontend/revision.h
frontend/plat_sdl.o frontend/libretro.o: frontend/revision.h

frontend/libpicofe/%.c:
	@echo "libpicofe module is missing, please run:"
	@echo "git submodule init && git submodule update"
	@exit 1

libpcsxcore/gte_nf.o: libpcsxcore/gte.c
	$(CC) -c -o $@ $^ $(CFLAGS) -DFLAGLESS

frontend/revision.h: FORCE
	@(git describe --always || echo) | sed -e 's/.*/#define REV "\0"/' > $@_
	@diff -q $@_ $@ > /dev/null 2>&1 || cp $@_ $@
	@rm $@_

%.o: %.S
	$(CC_AS) $(CFLAGS) -c $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

target_: $(TARGET)

$(TARGET): $(OBJS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJS)
else
	$(CC_LINK) -o $@ $^ $(LDFLAGS) $(LDLIBS) $(EXTRA_LDFLAGS)
endif

clean: $(PLAT_CLEAN) clean_plugins
	$(RM) $(TARGET) $(OBJS) $(TARGET).map frontend/revision.h

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

.PHONY: all clean target_ plugins_ clean_plugins FORCE

# ----------- release -----------

VER ?= $(shell git describe --always HEAD)

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

rel: pcsx plugins/dfsound/pcsxr_spu_area3.out $(PLUGINS) \
		frontend/pandora/pcsx.sh frontend/pandora/pcsx.pxml.templ frontend/pandora/pcsx.png \
		frontend/pandora/picorestore frontend/pandora/skin readme.txt COPYING
	rm -rf out
	mkdir -p out/plugins
	cp -r $^ out/
	sed -e 's/%PR%/$(VER)/g' out/pcsx.pxml.templ > out/pcsx.pxml
	rm out/pcsx.pxml.templ
	mv out/*.so out/plugins/
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
	mv out/pcsx_rearmed/*.so out/pcsx_rearmed/plugins/
	mv out/pcsx_rearmed/caanoo.gpe out/pcsx_rearmed/pcsx.gpe
	mv out/pcsx_rearmed/pcsx_rearmed.ini out/
	mkdir out/pcsx_rearmed/lib/
	cp ./lib/libbz2.so.1 out/pcsx_rearmed/lib/
	mkdir out/pcsx_rearmed/bios/
	cd out && zip -9 -r ../pcsx_rearmed_$(VER)_caanoo.zip *
endif

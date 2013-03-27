LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

APP_DIR := ../../src

ifneq ($(TARGET_ARCH_ABI),armeabi-v7a)
   NO_NEON_BUILD := 1
else
   NO_NEON_BUILD := $(NO_NEON)
endif

ifeq ($(NO_NEON_BUILD)$(TARGET_ARCH_ABI),1armeabi-v7a)
   LOCAL_MODULE    := retro-noneon
else
   LOCAL_MODULE    := retro
endif

ifeq ($(TARGET_ARCH),arm)
   LOCAL_ARM_MODE := arm

   LOCAL_CFLAGS += -DANDROID_ARM

   LOCAL_SRC_FILES += ../libpcsxcore/gte_arm.S

   # dynarec
   LOCAL_SRC_FILES += ../libpcsxcore/new_dynarec/new_dynarec.c ../libpcsxcore/new_dynarec/linkage_arm.S ../libpcsxcore/new_dynarec/emu_if.c ../libpcsxcore/new_dynarec/pcsxmem.c

   # spu
   LOCAL_SRC_FILES += ../plugins/dfsound/arm_utils.S

   # misc

   ifeq ($(NO_NEON_BUILD),1)
      # gpu
      LOCAL_CFLAGS += -DREARMED
      LOCAL_SRC_FILES += ../plugins/gpu_unai/gpulib_if.cpp ../plugins/gpu_unai/gpu_arm.s
      LOCAL_SRC_FILES += ../frontend/cspace_arm.S
   else
      LOCAL_ARM_NEON := true
      LOCAL_CFLAGS += -DNEON_BUILD -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
      LOCAL_SRC_FILES += ../libpcsxcore/gte_neon.S ../frontend/cspace_neon.S

      # gpu
      LOCAL_SRC_FILES += ../plugins/gpu_neon/psx_gpu_if.c ../plugins/gpu_neon/psx_gpu/psx_gpu_arm_neon.S
   endif
endif

ifeq ($(TARGET_ARCH),x86)
   LOCAL_CFLAGS += -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
   LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

ifneq ($(TARGET_ARCH),arm)
   # gpu
   LOCAL_CFLAGS += -DREARMED
   LOCAL_SRC_FILES += ../plugins/gpu_unai/gpulib_if.cpp
endif

$(shell cd "$(LOCAL_PATH)" && ((git describe || echo) | sed -e 's/.*/#define REV "\0"/' > ../frontend/revision.h_))
$(shell cd "$(LOCAL_PATH)" && (diff -q ../frontend/revision.h_ ../frontend/revision.h > /dev/null 2>&1 || cp ../frontend/revision.h_ ../frontend/revision.h))
$(shell cd "$(LOCAL_PATH)" && (rm ../frontend/revision.h_))

LOCAL_SRC_FILES += ../libpcsxcore/cdriso.c ../libpcsxcore/cdrom.c ../libpcsxcore/cheat.c ../libpcsxcore/debug.c \
   ../libpcsxcore/decode_xa.c ../libpcsxcore/disr3000a.c ../libpcsxcore/mdec.c \
   ../libpcsxcore/misc.c ../libpcsxcore/plugins.c ../libpcsxcore/ppf.c ../libpcsxcore/psxbios.c \
   ../libpcsxcore/psxcommon.c ../libpcsxcore/psxcounters.c ../libpcsxcore/psxdma.c ../libpcsxcore/psxhle.c \
   ../libpcsxcore/psxhw.c ../libpcsxcore/psxinterpreter.c ../libpcsxcore/psxmem.c ../libpcsxcore/r3000a.c \
   ../libpcsxcore/sio.c ../libpcsxcore/socket.c ../libpcsxcore/spu.c
LOCAL_SRC_FILES += ../libpcsxcore/gte.c ../libpcsxcore/gte_nf.c ../libpcsxcore/gte_divider.c

# spu
LOCAL_SRC_FILES += ../plugins/dfsound/dma.c ../plugins/dfsound/freeze.c \
   ../plugins/dfsound/registers.c ../plugins/dfsound/spu.c \
   ../plugins/dfsound/out.c ../plugins/dfsound/nullsnd.c

# builtin gpu
LOCAL_SRC_FILES += ../plugins/gpulib/gpu.c ../plugins/gpulib/vout_pl.c

# cdrcimg
LOCAL_SRC_FILES += ../plugins/cdrcimg/cdrcimg.c

# dfinput
LOCAL_SRC_FILES += ../plugins/dfinput/main.c ../plugins/dfinput/pad.c ../plugins/dfinput/guncon.c

# misc
LOCAL_SRC_FILES += ../frontend/main.c ../frontend/plugin.c ../frontend/cspace.c

# libretro
LOCAL_SRC_FILES += ../frontend/libretro.c

LOCAL_CFLAGS += -O3 -ffast-math -funroll-loops -DNDEBUG -D_FILE_OFFSET_BITS=64 -DHAVE_LIBRETRO -DNO_FRONTEND -DFRONTEND_SUPPORTS_RGB565
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
LOCAL_LDLIBS := -lz -llog

include $(BUILD_SHARED_LIBRARY)

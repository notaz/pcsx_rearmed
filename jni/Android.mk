LOCAL_PATH := $(call my-dir)

$(shell cd "$(LOCAL_PATH)" && ((git describe || echo) | sed -e 's/.*/#define REV "\0"/' > ../frontend/revision.h_))
$(shell cd "$(LOCAL_PATH)" && (diff -q ../frontend/revision.h_ ../frontend/revision.h > /dev/null 2>&1 || cp ../frontend/revision.h_ ../frontend/revision.h))
$(shell cd "$(LOCAL_PATH)" && (rm ../frontend/revision.h_))

ROOT_DIR     := $(LOCAL_PATH)/..
CORE_DIR     := $(ROOT_DIR)/libpcsxcore
SPU_DIR      := $(ROOT_DIR)/plugins/dfsound
GPU_DIR      := $(ROOT_DIR)/plugins/gpulib
CDR_DIR      := $(ROOT_DIR)/plugins/cdrcimg
INPUT_DIR    := $(ROOT_DIR)/plugins/dfinput
FRONTEND_DIR := $(ROOT_DIR)/frontend
NEON_DIR     := $(ROOT_DIR)/plugins/gpu_neon
UNAI_DIR     := $(ROOT_DIR)/plugins/gpu_unai
DYNAREC_DIR  := $(ROOT_DIR)/libpcsxcore/new_dynarec

# core
SOURCES_C := $(CORE_DIR)/cdriso.c \
             $(CORE_DIR)/cdrom.c \
             $(CORE_DIR)/cheat.c \
             $(CORE_DIR)/debug.c \
             $(CORE_DIR)/decode_xa.c \
             $(CORE_DIR)/disr3000a.c \
             $(CORE_DIR)/mdec.c \
             $(CORE_DIR)/misc.c \
             $(CORE_DIR)/plugins.c \
             $(CORE_DIR)/ppf.c \
             $(CORE_DIR)/psxbios.c \
             $(CORE_DIR)/psxcommon.c \
             $(CORE_DIR)/psxcounters.c \
             $(CORE_DIR)/psxdma.c \
             $(CORE_DIR)/psxhle.c \
             $(CORE_DIR)/psxhw.c \
             $(CORE_DIR)/psxinterpreter.c \
             $(CORE_DIR)/psxmem.c \
             $(CORE_DIR)/r3000a.c \
             $(CORE_DIR)/sio.c \
             $(CORE_DIR)/socket.c \
             $(CORE_DIR)/spu.c \
             $(CORE_DIR)/gte.c \
             $(CORE_DIR)/gte_nf.c \
             $(CORE_DIR)/gte_divider.c

# spu
SOURCES_C += $(SPU_DIR)/dma.c \
             $(SPU_DIR)/freeze.c \
             $(SPU_DIR)/registers.c \
             $(SPU_DIR)/spu.c \
             $(SPU_DIR)/out.c \
             $(SPU_DIR)/nullsnd.c

# gpu
SOURCES_C += $(GPU_DIR)/gpu.c \
             $(GPU_DIR)/vout_pl.c

# cdrcimg
SOURCES_C += $(CDR_DIR)/cdrcimg.c

# dfinput
SOURCES_C += $(INPUT_DIR)/main.c \
             $(INPUT_DIR)/pad.c \
             $(INPUT_DIR)/guncon.c

# frontend
SOURCES_C += $(FRONTEND_DIR)/main.c \
             $(FRONTEND_DIR)/plugin.c \
             $(FRONTEND_DIR)/cspace.c \
             $(FRONTEND_DIR)/libretro.c

# dynarec
SOURCES_C += $(DYNAREC_DIR)/backends/psx/emu_if.c

COREFLAGS := -ffast-math -funroll-loops -DHAVE_LIBRETRO -DNO_FRONTEND -DFRONTEND_SUPPORTS_RGB565 -DANDROID -DREARMED

ifeq ($(TARGET_ARCH),arm)
  SOURCES_ASM := $(CORE_DIR)/gte_arm.S \
                 $(SPU_DIR)/arm_utils.S \
                 $(DYNAREC_DIR)/arm/linkage_arm.S
  SOURCES_C   += $(DYNAREC_DIR)/new_dynarec.c \
                 $(DYNAREC_DIR)/backends/psx/pcsxmem.c
else
  COREFLAGS   += -DDRC_DISABLE
  SOURCES_ASM :=
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  COREFLAGS   += -DNEON_BUILD -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
  SOURCES_ASM += $(CORE_DIR)/gte_neon.S \
                 $(NEON_DIR)/psx_gpu/psx_gpu_arm_neon.S \
                 $(FRONTEND_DIR)/cspace_neon.S
  SOURCES_C   += $(NEON_DIR)/psx_gpu_if.c
else ifeq ($(TARGET_ARCH_ABI),armeabi)
  SOURCES_ASM += $(UNAI_DIR)/gpu_arm.S \
                 $(FRONTEND_DIR)/cspace_arm.S
  SOURCES_C += $(UNAI_DIR)/gpulib_if.cpp
else
  SOURCES_C += $(UNAI_DIR)/gpulib_if.cpp
endif

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE        := retro
LOCAL_SRC_FILES     := $(SOURCES_C) $(SOURCES_ASM)
LOCAL_CFLAGS        := $(COREFLAGS)
LOCAL_C_INCLUDES    := $(ROOT_DIR)/include
LOCAL_LDFLAGS       := -Wl,-version-script=$(FRONTEND_DIR)/link.T
LOCAL_LDLIBS        := -lz -llog
LOCAL_ARM_MODE      := arm

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  LOCAL_ARM_NEON  := true
endif
ifeq ($(TARGET_ARCH),arm)
  LOCAL_LDLIBS    += -Wl,-no-warn-shared-textrel
endif

include $(BUILD_SHARED_LIBRARY)

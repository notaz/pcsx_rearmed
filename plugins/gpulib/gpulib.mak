# depends on ARCH definition
# always adding gpulib to LDLIBS in case cspace is needed

LDFLAGS += -shared
ifeq "$(ARCH)" "arm"
 ARM_CORTEXA8 ?= 1
 ifeq "$(ARM_CORTEXA8)" "1"
  CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
  ASFLAGS += -mcpu=cortex-a8 -mfpu=neon
 else
  CFLAGS += -mcpu=arm926ej-s -mtune=arm926ej-s
  ASFLAGS += -mcpu=arm926ej-s -mfloat-abi=softfp
 endif
 EXT =
else
 CFLAGS += -m32
 LDFLAGS += -m32
 LDLIBS_GPULIB += `sdl-config --libs`
 EXT = .x86
endif
ifdef MAEMO
 CFLAGS += -DMAEMO
endif
ifdef DEBUG
 CFLAGS += -O0
endif

GPULIB_A = ../gpulib/gpulib$(EXT).a
LDLIBS += $(GPULIB_A)

ifdef BIN_STANDLALONE
TARGETS += $(BIN_STANDLALONE)$(EXT)
endif
ifdef BIN_GPULIB
TARGETS += $(BIN_GPULIB)$(EXT)
endif

all: $(GPULIB_A) $(TARGETS)

ifdef BIN_STANDLALONE
$(BIN_STANDLALONE)$(EXT): $(SRC) $(SRC_STANDALONE)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(LDLIBS_STANDALONE)
endif

ifdef BIN_GPULIB
$(BIN_GPULIB)$(EXT): $(SRC) $(SRC_GPULIB)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(LDLIBS_GPULIB)
endif

$(GPULIB_A):
	make -C ../gpulib/ all

clean:
	$(RM) $(TARGETS)

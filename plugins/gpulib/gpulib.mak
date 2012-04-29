# depends on ARCH definition
# always adding gpulib to deps in case cspace is needed
# users must include ../../config.mak

LDFLAGS += -shared
CFLAGS += $(PLUGIN_CFLAGS)
ifeq "$(ARCH)" "arm"
 EXT =
else
 #LDLIBS_GPULIB += `sdl-config --libs`
 EXT = .$(ARCH)
endif
ifdef DEBUG
 CFLAGS += -O0
endif

GPULIB_A = ../gpulib/gpulib$(EXT).a

ifdef BIN_STANDLALONE
TARGETS += $(BIN_STANDLALONE)$(EXT)
endif
ifdef BIN_GPULIB
TARGETS += $(BIN_GPULIB)$(EXT)
endif
CC_STANDLALONE = $(CC)
CC_GPULIB = $(CC)

all: ../../config.mak $(TARGETS)

ifdef BIN_STANDLALONE
ifneq ($(findstring .cpp,$(SRC_STANDALONE)),)
CC_STANDLALONE = $(CXX)
endif
$(BIN_STANDLALONE)$(EXT): $(SRC) $(SRC_STANDALONE) $(GPULIB_A)
	$(CC_STANDLALONE) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) $(LDLIBS_STANDALONE)
endif

ifdef BIN_GPULIB
ifneq ($(findstring .cpp,$(SRC_GPULIB)),)
CC_GPULIB = $(CXX)
endif
$(BIN_GPULIB)$(EXT): $(SRC) $(SRC_GPULIB) $(GPULIB_A)
	$(CC_GPULIB) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) $(LDLIBS_GPULIB)
endif

$(GPULIB_A):
	make -C ../gpulib/ all

clean:
	$(RM) $(TARGETS)

../../config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1

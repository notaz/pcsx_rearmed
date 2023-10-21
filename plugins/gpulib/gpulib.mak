# depends on ARCH definition
# always adding gpulib to deps (XXX might be no longer needed)
# users must include ../../config.mak

LDFLAGS += -shared -Wl,--no-undefined
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

ifdef BIN_STANDALONE
TARGETS += $(BIN_STANDALONE)
endif
ifdef BIN_GPULIB
TARGETS += $(BIN_GPULIB)
endif
CC_STANDLALONE = $(CC)
CC_GPULIB = $(CC)

WD = $(shell pwd)
PLUGINDIR = $(shell basename $(WD))

all: ../../config.mak $(TARGETS)

ifdef BIN_STANDALONE
ifneq ($(findstring .cpp,$(SRC_STANDALONE)),)
CC_STANDLALONE = $(CXX)
endif
$(BIN_STANDALONE): $(SRC) $(SRC_STANDALONE) $(GPULIB_A)
	$(CC_STANDLALONE) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) $(LDLIBS_STANDALONE)
	ln -fs $(PLUGINDIR)/$@ ../
endif

ifdef BIN_GPULIB
ifneq ($(findstring .cpp,$(SRC_GPULIB)),)
CC_GPULIB = $(CXX)
endif
$(BIN_GPULIB): $(SRC) $(SRC_GPULIB) $(GPULIB_A)
	$(CC_GPULIB) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) $(LDLIBS_GPULIB)
	ln -fs $(PLUGINDIR)/$@ ../
endif

$(GPULIB_A):
	$(MAKE) -C ../gpulib/ all

clean:
	$(RM) $(TARGETS)

../../config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1

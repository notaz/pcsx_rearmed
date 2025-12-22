# depends on ARCH definition
# always adding gpulib to deps (XXX might be no longer needed)
# users must include ../../config.mak

LDFLAGS += -shared
ifeq ($(GNU_LINKER),1)
LDFLAGS += -Wl,--no-undefined
endif
CFLAGS += $(PLUGIN_CFLAGS)
ifdef DEBUG
 CFLAGS += -O0
endif
ifndef NO_AUTODEPS
 CFLAGS += -MMD -MP
endif

GPULIB_A = ../gpulib/gpulib.$(ARCH).a

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
DEPS_GPULIB = $(SRC) $(SRC_GPULIB) $(GPULIB_A)
$(BIN_GPULIB): $(DEPS_GPULIB)
	$(CC_GPULIB) -o $@ $(CFLAGS) $(LDFLAGS) $(DEPS_GPULIB) $(LDLIBS) $(LDLIBS_GPULIB)
	ln -fs $(PLUGINDIR)/$@ ../

ifndef NO_AUTODEPS
$(BIN_GPULIB:.so=.d): ;
-include $(BIN_GPULIB:.so=.d)
endif
endif

$(GPULIB_A):
	$(MAKE) -C ../gpulib/ all

clean:
	$(RM) $(TARGETS) $(BIN_GPULIB:.so=.d)

../../config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1

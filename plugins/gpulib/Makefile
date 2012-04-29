CFLAGS += -ggdb -Wall
ifndef DEBUG
CFLAGS += -O2
endif

include ../../config.mak

OBJS += gpu.o

ifeq "$(ARCH)" "arm"
OBJS += vout_pl.o
EXT = a
else
#CFLAGS += `sdl-config --cflags`
#OBJS += vout_sdl.o
OBJS += vout_pl.o
EXT = $(ARCH).a
endif
ifeq "$(HAVE_NEON)" "1"
OBJS += cspace_neon.o
else
OBJS += cspace.o
endif
CFLAGS += $(PLUGIN_CFLAGS)

TARGET = gpulib.$(EXT)

all: ../../config.mak $(TARGET)

$(TARGET): $(OBJS)
	$(AR) crs $@ $^

clean:
	$(RM) $(TARGET) $(OBJS)

../../config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1

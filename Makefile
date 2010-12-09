#CROSS_COMPILE=
AS = $(CROSS_COMPILE)as
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld

CFLAGS += -ggdb -Ifrontend
LDFLAGS += -lz -lpthread -ldl -lpng
ifdef CROSS_COMPILE
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
OBJS += plugins/dfxvideo/cfg.o plugins/dfxvideo/fps.o plugins/dfxvideo/key.o plugins/dfxvideo/prim.o \
	plugins/dfxvideo/gpu.o plugins/dfxvideo/menu.o plugins/dfxvideo/soft.o plugins/dfxvideo/zn.o
ifdef X11
LDFLAGS += -lX11 -lXv
OBJS += plugins/dfxvideo/draw.o
else
CFLAGS += -D_MACGL # disables X in dfxvideo
OBJS += plugins/dfxvideo/draw_fb.o
endif

# gui
OBJS += gui/Config.o gui/Plugin.o

OBJS += frontend/main.o frontend/plugin.o frontend/plugin_lib.o
OBJS += frontend/omap.o frontend/menu.o
OBJS += frontend/linux/fbdev.o frontend/linux/in_evdev.o
OBJS += frontend/linux/plat.o frontend/linux/oshide.o
OBJS += frontend/common/fonts.o frontend/common/input.o frontend/common/readpng.o
ifdef CROSS_COMPILE
OBJS += frontend/arm_utils.o
endif
frontend/%.o: CFLAGS += -Wall -DIN_EVDEV

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,-Map=$@.map

spunull.so: plugins/spunull/spunull.c
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^

clean:
	$(RM) $(TARGET) $(OBJS)


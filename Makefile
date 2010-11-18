#CROSS_COMPILE=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld

CFLAGS += -Wall -ggdb -Ifrontend
LDFLAGS += -lz -lpthread -ldl
ifdef CROSS_COMPILE
CFLAGS += -O2 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=softfp -ffast-math
endif
TARGET = pcsx

all: $(TARGET)

# core
OBJS += libpcsxcore/cdriso.o libpcsxcore/cdrom.o libpcsxcore/cheat.o libpcsxcore/debug.o \
	libpcsxcore/decode_xa.o libpcsxcore/disr3000a.o libpcsxcore/gte.o libpcsxcore/mdec.o \
	libpcsxcore/misc.o libpcsxcore/plugins.o libpcsxcore/ppf.o libpcsxcore/psxbios.o \
	libpcsxcore/psxcommon.o libpcsxcore/psxcounters.o libpcsxcore/psxdma.o libpcsxcore/psxhle.o \
	libpcsxcore/psxhw.o libpcsxcore/psxinterpreter.o libpcsxcore/psxmem.o libpcsxcore/r3000a.o \
	libpcsxcore/sio.o libpcsxcore/socket.o libpcsxcore/spu.o
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
OBJS += frontend/linux/fbdev.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJS)


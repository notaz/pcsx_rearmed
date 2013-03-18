CC = $(CROSS_COMPILE)gcc
CFLAGS += -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
CFLAGS += -Wall -ggdb
CFLAGS += -fno-strict-aliasing

CFLAGS += `sdl-config --cflags`
LDFLAGS += `sdl-config --libs`

VPATH += ..

ifdef NEON
CFLAGS += -mcpu=cortex-a8 -mfpu=neon -DNEON_BUILD
ASFLAGS = $(CFLAGS)
OBJ += psx_gpu_arm_neon.o
endif
ifndef DEBUG
CFLAGS += -O2 -DNDEBUG
endif

OBJ += psx_gpu.o psx_gpu_parse.o psx_gpu_main.o

all: psx_gpu

psx_gpu: $(OBJ)

clean:
	$(RM) psx_gpu $(OBJ)

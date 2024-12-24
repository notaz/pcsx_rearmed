CC = $(CROSS_COMPILE)gcc
CFLAGS += -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP -DGPU_NEON
CFLAGS += -Wall -ggdb
CFLAGS += -fno-strict-aliasing

CFLAGS += `sdl-config --cflags`
LDLIBS += `sdl-config --libs`

ifdef NEON
CFLAGS += -mcpu=cortex-a8 -mfpu=neon -DNEON_BUILD
ASFLAGS = $(CFLAGS)
OBJ += ../psx_gpu_arm_neon.o
else
CFLAGS += -DNEON_BUILD -DSIMD_BUILD
OBJ += ../psx_gpu_simd.o
endif
ifndef DEBUG
CFLAGS += -O2 -DNDEBUG
endif

OBJ += ../psx_gpu_main.o ../../../gpulib/prim.o

all: psx_gpu

psx_gpu: $(OBJ)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) psx_gpu $(OBJ)

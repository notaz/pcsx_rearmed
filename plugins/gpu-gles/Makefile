##############################################################################
# MAKEFILE FOR PETE'S MESAGL GPU... just run "make"
##############################################################################

#PREFIX	= /mythtv/media/devel/toolchains/pandora/arm-2007q3/
CROSS_COMPILE ?= arm-none-linux-gnueabi-

#CC = $(PREFIX)/bin/$(CROSS_COMPILE)gcc
CC = $(CROSS_COMPILE)gcc

CFLAGS += -fPIC # -Wall
CFLAGS += -O3 -ffast-math -fomit-frame-pointer
CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8
# note: the below causes strange bugs/crashes
#CFLAGS += -mfloat-abi=softfp
CFLAGS += -DMAEMO_CHANGES # -DUSE_X11
CFLAGS += -I$(PREFIX)include

TARGET = gpuGLES.so
LDFLAGS += -shared -Wl,-soname,$(TARGET) -o $(TARGET)
LIB = -L$(PREFIX)lib -lGLES_CM -lX11 -lXau -lXdmcp

OBJ = gpuDraw.o gpuFps.o gpuPlugin.o gpuPrim.o gpuTexture.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIB)

clean:
	$(RM) $(TARGET) $(OBJ)

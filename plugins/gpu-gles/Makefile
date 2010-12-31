##############################################################################
# MAKEFILE FOR PETE'S MESAGL GPU... just run "make"
##############################################################################

##############################################################################
# 1. SETS (CCFLAGS3 is used)
##############################################################################

PREFIX	= /mythtv/media/devel/toolchains/pandora/arm-2007q3
TARGET	= arm-none-linux-gnueabi-

CC = $(PREFIX)/bin/$(TARGET)gcc

CCFLAGS = -fPIC -c -Wall -O3 -ffast-math -fomit-frame-pointer -DMAEMO_CHANGES -DUSE_X11


INCLUDE = -I$(PREFIX)/include
LINK = $(PREFIX)/bin/$(TARGET)gcc
LINKFLAGS = -shared -Wl,-soname,libgpuGLES.so -o libgpuGLES.so.1.0.0
OBJ =   gpuDraw.o gpuFps.o gpuPlugin.o gpuPrim.o gpuTexture.o

LIB = -L$(PREFIX)/lib -lGLES_CM -lX11 -lXau -lXdmcp -lstdc++

##############################################################################
# 2. MAIN RULE
##############################################################################

gpuPeopsMesaGL :	$(OBJ)
		$(LINK) $(LINKFLAGS) $(OBJ) $(LIB)

##############################################################################
# 3. GENERAL RULES
##############################################################################

%.o     : %.c
	$(CC) $(CCFLAGS) $(INCLUDE) $<

##############################################################################
# 4. SPECIFIC RULES
##############################################################################

clean:
	rm *.o libgpuGLES.so*
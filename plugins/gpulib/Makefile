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
CFLAGS += $(PLUGIN_CFLAGS)

# need to compile to another dir, same files are compiled
# into main binary without PIC
OBJS2 = $(patsubst %.o,obj/%.o,$(OBJS)) 

TARGET = gpulib.$(EXT)

all: ../../config.mak obj $(TARGET)

$(TARGET): $(OBJS2)
	$(AR) crs $@ $^

obj/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $^

obj/%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $^

clean:
	$(RM) $(TARGET) $(OBJS2)

obj:
	mkdir -p $@

../../config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1

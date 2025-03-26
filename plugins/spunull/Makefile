include ../../config.mak

TARGET = spunull.so
WD = $(shell pwd)
PLUGINDIR = $(shell basename $(WD))

all: ../../config.mak $(TARGET)

CFLAGS += $(PLUGIN_CFLAGS)

$(TARGET): spunull.c
	$(CC) $(CFLAGS) -shared -ggdb -O2 -o $@ $^
	ln -fs $(PLUGINDIR)/$(TARGET) ../

clean:
	$(RM) $(TARGET)

../../config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1

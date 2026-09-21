CFLAGS += -Wall -O2

TARGETS += mkm2cd psxcimg
psxcimg: LDLIBS += -lz

all: $(TARGETS)

clean:
	$(RM) $(TARGETS)

.PHONY: all clean

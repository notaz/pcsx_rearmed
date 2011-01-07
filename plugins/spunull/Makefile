CC = $(CROSS_COMPILE)gcc

all: spunull.so

-include Makefile.local

spunull.so: spunull.c
	$(CC) $(CFLAGS) -shared -fPIC -ggdb -O2 -o $@ $^

clean:
	$(RM) spunull.so

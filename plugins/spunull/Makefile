all: ../../config.mak spunull.so

include ../../config.mak

spunull.so: spunull.c
	$(CC) $(CFLAGS) -shared -fPIC -ggdb -O2 -o $@ $^

clean:
	$(RM) spunull.so

../../config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1

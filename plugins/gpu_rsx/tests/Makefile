CC ?= cc
TARGET ?= batch_test
NATIVE_TARGET ?= native_blend_test
JOURNAL_TARGET ?= journal_test
MAINTENANCE_TARGET ?= maintenance_test
CFLAGS += -std=c99 -O2 -Wall -Wextra -I../../../include -I../../../deps/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
$(TARGET): batch_test.c ../batch.h ../rsx.h ../libretro_rsx.h
	$(CC) $(CFLAGS) batch_test.c $(LDFLAGS) -o $@
$(NATIVE_TARGET): native_blend_test.c ../blend.h ../rsx.h ../libretro_rsx.h
	$(CC) $(CFLAGS) native_blend_test.c $(LDFLAGS) -o $@
$(JOURNAL_TARGET): journal_test.c ../journal.h ../rsx.h ../libretro_rsx.h
	$(CC) $(CFLAGS) journal_test.c $(LDFLAGS) -o $@
$(MAINTENANCE_TARGET): maintenance_test.c ../renderer.c ../journal.h ../command.c ../peops.c ../rsx.h
	$(CC) $(CFLAGS) -DGPU_RSX -ffunction-sections -fdata-sections maintenance_test.c ../command.c ../peops.c ../../gpulib/gpu.c $(LDFLAGS) -Wl,--gc-sections -lm -o $@
check: $(TARGET) $(NATIVE_TARGET) $(JOURNAL_TARGET) $(MAINTENANCE_TARGET)
	./$(TARGET)
	./$(NATIVE_TARGET)
	./$(JOURNAL_TARGET)
	./$(MAINTENANCE_TARGET)
clean:
	rm -f batch_test batch_test.exe native_blend_test native_blend_test.exe journal_test journal_test.exe
	rm -f maintenance_test maintenance_test.exe
.PHONY: check clean

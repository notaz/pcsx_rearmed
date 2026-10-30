#ifndef PS3_MEMORY_TEST_PPU_LV2_H
#define PS3_MEMORY_TEST_PPU_LV2_H
#include <sys/syscall.h>
#define lv2syscall1(n,a) mock_detect(n,a)
#define lv2syscall8(n,a,b,c,d,e,f,g,h) mock_allocate(n,a,b,c,d,e,f,g,h)
#define lv2syscall5(n,a,b,c,d,e) mock_free(n,a,b,c,d,e)
#endif

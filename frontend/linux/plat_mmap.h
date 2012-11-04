#include <stdlib.h>

void *plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed);
void *plat_mremap(void *ptr, size_t oldsize, size_t newsize);
void  plat_munmap(void *ptr, size_t size);

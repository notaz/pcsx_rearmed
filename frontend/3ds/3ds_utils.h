#ifndef _3DS_UTILS_H
#define _3DS_UTILS_H

#include <stdio.h>
#include <stdbool.h>

#define MEMOP_PROT      6
#define MEMOP_MAP       4
#define MEMOP_UNMAP     5

void* linearMemAlign(size_t size, size_t alignment);
void linearFree(void* mem);

int32_t svcDuplicateHandle(uint32_t* out, uint32_t original);
int32_t svcCloseHandle(uint32_t handle);
int32_t svcControlMemory(void* addr_out, void* addr0, void* addr1, uint32_t size, uint32_t op, uint32_t perm);
int32_t svcControlProcessMemory(uint32_t process, void* addr0, void* addr1, uint32_t size, uint32_t op, uint32_t perm);

int32_t threadCreate(void *(*entrypoint)(void*), void* arg, size_t stack_size, int32_t prio, int32_t affinity, bool detached);
int32_t threadJoin(int32_t thread, int64_t timeout_ns);
void threadFree(int32_t thread);
void threadExit(int32_t rc)  __attribute__((noreturn));

int32_t svcBackdoor(int32_t (*callback)(void));

#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)

void wait_for_input(void);

extern __attribute__((weak)) int  __ctr_svchax;

typedef int32_t (*ctr_callback_type)(void);

static inline void ctr_invalidate_ICache_kernel(void)
{
   __asm__ volatile(
      "cpsid aif\n\t"
      "mov r0, #0\n\t"
      "mcr p15, 0, r0, c7, c5, 0\n\t");
}

static inline void ctr_flush_DCache_kernel(void)
{
   __asm__ volatile(
      "cpsid aif\n\t"
      "mov r0, #0\n\t"
      "mcr p15, 0, r0, c7, c10, 0\n\t");
}

static inline void ctr_invalidate_ICache(void)
{
   svcBackdoor((ctr_callback_type)ctr_invalidate_ICache_kernel);
}

static inline void ctr_flush_DCache(void)
{
   svcBackdoor((ctr_callback_type)ctr_flush_DCache_kernel);
}


static inline void ctr_flush_invalidate_cache(void)
{
   ctr_flush_DCache();
   ctr_invalidate_ICache();
}


#endif // _3DS_UTILS_H

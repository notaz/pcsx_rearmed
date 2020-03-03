#ifndef _3DS_UTILS_H
#define _3DS_UTILS_H

#include <stdio.h>
#include <stdbool.h>

#define MEMOP_PROT      6
#define MEMOP_MAP       4
#define MEMOP_UNMAP     5

#define GET_VERSION_MAJOR(version)    ((version) >>24)

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

int32_t svcGetSystemInfo(int64_t* out, uint32_t type, int32_t param);

int32_t svcCreateSemaphore(uint32_t *sem, int32_t initial_count, uint32_t max_count);
int32_t svcReleaseSemaphore(int32_t *count, uint32_t sem, int32_t release_count);
int32_t svcWaitSynchronization(uint32_t handle, int64_t nanoseconds);

typedef int32_t LightLock;

void LightLock_Init(LightLock* lock);
void LightLock_Lock(LightLock* lock);
int LightLock_TryLock(LightLock* lock);
void LightLock_Unlock(LightLock* lock);

int32_t APT_CheckNew3DS(bool *out);

int32_t svcBackdoor(int32_t (*callback)(void));

#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)

void wait_for_input(void);

extern __attribute__((weak)) int  __ctr_svchax;

bool has_rosalina;

static void check_rosalina() {
  int64_t version;
  uint32_t major;

  has_rosalina = false;

  if (!svcGetSystemInfo(&version, 0x10000, 0)) {
     major = GET_VERSION_MAJOR(version);

     if (major >= 8)
       has_rosalina = true;
  }
}

void ctr_clear_cache(void);

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
   if (has_rosalina) {
      ctr_clear_cache();
   } else {
      ctr_flush_DCache();
      ctr_invalidate_ICache();
   }
}

#endif // _3DS_UTILS_H

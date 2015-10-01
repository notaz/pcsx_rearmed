#ifndef MMAN_H
#define MMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "3ds.h"
#include "stdlib.h"
#include "stdio.h"

#define PROT_READ       MEMPERM_READ
#define PROT_WRITE      MEMPERM_WRITE
#define PROT_EXEC       MEMPERM_EXECUTE
#define MAP_PRIVATE     2
#define MAP_ANONYMOUS   0x20

#define MAP_FAILED      ((void *)-1)

static inline void* mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
   (void)addr;
   (void)prot;
   (void)flags;
   (void)fd;
   (void)offset;

   void* addr_out;

   addr_out = malloc(len);
   if(!addr_out)
      return MAP_FAILED;

   return addr_out;
}

static inline int mprotect(void *addr, size_t len, int prot)
{
   extern int ctr_svchack_init_success;

   if(ctr_svchack_init_success)
   {
      uint32_t currentHandle;
      svcDuplicateHandle(&currentHandle, 0xFFFF8001);
      svcControlProcessMemory(currentHandle, (u32)addr, 0x0,
                              len, MEMOP_PROT, prot);
      svcCloseHandle(currentHandle);
      return 0;
   }

   printf("mprotect called without svcControlProcessMemory access !\n");
   return -1;
}

static inline int munmap(void *addr, size_t len)
{
   free(addr);
   return 0;

}

#ifdef __cplusplus
};
#endif

#endif // MMAN_H


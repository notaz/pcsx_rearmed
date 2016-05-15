#ifndef MMAN_H
#define MMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdlib.h"
#include "stdio.h"

#define PROT_READ       0b001
#define PROT_WRITE      0b010
#define PROT_EXEC       0b100
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
   (void)addr;
   (void)len;
   (void)prot;
   return 0;
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


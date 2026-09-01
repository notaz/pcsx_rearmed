#ifndef _VITA_SEMAPHORE_WRAP__
#define _VITA_SEMAPHORE_WRAP__

/* POSIX semaphore subset on top of SceKernel semaphores.  The vitasdk
 * sem_* live in libpthread, which RetroArch no longer links; this keeps
 * plugins/dfsound/spu.c off pthread on Vita like the 3DS wrapper does. */

#include <limits.h>
#include <psp2/kernel/threadmgr.h>

typedef SceUID sem_t;

static inline int sem_init(sem_t *sem, int pshared, unsigned int value)
{
   SceUID id = sceKernelCreateSema("pcsxr_sem", 0, (int)value, INT_MAX, NULL);
   (void)pshared;
   if (id < 0)
      return -1;
   *sem = id;
   return 0;
}

static inline int sem_post(sem_t *sem)
{
   return sceKernelSignalSema(*sem, 1) < 0 ? -1 : 0;
}

static inline int sem_wait(sem_t *sem)
{
   return sceKernelWaitSema(*sem, 1, NULL) < 0 ? -1 : 0;
}

static inline int sem_destroy(sem_t *sem)
{
   return sceKernelDeleteSema(*sem) < 0 ? -1 : 0;
}

#endif /* _VITA_SEMAPHORE_WRAP__ */

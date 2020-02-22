
#ifndef _3DS_PTHREAD_WRAP__
#define _3DS_PTHREAD_WRAP__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "3ds_utils.h"

#define CTR_PTHREAD_STACK_SIZE 0x10000
#define FALSE 0

typedef int32_t pthread_t;
typedef int pthread_attr_t;

typedef LightLock pthread_mutex_t;
typedef int pthread_mutexattr_t;

typedef struct {
  uint32_t semaphore;
  LightLock lock;
  uint32_t waiting;
} pthread_cond_t;

typedef int pthread_condattr_t;

static inline int pthread_create(pthread_t *thread,
      const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{
   int procnum = -2; // use default cpu
   bool isNew3DS;
   APT_CheckNew3DS(&isNew3DS);

   if (isNew3DS)
     procnum = 2;

   *thread = threadCreate(start_routine, arg, CTR_PTHREAD_STACK_SIZE, 0x25, procnum, FALSE);
   return 0;
}


static inline int pthread_join(pthread_t thread, void **retval)
{
   (void)retval;

   if(threadJoin(thread, INT64_MAX))
      return -1;

   threadFree(thread);

   return 0;
}


static inline void pthread_exit(void *retval)
{   
   (void)retval;

   threadExit(0);
}

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  LightLock_Init(mutex);
  return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
  LightLock_Lock(mutex);
  return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  LightLock_Unlock(mutex);
  return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
  return 0;
}

static inline int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  if (svcCreateSemaphore(&cond->semaphore, 0, 1))
    goto error;

  LightLock_Init(&cond->lock);
  cond->waiting = 0;
  return 0;

 error:
  svcCloseHandle(cond->semaphore);
  return -1;
}

static inline int pthread_cond_signal(pthread_cond_t *cond) {
  int32_t count;
  LightLock_Lock(&cond->lock);
  if (cond->waiting) {
    cond->waiting--;
    svcReleaseSemaphore(&count, cond->semaphore, 1);
  }
  LightLock_Unlock(&cond->lock);
  return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *lock) {
  LightLock_Lock(&cond->lock);
  cond->waiting++;
  LightLock_Unlock(lock);
  LightLock_Unlock(&cond->lock);
  svcWaitSynchronization(cond->semaphore, INT64_MAX);
  LightLock_Lock(lock);
  return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t *cond) {
  svcCloseHandle(cond->semaphore);
  return 0;
}


#endif //_3DS_PTHREAD_WRAP__


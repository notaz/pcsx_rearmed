
#ifndef _3DS_PTHREAD_WRAP__
#define _3DS_PTHREAD_WRAP__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "3ds_utils.h"

#define CTR_PTHREAD_STACK_SIZE 0x10000
#define FALSE 0

typedef struct {
  uint32_t semaphore;
  LightLock lock;
  uint32_t waiting;
} cond_t;

#if !defined(PTHREAD_SCOPE_PROCESS)
/* An earlier version of devkitARM does not define the pthread types. Can remove in r54+. */

typedef uint32_t pthread_t;
typedef int pthread_attr_t;

typedef LightLock pthread_mutex_t;
typedef int pthread_mutexattr_t;

typedef uint32_t pthread_cond_t;
typedef int pthread_condattr_t;

#endif

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
  cond_t *cond_data = calloc(1, sizeof(cond_t));
  if (!cond_data)
    goto error;

  if (svcCreateSemaphore(&cond_data->semaphore, 0, 1))
    goto error;

  LightLock_Init(&cond_data->lock);
  cond_data->waiting = 0;
  *cond = cond_data;
  return 0;

 error:
  svcCloseHandle(cond_data->semaphore);
  if (cond_data)
    free(cond_data);
  return -1;
}

static inline int pthread_cond_signal(pthread_cond_t *cond) {
  int32_t count;
  cond_t *cond_data = (cond_t *)*cond;
  LightLock_Lock(&cond_data->lock);
  if (cond_data->waiting) {
    cond_data->waiting--;
    svcReleaseSemaphore(&count, cond_data->semaphore, 1);
  }
  LightLock_Unlock(&cond_data->lock);
  return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *lock) {
  cond_t *cond_data = (cond_t *)*cond;
  LightLock_Lock(&cond_data->lock);
  cond_data->waiting++;
  LightLock_Unlock(lock);
  LightLock_Unlock(&cond_data->lock);
  svcWaitSynchronization(cond_data->semaphore, INT64_MAX);
  LightLock_Lock(lock);
  return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t *cond) {
  if (*cond) {
    cond_t *cond_data = (cond_t *)*cond;

    svcCloseHandle(cond_data->semaphore);
    free(*cond);
  }
  *cond = 0;
  return 0;
}


#endif //_3DS_PTHREAD_WRAP__


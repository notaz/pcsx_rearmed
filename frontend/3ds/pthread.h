
#ifndef _3DS_PTHREAD_WRAP__
#define _3DS_PTHREAD_WRAP__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "3ds_utils.h"

#define CTR_PTHREAD_STACK_SIZE 0x10000

typedef struct
{
   int32_t handle;
   uint32_t* stack;
}pthread_t;
typedef int pthread_attr_t;

static inline int pthread_create(pthread_t *thread,
      const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{

   thread->stack =  linearMemAlign(CTR_PTHREAD_STACK_SIZE, 8);

   svcCreateThread(&thread->handle, start_routine, arg,
                   (uint32_t*)((uint32_t)thread->stack + CTR_PTHREAD_STACK_SIZE),
                   0x25, 1);

   return 1;
}


static inline int pthread_join(pthread_t thread, void **retval)
{
   (void)retval;

   if(svcWaitSynchronization(thread.handle, INT64_MAX))
      return -1;

   linearFree(thread.stack);

   return 0;
}


static inline void pthread_exit(void *retval)
{   
   (void)retval;

   svcExitThread();
}


#endif //_3DS_PTHREAD_WRAP__


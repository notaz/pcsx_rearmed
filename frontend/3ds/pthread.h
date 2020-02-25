
#ifndef _3DS_PTHREAD_WRAP__
#define _3DS_PTHREAD_WRAP__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "3ds_utils.h"

#define CTR_PTHREAD_STACK_SIZE 0x10000

typedef int32_t pthread_t;
typedef int pthread_attr_t;

static inline int pthread_create(pthread_t *thread,
      const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{
   thread = threadCreate(start_routine, arg, CTR_PTHREAD_STACK_SIZE, 0x25, -2, FALSE);
   return 1;
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


#endif //_3DS_PTHREAD_WRAP__


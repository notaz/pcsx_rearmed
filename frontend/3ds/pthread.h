
#ifndef _3DS_PTHREAD_WRAP__
#define _3DS_PTHREAD_WRAP__

#include "3ds.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"


#define CTR_PTHREAD_STACK_SIZE 0x10000

typedef struct
{
   Handle handle;
   u32* stack;
}pthread_t;
typedef int pthread_attr_t;

//#ifndef DEBUG_HOLD
//#include "stdio.h"
//void wait_for_input(void);

//#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)
//#endif

static inline int pthread_create(pthread_t *thread,
      const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{

//   DEBUG_HOLD();

   thread->stack =  linearMemAlign(CTR_PTHREAD_STACK_SIZE, 8);

   svcCreateThread(&thread->handle, (ThreadFunc)start_routine,arg,
                   (u32*)((u32)thread->stack + CTR_PTHREAD_STACK_SIZE),
                   0x25, 1);

   return 1;
}


static inline int pthread_join(pthread_t thread, void **retval)
{
   (void)retval;

//   DEBUG_HOLD();
   if(svcWaitSynchronization(thread.handle, INT64_MAX))
      return -1;

   linearFree(thread.stack);

   return 0;
}


static inline void pthread_exit(void *retval)
{   
   (void)retval;

//   DEBUG_HOLD();
   svcExitThread();
}

//#undef DEBUG_HOLD

#endif //_3DS_PTHREAD_WRAP__


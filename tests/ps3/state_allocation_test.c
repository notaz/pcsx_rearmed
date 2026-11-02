#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static long cache_size;
static void *live_allocation;
static unsigned int allocation_calls;
static unsigned int free_calls;
static int allocation_failure;
static int invalid_free;

static long test_sysconf(int name)
{
   (void)name;
   return cache_size;
}

static void *test_malloc(size_t size)
{
   allocation_calls++;
   if (allocation_failure)
      return NULL;
   live_allocation = malloc(size);
   return live_allocation;
}

static void test_free(void *ptr)
{
   if (ptr != live_allocation)
   {
      invalid_free = 1;
      return;
   }
   free_calls++;
   live_allocation = NULL;
   free(ptr);
}

#ifndef _SC_LEVEL1_DCACHE_SIZE
#define _SC_LEVEL1_DCACHE_SIZE 1
#endif
#define sysconf test_sysconf
#define malloc test_malloc
#define free test_free
#include "../../deps/lightrec/lightrec.c"
#undef free
#undef malloc
#undef sysconf

int main(void)
{
   static const struct {
      long cache;
      size_t alignment;
   } cases[] = {
      { -1, 32768 }, { 0, 32768 },
      { 1, sizeof(void *) }, { 2, sizeof(void *) },
      { 4, sizeof(void *) > 4 ? sizeof(void *) : 4 },
      { 8, sizeof(void *) > 8 ? sizeof(void *) : 8 },
      { 16, 16 }, { 1024, 1024 }, { 16384, 16384 },
      { 32768, 32768 }, { 49152, 32768 }, { 65536, 65536 }
   };
   struct lightrec_state *state;
   const unsigned char *bytes;
   size_t i, j, size;
   unsigned int width, count = 0;

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      cache_size = cases[i].cache;
      for (width = 0; width < 2; width++)
      {
         state = lightrec_alloc_state(width != 0);
         if (!state || state->with_32bit_lut != (width != 0))
            return 1;
         if (((uintptr_t)state - 0x400) % cases[i].alignment)
            return 2;
         state->with_32bit_lut = false;
         bytes = (const unsigned char *)state;
         size = sizeof(*state) + (width ? 4 : sizeof(void *)) * CODE_LUT_SIZE;
         for (j = 0; j < size; j++)
            if (bytes[j])
               return 3;
         lightrec_free_state(state);
         if (invalid_free || live_allocation)
            return 4;
         count++;
      }
   }
   allocation_failure = 1;
   if (lightrec_alloc_state(false) || allocation_calls != count + 1 ||
       free_calls != count || live_allocation)
      return 5;
   printf("PASS: state alignment, zeroed LUTs, invalid cache sizes and allocation failures (%u cases)\n",
         count + 1);
   return 0;
}

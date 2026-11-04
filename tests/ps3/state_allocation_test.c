#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef _WIN32
#include <malloc.h>
#endif

static long cache_size;
static void *live_allocation;
static unsigned int allocation_calls;
static unsigned int free_calls;
static int allocation_failure;
static int invalid_free;
static int invalid_size;

static long test_sysconf(int name)
{
   (void)name;
   return cache_size;
}

static void *test_aligned_alloc(size_t alignment, size_t size)
{
   allocation_calls++;
   if (size % alignment)
   {
      invalid_size = 1;
      return NULL;
   }
   if (allocation_failure)
      return NULL;
#ifdef _WIN32
   live_allocation = _aligned_malloc(size, alignment);
#else
   live_allocation = aligned_alloc(alignment, size);
#endif
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
#ifdef _WIN32
   _aligned_free(ptr);
#else
   free(ptr);
#endif
}

#ifndef _SC_LEVEL1_DCACHE_SIZE
#define _SC_LEVEL1_DCACHE_SIZE 1
#endif
#define sysconf test_sysconf
#define LIGHTREC_ALIGNED_ALLOC test_aligned_alloc
#define LIGHTREC_ALIGNED_FREE test_free
#include "../../deps/lightrec/lightrec.c"
#undef LIGHTREC_ALIGNED_FREE
#undef LIGHTREC_ALIGNED_ALLOC
#undef sysconf

int main(void)
{
   static const struct {
      long cache;
      size_t alignment;
   } cases[] = {
      { -EINVAL, 32768 }, { -1, 32768 }, { 0, 32768 },
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
         if (!state || invalid_size || state->with_32bit_lut != (width != 0))
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

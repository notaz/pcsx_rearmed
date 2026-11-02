#include <stdint.h>
#include <stdio.h>
#include "../../libpcsxcore/lightrec/ps3.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

unsigned char *lightrec_ps3_code;
static unsigned int allocation_calls;
static unsigned int log_init_calls;
static unsigned int open_calls;
static int allocation_result;

int lightrec_ps3_init_code(FILE *log)
{
   (void)log;
   allocation_calls++;
   if (allocation_result)
      return allocation_result;
#ifdef _WIN32
   lightrec_ps3_code = VirtualAlloc(NULL, 8388608, MEM_COMMIT | MEM_RESERVE,
      PAGE_EXECUTE_READWRITE);
#else
   lightrec_ps3_code = mmap(NULL, 8388608, PROT_READ | PROT_WRITE | PROT_EXEC,
      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (lightrec_ps3_code == MAP_FAILED)
      lightrec_ps3_code = NULL;
#endif
   return lightrec_ps3_code ? 0 : -1;
}

void lightrec_ps3_free_code(void)
{
   if (!lightrec_ps3_code)
      return;
#ifdef _WIN32
   VirtualFree(lightrec_ps3_code, 0, MEM_RELEASE);
#else
   munmap(lightrec_ps3_code, 8388608);
#endif
   lightrec_ps3_code = NULL;
}

void lightrec_code_inv(void *ptr, uint32_t len)
{
   (void)ptr;
   (void)len;
}

void lightrec_ps3_log_init(void)
{
   log_init_calls++;
}

FILE *lightrec_ps3_open_log(const char *name, const char *mode)
{
   (void)name;
   (void)mode;
   open_calls++;
   return NULL;
}

int main(void)
{
   int result = lightrec_ps3_init();
#if PS3_LIGHTREC_SELFTEST
   unsigned int i;
   unsigned char code = 0;
#endif
   if (result || !lightrec_ps3_code || allocation_calls != 1)
      return 1;
#if PS3_LIGHTREC_SELFTEST
   for (i = 0; i < 64; i++)
      code |= lightrec_ps3_code[i];
   if (!code)
      return 5;
#endif
   if (log_init_calls != PS3_LIGHTREC_LOG ||
       open_calls != (1 + PS3_LIGHTREC_SELFTEST) * PS3_LIGHTREC_LOG)
      return 2;
   lightrec_ps3_free_code();
   allocation_result = -42;
   if (lightrec_ps3_init() != -42 || lightrec_ps3_code)
      return 3;
   if (allocation_calls != 2 || log_init_calls != 2 * PS3_LIGHTREC_LOG ||
       open_calls != (2 + PS3_LIGHTREC_SELFTEST) * PS3_LIGHTREC_LOG)
      return 4;
   puts("PASS: optional self-test, unavailable log files and allocation error propagation");
   return 0;
}

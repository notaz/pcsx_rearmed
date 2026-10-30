#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Host tests do not execute PPC cache instructions. */
#define __asm__
#define volatile(code)
#include "../../libpcsxcore/lightrec/ps3.c"
#undef volatile
#undef __asm__
#ifdef __PSL1GHT__
#undef sys_process_getpid
#endif

unsigned char lightrec_ps3_text_code[8388608];
static unsigned char allocated_code[8388608];
int mock_result;
static int hen_result;
static int allocation_result;
static int free_result;
static unsigned detection_calls;
static unsigned allocation_calls;
static unsigned free_calls;

int sys_process_getpid(void)
{
   return 42;
}

int sysProcessGetPid(void)
{
   return 42;
}

void mock_detect(unsigned n, unsigned opcode)
{
   assert(n == 8 && opcode == 0x1337);
   detection_calls++;
   mock_result = hen_result;
}

void mock_allocate(unsigned n, unsigned a, unsigned b, unsigned pid,
      unsigned size, unsigned page_size, unsigned flags, unsigned executable,
      uintptr_t table)
{
   uint64_t *pages = (uint64_t *)table;
   assert(n == 8 && a == 0x7777 && b == 0x33 && pid == 42);
   assert(size == 8388608 && !page_size && flags == 0x2f && executable == 1);
   allocation_calls++;
   mock_result = allocation_result;
   if (!mock_result)
   {
      pages[0] = (uintptr_t)allocated_code;
      pages[1] = 0x10000;
   }
}

void mock_free(unsigned n, unsigned a, unsigned b, unsigned pid,
      unsigned flags, uintptr_t table)
{
   uint64_t *pages = (uint64_t *)table;
   assert(n == 8 && a == 0x7777 && b == 0x34 && pid == 42 && flags == 0x2f);
   assert(pages[0] == (uintptr_t)allocated_code && pages[1] == 0x10000);
   free_calls++;
   mock_result = free_result;
}

int main(void)
{
   hen_result = 0x1337;
   memset(lightrec_ps3_text_code, 0xa5, sizeof(lightrec_ps3_text_code));
   assert(lightrec_ps3_init_code(NULL) == 0);
   assert(lightrec_ps3_code == lightrec_ps3_text_code);
   assert(!lightrec_ps3_code[0] && !lightrec_ps3_code[8388607]);
   assert(detection_calls == 1 && !allocation_calls && !free_calls);
   assert(lightrec_ps3_init_code(NULL) == 0 && detection_calls == 1);
   lightrec_ps3_free_code();
   assert(!lightrec_ps3_code && !free_calls);
   hen_result = -1;
   allocation_result = -42;
   assert(lightrec_ps3_init_code(NULL) == -42);
   assert(!lightrec_ps3_code && allocation_calls == 1);
   allocation_result = 0;
   memset(allocated_code, 0xa5, sizeof(allocated_code));
   assert(lightrec_ps3_init_code(NULL) == 0);
   assert(lightrec_ps3_code == allocated_code);
   assert(!lightrec_ps3_code[0] && !lightrec_ps3_code[8388607]);
   assert(allocation_calls == 2);
   assert(lightrec_ps3_init_code(NULL) == 0 && allocation_calls == 2);
   free_result = -42;
   lightrec_ps3_free_code();
   assert(lightrec_ps3_code == allocated_code && free_calls == 1);
   free_result = 0;
   lightrec_ps3_free_code();
   assert(!lightrec_ps3_code && free_calls == 2);
   assert(!page_table[0] && !page_table[1]);
   lightrec_ps3_free_code();
   assert(free_calls == 2);
   hen_result = 0x1337;
   assert(lightrec_ps3_init_code(NULL) == 0);
   lightrec_ps3_free_code();
   assert(!lightrec_ps3_code && free_calls == 2 && allocation_calls == 2);
   puts("PASS: HEN executable text, CFW PS3MAPI, reinitialization and failure handling");
   return 0;
}

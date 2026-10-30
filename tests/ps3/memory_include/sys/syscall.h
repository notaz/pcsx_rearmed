#ifndef PS3_MEMORY_TEST_SYSCALL_H
#define PS3_MEMORY_TEST_SYSCALL_H
#include <stdint.h>
extern int mock_result;
void mock_detect(unsigned n, unsigned opcode);
void mock_allocate(unsigned n, unsigned a, unsigned b, unsigned pid,
      unsigned size, unsigned page_size, unsigned flags, unsigned executable,
      uintptr_t table);
void mock_free(unsigned n, unsigned a, unsigned b, unsigned pid,
      unsigned flags, uintptr_t table);
#ifndef __PSL1GHT__
#define system_call_1(n,a) mock_detect(n,a)
#define system_call_8(n,a,b,c,d,e,f,g,h) mock_allocate(n,a,b,c,d,e,f,g,h)
#define system_call_5(n,a,b,c,d,e) mock_free(n,a,b,c,d,e)
#endif
#define return_to_user_prog(type) return (type)mock_result
#endif

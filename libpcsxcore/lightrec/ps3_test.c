#include <lightning.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#if defined(PS3_LIGHTREC_CORE) || defined(LIGHTREC_SMOKE_TEST)
#include <lightrec.h>
#endif

#ifdef __PS3__
#include "ps3.h"
#endif

static void test_log(FILE *log, const char *format, ...)
{
#if !defined(__PS3__) || PS3_LIGHTREC_LOG
   va_list args;
   if (!log)
      return;
   va_start(args, format);
   vfprintf(log, format, args);
   va_end(args);
   fflush(log);
#else
   (void)log;
   (void)format;
#endif
}

static volatile int bias = 7;

static int callback(int value)
{
   return value * 3 + bias;
}

static int callback9(int a, int b, int c, int d, int e,
      int f, int g, int h, int i)
{
   return a + b + c + d + e + f + g + h + i + bias;
}

#if defined(PS3_LIGHTREC_CORE) || defined(LIGHTREC_SMOKE_TEST)
static void test_cop2(struct lightrec_registers_cp2 *regs, uint32_t op)
{
}

static lightrec_gte_handler_t test_cop2_handler(uint32_t op, _Bool flagless)
{
   return test_cop2;
}

static void test_enable_ram(struct lightrec_state *state, _Bool enable)
{
}

static void write_opcode(unsigned char *ram, unsigned int offset,
      uint32_t opcode)
{
   ram[offset] = (unsigned char)opcode;
   ram[offset + 1] = (unsigned char)(opcode >> 8);
   ram[offset + 2] = (unsigned char)(opcode >> 16);
   ram[offset + 3] = (unsigned char)(opcode >> 24);
}

static int run_lightrec_test(FILE *log)
{
   struct lightrec_mem_map maps[PSX_MAP_UNKNOWN];
   struct lightrec_ops ops;
   struct lightrec_state *state;
   struct lightrec_registers *regs;
   unsigned char *ram;
   unsigned char *bios;
   unsigned char *hardware;
   unsigned char *parallel;
   unsigned int i;
   uint32_t pc;
   uint32_t flags;
   int result = 20;

   ram = calloc(1, 0x200000);
   bios = calloc(1, 0x80000);
   hardware = calloc(1, 0x9000);
   parallel = calloc(1, 0x10000);
   if (!ram || !bios || !hardware || !parallel)
      goto cleanup;

   memset(maps, 0, sizeof(maps));
   memset(&ops, 0, sizeof(ops));
   ops.cop2_hdl = test_cop2_handler;
   ops.enable_ram = test_enable_ram;
   maps[PSX_MAP_KERNEL_USER_RAM].address = ram;
   maps[PSX_MAP_KERNEL_USER_RAM].length = 0x200000;
   maps[PSX_MAP_BIOS].pc = 0x1fc00000;
   maps[PSX_MAP_BIOS].address = bios;
   maps[PSX_MAP_BIOS].length = 0x80000;
   maps[PSX_MAP_SCRATCH_PAD].pc = 0x1f800000;
   maps[PSX_MAP_SCRATCH_PAD].address = hardware;
   maps[PSX_MAP_SCRATCH_PAD].length = 0x400;
   maps[PSX_MAP_HW_REGISTERS].pc = 0x1f801000;
   maps[PSX_MAP_HW_REGISTERS].address = hardware + 0x1000;
   maps[PSX_MAP_HW_REGISTERS].length = 0x8000;
   maps[PSX_MAP_PARALLEL_PORT].pc = 0x1f000000;
   maps[PSX_MAP_PARALLEL_PORT].address = parallel;
   maps[PSX_MAP_PARALLEL_PORT].length = 0x10000;
   for (i = 0; i < 3; i++)
   {
      maps[PSX_MAP_MIRROR1 + i].pc = (i + 1) * 0x200000;
      maps[PSX_MAP_MIRROR1 + i].length = 0x200000;
      maps[PSX_MAP_MIRROR1 + i].address = ram;
      maps[PSX_MAP_MIRROR1 + i].mirror_of = &maps[PSX_MAP_KERNEL_USER_RAM];
   }
#ifdef __PS3__
   {
      extern void lightrec_code_inv(void *, uint32_t);
      maps[PSX_MAP_CODE_BUFFER].address = lightrec_ps3_code;
      maps[PSX_MAP_CODE_BUFFER].length = 8388608;
      ops.code_inv = lightrec_code_inv;
   }
#endif

   write_opcode(ram, 0x10000, 0x24020029); /* addiu v0, zero, 41 */
   write_opcode(ram, 0x10004, 0x08004008); /* j 0x10020 */
   write_opcode(ram, 0x10008, 0x24420001); /* addiu v0, v0, 1 */
   write_opcode(ram, 0x10020, 0x24030017); /* addiu v1, zero, 23 */
   write_opcode(ram, 0x10024, 0x0000000d); /* break */
   write_opcode(ram, 0x10028, 0x08004008); /* j 0x10020 */
   write_opcode(ram, 0x1002c, 0x00000000); /* nop */

   test_log(log, "Starting Lightrec MIPS block test\n");
   state = lightrec_init("ps3-lightrec-execution-test", maps,
         PSX_MAP_UNKNOWN, &ops);
   if (!state)
   {
      result = 21;
      goto cleanup;
   }
   regs = lightrec_get_registers(state);
   result = 0;
   for (i = 0; i < 3; i++)
   {
      regs->gpr[2] = regs->gpr[3] = 0;
      lightrec_reset_cycle_count(state, 0);
      test_log(log, "Executing Lightrec MIPS blocks; pass=%u\n", i);
      pc = lightrec_execute(state, 0x10000, 128);
      flags = lightrec_exit_flags(state);
      test_log(log, "MIPS blocks pass=%u pc=%08x v0=%u v1=%u flags=%u\n",
            i, (unsigned int)pc, (unsigned int)regs->gpr[2],
            (unsigned int)regs->gpr[3], (unsigned int)flags);
      if (regs->gpr[2] != 42 || regs->gpr[3] != 23 ||
            flags != LIGHTREC_EXIT_BREAK)
      {
         result = 22;
         break;
      }
   }
   lightrec_destroy(state);

cleanup:
   free(parallel);
   free(hardware);
   free(bios);
   free(ram);
   return result;
}
#endif

static int run_tests(FILE *log)
{
   jit_state_t *_jit;
   jit_node_t *arg;
   int (*fn)(int);
   int (*fn9)(int, int, int, int, int, int, int, int, int);
   int value;
   int i;

   init_jit("pcsx-ps3-lightrec-test");
   _jit = jit_new_state();
   jit_prolog();
   arg = jit_arg();
   jit_getarg(JIT_V0, arg);
   jit_prepare();
   jit_pushargr(JIT_V0);
   jit_finishi(callback);
   jit_retval(JIT_R0);
   jit_addr(JIT_R0, JIT_R0, JIT_V0);
   jit_retr(JIT_R0);
   jit_epilog();
   jit_realize();
   jit_set_data(NULL, 0, JIT_DISABLE_DATA | JIT_DISABLE_NOTE);
#ifdef __PS3__
   jit_set_code(lightrec_ps3_code, 65536);
#endif
   fn = jit_emit();
   if (!fn)
      return 1;
   test_log(log, "Generated callback function at %p\n", (void *)fn);
#if defined(__PS3__) && PS3_LIGHTREC_LOG
   {
      FILE *code = lightrec_ps3_open_log("ps3-lightrec-jit.bin", "wb");
      if (code)
      {
         fwrite(lightrec_ps3_code, 1, 512, code);
         fclose(code);
      }
   }
#endif
   jit_clear_state();
   value = fn(11);
   test_log(log, "callback + retained register: %d (expected 51)\n", value);
   if (value != 51)
      return 2;
   jit_destroy_state();

   _jit = jit_new_state();
   jit_prolog();
   for (i = 0; i < 8; i++)
      jit_arg();
   arg = jit_arg();
   jit_getarg(JIT_V0, arg);
   jit_prepare();
   for (i = 1; i <= 9; i++)
      jit_pushargi(i);
   jit_finishi(callback9);
   jit_retval(JIT_R0);
   jit_addr(JIT_R0, JIT_R0, JIT_V0);
   jit_retr(JIT_R0);
   jit_epilog();
   jit_realize();
   jit_set_data(NULL, 0, JIT_DISABLE_DATA | JIT_DISABLE_NOTE);
#ifdef __PS3__
   jit_set_code(lightrec_ps3_code, 65536);
#endif
   fn9 = jit_emit();
   if (!fn9)
      return 3;
   jit_clear_state();
   test_log(log, "Generated stack argument test at %p\n", (void *)fn9);
   value = fn9(1, 2, 3, 4, 5, 6, 7, 8, 99);
   test_log(log, "incoming + outgoing stack argument: %d (expected 151)\n", value);
   if (value != 151)
      return 4;
   jit_destroy_state();
   finish_jit();
   return 0;
}

int lightrec_ps3_jit_selftest(FILE *log)
{
   int result;
#ifndef __PS3__
   if (!log)
      log = stdout;
#endif
   test_log(log, "Lightrec JIT ABI tests; pointer=%u, word=%u\n",
         (unsigned)sizeof(void *), (unsigned)sizeof(jit_word_t));
   result = run_tests(log);
   test_log(log, "%s (%d)\n", result ? "FAIL" : "PASS", result);
#if defined(PS3_LIGHTREC_CORE) || defined(LIGHTREC_SMOKE_TEST)
   if (!result)
   {
      result = run_lightrec_test(log);
      test_log(log, "MIPS blocks %s (%d)\n", result ? "FAIL" : "PASS", result);
   }
#endif
   return result;
}

#ifndef PS3_LIGHTREC_CORE
int main(void)
{
   return lightrec_ps3_jit_selftest(stdout);
}
#endif

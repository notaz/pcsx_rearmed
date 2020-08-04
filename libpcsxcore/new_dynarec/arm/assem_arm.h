#ifndef __ASSEM_ARM_H__
#define __ASSEM_ARM_H__

#define HOST_REGS 13
#define HOST_CCREG 10
#define HOST_BTREG 8
#define EXCLUDE_REG 11

#define HOST_IMM8 1
#define HAVE_CMOV_IMM 1
#define HAVE_CONDITIONAL_CALL 1
#define RAM_SIZE 0x200000

#define REG_SHIFT 2

/* ARM calling convention:
   r0-r3, r12: caller-save
   r4-r11: callee-save */

#define ARG1_REG 0
#define ARG2_REG 1
#define ARG3_REG 2
#define ARG4_REG 3

/* GCC register naming convention:
   r10 = sl (base)
   r11 = fp (frame pointer)
   r12 = ip (scratch)
   r13 = sp (stack pointer)
   r14 = lr (link register)
   r15 = pc (program counter) */

#define FP 11
#define LR 14
#define HOST_TEMPREG 14

// Note: FP is set to &dynarec_local when executing generated code.
// Thus the local variables are actually global and not on the stack.

extern char *invc_ptr;

#define TARGET_SIZE_2 24 // 2^24 = 16 megabytes

// Code generator target address
#if   defined(BASE_ADDR_FIXED)
  // "round" address helpful for debug
  // this produces best code, but not many platforms allow it,
  // only use if you are sure this range is always free
  #define BASE_ADDR 0x1000000
  #define translation_cache (char *)BASE_ADDR
#elif defined(BASE_ADDR_DYNAMIC)
  // for platforms that can't just use .bss buffer, like vita
  // otherwise better to use the next option for closer branches
  extern char *translation_cache;
  #define BASE_ADDR (u_int)translation_cache
#else
  // using a static buffer in .bss
  extern char translation_cache[1 << TARGET_SIZE_2];
  #define BASE_ADDR (u_int)translation_cache
#endif

#endif /* __ASSEM_ARM_H__ */

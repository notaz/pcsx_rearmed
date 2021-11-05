#define HOST_REGS 13
#define HOST_CCREG 10
#define HOST_BTREG 8
#define EXCLUDE_REG 11

#define HOST_IMM8 1
#define HAVE_CMOV_IMM 1
#define HAVE_CONDITIONAL_CALL 1
#define RAM_SIZE 0x200000

/* ARM calling convention:
   r0-r3, r12: caller-save
   r4-r11: callee-save */

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
  #define BASE_ADDR_ 0x1000000
  #define translation_cache (u_char *)BASE_ADDR_
#elif defined(BASE_ADDR_DYNAMIC)
  // for platforms that can't just use .bss buffer, like vita
  // otherwise better to use the next option for closer branches
  extern u_char *translation_cache;
#else
  // using a static buffer in .bss
  extern u_char translation_cache[1 << TARGET_SIZE_2];
#endif

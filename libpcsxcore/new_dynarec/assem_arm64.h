#define HOST_REGS 29
#define HOST_BTREG 27
#define EXCLUDE_REG -1

#define HOST_IMM8 1
#define RAM_SIZE 0x200000

/* calling convention:
   r0 -r17: caller-save
   r19-r29: callee-save */

#define SP 31
#define WZR SP
#define XZR SP

#define LR 30
#define HOST_TEMPREG LR

// Note: FP is set to &dynarec_local when executing generated code.
// Thus the local variables are actually global and not on the stack.
#define FP 29
#define rFP x29

#define HOST_CCREG 28
#define rCC w28

// stack space
#define SSP_CALLEE_REGS (8*12)
#define SSP_CALLER_REGS (8*20)
#define SSP_ALL (SSP_CALLEE_REGS+SSP_CALLER_REGS)

#ifndef __ASSEMBLER__

extern char *invc_ptr;

#define TARGET_SIZE_2 24 // 2^24 = 16 megabytes

// Code generator target address
#if defined(BASE_ADDR_DYNAMIC)
  // for platforms that can't just use .bss buffer (are there any on arm64?)
  extern u_char *translation_cache;
#else
  // using a static buffer in .bss
  extern u_char translation_cache[1 << TARGET_SIZE_2];
#endif

#endif // !__ASSEMBLY__

#define HOST_IMM8 1

/* calling convention:
   r0 -r17: caller-save
   r19-r29: callee-save */

#define HOST_REGS 29
#define HOST_BTREG 27
#define EXCLUDE_REG -1

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

#define CALLER_SAVE_REGS 0x0007ffff
#define PREFERRED_REG_FIRST 19
#define PREFERRED_REG_LAST  27

// stack space
#define SSP_CALLEE_REGS (8*12)
#define SSP_CALLER_REGS (8*20)
#define SSP_ALL (SSP_CALLEE_REGS+SSP_CALLER_REGS)

#define TARGET_SIZE_2 24 // 2^24 = 16 megabytes

#ifndef __ASSEMBLER__

extern char *invc_ptr;

struct tramp_insns
{
  u_int ldr;
  u_int br;
};

static void clear_cache_arm64(char *start, char *end);

#endif // !__ASSEMBLY__

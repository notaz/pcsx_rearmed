#ifndef __REGUSE_H__
#define __REGUSE_H__

#ifdef __cplusplus
extern "C" {
#endif

// include basic types
#include "../psxcommon.h"

#define REGUSE_NONE    0x0000
#define REGUSE_UNKNOWN 0x0001

//sub functions
#define REGUSE_SPECIAL 0x0002
#define REGUSE_REGIMM  0x0004
#define REGUSE_COP0    0x0006
#define REGUSE_COP2    0x0008
#define REGUSE_BASIC   0x000a
#define REGUSE_SUBMASK 0x000e /* sub function mask */

#define REGUSE_ACC     0x0010 /* accumulator */
#define REGUSE_LOGIC   0x0020 /* logic operations */
#define REGUSE_MULT    0x0030 /* multiplier */
#define REGUSE_JUMP    0x0040 /* jump to dest */
#define REGUSE_JUMPR   0x0050 /* jump to reg */
#define REGUSE_BRANCH  0x0060 /* branch */
#define REGUSE_MEM_R   0x0070 /* read from memory */
#define REGUSE_MEM_W   0x0080 /* write to memory */
#define REGUSE_MEM     0x0090 /* read and write to memory */
#define REGUSE_SYS     0x00a0 /* syscall */
#define REGUSE_GTE     0x00b0 /* gte operation */
#define REGUSE_SUB     0x00f0 /* sub usage */
#define REGUSE_TYPEM   0x00f0 /* type mask */


#define REGUSE_RS_R    0x0100
#define REGUSE_RS_W    0x0200
#define REGUSE_RS     (REGUSE_RS_R | REGUSE_RS_W)
#define REGUSE_RT_R    0x0400
#define REGUSE_RT_W    0x0800
#define REGUSE_RT     (REGUSE_RT_R | REGUSE_RT_W)
#define REGUSE_RD_R    0x1000
#define REGUSE_RD_W    0x2000
#define REGUSE_RD     (REGUSE_RD_R | REGUSE_RD_W)

#define REGUSE_R31_W   0x4000 /* writes to link register (r31) */
#define REGUSE_PC      0x8000 /* reads pc */

#define REGUSE_LO_R    0x10000
#define REGUSE_LO_W    0x20000
#define REGUSE_LO     (REGUSE_LO_R | REGUSE_LO_W)
#define REGUSE_HI_R    0x40000
#define REGUSE_HI_W    0x80000
#define REGUSE_HI     (REGUSE_HI_R | REGUSE_HI_W)

#define REGUSE_COP0_RD_R    0x100000
#define REGUSE_COP0_RD_W    0x200000
#define REGUSE_COP0_RD     (REGUSE_COP0_RD_R | REGUSE_COP0_RD_W)
#define REGUSE_COP0_STATUS  0x400000
#define REGUSE_EXCEPTION    0x800000

#define REGUSE_COP2_RT_R    0x1000000
#define REGUSE_COP2_RT_W    0x2000000
#define REGUSE_COP2_RT     (REGUSE_COP2_RT_R | REGUSE_COP2_RT_W)
#define REGUSE_COP2_RD_R    0x4000000
#define REGUSE_COP2_RD_W    0x8000000
#define REGUSE_COP2_RD     (REGUSE_COP2_RD_R | REGUSE_COP2_RD_W)


// specific register use
#define REGUSE_READ   1
#define REGUSE_WRITE  2
#define REGUSE_RW     3

int useOfPsxReg(u32 code, int use, int psxreg) __attribute__ ((__pure__));;
int nextPsxRegUse(u32 pc, int psxreg) __attribute__ ((__pure__));;
int isPsxRegUsed(u32 pc, int psxreg) __attribute__ ((__pure__));;

#ifdef __cplusplus
}
#endif
#endif

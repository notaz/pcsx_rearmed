/*
 * (C) notaz, 2026
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef __GTE_ARM64_H__
#define __GTE_ARM64_H__

#include "psxcommon.h"

// note: doesn't use opcode, just maintaining common signature
void gteRTPS_sf1lm0_arm64(psxCP2Regs *cp2_regs, u32 opcode);
void gteRTPS_sf1lm0_nf_arm64(psxCP2Regs *cp2_regs, u32 opcode);

#endif /* __GTE_ARM64_H__ */

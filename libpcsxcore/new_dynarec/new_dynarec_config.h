#ifndef __NEW_DYNAREC_CONFIG_H__
#define __NEW_DYNAREC_CONFIG_H__

#define CORTEX_A8_BRANCH_PREDICTION_HACK 1
#define USE_MINI_HT 1
//#define REG_PREFETCH 1

#if defined(__MACH__)
#define NO_WRITE_EXEC 1
#endif
#ifdef VITA
#define BASE_ADDR_DYNAMIC 1
#endif

#endif /* __NEW_DYNAREC_CONFIG_H__ */



#define CORTEX_A8_BRANCH_PREDICTION_HACK 1
#define USE_MINI_HT 1
//#define REG_PREFETCH 1

#ifndef BASE_ADDR_FIXED
#define BASE_ADDR_FIXED 0
#endif

#ifdef __MACH__
#define NO_WRITE_EXEC
#endif

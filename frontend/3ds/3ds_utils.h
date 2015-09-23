#ifndef _3DS_UTILS_H
#define _3DS_UTILS_H

void ctr_invalidate_ICache(void);
void ctr_flush_DCache(void);

void ctr_flush_invalidate_cache(void);

int ctr_svchack_init(void);
void ctr_svchack_exit(void);

#if 0
#include "stdio.h"
void wait_for_input(void);

#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)
#endif

#endif // _3DS_UTILS_H

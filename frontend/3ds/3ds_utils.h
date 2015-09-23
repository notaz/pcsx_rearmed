#ifndef _3DS_UTILS_H
#define _3DS_UTILS_H

void ctr_invalidate_ICache(void);
void ctr_flush_DCache(void);

void ctr_flush_invalidate_cache(void);

int ctr_svchack_init(void);
void ctr_svchack_exit(void);

#endif // _3DS_UTILS_H

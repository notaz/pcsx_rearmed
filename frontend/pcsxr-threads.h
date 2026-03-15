#ifndef __PCSXR_THREADS_H__
#define __PCSXR_THREADS_H__

enum pcsxr_thread_type
{
	PCSXRT_CDR = 0,
	PCSXRT_DRC,
	PCSXRT_GPU,
	PCSXRT_SPU,
	PCSXRT_COUNT // must be last
};

extern int pcsxr_sthread_core_count;

void pcsxr_sthread_init(void);

#ifndef USE_C11_THREADS

/* use libretro-common rthreads */
//#include "rthreads/rthreads.h"
struct sthread;
struct slock;
struct scond;
typedef struct sthread sthread_t;
typedef struct slock slock_t;
typedef struct scond scond_t;

#define STRHEAD_RET_TYPE void
#define STRHEAD_RETURN()

struct sthread *pcsxr_sthread_create(void (*thread_func)(void*),
	enum pcsxr_thread_type type);
void sthread_join(struct sthread *thread);

struct slock *slock_new(void);
void slock_free(struct slock *lock);
void slock_lock(struct slock *lock);
void slock_unlock(struct slock *lock);

struct scond *scond_new(void);
void scond_free(struct scond *cond);
void scond_wait(struct scond *cond, struct slock *lock);
void scond_signal(struct scond *cond);

#else

/* C11 concurrency support */
#include <threads.h>

#define STRHEAD_RET_TYPE int
#define STRHEAD_RETURN() return 0

#define slock_new() ({ \
	mtx_t *lock = malloc(sizeof(*lock)); \
	if (lock) mtx_init(lock, mtx_plain); \
	lock; \
})

#define scond_new() ({ \
	cnd_t *cnd = malloc(sizeof(*cnd)); \
	if (cnd) cnd_init(cnd); \
	cnd; \
})

#define pcsxr_sthread_create(cb, unused) ({ \
	thrd_t *thd = malloc(sizeof(*thd)); \
	if (thd) \
		thrd_create(thd, cb, NULL); \
	thd; \
})

#define sthread_join(thrd) { \
	if (thrd) { \
		thrd_join(*thrd, NULL); \
		free(thrd); \
	} \
}

#define slock_free(lock) free(lock)
#define slock_lock(lock) mtx_lock(lock)
#define slock_unlock(lock) mtx_unlock(lock)
#define scond_free(cond) free(cond)
#define scond_wait(cond, lock) cnd_wait(cond, lock)
#define scond_signal(cond) cnd_signal(cond)
#define slock_t mtx_t
#define scond_t cnd_t
#define sthread_t thrd_t

#endif // USE_C11_THREADS

#endif // __PCSXR_THREADS_H__

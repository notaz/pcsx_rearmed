// temporary(?) workaround:
// https://github.com/libretro/libretro-common/pull/216
#ifdef _3DS
#include <3ds/svc.h>
#include <3ds/services/apt.h>
#include <sys/time.h>
#endif

#include "../deps/libretro-common/rthreads/rthreads.c"

// an "extension"
int sthread_set_name(sthread_t *thread, const char *name)
{
#if defined(__GLIBC__) || defined(__MACH__) || \
   (defined(__ANDROID_API__) && __ANDROID_API__ >= 26)
	if (thread)
		return pthread_setname_np(thread->id, name);
#endif
	return -1;
}

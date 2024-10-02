// temporary(?) workaround:
// https://github.com/libretro/libretro-common/pull/216
#ifdef _3DS
#include <3ds/svc.h>
#include <3ds/services/apt.h>
#include <sys/time.h>
#endif

#include "../deps/libretro-common/rthreads/rthreads.c"


#ifdef __GNUC__
# define likely(x)       __builtin_expect((x),1)
# define unlikely(x)     __builtin_expect((x),0)
# define noinline        __attribute__((noinline))
#else
# define likely(x)       (x)
# define unlikely(x)     (x)
# define noinline
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif


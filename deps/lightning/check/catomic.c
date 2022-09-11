#include <lightning.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void alarm_handler(int unused)
{
    _exit(1);
}

int
main(int argc, char *argv[])
{
    jit_state_t		*_jit;
    void		(*code)(void);
    jit_node_t		 *jmpi_main, *label;
    jit_node_t		 *func0, *func1, *func2, *func3;
    jit_node_t		 *patch0, *patch1, *patch2, *patch3;
    jit_word_t		  lock;
    pthread_t		  tids[4];

    /* If there is any bug, do not hang in "make check" */
    signal(SIGALRM, alarm_handler);
    alarm(5);
  
    init_jit(argv[0]);
    _jit = jit_new_state();

    jmpi_main = jit_jmpi();

#define defun(name, line)					\
    jit_name(#name);						\
    jit_note("catomic.c", line);				\
    name = jit_label();						\
     jit_prolog();						\
    jit_movi(JIT_V0, (jit_word_t)&lock);			\
    jit_movi(JIT_R1, 0);					\
    jit_movi(JIT_R2, line);					\
    /* spin until get the lock */				\
    label = jit_label();					\
    jit_casr(JIT_R0, JIT_V0, JIT_R1, JIT_R2);			\
    jit_patch_at(jit_beqi(JIT_R0, 0), label);			\
    /* lock acquired */						\
    jit_prepare();						\
    /* pretend to be doing something useful for 0.01 usec
     * while holding the lock */				\
    jit_pushargi(10000);					\
    jit_finishi(usleep);					\
    /* release lock */						\
    jit_movi(JIT_R1, 0);					\
    jit_str(JIT_V0, JIT_R1);					\
    /* Now test casi */						\
    jit_movi(JIT_R1, 0);					\
    jit_movi(JIT_R2, line);					\
    /* spin until get the lock */				\
    label = jit_label();					\
    jit_casi(JIT_R0, (jit_word_t)&lock, JIT_R1, JIT_R2);	\
    jit_patch_at(jit_beqi(JIT_R0, 0), label);			\
    /* lock acquired */						\
    jit_prepare();						\
    /* pretend to be doing something useful for 0.01 usec
     * while holding the lock */				\
    jit_pushargi(10000);					\
    jit_finishi(usleep);					\
    jit_prepare();						\
    /* for make check, just print "ok" */			\
    jit_pushargi((jit_word_t)"ok");				\
    /*jit_pushargi((jit_word_t)#name);*/			\
    jit_finishi(puts);						\
    /* release lock */						\
    jit_movi(JIT_R1, 0);					\
    jit_str(JIT_V0, JIT_R1);					\
    jit_ret();							\
    jit_epilog();
    defun(func0, __LINE__);
    defun(func1, __LINE__);
    defun(func2, __LINE__);
    defun(func3, __LINE__);

    jit_patch(jmpi_main);
    jit_name("main");
    jit_note("catomic.c", __LINE__);
    jit_prolog();

#define start(tid)						\
    /* set JIT_R0 to thread function */				\
    jit_patch_at(jit_movi(JIT_R0, 0), func##tid);		\
    jit_prepare();						\
    /* pthread_t first argument */				\
    jit_pushargi((jit_word_t)(tids + tid));			\
    /* pthread_attr_t second argument */			\
    jit_pushargi((jit_word_t)NULL);				\
    /* start routine third argument */				\
    jit_pushargr(JIT_R0);					\
    /* argument to start routine fourth argument */		\
    jit_pushargi((jit_word_t)NULL);				\
    /* start thread */						\
    jit_finishi(pthread_create);
    /* spawn four threads */
    start(0);
    start(1);
    start(2);
    start(3);

#define join(tid)						\
    /* load pthread_t value in JIT_R0 */			\
    jit_movi(JIT_R0, (jit_word_t)tids);				\
    jit_ldxi(JIT_R0, JIT_R0, tid * sizeof(pthread_t));		\
    jit_prepare();						\
    jit_pushargr(JIT_R0);					\
    jit_pushargi((jit_word_t)NULL);				\
    jit_finishi(pthread_join);
    /* wait for threads to finish */
    join(0);
    join(1);
    join(2);
    join(3);

    jit_prepare();
    jit_pushargi((jit_word_t)"ok");
    jit_finishi(puts);

    jit_ret();
    jit_epilog();

    code = jit_emit();

#if 1
    jit_disassemble();
#endif

    jit_clear_state();

    /* let first thread acquire the lock */
    lock = 0;
    
    (*code)();
    jit_destroy_state();

    finish_jit();

    return (0);
}

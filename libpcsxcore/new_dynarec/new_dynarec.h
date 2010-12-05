#define NEW_DYNAREC 1

extern int pcaddr;
extern int pending_exception;

void new_dynarec_init();
void new_dynarec_cleanup();
void new_dyna_start();

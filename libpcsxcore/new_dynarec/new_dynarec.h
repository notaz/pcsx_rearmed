#define NEW_DYNAREC 1

extern int pcaddr;
extern int pending_exception;
extern int stop;

void new_dynarec_init();
void new_dynarec_cleanup();
void new_dyna_start();

void invalidate_all_pages();
void invalidate_block(unsigned int block);

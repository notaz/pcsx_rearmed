#define NEW_DYNAREC 1

extern int pcaddr;
extern int pending_exception;
extern int stop;
extern int new_dynarec_did_compile;
extern int cycle_multiplier; // 100 for 1.0

void new_dynarec_init();
void new_dynarec_cleanup();
void new_dynarec_clear_full();
void new_dyna_start();

void invalidate_all_pages();
void invalidate_block(unsigned int block);

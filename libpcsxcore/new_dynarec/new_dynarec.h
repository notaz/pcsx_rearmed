#define NEW_DYNAREC 1

extern int pcaddr;
extern int pending_exception;
extern int stop;
extern int new_dynarec_did_compile;
extern int cycle_multiplier; // 100 for 1.0

#define NDHACK_NO_SMC_CHECK	(1<<0)
#define NDHACK_GTE_UNNEEDED	(1<<1)
#define NDHACK_GTE_NO_FLAGS	(1<<2)
extern int new_dynarec_hacks;

void new_dynarec_init();
void new_dynarec_cleanup();
void new_dynarec_clear_full();
void new_dyna_start();
int  new_dynarec_save_blocks(void *save, int size);
void new_dynarec_load_blocks(const void *save, int size);

void invalidate_all_pages();
void invalidate_block(unsigned int block);


extern u32 (*fetch)(u32 pc);

// called by "new_dynarec"
void execI();
void psxNULL();
void intApplyConfig();

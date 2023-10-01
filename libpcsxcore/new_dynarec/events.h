#include "../psxcommon.h"

extern int stop;

union psxCP0Regs_;
u32  schedule_timeslice(void);
void gen_interupt(union psxCP0Regs_ *cp0);

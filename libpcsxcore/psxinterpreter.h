#ifndef __PSXINTERPRETER_H__
#define __PSXINTERPRETER_H__

// get an opcode without triggering exceptions or affecting cache
u32 intFakeFetch(u32 pc);

// called by "new_dynarec"
void execI(psxRegisters *regs);
void intApplyConfig();
void MTC0(psxRegisters *regs_, int reg, u32 val);
void gteNULL(struct psxCP2Regs *regs);
extern void (*psxCP2[64])(struct psxCP2Regs *regs);

// called by lightrec
void intExecuteBlock(enum blockExecCaller caller);

#endif // __PSXINTERPRETER_H__

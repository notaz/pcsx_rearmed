#ifndef __PSXINTERPRETER_H__
#define __PSXINTERPRETER_H__

// called by "new_dynarec"
void execI();
void intApplyConfig();
void MTC0(psxRegisters *regs_, int reg, u32 val);
void gteNULL(struct psxCP2Regs *regs);
void (*psxCP2[64])(struct psxCP2Regs *regs);

#endif // __PSXINTERPRETER_H__

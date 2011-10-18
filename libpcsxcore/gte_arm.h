void gteRTPS_nf_arm(void *cp2_regs, int opcode);
void gteRTPT_nf_arm(void *cp2_regs, int opcode);
void gteNCLIP_arm(void *cp2_regs, int opcode);

// decomposed ops, nonstd calling convention
void gteMVMVA_part_arm(void *cp2_regs, int is_shift12);
void gteMVMVA_part_nf_arm(void *cp2_regs, int is_shift12);
void gteMVMVA_part_cv3sh12_arm(void *cp2_regs);

void gteMACtoIR_lm0(void *cp2_regs);
void gteMACtoIR_lm1(void *cp2_regs);
void gteMACtoIR_lm0_nf(void *cp2_regs);
void gteMACtoIR_lm1_nf(void *cp2_regs);

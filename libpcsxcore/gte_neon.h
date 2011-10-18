void gteRTPS_neon(void *cp2_regs, int opcode);
void gteRTPT_neon(void *cp2_regs, int opcode);

// decomposed ops, nonstd calling convention
void gteMVMVA_part_neon(void *cp2_regs, int opcode);

// after NEON call only, does not do gteIR
void gteMACtoIR_flags_neon(void *cp2_regs, int lm);

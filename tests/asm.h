
__attribute__((format(printf, 1, 2)))
int printf(const char *fmt, ...);
int syscall_a0(int func, ...);

int cpu_delay0(int *array_012);
int cpu_delay1(int *array_012);
int cpu_delay_j0(int *array_012);
int cpu_delay_j1(int *array_012);

void gte_rtps_sf1lm0(unsigned int *out);
void gte_rtps_sf1lm0_nclip(unsigned int *out);

// self-modifying code, the arg is unused
int smc_sw_reg(int *unused);
int smc_sw_reg_k1(int *unused);
int smc_sw_reg_ku(int *unused);
int smc_sw_c(int *unused);
int smc_sw_c_k1(int *unused);
int smc_sw_c_ku(int *unused);
int smc_sh(int *unused);
int smc_sb(int *unused);
int smc_swlr(int *unused);
int smc_swc2(int *unused);
int smc_multi(int *unused);
int smc_many(int *unused);
int smc_toggle(int *unused);
int smc_exec_k1(int *unused);
int smc_self(int *unused);

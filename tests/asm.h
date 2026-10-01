
__attribute__((format(printf, 1, 2)))
int printf(const char *fmt, ...);
int syscall_a0(int func, ...);

int cpu_delay0(int *array_012);
int cpu_delay1(int *array_012);
int cpu_delay_j0(int *array_012);
int cpu_delay_j1(int *array_012);

void gte_rtps_sf1lm0(unsigned int *out);
void gte_rtps_sf1lm0_nclip(unsigned int *out);

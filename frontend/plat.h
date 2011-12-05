void *plat_prepare_screenshot(int *w, int *h, int *bpp);
void plat_step_volume(int is_up);
int  plat_cpu_clock_get(void);
int  plat_cpu_clock_apply(int cpu_clock);
int  plat_get_bat_capacity(void);

// XXX
int  plat_pandora_init(void);

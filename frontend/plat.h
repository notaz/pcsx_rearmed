void plat_minimize(void);
void *plat_prepare_screenshot(int *w, int *h, int *bpp);
void plat_step_volume(int is_up);
int  plat_cpu_clock_get(void);
int  plat_cpu_clock_apply(int cpu_clock);
int  plat_get_bat_capacity(void);

// indirectly called from GPU plugin
void  plat_gvideo_open(void);
void *plat_gvideo_set_mode(int *w, int *h, int *bpp);
void *plat_gvideo_flip(void);
void  plat_gvideo_close(void);

// XXX
int  plat_pandora_init(void);

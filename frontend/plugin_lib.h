
enum {
	DKEY_SELECT = 0,
	DKEY_L3,
	DKEY_R3,
	DKEY_START,
	DKEY_UP,
	DKEY_RIGHT,
	DKEY_DOWN,
	DKEY_LEFT,
	DKEY_L2,
	DKEY_R2,
	DKEY_L1,
	DKEY_R1,
	DKEY_TRIANGLE,
	DKEY_CIRCLE,
	DKEY_CROSS,
	DKEY_SQUARE,
};
extern int in_type, in_keystate, in_a1[2], in_a2[2];
void in_update_analogs(void);

extern void *pl_fbdev_buf;

int   pl_fbdev_open(void);
void *pl_fbdev_set_mode(int w, int h, int bpp);
void *pl_fbdev_flip(void);
void  pl_fbdev_close(void);

void pl_text_out16(int x, int y, const char *texto, ...);
void pl_start_watchdog(void);

struct rearmed_cbs {
	void  (*pl_get_layer_pos)(int *x, int *y, int *w, int *h);
	int   (*pl_fbdev_open)(void);
	void *(*pl_fbdev_set_mode)(int w, int h, int bpp);
	void *(*pl_fbdev_flip)(void);
	void  (*pl_fbdev_close)(void);
	int  *fskip_option;
};

extern const struct rearmed_cbs pl_rearmed_cbs;

extern int pl_frame_interval;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

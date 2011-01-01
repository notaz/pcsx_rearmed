
extern int keystate;
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

extern void *pl_fbdev_buf;

int   pl_fbdev_open(void);
int   pl_fbdev_set_mode(int w, int h, int bpp);
void  pl_fbdev_flip(void);
void  pl_fbdev_close(void);

void pl_text_out16(int x, int y, const char *texto, ...);

struct rearmed_cbs {
	void (*pl_get_layer_pos)(int *x, int *y, int *w, int *h);
};

extern const struct rearmed_cbs pl_rearmed_cbs;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

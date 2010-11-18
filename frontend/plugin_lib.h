
extern void *pl_fbdev_buf;

int   pl_fbdev_init(void);
int   pl_fbdev_set_mode(int w, int h, int bpp);
void *pl_fbdev_flip(void);
void  pl_fbdev_finish(void);

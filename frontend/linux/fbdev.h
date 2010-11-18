struct vout_fbdev;

struct vout_fbdev *vout_fbdev_init(const char *fbdev_name, int *w, int *h, int no_dblbuf);
void *vout_fbdev_flip(struct vout_fbdev *fbdev);
void  vout_fbdev_wait_vsync(struct vout_fbdev *fbdev);
int   vout_fbdev_resize(struct vout_fbdev *fbdev, int w, int h,
			int left_border, int right_border, int top_border, int bottom_border,
			int no_dblbuf);
void  vout_fbdev_clear(struct vout_fbdev *fbdev);
void  vout_fbdev_clear_lines(struct vout_fbdev *fbdev, int y, int count);
int   vout_fbdev_get_fd(struct vout_fbdev *fbdev);
void  vout_fbdev_finish(struct vout_fbdev *fbdev);


void in_evdev_init(void *vdrv);
int  in_evdev_update(void *drv_data, const int *binds, int *result);

/* to be set somewhere in platform code */
extern struct in_default_bind in_evdev_defbinds[];

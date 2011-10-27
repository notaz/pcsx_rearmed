
extern int in_evdev_allow_abs_only;

void in_evdev_init(void);

/* to be set somewhere in platform code */
extern struct in_default_bind in_evdev_defbinds[];

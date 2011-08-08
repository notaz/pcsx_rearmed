#ifdef HAVE_TSLIB

struct tsdev;

struct tsdev *pl_gun_ts_init(void);
void pl_gun_ts_update(struct tsdev *ts, int *x, int *y, int *in);
void pl_set_gun_rect(int x, int y, int w, int h);

#else

#define pl_gun_ts_init() NULL
#define pl_gun_ts_update(...) do {} while (0)

#endif

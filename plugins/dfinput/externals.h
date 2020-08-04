#ifndef __P_EXTERNALS_H__
#define __P_EXTERNALS_H__

void dfinput_activate(void);

/* get gunstate from emu frontend,
 * xn, yn - layer position normalized to 0..1023 */
#define GUNIN_TRIGGER	(1<<0)
#define GUNIN_BTNA	(1<<1)
#define GUNIN_BTNB	(1<<2)
#define GUNIN_TRIGGER2	(1<<3)	/* offscreen trigger */
extern void pl_update_gun(int *xn, int *yn, int *xres, int *yres, int *in);

/* vibration trigger to frontend */
extern int in_enable_vibration;
extern void plat_trigger_vibrate(int pad, int low, int high);

#endif /* __P_EXTERNALS_H__ */

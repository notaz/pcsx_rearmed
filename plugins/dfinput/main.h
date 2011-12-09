#include "../../libpcsxcore/psemu_plugin_defs.h"

extern unsigned char CurPad, CurByte, CurCmd, CmdLen;

/* analog pad */
unsigned char PADpoll_pad(unsigned char value);
unsigned char PADstartPoll_pad(int pad);
void pad_init(void);

/* GunCon */
unsigned char PADpoll_guncon(unsigned char value);
unsigned char PADstartPoll_guncon(int pad);
void guncon_init(void);

void dfinput_activate(void);

/* get button state and pad type from main emu */
extern long (*PAD1_readPort1)(PadDataS *pad);
extern long (*PAD2_readPort2)(PadDataS *pad);

/* get gunstate from emu frontend, x range 0-1023 */
#define GUNIN_TRIGGER	(1<<0)
#define GUNIN_BTNA	(1<<1)
#define GUNIN_BTNB	(1<<2)
#define GUNIN_TRIGGER2	(1<<3)	/* offscreen trigger */
extern void pl_update_gun(int *xn, int *xres, int *y, int *in);

/* vibration trigger to frontend */
extern int in_enable_vibration;
extern void plat_trigger_vibrate(void);

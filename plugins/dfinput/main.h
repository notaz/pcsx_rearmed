#ifndef __P_MAIN_H__
#define __P_MAIN_H__

#include "psemu_plugin_defs.h"
#include "externals.h"

extern unsigned char CurPad, CurByte, CurCmd, CmdLen;

/* analog pad */
unsigned char PADpoll_pad(unsigned char value);
unsigned char PADstartPoll_pad(int pad);
void pad_init(void);

/* GunCon */
unsigned char PADpoll_guncon(unsigned char value);
unsigned char PADstartPoll_guncon(int pad);
void guncon_init(void);

/* get button state and pad type from main emu */
extern long (*PAD1_readPort1)(PadDataS *pad);
extern long (*PAD2_readPort2)(PadDataS *pad);

#endif /* __P_MAIN_H__ */

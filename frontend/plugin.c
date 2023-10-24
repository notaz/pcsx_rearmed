/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "plugin_lib.h"
#include "plugin.h"
#include "psemu_plugin_defs.h"
#include "../libpcsxcore/system.h"
#include "../libpcsxcore/psxcommon.h"
#include "../plugins/cdrcimg/cdrcimg.h"

// this can't be __stdcall like it was in PSEmu API as too many functions are mixed up
#undef CALLBACK
#define CALLBACK

/* CDR */
struct CdrStat;
static long CALLBACK CDRinit(void) { return 0; }
static long CALLBACK CDRshutdown(void) { return 0; }
static long CALLBACK CDRopen(void) { return 0; }
static long CALLBACK CDRclose(void) { return 0; }
static long CALLBACK CDRgetTN(unsigned char *_) { return 0; }
static long CALLBACK CDRgetTD(unsigned char _, unsigned char *__) { return 0; }
static boolean CALLBACK CDRreadTrack(unsigned char *_) { return FALSE; }
static unsigned char * CALLBACK CDRgetBuffer(void) { return NULL; }
static unsigned char * CALLBACK CDRgetBufferSub(int sector) { return NULL; }
static long CALLBACK CDRconfigure(void) { return 0; }
static long CALLBACK CDRtest(void) { return 0; }
static void CALLBACK CDRabout(void) { return; }
static long CALLBACK CDRplay(unsigned char *_) { return 0; }
static long CALLBACK CDRstop(void) { return 0; }
static long CALLBACK CDRsetfilename(char *_) { return 0; }
static long CALLBACK CDRgetStatus(struct CdrStat *_) { return 0; }
static char * CALLBACK CDRgetDriveLetter(void) { return NULL; }
static long CALLBACK CDRreadCDDA(unsigned char _, unsigned char __, unsigned char ___, unsigned char *____) { return 0; }
static long CALLBACK CDRgetTE(unsigned char _, unsigned char *__, unsigned char *___, unsigned char *____) { return 0; }

/* GPU */
static void CALLBACK GPUdisplayText(char *_) { return; }

/* SPU */
extern long CALLBACK SPUopen(void);
extern long CALLBACK SPUinit(void);
extern long CALLBACK SPUshutdown(void);
extern long CALLBACK SPUclose(void);
extern void CALLBACK SPUwriteRegister(unsigned long, unsigned short, unsigned int);
extern unsigned short CALLBACK SPUreadRegister(unsigned long, unsigned int);
extern void CALLBACK SPUwriteDMAMem(unsigned short *, int, unsigned int);
extern void CALLBACK SPUreadDMAMem(unsigned short *, int, unsigned int);
extern void CALLBACK SPUplayADPCMchannel(void *, unsigned int, int);
extern void CALLBACK SPUregisterCallback(void (*cb)(int));
extern void CALLBACK SPUregisterScheduleCb(void (*cb)(unsigned int));
extern long CALLBACK SPUfreeze(unsigned int, void *, unsigned int);
extern void CALLBACK SPUasync(unsigned int, unsigned int);
extern int  CALLBACK SPUplayCDDAchannel(short *, int, unsigned int, int);

/* PAD */
static long CALLBACK PADinit(long _) { return 0; }
static long CALLBACK PADopen(unsigned long *_) { return 0; }
static long CALLBACK PADshutdown(void) { return 0; }
static long CALLBACK PADclose(void) { return 0; }
static void CALLBACK PADsetSensitive(int _) { return; }

static long CALLBACK PADreadPort1(PadDataS *pad) {
	int pad_index = pad->requestPadIndex;

	pad->controllerType = in_type[pad_index];
	pad->buttonStatus = ~in_keystate[pad_index];

	pad->portMultitap = multitap1;

	if (in_type[pad_index] == PSE_PAD_TYPE_ANALOGJOY || in_type[pad_index] == PSE_PAD_TYPE_ANALOGPAD || in_type[pad_index] == PSE_PAD_TYPE_NEGCON || in_type[pad_index] == PSE_PAD_TYPE_GUNCON || in_type[pad_index] == PSE_PAD_TYPE_GUN)
	{
		pad->leftJoyX = in_analog_left[pad_index][0];
		pad->leftJoyY = in_analog_left[pad_index][1];
		pad->rightJoyX = in_analog_right[pad_index][0];
		pad->rightJoyY = in_analog_right[pad_index][1];

		pad->absoluteX = in_analog_left[pad_index][0];
		pad->absoluteY = in_analog_left[pad_index][1];
	}

	if (in_type[pad_index] == PSE_PAD_TYPE_MOUSE)
	{
		pad->moveX = in_mouse[pad_index][0];
		pad->moveY = in_mouse[pad_index][1];
	}

	return 0;
}

static long CALLBACK PADreadPort2(PadDataS *pad) {
	int pad_index = pad->requestPadIndex;

	pad->controllerType = in_type[pad_index];
	pad->buttonStatus = ~in_keystate[pad_index];

	pad->portMultitap = multitap2;

	if (in_type[pad_index] == PSE_PAD_TYPE_ANALOGJOY || in_type[pad_index] == PSE_PAD_TYPE_ANALOGPAD || in_type[pad_index] == PSE_PAD_TYPE_NEGCON || in_type[pad_index] == PSE_PAD_TYPE_GUNCON || in_type[pad_index] == PSE_PAD_TYPE_GUN)
	{
		pad->leftJoyX = in_analog_left[pad_index][0];
		pad->leftJoyY = in_analog_left[pad_index][1];
		pad->rightJoyX = in_analog_right[pad_index][0];
		pad->rightJoyY = in_analog_right[pad_index][1];

		pad->absoluteX = in_analog_left[pad_index][0];
		pad->absoluteY = in_analog_left[pad_index][1];
	}

	if (in_type[pad_index] == PSE_PAD_TYPE_MOUSE)
	{
		pad->moveX = in_mouse[pad_index][0];
		pad->moveY = in_mouse[pad_index][1];
	}

	return 0;
}

/* GPU */
extern long GPUopen(unsigned long *, char *, char *);
extern long GPUinit(void);
extern long GPUshutdown(void);
extern long GPUclose(void);
extern void GPUwriteStatus(uint32_t);
extern void GPUwriteData(uint32_t);
extern void GPUwriteDataMem(uint32_t *, int);
extern uint32_t GPUreadStatus(void);
extern uint32_t GPUreadData(void);
extern void GPUreadDataMem(uint32_t *, int);
extern long GPUdmaChain(uint32_t *, uint32_t, uint32_t *);
extern void GPUupdateLace(void);
extern long GPUfreeze(uint32_t, void *);
extern void GPUvBlank(int, int);
extern void GPUgetScreenInfo(int *y, int *base_hres);
extern void GPUrearmedCallbacks(const struct rearmed_cbs *cbs);


#define DIRECT(id, name) \
	{ id, #name, name }

#define DIRECT_CDR(name) DIRECT(PLUGIN_CDR, name)
#define DIRECT_SPU(name) DIRECT(PLUGIN_SPU, name)
#define DIRECT_GPU(name) DIRECT(PLUGIN_GPU, name)
#define DIRECT_PAD(name) DIRECT(PLUGIN_PAD, name)

static const struct {
	int id;
	const char *name;
	void *func;
} plugin_funcs[] = {
	/* CDR */
	DIRECT_CDR(CDRinit),
	DIRECT_CDR(CDRshutdown),
	DIRECT_CDR(CDRopen),
	DIRECT_CDR(CDRclose),
	DIRECT_CDR(CDRtest),
	DIRECT_CDR(CDRgetTN),
	DIRECT_CDR(CDRgetTD),
	DIRECT_CDR(CDRreadTrack),
	DIRECT_CDR(CDRgetBuffer),
	DIRECT_CDR(CDRgetBufferSub),
	DIRECT_CDR(CDRplay),
	DIRECT_CDR(CDRstop),
	DIRECT_CDR(CDRgetStatus),
	DIRECT_CDR(CDRgetDriveLetter),
	DIRECT_CDR(CDRconfigure),
	DIRECT_CDR(CDRabout),
	DIRECT_CDR(CDRsetfilename),
	DIRECT_CDR(CDRreadCDDA),
	DIRECT_CDR(CDRgetTE),
	/* SPU */
	DIRECT_SPU(SPUinit),
	DIRECT_SPU(SPUshutdown),
	DIRECT_SPU(SPUopen),
	DIRECT_SPU(SPUclose),
	DIRECT_SPU(SPUwriteRegister),
	DIRECT_SPU(SPUreadRegister),
	DIRECT_SPU(SPUwriteDMAMem),
	DIRECT_SPU(SPUreadDMAMem),
	DIRECT_SPU(SPUplayADPCMchannel),
	DIRECT_SPU(SPUfreeze),
	DIRECT_SPU(SPUregisterCallback),
	DIRECT_SPU(SPUregisterScheduleCb),
	DIRECT_SPU(SPUasync),
	DIRECT_SPU(SPUplayCDDAchannel),
	/* PAD */
	DIRECT_PAD(PADinit),
	DIRECT_PAD(PADshutdown),
	DIRECT_PAD(PADopen),
	DIRECT_PAD(PADclose),
	DIRECT_PAD(PADsetSensitive),
	DIRECT_PAD(PADreadPort1),
	DIRECT_PAD(PADreadPort2),
/*
	DIRECT_PAD(PADquery),
	DIRECT_PAD(PADconfigure),
	DIRECT_PAD(PADtest),
	DIRECT_PAD(PADabout),
	DIRECT_PAD(PADkeypressed),
	DIRECT_PAD(PADstartPoll),
	DIRECT_PAD(PADpoll),
*/
	/* GPU */
	DIRECT_GPU(GPUupdateLace),
	DIRECT_GPU(GPUinit),
	DIRECT_GPU(GPUshutdown),
	DIRECT_GPU(GPUopen),
	DIRECT_GPU(GPUclose),
	DIRECT_GPU(GPUreadStatus),
	DIRECT_GPU(GPUreadData),
	DIRECT_GPU(GPUreadDataMem),
	DIRECT_GPU(GPUwriteStatus),
	DIRECT_GPU(GPUwriteData),
	DIRECT_GPU(GPUwriteDataMem),
	DIRECT_GPU(GPUdmaChain),
	DIRECT_GPU(GPUfreeze),
	DIRECT_GPU(GPUvBlank),
	DIRECT_GPU(GPUgetScreenInfo),
	DIRECT_GPU(GPUrearmedCallbacks),

	DIRECT_GPU(GPUdisplayText),
/*
	DIRECT_GPU(GPUkeypressed),
	DIRECT_GPU(GPUmakeSnapshot),
	DIRECT_GPU(GPUconfigure),
	DIRECT_GPU(GPUtest),
	DIRECT_GPU(GPUabout),
	DIRECT_GPU(GPUgetScreenPic),
	DIRECT_GPU(GPUshowScreenPic),
*/
};

void *plugin_link(enum builtint_plugins_e id, const char *sym)
{
	int i;

	if (id == PLUGIN_CDRCIMG)
		return cdrcimg_get_sym(sym);

	for (i = 0; i < ARRAY_SIZE(plugin_funcs); i++) {
		if (id != plugin_funcs[i].id)
			continue;

		if (strcmp(sym, plugin_funcs[i].name) != 0)
			continue;

		return plugin_funcs[i].func;
	}

	//fprintf(stderr, "plugin_link: missing symbol %d %s\n", id, sym);
	return NULL;
}

void plugin_call_rearmed_cbs(void)
{
	extern void *hGPUDriver;
	void (*rearmed_set_cbs)(const struct rearmed_cbs *cbs);

	pl_rearmed_cbs.screen_centering_type_default =
		Config.hacks.gpu_centering ? C_INGAME : C_AUTO;

	rearmed_set_cbs = SysLoadSym(hGPUDriver, "GPUrearmedCallbacks");
	if (rearmed_set_cbs != NULL)
		rearmed_set_cbs(&pl_rearmed_cbs);
}

#ifdef PCNT

/* basic profile stuff */
#include "pcnt.h"

unsigned int pcounters[PCNT_CNT];
unsigned int pcounter_starts[PCNT_CNT];

#define pc_hook_func(name, args, pargs, cnt) \
extern void (*name) args; \
static void (*o_##name) args; \
static void w_##name args \
{ \
	unsigned int pc_start = pcnt_get(); \
	o_##name pargs; \
	pcounters[cnt] += pcnt_get() - pc_start; \
}

#define pc_hook_func_ret(retn, name, args, pargs, cnt) \
extern retn (*name) args; \
static retn (*o_##name) args; \
static retn w_##name args \
{ \
	retn ret; \
	unsigned int pc_start = pcnt_get(); \
	ret = o_##name pargs; \
	pcounters[cnt] += pcnt_get() - pc_start; \
	return ret; \
}

pc_hook_func              (GPU_writeStatus, (uint32_t a0), (a0), PCNT_GPU)
pc_hook_func              (GPU_writeData, (uint32_t a0), (a0), PCNT_GPU)
pc_hook_func              (GPU_writeDataMem, (uint32_t *a0, int a1), (a0, a1), PCNT_GPU)
pc_hook_func_ret(uint32_t, GPU_readStatus, (void), (), PCNT_GPU)
pc_hook_func_ret(uint32_t, GPU_readData, (void), (), PCNT_GPU)
pc_hook_func              (GPU_readDataMem, (uint32_t *a0, int a1), (a0, a1), PCNT_GPU)
pc_hook_func_ret(long,     GPU_dmaChain, (uint32_t *a0, int32_t a1), (a0, a1), PCNT_GPU)
pc_hook_func              (GPU_updateLace, (void), (), PCNT_GPU)

pc_hook_func              (SPU_writeRegister, (unsigned long a0, unsigned short a1, uint32_t a2), (a0, a1, a2), PCNT_SPU)
pc_hook_func_ret(unsigned short,SPU_readRegister, (unsigned long a0, , unsigned int a1), (a0, a1), PCNT_SPU)
pc_hook_func              (SPU_writeDMAMem, (unsigned short *a0, int a1, uint32_t a2), (a0, a1, a2), PCNT_SPU)
pc_hook_func              (SPU_readDMAMem, (unsigned short *a0, int a1, uint32_t a2), (a0, a1, a2), PCNT_SPU)
pc_hook_func              (SPU_playADPCMchannel, (void *a0, unsigned int a1, int a2), (a0, a1, a2), PCNT_SPU)
pc_hook_func              (SPU_async, (uint32_t a0, uint32_t a1), (a0, a1), PCNT_SPU)
pc_hook_func_ret(int,      SPU_playCDDAchannel, (short *a0, int a1, unsigned int a2, int a3), (a0, a1, a2, a3), PCNT_SPU)

#define hook_it(name) { \
	o_##name = name; \
	name = w_##name; \
}

void pcnt_hook_plugins(void)
{
	pcnt_init();

	hook_it(GPU_writeStatus);
	hook_it(GPU_writeData);
	hook_it(GPU_writeDataMem);
	hook_it(GPU_readStatus);
	hook_it(GPU_readData);
	hook_it(GPU_readDataMem);
	hook_it(GPU_dmaChain);
	hook_it(GPU_updateLace);
	hook_it(SPU_writeRegister);
	hook_it(SPU_readRegister);
	hook_it(SPU_writeDMAMem);
	hook_it(SPU_readDMAMem);
	hook_it(SPU_playADPCMchannel);
	hook_it(SPU_async);
	hook_it(SPU_playCDDAchannel);
}

// hooked into recompiler
void pcnt_gte_start(int op)
{
	pcnt_start(PCNT_GTE);
}

void pcnt_gte_end(int op)
{
	pcnt_end(PCNT_GTE);
}

#endif

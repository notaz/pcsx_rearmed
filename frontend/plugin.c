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
#include "../plugins/cdrcimg/cdrcimg.h"

static int dummy_func() {
	return 0;
}

static long CDRreadTrack(unsigned char *time) {
	fprintf(stderr, "CDRreadTrack\n");
	return -1;
}

/* SPU */
extern long SPUopen(void);
extern long SPUinit(void);
extern long SPUshutdown(void);
extern long SPUclose(void);
extern void SPUplaySample(unsigned char);
extern void SPUwriteRegister(unsigned long, unsigned short);
extern unsigned short SPUreadRegister(unsigned long);
extern void SPUwriteDMA(unsigned short);
extern unsigned short SPUreadDMA(void);
extern void SPUwriteDMAMem(unsigned short *, int);
extern void SPUreadDMAMem(unsigned short *, int);
extern void SPUplayADPCMchannel(void *);
extern void SPUregisterCallback(void (*callback)(void));
extern long SPUconfigure(void);
extern long SPUtest(void);
extern void SPUabout(void);
extern long SPUfreeze(unsigned int, void *);
extern void SPUasync(unsigned int);
extern void SPUplayCDDAchannel(short *, int);

/* PAD */
static uint8_t pad_buf[] = { 0x41, 0x5A, 0xFF, 0xFF };
static uint8_t pad_byte;

static unsigned char PADstartPoll(int pad) {
	pad_byte = 0;
	pad_buf[2] = ~keystate;
	pad_buf[3] = ~keystate >> 8;

	return 0xFF;
}

static unsigned char PADpoll(unsigned char value) {
	if (pad_byte >= 4)
		return 0;

	return pad_buf[pad_byte++];
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
extern long GPUdmaChain(uint32_t *,uint32_t);
extern void GPUupdateLace(void);
extern long GPUconfigure(void);
extern long GPUtest(void);
extern void GPUabout(void);
extern void GPUmakeSnapshot(void);
extern void GPUkeypressed(int);
extern void GPUdisplayText(char *);
extern long GPUfreeze(uint32_t, void *);
extern long GPUgetScreenPic(unsigned char *);
extern long GPUshowScreenPic(unsigned char *);
extern void GPUclearDynarec(void (*callback)(void));
extern void GPUvBlank(int);


#define DUMMY(id, name) \
	{ id, #name, dummy_func }

#define DIRECT(id, name) \
	{ id, #name, name }

#define DUMMY_CDR(name)  DUMMY(PLUGIN_CDR, name)
#define DUMMY_PAD(name)  DUMMY(PLUGIN_PAD, name)
#define DIRECT_SPU(name) DIRECT(PLUGIN_SPU, name)
#define DIRECT_GPU(name) DIRECT(PLUGIN_GPU, name)
#define DIRECT_PAD(name) DIRECT(PLUGIN_PAD, name)

static const struct {
	int id;
	const char *name;
	void *func;
} plugin_funcs[] = {
	/* CDR */
	DUMMY_CDR(CDRinit),
	DUMMY_CDR(CDRshutdown),
	DUMMY_CDR(CDRopen),
	DUMMY_CDR(CDRclose),
	DUMMY_CDR(CDRtest),
	DUMMY_CDR(CDRgetTN),
	DUMMY_CDR(CDRgetTD),
	DUMMY_CDR(CDRreadTrack),
	DUMMY_CDR(CDRgetBuffer),
	DUMMY_CDR(CDRgetBufferSub),
	DUMMY_CDR(CDRplay),
	DUMMY_CDR(CDRstop),
	DUMMY_CDR(CDRgetStatus),
	DUMMY_CDR(CDRgetDriveLetter),
	DUMMY_CDR(CDRconfigure),
	DUMMY_CDR(CDRabout),
	DUMMY_CDR(CDRsetfilename),
	DUMMY_CDR(CDRreadCDDA),
	DUMMY_CDR(CDRgetTE),
	DIRECT(PLUGIN_CDR, CDRreadTrack),
	/* SPU */
	DIRECT_SPU(SPUconfigure),
	DIRECT_SPU(SPUabout),
	DIRECT_SPU(SPUinit),
	DIRECT_SPU(SPUshutdown),
	DIRECT_SPU(SPUtest),
	DIRECT_SPU(SPUopen),
	DIRECT_SPU(SPUclose),
//	DIRECT_SPU(SPUplaySample), // unused?
	DIRECT_SPU(SPUwriteRegister),
	DIRECT_SPU(SPUreadRegister),
	DIRECT_SPU(SPUwriteDMA),
	DIRECT_SPU(SPUreadDMA),
	DIRECT_SPU(SPUwriteDMAMem),
	DIRECT_SPU(SPUreadDMAMem),
	DIRECT_SPU(SPUplayADPCMchannel),
	DIRECT_SPU(SPUfreeze),
	DIRECT_SPU(SPUregisterCallback),
	DIRECT_SPU(SPUasync),
	DIRECT_SPU(SPUplayCDDAchannel),
	/* PAD */
	DUMMY_PAD(PADconfigure),
	DUMMY_PAD(PADabout),
	DUMMY_PAD(PADinit),
	DUMMY_PAD(PADshutdown),
	DUMMY_PAD(PADtest),
	DUMMY_PAD(PADopen),
	DUMMY_PAD(PADclose),
	DUMMY_PAD(PADquery),
	DUMMY_PAD(PADreadPort1),
	DUMMY_PAD(PADreadPort2),
	DUMMY_PAD(PADkeypressed),
	DUMMY_PAD(PADsetSensitive),
	DIRECT_PAD(PADstartPoll),
	DIRECT_PAD(PADpoll),
	/* GPU */
	DIRECT_GPU(GPUupdateLace),
	DIRECT_GPU(GPUinit),
	DIRECT_GPU(GPUshutdown),
	DIRECT_GPU(GPUconfigure),
	DIRECT_GPU(GPUtest),
	DIRECT_GPU(GPUabout),
	DIRECT_GPU(GPUopen),
	DIRECT_GPU(GPUclose),
	DIRECT_GPU(GPUreadStatus),
	DIRECT_GPU(GPUreadData),
	DIRECT_GPU(GPUreadDataMem),
	DIRECT_GPU(GPUwriteStatus),
	DIRECT_GPU(GPUwriteData),
	DIRECT_GPU(GPUwriteDataMem),
	DIRECT_GPU(GPUdmaChain),
	DIRECT_GPU(GPUkeypressed),
	DIRECT_GPU(GPUdisplayText),
	DIRECT_GPU(GPUmakeSnapshot),
	DIRECT_GPU(GPUfreeze),
	DIRECT_GPU(GPUgetScreenPic),
	DIRECT_GPU(GPUshowScreenPic),
//	DIRECT_GPU(GPUclearDynarec),
//	DIRECT_GPU(GPUvBlank),
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

	fprintf(stderr, "plugin_link: missing symbol %d %s\n", id, sym);
	return NULL;
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

pc_hook_func              (SPU_writeRegister, (unsigned long a0, unsigned short a1), (a0, a1), PCNT_SPU)
pc_hook_func_ret(unsigned short,SPU_readRegister, (unsigned long a0), (a0), PCNT_SPU)
pc_hook_func              (SPU_writeDMA, (unsigned short a0), (a0), PCNT_SPU)
pc_hook_func_ret(unsigned short,SPU_readDMA, (void), (), PCNT_SPU)
pc_hook_func              (SPU_writeDMAMem, (unsigned short *a0, int a1), (a0, a1), PCNT_SPU)
pc_hook_func              (SPU_readDMAMem, (unsigned short *a0, int a1), (a0, a1), PCNT_SPU)
pc_hook_func              (SPU_playADPCMchannel, (void *a0), (a0), PCNT_SPU)
pc_hook_func              (SPU_async, (unsigned int a0), (a0), PCNT_SPU)
pc_hook_func              (SPU_playCDDAchannel, (short *a0, int a1), (a0, a1), PCNT_SPU)

#define hook_it(name) { \
	o_##name = name; \
	name = w_##name; \
}

void pcnt_hook_plugins(void)
{
	/* test it first */
	pcnt_get();

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
	hook_it(SPU_writeDMA);
	hook_it(SPU_readDMA);
	hook_it(SPU_writeDMAMem);
	hook_it(SPU_readDMAMem);
	hook_it(SPU_playADPCMchannel);
	hook_it(SPU_async);
	hook_it(SPU_playCDDAchannel);
}

#endif

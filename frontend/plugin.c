/*
 * (C) notaz, 2010
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "plugin.h"

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
static uint8_t CurByte;

static unsigned char PADstartPoll(int pad) {
	CurByte = 0;
	return 0xFF;
}

static unsigned char PADpoll(unsigned char value) {
	static uint8_t buf[] = {0x41, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80};
	if (CurByte >= 4)
		return 0;
	return buf[CurByte++];
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


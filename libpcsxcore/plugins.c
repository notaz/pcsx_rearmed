/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

/*
* Plugin library callback/access functions.
*/

#include <stdio.h>
#include <time.h>
#include "plugins.h"
#include "cdriso.h"
#include "cdrom-async.h"
#include "psxcounters.h"

static char IsoFile[MAXPATHLEN] = "";
static s64 cdOpenCaseTime = 0;

GPUupdateLace         GPU_updateLace;
GPUinit               GPU_init;
GPUshutdown           GPU_shutdown;
GPUopen               GPU_open;
GPUclose              GPU_close;
GPUreadStatus         GPU_readStatus;
GPUreadData           GPU_readData;
GPUreadDataMem        GPU_readDataMem;
GPUwriteStatus        GPU_writeStatus;
GPUwriteData          GPU_writeData;
GPUwriteDataMem       GPU_writeDataMem;
GPUdmaChain           GPU_dmaChain;
GPUfreeze             GPU_freeze;
GPUvBlank             GPU_vBlank;
GPUgetScreenInfo      GPU_getScreenInfo;

SPUinit               SPU_init;
SPUshutdown           SPU_shutdown;
SPUopen               SPU_open;
SPUclose              SPU_close;
SPUwriteRegister      SPU_writeRegister;
SPUreadRegister       SPU_readRegister;
SPUwriteDMAMem        SPU_writeDMAMem;
SPUreadDMAMem         SPU_readDMAMem;
SPUplayADPCMchannel   SPU_playADPCMchannel;
SPUfreeze             SPU_freeze;
SPUregisterCallback   SPU_registerCallback;
SPUregisterScheduleCb SPU_registerScheduleCb;
SPUasync              SPU_async;
SPUplayCDDAchannel    SPU_playCDDAchannel;
SPUsetCDvol           SPU_setCDvol;

#ifdef ENABLE_SIO1API

SIO1init              SIO1_init;
SIO1shutdown          SIO1_shutdown;
SIO1open              SIO1_open;
SIO1close             SIO1_close;
SIO1test              SIO1_test;
SIO1configure         SIO1_configure;
SIO1about             SIO1_about;
SIO1pause             SIO1_pause;
SIO1resume            SIO1_resume;
SIO1keypressed        SIO1_keypressed;
SIO1writeData8        SIO1_writeData8;
SIO1writeData16       SIO1_writeData16;
SIO1writeData32       SIO1_writeData32;
SIO1writeStat16       SIO1_writeStat16;
SIO1writeStat32       SIO1_writeStat32;
SIO1writeMode16       SIO1_writeMode16;
SIO1writeMode32       SIO1_writeMode32;
SIO1writeCtrl16       SIO1_writeCtrl16;
SIO1writeCtrl32       SIO1_writeCtrl32;
SIO1writeBaud16       SIO1_writeBaud16;
SIO1writeBaud32       SIO1_writeBaud32;
SIO1readData8         SIO1_readData8;
SIO1readData16        SIO1_readData16;
SIO1readData32        SIO1_readData32;
SIO1readStat16        SIO1_readStat16;
SIO1readStat32        SIO1_readStat32;
SIO1readMode16        SIO1_readMode16;
SIO1readMode32        SIO1_readMode32;
SIO1readCtrl16        SIO1_readCtrl16;
SIO1readCtrl32        SIO1_readCtrl32;
SIO1readBaud16        SIO1_readBaud16;
SIO1readBaud32        SIO1_readBaud32;
SIO1registerCallback  SIO1_registerCallback;

#endif

static const char *err;

#define CheckErr(func) { \
	err = SysLibError(); \
	if (err != NULL) { SysMessage(_("Error loading %s: %s"), func, err); return -1; } \
}

#define LoadSym(dest, src, name, checkerr) { \
	dest = (src)SysLoadSym(drv, name); \
	if (checkerr) { CheckErr(name); } \
}

void *hGPUDriver = NULL;

static void CALLBACK GPU__vBlank(int val) {}
static void CALLBACK GPU__getScreenInfo(int *y, int *base_hres) {}

#define LoadGpuSym1(dest, name) \
	LoadSym(GPU_##dest, GPU##dest, name, TRUE);

#define LoadGpuSym0(dest, name) \
	LoadSym(GPU_##dest, GPU##dest, name, FALSE); \
	if (GPU_##dest == NULL) GPU_##dest = (GPU##dest) GPU__##dest;

#define LoadGpuSymN(dest, name) \
	LoadSym(GPU_##dest, GPU##dest, name, FALSE);

static int LoadGPUplugin(const char *GPUdll) {
	void *drv;

	hGPUDriver = SysLoadLibrary(GPUdll);
	if (hGPUDriver == NULL) {
		SysMessage (_("Could not load GPU plugin %s!"), GPUdll); return -1;
	}
	drv = hGPUDriver;
	LoadGpuSym1(init, "GPUinit");
	LoadGpuSym1(shutdown, "GPUshutdown");
	LoadGpuSym1(open, "GPUopen");
	LoadGpuSym1(close, "GPUclose");
	LoadGpuSym1(readData, "GPUreadData");
	LoadGpuSym1(readDataMem, "GPUreadDataMem");
	LoadGpuSym1(readStatus, "GPUreadStatus");
	LoadGpuSym1(writeData, "GPUwriteData");
	LoadGpuSym1(writeDataMem, "GPUwriteDataMem");
	LoadGpuSym1(writeStatus, "GPUwriteStatus");
	LoadGpuSym1(dmaChain, "GPUdmaChain");
	LoadGpuSym1(updateLace, "GPUupdateLace");
	LoadGpuSym1(freeze, "GPUfreeze");
	LoadGpuSym0(vBlank, "GPUvBlank");
	LoadGpuSym0(getScreenInfo, "GPUgetScreenInfo");

	return 0;
}

int CDR__getStatus(struct CdrStat *stat) {
	if (cdOpenCaseTime < 0 || cdOpenCaseTime > (s64)time(NULL))
		stat->Status = 0x10;
	else
		stat->Status = 0;

	return 0;
}

static void *hSPUDriver = NULL;
static void CALLBACK SPU__registerScheduleCb(void (CALLBACK *cb)(unsigned int)) {}
static void CALLBACK SPU__setCDvol(unsigned char ll, unsigned char lr,
		unsigned char rl, unsigned char rr, unsigned int cycle) {}

#define LoadSpuSym1(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, TRUE);

#define LoadSpuSym0(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, FALSE); \
	if (SPU_##dest == NULL) SPU_##dest = SPU__##dest;

#define LoadSpuSymN(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, FALSE);

static int LoadSPUplugin(const char *SPUdll) {
	void *drv;

	hSPUDriver = SysLoadLibrary(SPUdll);
	if (hSPUDriver == NULL) {
		SysMessage (_("Could not load SPU plugin %s!"), SPUdll); return -1;
	}
	drv = hSPUDriver;
	LoadSpuSym1(init, "SPUinit");
	LoadSpuSym1(shutdown, "SPUshutdown");
	LoadSpuSym1(open, "SPUopen");
	LoadSpuSym1(close, "SPUclose");
	LoadSpuSym1(writeRegister, "SPUwriteRegister");
	LoadSpuSym1(readRegister, "SPUreadRegister");
	LoadSpuSym1(writeDMAMem, "SPUwriteDMAMem");
	LoadSpuSym1(readDMAMem, "SPUreadDMAMem");
	LoadSpuSym1(playADPCMchannel, "SPUplayADPCMchannel");
	LoadSpuSym1(freeze, "SPUfreeze");
	LoadSpuSym1(registerCallback, "SPUregisterCallback");
	LoadSpuSym0(registerScheduleCb, "SPUregisterScheduleCb");
	LoadSpuSymN(async, "SPUasync");
	LoadSpuSymN(playCDDAchannel, "SPUplayCDDAchannel");
	LoadSpuSym0(setCDvol, "SPUsetCDvol");

	return 0;
}

#ifdef ENABLE_SIO1API

void *hSIO1Driver = NULL;

long CALLBACK SIO1__init(void) { return 0; }
long CALLBACK SIO1__shutdown(void) { return 0; }
long CALLBACK SIO1__open(void) { return 0; }
long CALLBACK SIO1__close(void) { return 0; }
long CALLBACK SIO1__configure(void) { return 0; }
long CALLBACK SIO1__test(void) { return 0; }
void CALLBACK SIO1__about(void) {}
void CALLBACK SIO1__pause(void) {}
void CALLBACK SIO1__resume(void) {}
long CALLBACK SIO1__keypressed(int key) { return 0; }
void CALLBACK SIO1__writeData8(unsigned char val) {}
void CALLBACK SIO1__writeData16(unsigned short val) {}
void CALLBACK SIO1__writeData32(unsigned long val) {}
void CALLBACK SIO1__writeStat16(unsigned short val) {}
void CALLBACK SIO1__writeStat32(unsigned long val) {}
void CALLBACK SIO1__writeMode16(unsigned short val) {}
void CALLBACK SIO1__writeMode32(unsigned long val) {}
void CALLBACK SIO1__writeCtrl16(unsigned short val) {}
void CALLBACK SIO1__writeCtrl32(unsigned long val) {}
void CALLBACK SIO1__writeBaud16(unsigned short val) {}
void CALLBACK SIO1__writeBaud32(unsigned long val) {}
unsigned char CALLBACK SIO1__readData8(void) { return 0; }
unsigned short CALLBACK SIO1__readData16(void) { return 0; }
unsigned long CALLBACK SIO1__readData32(void) { return 0; }
unsigned short CALLBACK SIO1__readStat16(void) { return 0; }
unsigned long CALLBACK SIO1__readStat32(void) { return 0; }
unsigned short CALLBACK SIO1__readMode16(void) { return 0; }
unsigned long CALLBACK SIO1__readMode32(void) { return 0; }
unsigned short CALLBACK SIO1__readCtrl16(void) { return 0; }
unsigned long CALLBACK SIO1__readCtrl32(void) { return 0; }
unsigned short CALLBACK SIO1__readBaud16(void) { return 0; }
unsigned long CALLBACK SIO1__readBaud32(void) { return 0; }
void CALLBACK SIO1__registerCallback(void (CALLBACK *callback)(void)) {};

void CALLBACK SIO1irq(void) {
	psxHu32ref(0x1070) |= SWAPu32(0x100);
}

#define LoadSio1Sym1(dest, name) \
	LoadSym(SIO1_##dest, SIO1##dest, name, TRUE);

#define LoadSio1SymN(dest, name) \
	LoadSym(SIO1_##dest, SIO1##dest, name, FALSE);

#define LoadSio1Sym0(dest, name) \
	LoadSym(SIO1_##dest, SIO1##dest, name, FALSE); \
	if (SIO1_##dest == NULL) SIO1_##dest = (SIO1##dest) SIO1__##dest;

static int LoadSIO1plugin(const char *SIO1dll) {
	void *drv;

	hSIO1Driver = SysLoadLibrary(SIO1dll);
	if (hSIO1Driver == NULL) {
		SysMessage (_("Could not load SIO1 plugin %s!"), SIO1dll); return -1;
	}
	drv = hSIO1Driver;

	LoadSio1Sym0(init, "SIO1init");
	LoadSio1Sym0(shutdown, "SIO1shutdown");
	LoadSio1Sym0(open, "SIO1open");
	LoadSio1Sym0(close, "SIO1close");
	LoadSio1Sym0(pause, "SIO1pause");
	LoadSio1Sym0(resume, "SIO1resume");
	LoadSio1Sym0(keypressed, "SIO1keypressed");
	LoadSio1Sym0(configure, "SIO1configure");
	LoadSio1Sym0(test, "SIO1test");
	LoadSio1Sym0(about, "SIO1about");
	LoadSio1Sym0(writeData8, "SIO1writeData8");
	LoadSio1Sym0(writeData16, "SIO1writeData16");
	LoadSio1Sym0(writeData32, "SIO1writeData32");
	LoadSio1Sym0(writeStat16, "SIO1writeStat16");
	LoadSio1Sym0(writeStat32, "SIO1writeStat32");
	LoadSio1Sym0(writeMode16, "SIO1writeMode16");
	LoadSio1Sym0(writeMode32, "SIO1writeMode32");
	LoadSio1Sym0(writeCtrl16, "SIO1writeCtrl16");
	LoadSio1Sym0(writeCtrl32, "SIO1writeCtrl32");
	LoadSio1Sym0(writeBaud16, "SIO1writeBaud16");
	LoadSio1Sym0(writeBaud32, "SIO1writeBaud32");
	LoadSio1Sym0(readData16, "SIO1readData16");
	LoadSio1Sym0(readData32, "SIO1readData32");
	LoadSio1Sym0(readStat16, "SIO1readStat16");
	LoadSio1Sym0(readStat32, "SIO1readStat32");
	LoadSio1Sym0(readMode16, "SIO1readMode16");
	LoadSio1Sym0(readMode32, "SIO1readMode32");
	LoadSio1Sym0(readCtrl16, "SIO1readCtrl16");
	LoadSio1Sym0(readCtrl32, "SIO1readCtrl32");
	LoadSio1Sym0(readBaud16, "SIO1readBaud16");
	LoadSio1Sym0(readBaud32, "SIO1readBaud32");
	LoadSio1Sym0(registerCallback, "SIO1registerCallback");

	return 0;
}

#endif

int LoadPlugins() {
	char Plugin[MAXPATHLEN * 2];
	int ret;

	ReleasePlugins();
	SysLibError();

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Gpu);
	if (LoadGPUplugin(Plugin) == -1) return -1;

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Spu);
	if (LoadSPUplugin(Plugin) == -1) return -1;

#ifdef ENABLE_SIO1API
	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Sio1);
	if (LoadSIO1plugin(Plugin) == -1) return -1;
#endif

	ret = cdra_init();
	if (ret < 0) { SysMessage (_("Error initializing CD-ROM plugin: %d"), ret); return -1; }
	ret = GPU_init();
	if (ret < 0) { SysMessage (_("Error initializing GPU plugin: %d"), ret); return -1; }
	ret = SPU_init();
	if (ret < 0) { SysMessage (_("Error initializing SPU plugin: %d"), ret); return -1; }

#ifdef ENABLE_SIO1API
	ret = SIO1_init();
	if (ret < 0) { SysMessage (_("Error initializing SIO1 plugin: %d"), ret); return -1; }
#endif

	SysPrintf(_("Plugins loaded.\n"));
	return 0;
}

void ReleasePlugins() {
	cdra_shutdown();
	if (hGPUDriver != NULL) GPU_shutdown();
	if (hSPUDriver != NULL) SPU_shutdown();

	if (hGPUDriver != NULL) { SysCloseLibrary(hGPUDriver); hGPUDriver = NULL; }
	if (hSPUDriver != NULL) { SysCloseLibrary(hSPUDriver); hSPUDriver = NULL; }

#ifdef ENABLE_SIO1API
	if (hSIO1Driver != NULL) {
		SIO1_shutdown();
		SysCloseLibrary(hSIO1Driver);
		hSIO1Driver = NULL;
	}
#endif
}

// for CD swap
int ReloadCdromPlugin()
{
	cdra_shutdown();
	return cdra_init();
}

void SetIsoFile(const char *filename) {
	if (filename == NULL) {
		IsoFile[0] = '\0';
		return;
	}
	strncpy(IsoFile, filename, MAXPATHLEN - 1);
}

const char *GetIsoFile(void) {
	return IsoFile;
}

boolean UsingIso(void) {
	return (IsoFile[0] != '\0');
}

void SetCdOpenCaseTime(s64 time) {
	cdOpenCaseTime = time;
}

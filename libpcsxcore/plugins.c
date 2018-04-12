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

#include "plugins.h"
#include "cdriso.h"
#include "../plugins/dfinput/externals.h"

static char IsoFile[MAXPATHLEN] = "";
static s64 cdOpenCaseTime = 0;

GPUupdateLace         GPU_updateLace;
GPUinit               GPU_init;
GPUshutdown           GPU_shutdown; 
GPUconfigure          GPU_configure;
GPUtest               GPU_test;
GPUabout              GPU_about;
GPUopen               GPU_open;
GPUclose              GPU_close;
GPUreadStatus         GPU_readStatus;
GPUreadData           GPU_readData;
GPUreadDataMem        GPU_readDataMem;
GPUwriteStatus        GPU_writeStatus; 
GPUwriteData          GPU_writeData;
GPUwriteDataMem       GPU_writeDataMem;
GPUdmaChain           GPU_dmaChain;
GPUkeypressed         GPU_keypressed;
GPUdisplayText        GPU_displayText;
GPUmakeSnapshot       GPU_makeSnapshot;
GPUfreeze             GPU_freeze;
GPUgetScreenPic       GPU_getScreenPic;
GPUshowScreenPic      GPU_showScreenPic;
GPUclearDynarec       GPU_clearDynarec;
GPUvBlank             GPU_vBlank;

CDRinit               CDR_init;
CDRshutdown           CDR_shutdown;
CDRopen               CDR_open;
CDRclose              CDR_close; 
CDRtest               CDR_test;
CDRgetTN              CDR_getTN;
CDRgetTD              CDR_getTD;
CDRreadTrack          CDR_readTrack;
CDRgetBuffer          CDR_getBuffer;
CDRplay               CDR_play;
CDRstop               CDR_stop;
CDRgetStatus          CDR_getStatus;
CDRgetDriveLetter     CDR_getDriveLetter;
CDRgetBufferSub       CDR_getBufferSub;
CDRconfigure          CDR_configure;
CDRabout              CDR_about;
CDRsetfilename        CDR_setfilename;
CDRreadCDDA           CDR_readCDDA;
CDRgetTE              CDR_getTE;

SPUconfigure          SPU_configure;
SPUabout              SPU_about;
SPUinit               SPU_init;
SPUshutdown           SPU_shutdown;
SPUtest               SPU_test;
SPUopen               SPU_open;
SPUclose              SPU_close;
SPUplaySample         SPU_playSample;
SPUwriteRegister      SPU_writeRegister;
SPUreadRegister       SPU_readRegister;
SPUwriteDMA           SPU_writeDMA;
SPUreadDMA            SPU_readDMA;
SPUwriteDMAMem        SPU_writeDMAMem;
SPUreadDMAMem         SPU_readDMAMem;
SPUplayADPCMchannel   SPU_playADPCMchannel;
SPUfreeze             SPU_freeze;
SPUregisterCallback   SPU_registerCallback;
SPUregisterScheduleCb SPU_registerScheduleCb;
SPUasync              SPU_async;
SPUplayCDDAchannel    SPU_playCDDAchannel;

PADconfigure          PAD1_configure;
PADabout              PAD1_about;
PADinit               PAD1_init;
PADshutdown           PAD1_shutdown;
PADtest               PAD1_test;
PADopen               PAD1_open;
PADclose              PAD1_close;
PADquery              PAD1_query;
PADreadPort1          PAD1_readPort1;
PADkeypressed         PAD1_keypressed;
PADstartPoll          PAD1_startPoll;
PADpoll               PAD1_poll;
PADsetSensitive       PAD1_setSensitive;

PADconfigure          PAD2_configure;
PADabout              PAD2_about;
PADinit               PAD2_init;
PADshutdown           PAD2_shutdown;
PADtest               PAD2_test;
PADopen               PAD2_open;
PADclose              PAD2_close;
PADquery              PAD2_query;
PADreadPort2          PAD2_readPort2;
PADkeypressed         PAD2_keypressed;
PADstartPoll          PAD2_startPoll;
PADpoll               PAD2_poll;
PADsetSensitive       PAD2_setSensitive;

NETinit               NET_init;
NETshutdown           NET_shutdown;
NETopen               NET_open;
NETclose              NET_close; 
NETtest               NET_test;
NETconfigure          NET_configure;
NETabout              NET_about;
NETpause              NET_pause;
NETresume             NET_resume;
NETqueryPlayer        NET_queryPlayer;
NETsendData           NET_sendData;
NETrecvData           NET_recvData;
NETsendPadData        NET_sendPadData;
NETrecvPadData        NET_recvPadData;
NETsetInfo            NET_setInfo;
NETkeypressed         NET_keypressed;

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
	if (checkerr) { CheckErr(name); } else SysLibError(); \
}

void *hGPUDriver = NULL;

void CALLBACK GPU__displayText(char *pText) {
	SysPrintf("%s\n", pText);
}

long CALLBACK GPU__configure(void) { return 0; }
long CALLBACK GPU__test(void) { return 0; }
void CALLBACK GPU__about(void) {}
void CALLBACK GPU__makeSnapshot(void) {}
void CALLBACK GPU__keypressed(int key) {}
long CALLBACK GPU__getScreenPic(unsigned char *pMem) { return -1; }
long CALLBACK GPU__showScreenPic(unsigned char *pMem) { return -1; }
void CALLBACK GPU__clearDynarec(void (CALLBACK *callback)(void)) {}
void CALLBACK GPU__vBlank(int val) {}

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
		GPU_configure = NULL;
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
	LoadGpuSym0(keypressed, "GPUkeypressed");
	LoadGpuSym0(displayText, "GPUdisplayText");
	LoadGpuSym0(makeSnapshot, "GPUmakeSnapshot");
	LoadGpuSym1(freeze, "GPUfreeze");
	LoadGpuSym0(getScreenPic, "GPUgetScreenPic");
	LoadGpuSym0(showScreenPic, "GPUshowScreenPic");
	LoadGpuSym0(clearDynarec, "GPUclearDynarec");
    LoadGpuSym0(vBlank, "GPUvBlank");
	LoadGpuSym0(configure, "GPUconfigure");
	LoadGpuSym0(test, "GPUtest");
	LoadGpuSym0(about, "GPUabout");

	return 0;
}

void *hCDRDriver = NULL;

long CALLBACK CDR__play(unsigned char *sector) { return 0; }
long CALLBACK CDR__stop(void) { return 0; }

long CALLBACK CDR__getStatus(struct CdrStat *stat) {
	if (cdOpenCaseTime < 0 || cdOpenCaseTime > (s64)time(NULL))
		stat->Status = 0x10;
	else
		stat->Status = 0;

	return 0;
}

char* CALLBACK CDR__getDriveLetter(void) { return NULL; }
long CALLBACK CDR__configure(void) { return 0; }
long CALLBACK CDR__test(void) { return 0; }
void CALLBACK CDR__about(void) {}
long CALLBACK CDR__setfilename(char*filename) { return 0; }

#define LoadCdrSym1(dest, name) \
	LoadSym(CDR_##dest, CDR##dest, name, TRUE);

#define LoadCdrSym0(dest, name) \
	LoadSym(CDR_##dest, CDR##dest, name, FALSE); \
	if (CDR_##dest == NULL) CDR_##dest = (CDR##dest) CDR__##dest;

#define LoadCdrSymN(dest, name) \
	LoadSym(CDR_##dest, CDR##dest, name, FALSE);

static int LoadCDRplugin(const char *CDRdll) {
	void *drv;

	if (CDRdll == NULL) {
		cdrIsoInit();
		return 0;
	}

	hCDRDriver = SysLoadLibrary(CDRdll);
	if (hCDRDriver == NULL) {
		CDR_configure = NULL;
		SysMessage (_("Could not load CD-ROM plugin %s!"), CDRdll);  return -1;
	}
	drv = hCDRDriver;
	LoadCdrSym1(init, "CDRinit");
	LoadCdrSym1(shutdown, "CDRshutdown");
	LoadCdrSym1(open, "CDRopen");
	LoadCdrSym1(close, "CDRclose");
	LoadCdrSym1(getTN, "CDRgetTN");
	LoadCdrSym1(getTD, "CDRgetTD");
	LoadCdrSym1(readTrack, "CDRreadTrack");
	LoadCdrSym1(getBuffer, "CDRgetBuffer");
	LoadCdrSym1(getBufferSub, "CDRgetBufferSub");
	LoadCdrSym0(play, "CDRplay");
	LoadCdrSym0(stop, "CDRstop");
	LoadCdrSym0(getStatus, "CDRgetStatus");
	LoadCdrSym0(getDriveLetter, "CDRgetDriveLetter");
	LoadCdrSym0(configure, "CDRconfigure");
	LoadCdrSym0(test, "CDRtest");
	LoadCdrSym0(about, "CDRabout");
	LoadCdrSym0(setfilename, "CDRsetfilename");
	LoadCdrSymN(readCDDA, "CDRreadCDDA");
	LoadCdrSymN(getTE, "CDRgetTE");

	return 0;
}

void *hSPUDriver = NULL;

long CALLBACK SPU__configure(void) { return 0; }
void CALLBACK SPU__about(void) {}
long CALLBACK SPU__test(void) { return 0; }
void CALLBACK SPU__registerScheduleCb(void (CALLBACK *cb)(unsigned int)) {}

#define LoadSpuSym1(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, TRUE);

#define LoadSpuSym0(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, FALSE); \
	if (SPU_##dest == NULL) SPU_##dest = (SPU##dest) SPU__##dest;

#define LoadSpuSymN(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, FALSE);

static int LoadSPUplugin(const char *SPUdll) {
	void *drv;

	hSPUDriver = SysLoadLibrary(SPUdll);
	if (hSPUDriver == NULL) {
		SPU_configure = NULL;
		SysMessage (_("Could not load SPU plugin %s!"), SPUdll); return -1;
	}
	drv = hSPUDriver;
	LoadSpuSym1(init, "SPUinit");
	LoadSpuSym1(shutdown, "SPUshutdown");
	LoadSpuSym1(open, "SPUopen");
	LoadSpuSym1(close, "SPUclose");
	LoadSpuSym0(configure, "SPUconfigure");
	LoadSpuSym0(about, "SPUabout");
	LoadSpuSym0(test, "SPUtest");
	LoadSpuSym1(writeRegister, "SPUwriteRegister");
	LoadSpuSym1(readRegister, "SPUreadRegister");		
	LoadSpuSym1(writeDMA, "SPUwriteDMA");
	LoadSpuSym1(readDMA, "SPUreadDMA");
	LoadSpuSym1(writeDMAMem, "SPUwriteDMAMem");
	LoadSpuSym1(readDMAMem, "SPUreadDMAMem");
	LoadSpuSym1(playADPCMchannel, "SPUplayADPCMchannel");
	LoadSpuSym1(freeze, "SPUfreeze");
	LoadSpuSym1(registerCallback, "SPUregisterCallback");
	LoadSpuSym0(registerScheduleCb, "SPUregisterScheduleCb");
	LoadSpuSymN(async, "SPUasync");
	LoadSpuSymN(playCDDAchannel, "SPUplayCDDAchannel");

	return 0;
}

void *hPAD1Driver = NULL;
void *hPAD2Driver = NULL;

static int multitap1 = -1;
static int multitap2 = -1;
//Pad information, keystate, mode, config mode, vibration
static PadDataS pad[8];

static int reqPos, respSize, req;
static int ledStateReq44[8];

static unsigned char buf[256];
static unsigned char bufMulti[34] = { 0x80, 0x5a, 
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
									
unsigned char stdpar[8] = { 0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned char multitappar[34] = { 0x80, 0x5a, 
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
									
//response for request 44, 45, 46, 47, 4C, 4D
static unsigned char resp45[8]    = {0xF3, 0x5A, 0x01, 0x02, 0x00, 0x02, 0x01, 0x00};
static unsigned char resp46_00[8] = {0xF3, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A};
static unsigned char resp46_01[8] = {0xF3, 0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14};
static unsigned char resp47[8]    = {0xF3, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
static unsigned char resp4C_00[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
static unsigned char resp4C_01[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00};
static unsigned char resp4D[8]    = {0xF3, 0x5A, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};

//fixed reponse of request number 41, 48, 49, 4A, 4B, 4E, 4F
static unsigned char resp40[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp41[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp43[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp44[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp49[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp4A[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp4B[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp4E[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char resp4F[8] = {0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Resquest of psx core
enum {
	// REQUEST
	// first call of this request for the pad, the pad is configured as an digital pad.
	// 0x0X, 0x42, 0x0Y, 0xZZ, 0xAA, 0x00, 0x00, 0x00, 0x00
	// X pad number (used for the multitap, first request response 0x00, 0x80, 0x5A, (8 bytes pad A), (8 bytes pad B), (8 bytes pad C), (8 bytes pad D)
	// Y if 1 : psx request the full length response for the multitap, 3 bytes header and 4 block of 8 bytes per pad
	// Y if 0 : psx request a pad key state
	// ZZ rumble small motor 00-> OFF, 01 -> ON
	// AA rumble large motor speed 0x00 -> 0xFF
	// RESPONSE
	// header 3 Bytes
	// 0x00 
	// PadId -> 0x41 for digital pas, 0x73 for analog pad 
	// 0x5A mode has not change (no press on analog button on the center of pad), 0x00 the analog button have been pressed and the mode switch
	// 6 Bytes for keystates
	CMD_READ_DATA_AND_VIBRATE = 0x42,
	
	// REQUEST
	// Header
	// 0x0N, 0x43, 0x00, XX, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	// XX = 00 -> Normal mode : Seconde bytes of response = padId
	// XX = 01 -> Configuration mode : Seconde bytes of response = 0xF3
	// RESPONSE
	// enter in config mode example : 
	// req : 01 43 00 01 00 00 00 00 00 00
	// res : 00 41 5A buttons state, analog states
	// exit config mode : 
	// req : 01 43 00 00 00 00 00 00 00 00
	// res : 00 F3 5A buttons state, analog states
	CMD_CONFIG_MODE = 0x43,
	
	// Set led State
	// REQUEST
	// 0x0N, 0x44, 0x00, VAL, SEL, 0x00, 0x00, 0x00, 0x00
	// If sel = 2 then
	// VAL = 00 -> OFF
	// VAL = 01 -> ON
	// RESPONSE
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	CMD_SET_MODE_AND_LOCK = 0x44,
	
	// Get Analog Led state
	// REQUEST
	// 0x0N, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// 0x00, 0xF3, 0x5A, 0x01, 0x02, VAL, 0x02, 0x01, 0x00
	// VAL = 00 Led OFF
	// VAL = 01 Led ON
	CMD_QUERY_MODEL_AND_MODE = 0x45,
	
	//Get Variable A
	// REQUEST
	// 0x0N, 0x46, 0x00, 0xXX, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// XX=00
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A
	// XX=01
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14
	CMD_QUERY_ACT = 0x46,
	
	// REQUEST
	// 0x0N, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00
	CMD_QUERY_COMB = 0x47,
	
	// REQUEST
	// 0x0N, 0x4C, 0x00, 0xXX, 0x00, 0x00, 0x00, 0x00, 0x00
	// RESPONSE
	// XX = 0
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00
	// XX = 1
	// 0x00, 0xF3, 0x5A, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00
	CMD_QUERY_MODE = 0x4C,
	
	// REQUEST
	// 0x0N, 0x4D, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	// RESPONSE
	// 0x00, 0xF3, 0x5A, old value or
	// AA = 01 unlock large motor (and swap VAL1 and VAL2)
	// BB = 01 unlock large motor (default)
	// CC, DD, EE, FF = all FF -> unlock small motor
	//
	// default repsonse for analog pad with 2 motor : 0x00 0xF3 0x5A 0x00 0x01 0xFF 0xFF 0xFF 0xFF
	//
	CMD_VIBRATION_TOGGLE = 0x4D,
	REQ40 = 0x40,
	REQ41 = 0x41,
	REQ49 = 0x49,
	REQ4A = 0x4A,
	REQ4B = 0x4B,
	REQ4E = 0x4E,
	REQ4F = 0x4F
};




//NO MULTITAP

void initBufForRequest(int padIndex, char value){
	switch (value){
		//Pad keystate already in buffer
		//case CMD_READ_DATA_AND_VIBRATE :
		//	break;
		case CMD_CONFIG_MODE :
			if (pad[padIndex].configMode == 1) {
				memcpy(buf, resp43, 8);
				break;
			}
			//else, not in config mode, pad keystate return (already in the buffer)
			break;
		case CMD_SET_MODE_AND_LOCK :
			memcpy(buf, resp44, 8);
			break;
		case CMD_QUERY_MODEL_AND_MODE :
			memcpy(buf, resp45, 8);
			break;
		case CMD_QUERY_ACT :
			memcpy(buf, resp46_00, 8);
			break;
		case CMD_QUERY_COMB :
			memcpy(buf, resp47, 8);
			break;
		case CMD_QUERY_MODE :
			memcpy(buf, resp4C_00, 8);
			break;
		case CMD_VIBRATION_TOGGLE :
			memcpy(buf, resp4D, 8);
			break;
		case REQ40 :
			memcpy(buf, resp40, 8);
			break;
		case REQ41 :
			memcpy(buf, resp41, 8);
			break;
		case REQ49 :
			memcpy(buf, resp49, 8);
			break;
		case REQ4A :
			memcpy(buf, resp4A, 8);
			break;
		case REQ4B :
			memcpy(buf, resp4B, 8);
			break;
		case REQ4E :
			memcpy(buf, resp4E, 8);
			break;
		case REQ4F :
			memcpy(buf, resp4F, 8);
			break;
	}
}




void reqIndex2Treatment(int padIndex, char value){
	switch (req){
		case CMD_CONFIG_MODE :
			//0x43
			if (value == 0) {
				pad[padIndex].configMode = 0;
			} else {
				pad[padIndex].configMode = 1;
			}
			break;
		case CMD_SET_MODE_AND_LOCK :
			//0x44 store the led state for change mode if the next value = 0x02
			//0x01 analog ON
			//0x00 analog OFF
			ledStateReq44[padIndex] = value;
			break;
		case CMD_QUERY_ACT :
			//0x46
			if (value == 1) {
				memcpy(buf, resp46_01, 8);
			}
			break;
		case CMD_QUERY_MODE :
			if (value == 1) {
				memcpy(buf, resp4C_01, 8);
			}
			break;
		case CMD_VIBRATION_TOGGLE :
			//0x4D
			memcpy(buf, resp4D, 8);
			break;
		case CMD_READ_DATA_AND_VIBRATE:
			//mem the vibration value for small motor;
			pad[padIndex].Vib[0] = value;
			break;
	}
}
	
void vibrate(int padIndex){
	if (pad[padIndex].Vib[0] != pad[padIndex].VibF[0] || pad[padIndex].Vib[1] != pad[padIndex].VibF[1]) {
		//value is different update Value and call libretro for vibration
		pad[padIndex].VibF[0] = pad[padIndex].Vib[0];
		pad[padIndex].VibF[1] = pad[padIndex].Vib[1];
		plat_trigger_vibrate(padIndex, pad[padIndex].VibF[0], pad[padIndex].VibF[1]);
		//printf("vibration pad %i", padIndex);
	}
}




//Build response for 0x42 request Pad in port
void _PADstartPoll(PadDataS *pad) {
    switch (pad->controllerType) {
        case PSE_PAD_TYPE_MOUSE:
			stdpar[0] = 0x12;
            stdpar[2] = pad->buttonStatus & 0xff;
            stdpar[3] = pad->buttonStatus >> 8;
            stdpar[4] = pad->moveX;
            stdpar[5] = pad->moveY;
            memcpy(buf, stdpar, 6);
            respSize = 6;
            break;
        case PSE_PAD_TYPE_NEGCON: // npc101/npc104(slph00001/slph00069)
            stdpar[0] = 0x23;
            stdpar[2] = pad->buttonStatus & 0xff;
            stdpar[3] = pad->buttonStatus >> 8;
            stdpar[4] = pad->rightJoyX;
            stdpar[5] = pad->rightJoyY;
            stdpar[6] = pad->leftJoyX;
            stdpar[7] = pad->leftJoyY;
            memcpy(buf, stdpar, 8);
            respSize = 8;
            break;
        case PSE_PAD_TYPE_ANALOGPAD: // scph1150
            stdpar[0] = 0x73;
            stdpar[2] = pad->buttonStatus & 0xff;
            stdpar[3] = pad->buttonStatus >> 8;
            stdpar[4] = pad->rightJoyX;
            stdpar[5] = pad->rightJoyY;
            stdpar[6] = pad->leftJoyX;
            stdpar[7] = pad->leftJoyY;
            memcpy(buf, stdpar, 8);
            respSize = 8;
            break;
        case PSE_PAD_TYPE_ANALOGJOY: // scph1110
            stdpar[0] = 0x53;
            stdpar[2] = pad->buttonStatus & 0xff;
            stdpar[3] = pad->buttonStatus >> 8;
            stdpar[4] = pad->rightJoyX;
            stdpar[5] = pad->rightJoyY;
            stdpar[6] = pad->leftJoyX;
            stdpar[7] = pad->leftJoyY;
            memcpy(buf, stdpar, 8);
            respSize = 8;
            break;
        case PSE_PAD_TYPE_STANDARD:
        default:
        	stdpar[0] = 0x41;
            stdpar[2] = pad->buttonStatus & 0xff;
            stdpar[3] = pad->buttonStatus >> 8;
            //avoid analog value in multitap mode if change pad type in game.
            stdpar[4] = 0xff;
            stdpar[5] = 0xff;
            stdpar[6] = 0xff;
            stdpar[7] = 0xff;
        	memcpy(buf, stdpar, 8);
        	respSize = 8;
    }
}


//Build response for 0x42 request Multitap in port
//Response header for multitap : 0x80, 0x5A, (Pad information port 1-2A), (Pad information port 1-2B), (Pad information port 1-2C), (Pad information port 1-2D)
void _PADstartPollMultitap(PadDataS* padd) {
    int i, offset;
    for(i = 0; i < 4; i++) {
    	offset = 2 + (i * 8);
	_PADstartPoll(&padd[i]);
	memcpy(multitappar+offset, stdpar, 8);
    }
    memcpy(bufMulti, multitappar, 34);
    respSize = 34;
}


unsigned char _PADpoll(int port, unsigned char value) {
	if (reqPos == 0) {
		//mem the request number
		req = value;
		//copy the default value of request response in buffer instead of the keystate
		initBufForRequest(port, value);
	}
	
	//if no new request the pad return 0xff, for signaling connected
	if (reqPos >= respSize) return 0xff;
	
	switch(reqPos){
		case 2:
			reqIndex2Treatment(port, value);
		break;
		case 3:
			switch(req) {
				case CMD_SET_MODE_AND_LOCK :
					//change mode on pad
				break;
				case CMD_READ_DATA_AND_VIBRATE:
				//mem the vibration value for Large motor;
				pad[port].Vib[1] = value;
				//vibration
				vibrate(port);
				break;
			}
		break;
	}
	return buf[reqPos++];
}


unsigned char _PADpollMultitap(int port, unsigned char value) {
	if (reqPos >= respSize) return 0xff;
	return bufMulti[reqPos++];
}


// refresh the button state on port 1.
// int pad is not needed.
unsigned char CALLBACK PAD1__startPoll(int pad) {
	reqPos = 0;
	// first call the pad provide if a multitap is connected between the psx and himself
	if (multitap1 == -1) {
		PadDataS padd;
		padd.requestPadIndex = 0;
		PAD1_readPort1(&padd);
		multitap1 = padd.portMultitap;
	}
	// just one pad is on port 1 : NO MULTITAP
	if (multitap1 == 0) {
		PadDataS padd;
		padd.requestPadIndex = 0;
		PAD1_readPort1(&padd);
		_PADstartPoll(&padd);
	} else {
		// a multitap is plugged : refresh all pad.
		int i;
		PadDataS padd[4];
		for(i = 0; i < 4; i++) {
			padd[i].requestPadIndex = i;
			PAD1_readPort1(&padd[i]);
		}
		_PADstartPollMultitap(padd);
	}
	//printf("\npad 1 : ");
	return 0x00;
}

unsigned char CALLBACK PAD1__poll(unsigned char value) {
	char tmp;
	if (multitap1 == 1) {
		tmp = _PADpollMultitap(0, value);
	} else {
		tmp = _PADpoll(0, value);
	}
	//printf("%2x:%2x, ",value,tmp);
	return tmp;
	
}


long CALLBACK PAD1__configure(void) { return 0; }
void CALLBACK PAD1__about(void) {}
long CALLBACK PAD1__test(void) { return 0; }
long CALLBACK PAD1__query(void) { return 3; }
long CALLBACK PAD1__keypressed() { return 0; }

#define LoadPad1Sym1(dest, name) \
	LoadSym(PAD1_##dest, PAD##dest, name, TRUE);

#define LoadPad1SymN(dest, name) \
	LoadSym(PAD1_##dest, PAD##dest, name, FALSE);

#define LoadPad1Sym0(dest, name) \
	LoadSym(PAD1_##dest, PAD##dest, name, FALSE); \
	if (PAD1_##dest == NULL) PAD1_##dest = (PAD##dest) PAD1__##dest;

static int LoadPAD1plugin(const char *PAD1dll) {
	void *drv;

	hPAD1Driver = SysLoadLibrary(PAD1dll);
	if (hPAD1Driver == NULL) {
		PAD1_configure = NULL;
		SysMessage (_("Could not load Controller 1 plugin %s!"), PAD1dll); return -1;
	}
	drv = hPAD1Driver;
	LoadPad1Sym1(init, "PADinit");
	LoadPad1Sym1(shutdown, "PADshutdown");
	LoadPad1Sym1(open, "PADopen");
	LoadPad1Sym1(close, "PADclose");
	LoadPad1Sym0(query, "PADquery");
	LoadPad1Sym1(readPort1, "PADreadPort1");
	LoadPad1Sym0(configure, "PADconfigure");
	LoadPad1Sym0(test, "PADtest");
	LoadPad1Sym0(about, "PADabout");
	LoadPad1Sym0(keypressed, "PADkeypressed");
	LoadPad1Sym0(startPoll, "PADstartPoll");
	LoadPad1Sym0(poll, "PADpoll");
	LoadPad1SymN(setSensitive, "PADsetSensitive");

	return 0;
}

unsigned char CALLBACK PAD2__startPoll(int pad) {
	int pad_index;

	reqPos = 0;
	if (multitap1 == 0 && (multitap2 == 0 || multitap2 == 2)) {
		pad_index = 1;
	} else if(multitap1 == 1 && (multitap2 == 0 || multitap2 == 2)) {
		pad_index = 4;
	} else {
		pad_index = 0;
	}

	//first call the pad provide if a multitap is connected between the psx and himself
	if (multitap2 == -1) {
		PadDataS padd;
		padd.requestPadIndex = pad_index;
		PAD2_readPort2(&padd);
		multitap2 = padd.portMultitap;
	}
	
	// just one pad is on port 1 : NO MULTITAP
	if (multitap2 == 0) {
		PadDataS padd;
		padd.requestPadIndex = pad_index;
		PAD2_readPort2(&padd);
		_PADstartPoll(&padd);
	} else {
		// a multitap is plugged : refresh all pad.
		int i;
		PadDataS padd[4];
		for(i = 0; i < 4; i++) {
			padd[i].requestPadIndex = i+pad_index;
			PAD2_readPort2(&padd[i]);
		}
		_PADstartPollMultitap(padd);
	}
	//printf("\npad 2 : ");
	return 0x00;
}

unsigned char CALLBACK PAD2__poll(unsigned char value) {
	char tmp;
	if (multitap2 == 2) {
		tmp = _PADpollMultitap(1, value);
	} else {
		tmp = _PADpoll(1, value);
	}
	//printf("%2x:%2x, ",value,tmp);
	return tmp;
}

long CALLBACK PAD2__configure(void) { return 0; }
void CALLBACK PAD2__about(void) {}
long CALLBACK PAD2__test(void) { return 0; }
long CALLBACK PAD2__query(void) { return PSE_PAD_USE_PORT1 | PSE_PAD_USE_PORT2; }
long CALLBACK PAD2__keypressed() { return 0; }

#define LoadPad2Sym1(dest, name) \
	LoadSym(PAD2_##dest, PAD##dest, name, TRUE);

#define LoadPad2Sym0(dest, name) \
	LoadSym(PAD2_##dest, PAD##dest, name, FALSE); \
	if (PAD2_##dest == NULL) PAD2_##dest = (PAD##dest) PAD2__##dest;

#define LoadPad2SymN(dest, name) \
	LoadSym(PAD2_##dest, PAD##dest, name, FALSE);

static int LoadPAD2plugin(const char *PAD2dll) {
	void *drv;

	hPAD2Driver = SysLoadLibrary(PAD2dll);
	if (hPAD2Driver == NULL) {
		PAD2_configure = NULL;
		SysMessage (_("Could not load Controller 2 plugin %s!"), PAD2dll); return -1;
	}
	drv = hPAD2Driver;
	LoadPad2Sym1(init, "PADinit");
	LoadPad2Sym1(shutdown, "PADshutdown");
	LoadPad2Sym1(open, "PADopen");
	LoadPad2Sym1(close, "PADclose");
	LoadPad2Sym0(query, "PADquery");
	LoadPad2Sym1(readPort2, "PADreadPort2");
	LoadPad2Sym0(configure, "PADconfigure");
	LoadPad2Sym0(test, "PADtest");
	LoadPad2Sym0(about, "PADabout");
	LoadPad2Sym0(keypressed, "PADkeypressed");
	LoadPad2Sym0(startPoll, "PADstartPoll");
	LoadPad2Sym0(poll, "PADpoll");
	LoadPad2SymN(setSensitive, "PADsetSensitive");

	return 0;
}

void *hNETDriver = NULL;

void CALLBACK NET__setInfo(netInfo *info) {}
void CALLBACK NET__keypressed(int key) {}
long CALLBACK NET__configure(void) { return 0; }
long CALLBACK NET__test(void) { return 0; }
void CALLBACK NET__about(void) {}

#define LoadNetSym1(dest, name) \
	LoadSym(NET_##dest, NET##dest, name, TRUE);

#define LoadNetSymN(dest, name) \
	LoadSym(NET_##dest, NET##dest, name, FALSE);

#define LoadNetSym0(dest, name) \
	LoadSym(NET_##dest, NET##dest, name, FALSE); \
	if (NET_##dest == NULL) NET_##dest = (NET##dest) NET__##dest;

static int LoadNETplugin(const char *NETdll) {
	void *drv;

	hNETDriver = SysLoadLibrary(NETdll);
	if (hNETDriver == NULL) {
		SysMessage (_("Could not load NetPlay plugin %s!"), NETdll); return -1;
	}
	drv = hNETDriver;
	LoadNetSym1(init, "NETinit");
	LoadNetSym1(shutdown, "NETshutdown");
	LoadNetSym1(open, "NETopen");
	LoadNetSym1(close, "NETclose");
	LoadNetSymN(sendData, "NETsendData");
	LoadNetSymN(recvData, "NETrecvData");
	LoadNetSym1(sendPadData, "NETsendPadData");
	LoadNetSym1(recvPadData, "NETrecvPadData");
	LoadNetSym1(queryPlayer, "NETqueryPlayer");
	LoadNetSym1(pause, "NETpause");
	LoadNetSym1(resume, "NETresume");
	LoadNetSym0(setInfo, "NETsetInfo");
	LoadNetSym0(keypressed, "NETkeypressed");
	LoadNetSym0(configure, "NETconfigure");
	LoadNetSym0(test, "NETtest");
	LoadNetSym0(about, "NETabout");

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

void CALLBACK clearDynarec(void) {
	psxCpu->Reset();
}

int LoadPlugins() {
	int ret;
	char Plugin[MAXPATHLEN];

	ReleasePlugins();
	SysLibError();

	if (UsingIso()) {
		LoadCDRplugin(NULL);
	} else {
		sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Cdr);
		if (LoadCDRplugin(Plugin) == -1) return -1;
	}

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Gpu);
	if (LoadGPUplugin(Plugin) == -1) return -1;

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Spu);
	if (LoadSPUplugin(Plugin) == -1) return -1;

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Pad1);
	if (LoadPAD1plugin(Plugin) == -1) return -1;

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Pad2);
	if (LoadPAD2plugin(Plugin) == -1) return -1;

	if (strcmp("Disabled", Config.Net) == 0 || strcmp("", Config.Net) == 0)
		Config.UseNet = FALSE;
	else {
		Config.UseNet = TRUE;
		sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Net);
		if (LoadNETplugin(Plugin) == -1) Config.UseNet = FALSE;
	}

#ifdef ENABLE_SIO1API
	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Sio1);
	if (LoadSIO1plugin(Plugin) == -1) return -1;
#endif

	ret = CDR_init();
	if (ret < 0) { SysMessage (_("Error initializing CD-ROM plugin: %d"), ret); return -1; }
	ret = GPU_init();
	if (ret < 0) { SysMessage (_("Error initializing GPU plugin: %d"), ret); return -1; }
	ret = SPU_init();
	if (ret < 0) { SysMessage (_("Error initializing SPU plugin: %d"), ret); return -1; }
	ret = PAD1_init(1);
	if (ret < 0) { SysMessage (_("Error initializing Controller 1 plugin: %d"), ret); return -1; }
	ret = PAD2_init(2);
	if (ret < 0) { SysMessage (_("Error initializing Controller 2 plugin: %d"), ret); return -1; }

	if (Config.UseNet) {
		ret = NET_init();
		if (ret < 0) { SysMessage (_("Error initializing NetPlay plugin: %d"), ret); return -1; }
	}

#ifdef ENABLE_SIO1API
	ret = SIO1_init();
	if (ret < 0) { SysMessage (_("Error initializing SIO1 plugin: %d"), ret); return -1; }
#endif

	SysPrintf(_("Plugins loaded.\n"));
	return 0;
}

void ReleasePlugins() {
	if (Config.UseNet) {
		int ret = NET_close();
		if (ret < 0) Config.UseNet = FALSE;
	}
	NetOpened = FALSE;

	if (hCDRDriver != NULL || cdrIsoActive()) CDR_shutdown();
	if (hGPUDriver != NULL) GPU_shutdown();
	if (hSPUDriver != NULL) SPU_shutdown();
	if (hPAD1Driver != NULL) PAD1_shutdown();
	if (hPAD2Driver != NULL) PAD2_shutdown();

	if (Config.UseNet && hNETDriver != NULL) NET_shutdown(); 

	if (hCDRDriver != NULL) SysCloseLibrary(hCDRDriver); hCDRDriver = NULL;
	if (hGPUDriver != NULL) SysCloseLibrary(hGPUDriver); hGPUDriver = NULL;
	if (hSPUDriver != NULL) SysCloseLibrary(hSPUDriver); hSPUDriver = NULL;
	if (hPAD1Driver != NULL) SysCloseLibrary(hPAD1Driver); hPAD1Driver = NULL;
	if (hPAD2Driver != NULL) SysCloseLibrary(hPAD2Driver); hPAD2Driver = NULL;

	if (Config.UseNet && hNETDriver != NULL) {
		SysCloseLibrary(hNETDriver); hNETDriver = NULL;
	}

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
	if (hCDRDriver != NULL || cdrIsoActive()) CDR_shutdown();
	if (hCDRDriver != NULL) SysCloseLibrary(hCDRDriver); hCDRDriver = NULL;

	if (UsingIso()) {
		LoadCDRplugin(NULL);
	} else {
		char Plugin[MAXPATHLEN];
		sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Cdr);
		if (LoadCDRplugin(Plugin) == -1) return -1;
	}

	return CDR_init();
}

void SetIsoFile(const char *filename) {
	if (filename == NULL) {
		IsoFile[0] = '\0';
		return;
	}
	strncpy(IsoFile, filename, MAXPATHLEN);
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

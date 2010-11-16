/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2002  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "psxcommon.h"
#include "plugins.h"
#include "spu.h"

void OnFile_Exit();

unsigned long gpuDisp;

long SPU__open(void) {
	return SPU_open();
}

int StatesC = 0;
extern int UseGui;
int ShowPic=0;

void gpuShowPic() {
}

void PADhandleKey(int key) {
}

long PAD1__open(void) {
	return PAD1_open(&gpuDisp);
}

long PAD2__open(void) {
	return PAD2_open(&gpuDisp);
}

void OnFile_Exit();

void SignalExit(int sig) {
	ClosePlugins();
	OnFile_Exit();
}

void SPUirq(void);

#define PARSEPATH(dst, src) \
	ptr = src + strlen(src); \
	while (*ptr != '\\' && ptr != src) ptr--; \
	if (ptr != src) { \
		strcpy(dst, ptr+1); \
	}

int _OpenPlugins() {
	static char path[1024];
	CFURLRef pathUrl;
	int ret;

	//signal(SIGINT, SignalExit);
	//signal(SIGPIPE, SignalExit);

	GPU_clearDynarec(clearDynarec);
	
	pathUrl = CFBundleCopyResourceURL(CFBundleGetMainBundle(), CFSTR("gpuPeopsSoftX.cfg"), NULL, NULL);
	if (pathUrl)
		CFURLGetFileSystemRepresentation(pathUrl, true, path, 1024);
	
	ret = CDR_open();
	if (ret < 0) { SysMessage(_("Error Opening CDR Plugin")); return -1; }
	ret = SPU_open();
	if (ret < 0) { SysMessage(_("Error Opening SPU Plugin")); return -1; }
	SPU_registerCallback(SPUirq);
	ret = GPU_open(&gpuDisp, "PCSX", /*pathUrl ? path :*/ NULL);
	if (ret < 0) { SysMessage(_("Error Opening GPU Plugin")); return -1; }
	ret = PAD1_open(&gpuDisp);
	if (ret < 0) { SysMessage(_("Error Opening PAD1 Plugin")); return -1; }
	ret = PAD2_open(&gpuDisp);
	if (ret < 0) { SysMessage(_("Error Opening PAD2 Plugin")); return -1; }

	return 0;
}

int OpenPlugins() {
	int ret;

	while ((ret = _OpenPlugins()) == -2) {
		ReleasePlugins();
		LoadMcds(Config.Mcd1, Config.Mcd2);
		if (LoadPlugins() == -1) return -1;
	}
	return ret;	
}

void ClosePlugins() {
	int ret;

	//signal(SIGINT, SIG_DFL);
	//signal(SIGPIPE, SIG_DFL);
	ret = CDR_close();
	if (ret < 0) { SysMessage(_("Error Closing CDR Plugin")); return; }
	ret = SPU_close();
	if (ret < 0) { SysMessage(_("Error Closing SPU Plugin")); return; }
	ret = PAD1_close();
	if (ret < 0) { SysMessage(_("Error Closing PAD1 Plugin")); return; }
	ret = PAD2_close();
	if (ret < 0) { SysMessage(_("Error Closing PAD2 Plugin")); return; }
	ret = GPU_close();
	if (ret < 0) { SysMessage(_("Error Closing GPU Plugin")); return; }
}

void ResetPlugins() {
	int ret;

	CDR_shutdown();
	GPU_shutdown();
	SPU_shutdown();
	PAD1_shutdown();
	PAD2_shutdown();

	ret = CDR_init();
	if (ret < 0) { SysMessage(_("CDRinit error: %d"), ret); return; }
	ret = GPU_init();
	if (ret < 0) { SysMessage(_("GPUinit error: %d"), ret); return; }
	ret = SPU_init();
	if (ret < 0) { SysMessage(_("SPUinit error: %d"), ret); return; }
	ret = PAD1_init(1);
	if (ret < 0) { SysMessage(_("PAD1init error: %d"), ret); return; }
	ret = PAD2_init(2);
	if (ret < 0) { SysMessage(_("PAD2init error: %d"), ret); return; }

	NetOpened = FALSE;
}


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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <X11/keysym.h>
#include <signal.h>

#include "Linux.h"

#include "../libpcsxcore/plugins.h"
#include "../libpcsxcore/spu.h"
#include "../libpcsxcore/cdriso.h"

#include "nopic.h"

#define MAX_SLOTS 5	/* ADB TODO Same as Gtk2Gui.c */

void OnFile_Exit();

unsigned long gpuDisp;

int StatesC = 0;
extern int UseGui;
int ShowPic = 0;

void gpuShowPic() {
	gchar *state_filename;
	gzFile f;

	if (!ShowPic) {
		unsigned char *pMem;

		pMem = (unsigned char *) malloc(128*96*3);
		if (pMem == NULL) return;

		state_filename = get_state_filename (StatesC);

		GPU_freeze(2, (GPUFreeze_t *)&StatesC);

		f = gzopen(state_filename, "rb");
		if (f != NULL) {
			gzseek(f, 32, SEEK_SET); // skip header
			gzread(f, pMem, 128*96*3);
			gzclose(f);
		} else {
			memcpy(pMem, NoPic_Image.pixel_data, 128*96*3);
			DrawNumBorPic(pMem, StatesC+1);
		}
		GPU_showScreenPic(pMem);

		free(pMem);
		ShowPic = 1;
		g_free (state_filename);
	} else {
		GPU_showScreenPic(NULL);
		ShowPic = 0;
	}
}

void KeyStateSave(int i) {
	gchar *state_filename;

	state_filename = get_state_filename (i);
	state_save (state_filename);

	g_free (state_filename);
}

void KeyStateLoad(int i) {
	gchar *state_filename;

	state_filename = get_state_filename (i);
	state_load (state_filename);

	g_free (state_filename);

	// HACKHACK: prevent crash when using recompiler due to execution not
	// returned from compiled code. This WILL cause memory leak, however a
	// large amount of refactor is needed for a proper fix.
	if (Config.Cpu == CPU_DYNAREC) psxCpu->Execute();
}

static short modctrl = 0, modalt = 0;

/* Handle keyboard keystrokes */
void PADhandleKey(int key) {
	char Text[MAXPATHLEN];
	gchar *state_filename;

	short rel = 0;	//released key flag

	if (key == 0)
		return;

	if ((key >> 30) & 1)	//specific to dfinput (padJoy)
		rel = 1;

	if (rel) {
		switch (key & ~0x40000000) {
		case XK_Alt_L:
		case XK_Alt_R:
			modalt=0;
			break;
		case XK_Control_L:
		case XK_Control_R:
			modctrl=0;
			break;
		}
		return;
	}

	switch (key) {
		case XK_Alt_L:
		case XK_Alt_R:
			modalt=1;
			break;
		case XK_Control_L:
		case XK_Control_R:
			modctrl=1;
			break;

		case XK_0:
			if (modalt && modctrl)
				return;
			if (modalt) KeyStateLoad(10);
			break;

		case XK_1:
			if (modalt && modctrl)
				return;
			if (modalt) KeyStateLoad(0);
			if (modctrl) KeyStateSave(0);
			break;
		case XK_2:
			if (modalt && modctrl)
				return;
			if (modalt) KeyStateLoad(1);
			if (modctrl) KeyStateSave(1);
			break;
		case XK_3:
			if (modalt && modctrl)
				return;
			if (modalt) KeyStateLoad(2);
			if (modctrl) KeyStateSave(2);
			break;
		case XK_4:
			if (modalt && modctrl)
				return;
			if (modalt) KeyStateLoad(3);
			if (modctrl) KeyStateSave(3);
			break;
		case XK_5:
			if (modalt && modctrl)
				return;
			if (modalt) KeyStateLoad(4);
			if (modctrl) KeyStateSave(4);
			break;

		case XK_F1:
			GPU_freeze(2, (GPUFreeze_t *)&StatesC);
			state_filename = get_state_filename (StatesC);
			state_save (state_filename);

			g_free (state_filename);

			if (ShowPic) { ShowPic = 0; gpuShowPic(); }

			break;
		case XK_F2:
			if (StatesC < (MAX_SLOTS - 1)) StatesC++;
			else StatesC = 0;
			GPU_freeze(2, (GPUFreeze_t *)&StatesC);
			if (ShowPic) { ShowPic = 0; gpuShowPic(); }
			break;
		case XK_F3:
			state_filename = get_state_filename (StatesC);
			state_load (state_filename);

			g_free (state_filename);

			// HACKHACK: prevent crash when using recompiler due to execution not
			// returned from compiled code. This WILL cause memory leak, however a
			// large amount of refactor is needed for a proper fix.
			if (Config.Cpu == CPU_DYNAREC) psxCpu->Execute();

			break;
		case XK_F4:
			gpuShowPic();
			break;
		case XK_F5:
			Config.Sio ^= 0x1;
			if (Config.Sio)
				sprintf(Text, _("SIO IRQ Always Enabled"));
			else sprintf(Text, _("SIO IRQ Not Always Enabled"));
			GPU_displayText(Text);
			break;
		case XK_F6:
			Config.Mdec ^= 0x1;
			if (Config.Mdec)
				sprintf(Text, _("Black & White Mdecs Only Enabled"));
			else sprintf(Text, _("Black & White Mdecs Only Disabled"));
			GPU_displayText(Text);
			break;
		case XK_F7:
			Config.Xa ^= 0x1;
			if (Config.Xa == 0)
				sprintf (Text, _("XA Enabled"));
			else sprintf (Text, _("XA Disabled"));
			GPU_displayText(Text);
			break;
		case XK_F8:
			GPU_makeSnapshot();
			break;
		case XK_F9:
			SetCdOpenCaseTime(-1);
			break;
		case XK_F10:
			SetCdOpenCaseTime(0);
			break;
		case XK_Escape:
			// TODO
			// the architecture is too broken to actually restart the GUI
			// because SysUpdate is called from deep within the actual
			// execution of the emulation code
			// Fixing this would probably require a complete reworking of
			// all functions, so that they return 0 or 1 for success
			// that way, execution wouldn't continue
			if (CdromId[0] != '\0')
				KeyStateSave(10);
			ClosePlugins();
			UpdateMenuSlots();
			if (!UseGui) OnFile_Exit();
			StartGui();
			break;
		case XK_Return:		//0xff0d
			if (modalt)	//alt-return
				//I just made this up: a special sym for fullscreen because the current interface can't handle key mods
				//though it can be used in the future as a convention...eg bit 29 for alt, bit 28 for cntl, etc.
				GPU_keypressed( (1<<29) | 0xFF0D );
			break;
		default:
			GPU_keypressed(key);
			if (Config.UseNet) NET_keypressed(key);
	}	
}

void OnFile_Exit();

void SignalExit(int sig) {
	ClosePlugins();
	OnFile_Exit();
}

#define PARSEPATH(dst, src) \
	ptr = src + strlen(src); \
	while (*ptr != '\\' && ptr != src) ptr--; \
	if (ptr != src) { \
		strcpy(dst, ptr+1); \
	}

int _OpenPlugins() {
	int ret;

	signal(SIGINT, SignalExit);
	signal(SIGPIPE, SignalExit);

	GPU_clearDynarec(clearDynarec);

	ret = CDR_open();
	if (ret < 0) { SysMessage(_("Error opening CD-ROM plugin!")); return -1; }
	ret = SPU_open();
	if (ret < 0) { SysMessage(_("Error opening SPU plugin!")); return -1; }
	SPU_registerCallback(SPUirq);
	ret = GPU_open(&gpuDisp, "PCSX", NULL);
	if (ret < 0) { SysMessage(_("Error opening GPU plugin!")); return -1; }
	ret = PAD1_open(&gpuDisp);
	if (ret < 0) { SysMessage(_("Error opening Controller 1 plugin!")); return -1; }
	ret = PAD2_open(&gpuDisp);
	if (ret < 0) { SysMessage(_("Error opening Controller 2 plugin!")); return -1; }

	if (Config.UseNet && !NetOpened) {
		netInfo info;
		char path[MAXPATHLEN];
		char dotdir[MAXPATHLEN];

		strncpy(dotdir, getenv("HOME"), MAXPATHLEN-100);
		strcat(dotdir, "/.pcsx/plugins/");

		strcpy(info.EmuName, "PCSX " PACKAGE_VERSION);
		strncpy(info.CdromID, CdromId, 9);
		strncpy(info.CdromLabel, CdromLabel, 9);
		info.psxMem = psxM;
		info.GPU_showScreenPic = GPU_showScreenPic;
		info.GPU_displayText = GPU_displayText;
		info.GPU_showScreenPic = GPU_showScreenPic;
		info.PAD_setSensitive = PAD1_setSensitive;
		sprintf(path, "%s%s", Config.BiosDir, Config.Bios);
		strcpy(info.BIOSpath, path);
		strcpy(info.MCD1path, Config.Mcd1);
		strcpy(info.MCD2path, Config.Mcd2);
		sprintf(path, "%s%s", dotdir, Config.Gpu);
		strcpy(info.GPUpath, path);
		sprintf(path, "%s%s", dotdir, Config.Spu);
		strcpy(info.SPUpath, path);
		sprintf(path, "%s%s", dotdir, Config.Cdr);
		strcpy(info.CDRpath, path);
		NET_setInfo(&info);

		ret = NET_open(&gpuDisp);
		if (ret < 0) {
			if (ret == -2) {
				// -2 is returned when something in the info
				// changed and needs to be synced
				char *ptr;

				PARSEPATH(Config.Bios, info.BIOSpath);
				PARSEPATH(Config.Gpu,  info.GPUpath);
				PARSEPATH(Config.Spu,  info.SPUpath);
				PARSEPATH(Config.Cdr,  info.CDRpath);

				strcpy(Config.Mcd1, info.MCD1path);
				strcpy(Config.Mcd2, info.MCD2path);
				return -2;
			} else {
				Config.UseNet = FALSE;
			}
		} else {
			if (NET_queryPlayer() == 1) {
				if (SendPcsxInfo() == -1) Config.UseNet = FALSE;
			} else {
				if (RecvPcsxInfo() == -1) Config.UseNet = FALSE;
			}
		}
		NetOpened = TRUE;
	} else if (Config.UseNet) {
		NET_resume();
	}

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

	signal(SIGINT, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	ret = CDR_close();
	if (ret < 0) { SysMessage(_("Error closing CD-ROM plugin!")); return; }
	ret = SPU_close();
	if (ret < 0) { SysMessage(_("Error closing SPU plugin!")); return; }
	ret = PAD1_close();
	if (ret < 0) { SysMessage(_("Error closing Controller 1 Plugin!")); return; }
	ret = PAD2_close();
	if (ret < 0) { SysMessage(_("Error closing Controller 2 plugin!")); return; }
	ret = GPU_close();
	if (ret < 0) { SysMessage(_("Error closing GPU plugin!")); return; }

	if (Config.UseNet) {
		NET_pause();
	}
}

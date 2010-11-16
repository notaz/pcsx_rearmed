/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2003  Pcsx Team
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

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include "psxcommon.h"
#include "plugin.h"
#include "plugins.h"
#include "resource.h"
#include "Win32.h"

#define QueryKeyV(name, var) \
	size = sizeof(DWORD); \
	if (RegQueryValueEx(myKey, name, 0, &type, (LPBYTE)&tmp, &size) != 0) { if (err) { RegCloseKey(myKey); return -1; } } \
	var = tmp;

#define QueryKey(s, name, var) \
	size = s; \
	if (RegQueryValueEx(myKey, name, 0, &type, (LPBYTE)var, &size) != 0) { if (err) { RegCloseKey(myKey); return -1; } }

#define SetKeyV(name, var) \
	tmp = var; \
	RegSetValueEx(myKey, name, 0, REG_DWORD, (LPBYTE)&tmp, sizeof(DWORD));

#define SetKey(name, var, s, t) \
	RegSetValueEx(myKey, name, 0, t, (LPBYTE)var, s);

int LoadConfig() {
	HKEY myKey;
	DWORD type, size, tmp;
	PcsxConfig *Conf = &Config;
	int err;
#ifdef ENABLE_NLS
	char text[256];
#endif

	if (RegOpenKeyEx(HKEY_CURRENT_USER,cfgfile,0,KEY_ALL_ACCESS,&myKey)!=ERROR_SUCCESS) return -1;

	err = 1;
	QueryKey(256, "Bios", Conf->Bios);
	QueryKey(256, "Gpu",  Conf->Gpu);
	QueryKey(256, "Spu",  Conf->Spu);
	QueryKey(256, "Cdr",  Conf->Cdr);
	QueryKey(256, "Pad1", Conf->Pad1);
	QueryKey(256, "Pad2", Conf->Pad2);
	QueryKey(256, "Mcd1", Conf->Mcd1);
	QueryKey(256, "Mcd2", Conf->Mcd2);
	QueryKey(256, "PluginsDir", Conf->PluginsDir);
	QueryKey(256, "BiosDir",    Conf->BiosDir);
	err = 0;
	QueryKey(256, "Net",  Conf->Net);
	QueryKey(256, "Lang", Conf->Lang);

	QueryKeyV("Xa",       Conf->Xa);
	QueryKeyV("Sio",      Conf->Sio);
	QueryKeyV("Mdec",     Conf->Mdec);
	QueryKeyV("PsxAuto",  Conf->PsxAuto);
	QueryKeyV("Cdda",     Conf->Cdda);
	QueryKeyV("Debug",    Conf->Debug);
	QueryKeyV("PsxOut",   Conf->PsxOut);
	QueryKeyV("SpuIrq",   Conf->SpuIrq);
	QueryKeyV("RCntFix",  Conf->RCntFix);
	QueryKeyV("VSyncWA",  Conf->VSyncWA);

	QueryKeyV("Cpu",      Conf->Cpu);
	QueryKeyV("PsxType",  Conf->PsxType);

	if (Config.Cpu == CPU_DYNAREC) {
		Config.Debug = 0; // don't enable debugger if using dynarec core
	}

	RegCloseKey(myKey);

#ifdef ENABLE_NLS
	sprintf(text, "LANGUAGE=%s", Conf->Lang);
	gettext_putenv(text);
#endif

	return 0;
}

/////////////////////////////////////////////////////////

void SaveConfig() {
	HKEY myKey;
	DWORD myDisp, tmp;
	PcsxConfig *Conf = &Config;

	RegCreateKeyEx(HKEY_CURRENT_USER, cfgfile, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &myKey, &myDisp);

	SetKey("Bios", Conf->Bios, strlen(Conf->Bios), REG_SZ);
	SetKey("Gpu",  Conf->Gpu,  strlen(Conf->Gpu),  REG_SZ);
	SetKey("Spu",  Conf->Spu,  strlen(Conf->Spu),  REG_SZ);
	SetKey("Cdr",  Conf->Cdr,  strlen(Conf->Cdr),  REG_SZ);
	SetKey("Pad1", Conf->Pad1, strlen(Conf->Pad1), REG_SZ);
	SetKey("Pad2", Conf->Pad2, strlen(Conf->Pad2), REG_SZ);
	SetKey("Net",  Conf->Net,  strlen(Conf->Net),  REG_SZ);
	SetKey("Mcd1", Conf->Mcd1, strlen(Conf->Mcd1), REG_SZ);
	SetKey("Mcd2", Conf->Mcd2, strlen(Conf->Mcd2), REG_SZ);
	SetKey("Lang", Conf->Lang, strlen(Conf->Lang), REG_SZ);
	SetKey("PluginsDir", Conf->PluginsDir, strlen(Conf->PluginsDir), REG_SZ);
	SetKey("BiosDir",    Conf->BiosDir,    strlen(Conf->BiosDir), REG_SZ);

	SetKeyV("Xa",      Conf->Xa);
	SetKeyV("Sio",     Conf->Sio);
	SetKeyV("Mdec",    Conf->Mdec);
	SetKeyV("PsxAuto", Conf->PsxAuto);
	SetKeyV("Cdda",    Conf->Cdda);
	SetKeyV("Debug",   Conf->Debug);
	SetKeyV("PsxOut",  Conf->PsxOut);
	SetKeyV("SpuIrq",  Conf->SpuIrq);
	SetKeyV("RCntFix", Conf->RCntFix);
	SetKeyV("VSyncWA", Conf->VSyncWA);

	SetKeyV("Cpu",     Conf->Cpu);
	SetKeyV("PsxType", Conf->PsxType);

	RegCloseKey(myKey);
}

/////////////////////////////////////////////////////////

#define ComboAddPlugin(hw, str) { \
	lp = (char *)malloc(strlen(FindData.cFileName)+8); \
	sprintf(lp, "%s", FindData.cFileName); \
	i = ComboBox_AddString(hw, tmpStr); \
	ComboBox_SetItemData(hw, i, lp); \
	if (stricmp(str, lp)==0) \
		ComboBox_SetCurSel(hw, i); \
}

BOOL OnConfigurePluginsDialog(HWND hW) {
	WIN32_FIND_DATA FindData;
	HANDLE Find;
	HANDLE Lib;
	PSEgetLibType    PSE_GetLibType;
	PSEgetLibName    PSE_GetLibName;
	PSEgetLibVersion PSE_GetLibVersion;
	HWND hWC_GPU=GetDlgItem(hW,IDC_LISTGPU);
	HWND hWC_SPU=GetDlgItem(hW,IDC_LISTSPU);
	HWND hWC_CDR=GetDlgItem(hW,IDC_LISTCDR);
	HWND hWC_PAD1=GetDlgItem(hW,IDC_LISTPAD1);
	HWND hWC_PAD2=GetDlgItem(hW,IDC_LISTPAD2);
	HWND hWC_BIOS=GetDlgItem(hW,IDC_LISTBIOS);
	char tmpStr[256];
	char *lp;
	int i;

	strcpy(tmpStr, Config.PluginsDir);
	strcat(tmpStr, "*.dll");
	Find = FindFirstFile(tmpStr, &FindData);

	do {
		if (Find == INVALID_HANDLE_VALUE) break;
		sprintf(tmpStr,"%s%s", Config.PluginsDir, FindData.cFileName);
		Lib = LoadLibrary(tmpStr);
		if (Lib != NULL) {
			PSE_GetLibType = (PSEgetLibType) GetProcAddress((HMODULE)Lib,"PSEgetLibType");
			PSE_GetLibName = (PSEgetLibName) GetProcAddress((HMODULE)Lib,"PSEgetLibName");
			PSE_GetLibVersion = (PSEgetLibVersion) GetProcAddress((HMODULE)Lib,"PSEgetLibVersion");

			if (PSE_GetLibType != NULL && PSE_GetLibName != NULL && PSE_GetLibVersion != NULL) {
				unsigned long version = PSE_GetLibVersion();
				long type;

				sprintf(tmpStr, "%s %d.%d", PSE_GetLibName(), (int)(version>>8)&0xff, (int)version&0xff);
				type = PSE_GetLibType();
				if (type & PSE_LT_CDR) {
					ComboAddPlugin(hWC_CDR, Config.Cdr);
				}

				if (type & PSE_LT_SPU) {
					ComboAddPlugin(hWC_SPU, Config.Spu);
				}

				if (type & PSE_LT_GPU) {
					ComboAddPlugin(hWC_GPU, Config.Gpu);
				}

				if (type & PSE_LT_PAD) {
					PADquery query;

					query = (PADquery)GetProcAddress((HMODULE)Lib, "PADquery");
					if (query != NULL) {
						if (query() & 0x1)
							ComboAddPlugin(hWC_PAD1, Config.Pad1);
						if (query() & 0x2)
							ComboAddPlugin(hWC_PAD2, Config.Pad2);
					} else { // just a guess
						ComboAddPlugin(hWC_PAD1, Config.Pad1);
					}
				}
			}
		}
	} while (FindNextFile(Find,&FindData));

	if (Find != INVALID_HANDLE_VALUE) FindClose(Find);

// BIOS

	lp = (char *)malloc(strlen("HLE") + 1);
	sprintf(lp, "HLE");
	i = ComboBox_AddString(hWC_BIOS, _("Simulate Psx Bios"));
	ComboBox_SetItemData(hWC_BIOS, i, lp);
	if (stricmp(Config.Bios, lp)==0)
		ComboBox_SetCurSel(hWC_BIOS, i);

	strcpy(tmpStr, Config.BiosDir);
	strcat(tmpStr, "*");
	Find=FindFirstFile(tmpStr, &FindData);

	do {
		if (Find==INVALID_HANDLE_VALUE) break;
		if (!strcmp(FindData.cFileName, ".")) continue;
		if (!strcmp(FindData.cFileName, "..")) continue;
		if (FindData.nFileSizeLow != 1024 * 512) continue;
		lp = (char *)malloc(strlen(FindData.cFileName)+8);
		sprintf(lp, "%s", (char *)FindData.cFileName);
		i = ComboBox_AddString(hWC_BIOS, FindData.cFileName);
		ComboBox_SetItemData(hWC_BIOS, i, lp);
		if (Config.Bios[0]=='\0') {
			ComboBox_SetCurSel(hWC_BIOS, i);
			strcpy(Config.Bios, FindData.cFileName);
		} else if (stricmp(Config.Bios, FindData.cFileName)==0)
			ComboBox_SetCurSel(hWC_BIOS, i);
	} while (FindNextFile(Find,&FindData));

	if (Find!=INVALID_HANDLE_VALUE) FindClose(Find);

	if (ComboBox_GetCurSel(hWC_CDR ) == -1)
		ComboBox_SetCurSel(hWC_CDR,  0);
	if (ComboBox_GetCurSel(hWC_GPU ) == -1)
		ComboBox_SetCurSel(hWC_GPU,  0);
	if (ComboBox_GetCurSel(hWC_SPU ) == -1)
		ComboBox_SetCurSel(hWC_SPU,  0);
	if (ComboBox_GetCurSel(hWC_PAD1) == -1)
		ComboBox_SetCurSel(hWC_PAD1, 0);
	if (ComboBox_GetCurSel(hWC_PAD2) == -1)
		ComboBox_SetCurSel(hWC_PAD2, 0);
	if (ComboBox_GetCurSel(hWC_BIOS) == -1)
		ComboBox_SetCurSel(hWC_BIOS, 0);

	return TRUE;
}
	
#define CleanCombo(item) \
	hWC = GetDlgItem(hW, item); \
	iCnt = ComboBox_GetCount(hWC); \
	for (i=0; i<iCnt; i++) { \
		lp = (char *)ComboBox_GetItemData(hWC, i); \
		if (lp) free(lp); \
	} \
	ComboBox_ResetContent(hWC);

void CleanUpCombos(HWND hW) {
	int i,iCnt;HWND hWC;char * lp;

	CleanCombo(IDC_LISTGPU);
	CleanCombo(IDC_LISTSPU);
	CleanCombo(IDC_LISTCDR);
	CleanCombo(IDC_LISTPAD1);
	CleanCombo(IDC_LISTPAD2);
	CleanCombo(IDC_LISTBIOS);
}

void OnCancel(HWND hW) {
	CleanUpCombos(hW);
	EndDialog(hW,FALSE);
}

char *GetSelDLL(HWND hW,int id) {
	HWND hWC = GetDlgItem(hW,id);
	int iSel;
	iSel = ComboBox_GetCurSel(hWC);
	if (iSel<0) return NULL;
	return (char *)ComboBox_GetItemData(hWC, iSel);
}

void OnOK(HWND hW) {
	char *gpuDLL=GetSelDLL(hW,IDC_LISTGPU);
	char *spuDLL=GetSelDLL(hW,IDC_LISTSPU);
	char *cdrDLL=GetSelDLL(hW,IDC_LISTCDR);
	char *pad1DLL=GetSelDLL(hW,IDC_LISTPAD1);
	char *pad2DLL=GetSelDLL(hW,IDC_LISTPAD2);
	char *biosFILE=GetSelDLL(hW,IDC_LISTBIOS);

    if (gpuDLL == NULL || spuDLL == NULL || cdrDLL == NULL || pad1DLL == NULL ||
		pad2DLL == NULL || biosFILE == NULL) {
		MessageBox(hW, _("Configuration not OK!"), _("Error"), MB_OK | MB_ICONERROR);
		return;
	}

	strcpy(Config.Bios, biosFILE);
	strcpy(Config.Gpu,  gpuDLL);
	strcpy(Config.Spu,  spuDLL);
	strcpy(Config.Cdr,  cdrDLL);
	strcpy(Config.Pad1, pad1DLL);
	strcpy(Config.Pad2, pad2DLL);

	SaveConfig();

	CleanUpCombos(hW);

	if (!ConfPlug) {
		LoadPlugins();
	}
	EndDialog(hW,TRUE);
}


#define ConfPlugin(src, confs, name) \
	void *drv; \
	src conf; \
	char * pDLL = GetSelDLL(hW, confs); \
	char file[256]; \
	if(pDLL==NULL) return; \
	strcpy(file, Config.PluginsDir); \
	strcat(file, pDLL); \
	drv = SysLoadLibrary(file); \
	if (drv == NULL) return; \
	conf = (src) SysLoadSym(drv, name); \
	if (SysLibError() == NULL) conf(); \
	SysCloseLibrary(drv);

void ConfigureGPU(HWND hW) {
	ConfPlugin(GPUconfigure, IDC_LISTGPU, "GPUconfigure");
}

void ConfigureSPU(HWND hW) {
	ConfPlugin(SPUconfigure, IDC_LISTSPU, "SPUconfigure");
}

void ConfigureCDR(HWND hW) {
	ConfPlugin(CDRconfigure, IDC_LISTCDR, "CDRconfigure");
}

void ConfigureNET(HWND hW) {
	ConfPlugin(NETconfigure, IDC_LISTNET, "NETconfigure");
}

void ConfigurePAD1(HWND hW) {
	ConfPlugin(PADconfigure, IDC_LISTPAD1, "PADconfigure");
}

void ConfigurePAD2(HWND hW) {
	ConfPlugin(PADconfigure, IDC_LISTPAD2, "PADconfigure");
}


void AboutGPU(HWND hW) {
	ConfPlugin(GPUabout, IDC_LISTGPU, "GPUabout");
}

void AboutSPU(HWND hW) {
	ConfPlugin(SPUabout, IDC_LISTSPU, "SPUabout");
}

void AboutCDR(HWND hW) {
	ConfPlugin(CDRabout, IDC_LISTCDR, "CDRabout");
}

void AboutNET(HWND hW) {
	ConfPlugin(NETabout, IDC_LISTNET, "NETabout");
}

void AboutPAD1(HWND hW) {
	ConfPlugin(PADabout, IDC_LISTPAD1, "PADabout");
}

void AboutPAD2(HWND hW) {
	ConfPlugin(PADabout, IDC_LISTPAD2, "PADabout");
}


#define TestPlugin(src, confs, name) \
	void *drv; \
	src conf; \
	int ret = 0; \
	char * pDLL = GetSelDLL(hW, confs); \
	char file[256]; \
	if (pDLL== NULL) return; \
	strcpy(file, Config.PluginsDir); \
	strcat(file, pDLL); \
	drv = SysLoadLibrary(file); \
	if (drv == NULL) return; \
	conf = (src) SysLoadSym(drv, name); \
	if (SysLibError() == NULL) { \
		ret = conf(); \
		if (ret == 0) \
			 SysMessage(_("This plugin reports that should work correctly")); \
		else SysMessage(_("This plugin reports that should not work correctly")); \
	} \
	SysCloseLibrary(drv);

void TestGPU(HWND hW) {
	TestPlugin(GPUtest, IDC_LISTGPU, "GPUtest");
}

void TestSPU(HWND hW) {
	TestPlugin(SPUtest, IDC_LISTSPU, "SPUtest");
}

void TestCDR(HWND hW) {
	TestPlugin(CDRtest, IDC_LISTCDR, "CDRtest");
}

void TestNET(HWND hW) {
	TestPlugin(NETtest, IDC_LISTNET, "NETtest");
}

void TestPAD1(HWND hW) {
	TestPlugin(PADtest, IDC_LISTPAD1, "PADtest");
}

void TestPAD2(HWND hW) {
	TestPlugin(PADtest, IDC_LISTPAD2, "PADtest");
}

#include <shlobj.h>

int SelectPath(HWND hW, char *Title, char *Path) {
	LPITEMIDLIST pidl;
	BROWSEINFO bi;
	char Buffer[256];

	bi.hwndOwner = hW;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = Buffer;
	bi.lpszTitle = Title;
	bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
	bi.lpfn = NULL;
	bi.lParam = 0;
	if ((pidl = SHBrowseForFolder(&bi)) != NULL) {
		if (SHGetPathFromIDList(pidl, Path)) {
			int len = strlen(Path);

			if (Path[len - 1] != '\\') { strcat(Path,"\\"); }
			return 0;
		}
	}
	return -1;
}

void SetPluginsDir(HWND hW) {
	char Path[256];

	if (SelectPath(hW, _("Select Plugins Directory"), Path) == -1) return;
	strcpy(Config.PluginsDir, Path);
	CleanUpCombos(hW);
	OnConfigurePluginsDialog(hW);
}

void SetBiosDir(HWND hW) {
	char Path[256];

	if (SelectPath(hW, _("Select Bios Directory"), Path) == -1) return;
	strcpy(Config.BiosDir, Path);
	CleanUpCombos(hW);
	OnConfigurePluginsDialog(hW);
}

BOOL CALLBACK ConfigurePluginsDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Configuration"));

			Button_SetText(GetDlgItem(hW, IDOK), _("OK"));
			Button_SetText(GetDlgItem(hW, IDCANCEL), _("Cancel"));
			Static_SetText(GetDlgItem(hW, IDC_GRAPHICS), _("Graphics"));
			Static_SetText(GetDlgItem(hW, IDC_FIRSTCONTROLLER), _("First Controller"));
			Static_SetText(GetDlgItem(hW, IDC_SECONDCONTROLLER), _("Second Controller"));
			Static_SetText(GetDlgItem(hW, IDC_SOUND), _("Sound"));
			Static_SetText(GetDlgItem(hW, IDC_CDROM), _("Cdrom"));
			Static_SetText(GetDlgItem(hW, IDC_BIOS), _("Bios"));
			Button_SetText(GetDlgItem(hW, IDC_BIOSDIR), _("Set Bios Directory"));
			Button_SetText(GetDlgItem(hW, IDC_PLUGINSDIR), _("Set Plugins Directory"));
			Button_SetText(GetDlgItem(hW, IDC_CONFIGGPU), _("Configure..."));
			Button_SetText(GetDlgItem(hW, IDC_TESTGPU), _("Test..."));
			Button_SetText(GetDlgItem(hW, IDC_ABOUTGPU), _("About..."));
			Button_SetText(GetDlgItem(hW, IDC_CONFIGSPU), _("Configure..."));
			Button_SetText(GetDlgItem(hW, IDC_TESTSPU), _("Test..."));
			Button_SetText(GetDlgItem(hW, IDC_ABOUTSPU), _("About..."));
			Button_SetText(GetDlgItem(hW, IDC_CONFIGCDR), _("Configure..."));
			Button_SetText(GetDlgItem(hW, IDC_TESTCDR), _("Test..."));
			Button_SetText(GetDlgItem(hW, IDC_ABOUTCDR), _("About..."));
			Button_SetText(GetDlgItem(hW, IDC_CONFIGPAD1), _("Configure..."));
			Button_SetText(GetDlgItem(hW, IDC_TESTPAD1), _("Test..."));
			Button_SetText(GetDlgItem(hW, IDC_ABOUTPAD1), _("About..."));
			Button_SetText(GetDlgItem(hW, IDC_CONFIGPAD2), _("Configure..."));
			Button_SetText(GetDlgItem(hW, IDC_TESTPAD2), _("Test..."));
			Button_SetText(GetDlgItem(hW, IDC_ABOUTPAD2), _("About..."));

			return OnConfigurePluginsDialog(hW);

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDC_CONFIGGPU:  ConfigureGPU(hW); return TRUE;
       			case IDC_CONFIGSPU:  ConfigureSPU(hW); return TRUE;
       			case IDC_CONFIGCDR:  ConfigureCDR(hW); return TRUE;
       			case IDC_CONFIGPAD1: ConfigurePAD1(hW); return TRUE;
       			case IDC_CONFIGPAD2: ConfigurePAD2(hW); return TRUE;

				case IDC_TESTGPU:    TestGPU(hW);   return TRUE;
				case IDC_TESTSPU:    TestSPU(hW);   return TRUE;
				case IDC_TESTCDR:    TestCDR(hW);   return TRUE;
				case IDC_TESTPAD1:   TestPAD1(hW);  return TRUE;
				case IDC_TESTPAD2:   TestPAD2(hW);  return TRUE;

				case IDC_ABOUTGPU:   AboutGPU(hW);  return TRUE;
				case IDC_ABOUTSPU:   AboutSPU(hW);  return TRUE;
                case IDC_ABOUTCDR:   AboutCDR(hW);  return TRUE;
    	        case IDC_ABOUTPAD1:  AboutPAD1(hW); return TRUE;
    		    case IDC_ABOUTPAD2:  AboutPAD2(hW); return TRUE;

				case IDC_PLUGINSDIR: SetPluginsDir(hW); return TRUE;
				case IDC_BIOSDIR:	 SetBiosDir(hW);	return TRUE;

				case IDCANCEL: 
					OnCancel(hW); 
					if (CancelQuit) {
						SysClose(); exit(1);
					}
					return TRUE;
				case IDOK:     
					OnOK(hW);     
					return TRUE;
			}
	}
	return FALSE;
}


void ConfigurePlugins(HWND hWnd) {
    DialogBox(gApp.hInstance,
              MAKEINTRESOURCE(IDD_CONFIG),
              hWnd,  
              (DLGPROC)ConfigurePluginsDlgProc);
}

// NetPlay Config Dialog

BOOL OnConfigureNetPlayDialog(HWND hW) {
	WIN32_FIND_DATA FindData;
	HANDLE Find;
	HANDLE Lib;
	PSEgetLibType    PSE_GetLibType;
	PSEgetLibName    PSE_GetLibName;
	PSEgetLibVersion PSE_GetLibVersion;
	HWND hWC_NET=GetDlgItem(hW,IDC_LISTNET);
	char tmpStr[256];
	char *lp;
	int i;

	strcpy(tmpStr, Config.PluginsDir);
	strcat(tmpStr, "*.dll");
	Find = FindFirstFile(tmpStr, &FindData);

	lp = (char *)malloc(strlen("Disabled")+8);
	sprintf(lp, "Disabled");
	i = ComboBox_AddString(hWC_NET, "Disabled");
	ComboBox_SetItemData(hWC_NET, i, lp);
	ComboBox_SetCurSel(hWC_NET,  0);

	do {
		if (Find==INVALID_HANDLE_VALUE) break;
		sprintf(tmpStr,"%s%s", Config.PluginsDir, FindData.cFileName);
		Lib = LoadLibrary(tmpStr);
		if (Lib!=NULL) {
			PSE_GetLibType = (PSEgetLibType) GetProcAddress((HMODULE)Lib,"PSEgetLibType");
			PSE_GetLibName = (PSEgetLibName) GetProcAddress((HMODULE)Lib,"PSEgetLibName");
			PSE_GetLibVersion = (PSEgetLibVersion) GetProcAddress((HMODULE)Lib,"PSEgetLibVersion");

			if (PSE_GetLibType != NULL && PSE_GetLibName != NULL && PSE_GetLibVersion != NULL) {
				unsigned long version = PSE_GetLibVersion();
				long type;

				sprintf(tmpStr, "%s %d.%d", PSE_GetLibName(), (int)(version>>8)&0xff, (int)version&0xff);
				type = PSE_GetLibType();
				if (type & PSE_LT_NET  && ((version >> 16) == 2)) {
					ComboAddPlugin(hWC_NET, Config.Net);
				}
			}
		}
	} while (FindNextFile(Find,&FindData));

	if (Find!=INVALID_HANDLE_VALUE) FindClose(Find);

	return TRUE;
}

BOOL CALLBACK ConfigureNetPlayDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int i,iCnt;HWND hWC;char * lp;

	switch(uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("NetPlay Configuration"));

			Button_SetText(GetDlgItem(hW, IDOK), _("OK"));
			Button_SetText(GetDlgItem(hW, IDCANCEL), _("Cancel"));
			Static_SetText(GetDlgItem(hW, IDC_NETPLAY), _("NetPlay"));
			Button_SetText(GetDlgItem(hW, IDC_CONFIGNET), _("Configure..."));
			Button_SetText(GetDlgItem(hW, IDC_TESTNET), _("Test..."));
			Button_SetText(GetDlgItem(hW, IDC_ABOUTNET), _("About..."));
			Static_SetText(GetDlgItem(hW, IDC_NETPLAYNOTE), _("Note: The NetPlay Plugin Directory should be the same as the other Plugins."));

			OnConfigureNetPlayDialog(hW);
			return TRUE;

		case WM_COMMAND: {
     		switch (LOWORD(wParam)) {
				case IDC_CONFIGNET:  ConfigureNET(hW); return TRUE;
				case IDC_TESTNET:    TestNET(hW);   return TRUE;
				case IDC_ABOUTNET:   AboutNET(hW);  return TRUE;

				case IDCANCEL: 
					CleanCombo(IDC_LISTNET);
					EndDialog(hW,FALSE); 
					return TRUE;

				case IDOK:
					strcpy(Config.Net, GetSelDLL(hW, IDC_LISTNET));
					SaveConfig();
					CleanUpCombos(hW);
					LoadPlugins();
					CleanCombo(IDC_LISTNET);
					EndDialog(hW,TRUE);
					return TRUE;
			}
		}
	}

	return FALSE;
}

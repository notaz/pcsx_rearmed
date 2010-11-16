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
#include <commctrl.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "resource.h"
#include "AboutDlg.h"

#include "psxcommon.h"
#include "plugin.h"
#include "debug.h"
#include "Win32.h"
#include "sio.h"
#include "misc.h"
#include "cheat.h"

#ifdef __MINGW32__
#ifndef LVM_GETSELECTIONMARK
#define LVM_GETSELECTIONMARK (LVM_FIRST+66)
#endif
#ifndef ListView_GetSelectionMark
#define ListView_GetSelectionMark(w) (INT)SNDMSG((w),LVM_GETSELECTIONMARK,0,0)
#endif
#endif

int AccBreak = 0;
int ConfPlug = 0;
int StatesC = 0;
int CancelQuit = 0;
char cfgfile[256];
int Running = 0;
char PcsxDir[256];

static HDC          hDC;
static HDC          hdcmem;
static HBITMAP      hBm;
static BITMAP       bm;

#ifdef ENABLE_NLS

unsigned int langsMax;

typedef struct {
	char lang[256];
} _langs;
_langs *langs = NULL;

typedef struct {
	char			id[8];
	char			name[64];
	LANGID			langid;
} LangDef;

LangDef sLangs[] = {
	{ "ar",		N_("Arabic"),				0x0401 },
	{ "ca",		N_("Catalan"),				0x0403 },
	{ "de",		N_("German"),				0x0407 },
	{ "el",		N_("Greek"),				0x0408 },
	{ "en",		N_("English"),				0x0409 },
	{ "es",		N_("Spanish"),				0x040a },
	{ "fr_FR",	N_("French"),				0x040c },
	{ "it",		N_("Italian"),				0x0410 },
	{ "pt",		N_("Portuguese"),			0x0816 },
	{ "pt_BR",	N_("Portuguese (Brazilian)"),		0x0416 },
	{ "ro",		N_("Romanian"),				0x0418 },
	{ "ru_RU",	N_("Russian"),				0x0419 },
	{ "zh_CN",	N_("Simplified Chinese"),	0x0804 },
	{ "zh_TW",	N_("Traditional Chinese"),	0x0404 },
	{ "ja",		N_("Japanese"),				0x0411 },
	{ "ko",		N_("Korean"),				0x0412 },
	{ "", "", 0xFFFF },
};

char *ParseLang(char *id) {
	int i=0;

	while (sLangs[i].id[0] != '\0') {
		if (!strcmp(id, sLangs[i].id))
			return _(sLangs[i].name);
		i++;
	}

	return id;
}

static void SetDefaultLang(void) {
	LANGID langid;
	int i;

	langid = GetSystemDefaultLangID();

	i = 0;
	while (sLangs[i].id[0] != '\0') {
		if (langid == sLangs[i].langid) {
			strcpy(Config.Lang, sLangs[i].id);
			return;
		}
		i++;
	}

	strcpy(Config.Lang, "English");
}

#endif

void strcatz(char *dst, char *src) {
	int len = strlen(dst) + 1;
	strcpy(dst + len, src);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	strcpy(cfgfile, "Software\\Pcsx");

	gApp.hInstance = hInstance;

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, "Langs\\");
	textdomain(PACKAGE);
#endif

	Running = 0;

	GetCurrentDirectory(256, PcsxDir);

	memset(&Config, 0, sizeof(PcsxConfig));
	strcpy(Config.Net, "Disabled");
	if (LoadConfig() == -1) {
		Config.PsxAuto = 1;
		strcpy(Config.PluginsDir, "Plugins\\");
		strcpy(Config.BiosDir,    "Bios\\");

		strcpy(Config.Mcd1, "memcards\\Mcd001.mcr");
		strcpy(Config.Mcd2, "memcards\\Mcd002.mcr");

		ConfPlug = 1;

#ifdef ENABLE_NLS
		{
			char text[256];
			SetDefaultLang();
			sprintf(text, "LANGUAGE=%s", Config.Lang);
			gettext_putenv(text);
		}
#endif

		ConfigurePlugins(gApp.hWnd);

		if (LoadConfig() == -1) {
			return 0;
		}
	}

	strcpy(Config.PatchesDir, "Patches\\");

#ifdef ENABLE_NLS
	if (Config.Lang[0] == 0) {
		SetDefaultLang();
		SaveConfig();
		LoadConfig();
	}
#endif

	if (SysInit() == -1) return 1;

	CreateMainWindow(nCmdShow);

	RunGui();

	return 0;
}

void RunGui() {
	MSG msg;

	for (;;) {
		if(PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void RestoreWindow() {
	AccBreak = 1;
	DestroyWindow(gApp.hWnd);
	CreateMainWindow(SW_SHOWNORMAL);
	ShowCursor(TRUE);
	SetCursor(LoadCursor(gApp.hInstance, IDC_ARROW));
	ShowCursor(TRUE);
}

int Slots[5] = { -1, -1, -1, -1, -1 };

void ResetMenuSlots() {
	int i;

	for (i = 0; i < 5; i++) {
		if (Slots[i] == -1)
			EnableMenuItem(GetSubMenu(gApp.hMenu, 0), ID_FILE_STATES_LOAD_SLOT1+i, MF_GRAYED);
		else 
			EnableMenuItem(GetSubMenu(gApp.hMenu, 0), ID_FILE_STATES_LOAD_SLOT1+i, MF_ENABLED);
	}
}

void UpdateMenuSlots() {
	char str[256];
	int i;

	for (i = 0; i < 5; i++) {
		GetStateFilename(str, i);
		Slots[i] = CheckState(str);
	}
}

void OpenConsole() {
	if (hConsole) return;
	AllocConsole();
	SetConsoleTitle("Psx Output");
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

void CloseConsole() {
	FreeConsole();
	hConsole = NULL;
}

void States_Load(int num) {
	char Text[256];
	int ret;

	SetMenu(gApp.hWnd, NULL);
	OpenPlugins(gApp.hWnd);

	GetStateFilename(Text, num);

	ret = LoadState(Text);
	if (ret == 0)
		 sprintf(Text, _("*PCSX*: Loaded State %d"), num+1);
	else sprintf(Text, _("*PCSX*: Error Loading State %d"), num+1);
	GPU_displayText(Text);

	Running = 1;
	CheatSearchBackupMemory();
	psxCpu->Execute();
}

void States_Save(int num) {
	char Text[256];
	int ret;

	SetMenu(gApp.hWnd, NULL);
	OpenPlugins(gApp.hWnd);

	GPU_updateLace();

	GetStateFilename(Text, num);
	GPU_freeze(2, (GPUFreeze_t *)&num);
	ret = SaveState(Text);
	if (ret == 0)
		 sprintf(Text, _("*PCSX*: Saved State %d"), num+1);
	else sprintf(Text, _("*PCSX*: Error Saving State %d"), num+1);
	GPU_displayText(Text);

	Running = 1;
	CheatSearchBackupMemory();
	psxCpu->Execute();
}

void OnStates_LoadOther() {
	OPENFILENAME ofn;
	char szFileName[MAXPATHLEN];
	char szFileTitle[MAXPATHLEN];
	char szFilter[256];

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));
	memset(&szFilter,    0, sizeof(szFilter));

	strcpy(szFilter, _("PCSX State Format"));
	strcatz(szFilter, "*.*");

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= gApp.hWnd;
	ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter		= NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFileName;
	ofn.nMaxFile			= MAXPATHLEN;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= MAXPATHLEN;
	ofn.lpstrTitle			= NULL;
	ofn.lpstrDefExt			= NULL;
	ofn.Flags			= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) {
		char Text[256];
		int ret;

		SetMenu(gApp.hWnd, NULL);
		OpenPlugins(gApp.hWnd);

		ret = LoadState(szFileName);
		if (ret == 0)
			 sprintf(Text, _("*PCSX*: Loaded State %s"), szFileName);
		else sprintf(Text, _("*PCSX*: Error Loading State %s"), szFileName);
		GPU_displayText(Text);

		Running = 1;
		psxCpu->Execute();
	}
} 

void OnStates_Save1() { States_Save(0); } 
void OnStates_Save2() { States_Save(1); } 
void OnStates_Save3() { States_Save(2); } 
void OnStates_Save4() { States_Save(3); } 
void OnStates_Save5() { States_Save(4); } 

void OnStates_SaveOther() {
	OPENFILENAME ofn;
	char szFileName[MAXPATHLEN];
	char szFileTitle[MAXPATHLEN];
	char szFilter[256];

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));
	memset(&szFilter,    0, sizeof(szFilter));

	strcpy(szFilter, _("PCSX State Format"));
	strcatz(szFilter, "*.*");

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= gApp.hWnd;
	ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFileName;
	ofn.nMaxFile			= MAXPATHLEN;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= MAXPATHLEN;
	ofn.lpstrTitle			= NULL;
	ofn.lpstrDefExt			= NULL;
	ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) {
		char Text[256];
		int ret;

		SetMenu(gApp.hWnd, NULL);
		OpenPlugins(gApp.hWnd);

		ret = SaveState(szFileName);
		if (ret == 0)
			 sprintf(Text, _("*PCSX*: Saved State %s"), szFileName);
		else sprintf(Text, _("*PCSX*: Error Saving State %s"), szFileName);
		GPU_displayText(Text);

		Running = 1;
		psxCpu->Execute();
	}
} 

LRESULT WINAPI MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	char File[256];
	PAINTSTRUCT ps;

	switch (msg) {
		case WM_CREATE:
			hBm = LoadBitmap(gApp.hInstance, MAKEINTRESOURCE(MAIN_LOGO));
			GetObject(hBm, sizeof(BITMAP), (LPVOID)&bm);
			hDC = GetDC(hWnd);
			hdcmem = CreateCompatibleDC(hDC);
			ReleaseDC(hWnd, hDC);
			break;

		case WM_PAINT:
			hDC = BeginPaint(hWnd, &ps);
			SelectObject(hdcmem, hBm);
			if (!Running) BitBlt(hDC, 0, 0, bm.bmWidth, bm.bmHeight, hdcmem, 0, 0, SRCCOPY);
			EndPaint(hWnd, &ps);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case ID_FILE_EXIT:
					SysClose();
					PostQuitMessage(0);
					exit(0);
					return TRUE;

				case ID_FILE_RUN_CD:
					SetIsoFile(NULL);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					SysReset();
					if (CheckCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("The CD does not appear to be a valid Playstation CD"));
						return TRUE;
					}
					if (LoadCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("Could not load CD-ROM!"));
						return TRUE;
					}
					ShowCursor(FALSE);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_RUNBIOS:
					if (strcmp(Config.Bios, "HLE") == 0) {
						SysMessage(_("Running BIOS is not supported with Internal HLE Bios."));
						return TRUE;
					}
					SetIsoFile(NULL);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					ShowCursor(FALSE);
					SysReset();
					CdromId[0] = '\0';
					CdromLabel[0] = '\0';
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_RUN_ISO:
					if (!Open_Iso_Proc(File)) return TRUE;
					SetIsoFile(File);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					SysReset();
					if (CheckCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("The CD does not appear to be a valid Playstation CD"));
						return TRUE;
					}
					if (LoadCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("Could not load CD-ROM!"));
						return TRUE;
					}
					ShowCursor(FALSE);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_RUN_EXE:
					if (!Open_File_Proc(File)) return TRUE;
					SetIsoFile(NULL);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					SysReset();
					CheckCdrom();
					Load(File);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_STATES_LOAD_SLOT1: States_Load(0); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT2: States_Load(1); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT3: States_Load(2); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT4: States_Load(3); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT5: States_Load(4); return TRUE;
				case ID_FILE_STATES_LOAD_OTHER: OnStates_LoadOther(); return TRUE;

				case ID_FILE_STATES_SAVE_SLOT1: States_Save(0); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT2: States_Save(1); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT3: States_Save(2); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT4: States_Save(3); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT5: States_Save(4); return TRUE;
				case ID_FILE_STATES_SAVE_OTHER: OnStates_SaveOther(); return TRUE;

				case ID_EMULATOR_RUN:
					SetMenu(hWnd, NULL);
					OpenPlugins(hWnd);
					ShowCursor(FALSE);
					Running = 1;
					CheatSearchBackupMemory();
					psxCpu->Execute();
					return TRUE;

				case ID_EMULATOR_RESET:
					SetMenu(hWnd, NULL);
					OpenPlugins(hWnd);
					SysReset();
					CheckCdrom();
					LoadCdrom();
					ShowCursor(FALSE);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_EMULATOR_SWITCH_ISO:
					if (!Open_Iso_Proc(File)) return TRUE;
					SetIsoFile(File);
					SetMenu(hWnd, NULL);
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					ShowCursor(FALSE);
					Running = 1;
					SetCdOpenCaseTime(time(NULL) + 2);
					CheatSearchBackupMemory();
					psxCpu->Execute();
					return TRUE;

				case ID_CONFIGURATION_GRAPHICS:
					if (GPU_configure) GPU_configure();
					return TRUE;

				case ID_CONFIGURATION_SOUND:
					if (SPU_configure) SPU_configure();
					return TRUE;

				case ID_CONFIGURATION_CONTROLLERS:
					if (PAD1_configure) PAD1_configure();
					if (strcmp(Config.Pad1, Config.Pad2)) if (PAD2_configure) PAD2_configure();
					return TRUE;

				case ID_CONFIGURATION_CDROM:
				    if (CDR_configure) CDR_configure();
					return TRUE;

				case ID_CONFIGURATION_NETPLAY:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_NETPLAY), hWnd, (DLGPROC)ConfigureNetPlayDlgProc);
					return TRUE;

				case ID_CONFIGURATION_MEMORYCARDMANAGER:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_MCDCONF), hWnd, (DLGPROC)ConfigureMcdsDlgProc);
					return TRUE;

				case ID_CONFIGURATION_CPU:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CPUCONF), hWnd, (DLGPROC)ConfigureCpuDlgProc);
					return TRUE;

				case ID_CONFIGURATION:
					ConfigurePlugins(hWnd);
					return TRUE;

				case ID_CONFIGURATION_CHEATLIST:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATLIST), hWnd, (DLGPROC)CheatDlgProc);
					break;

				case ID_CONFIGURATION_CHEATSEARCH:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATSEARCH), hWnd, (DLGPROC)CheatSearchDlgProc);
					break;

				case ID_HELP_ABOUT:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(ABOUT_DIALOG), hWnd, (DLGPROC)AboutDlgProc);
					return TRUE;

				default:
#ifdef ENABLE_NLS
					if (LOWORD(wParam) >= ID_LANGS && LOWORD(wParam) <= (ID_LANGS + langsMax)) {
						AccBreak = 1;
						DestroyWindow(gApp.hWnd);
						ChangeLanguage(langs[LOWORD(wParam) - ID_LANGS].lang);
						CreateMainWindow(SW_NORMAL);
						return TRUE;
					}
#endif
					break;
			}
			break;

		case WM_SYSKEYDOWN:
			if (wParam != VK_F10)
				return DefWindowProc(hWnd, msg, wParam, lParam);
		case WM_KEYDOWN:
			PADhandleKey(wParam);
			return TRUE;

		case WM_DESTROY:
			if (!AccBreak) {
				if (Running) ClosePlugins();
				SysClose();
				PostQuitMessage(0);
				exit(0);
			}
			else AccBreak = 0;

			DeleteObject(hBm);
			DeleteDC(hdcmem);
			return TRUE;

		case WM_QUIT:
			exit(0);
			break;

		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return FALSE;
}

HWND mcdDlg;
McdBlock Blocks[2][15];
int IconC[2][15];
HIMAGELIST Iiml[2];
HICON eICON;

void CreateListView(int idc) {
	HWND List;
	LV_COLUMN col;

	List = GetDlgItem(mcdDlg, idc);

	col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	col.fmt  = LVCFMT_LEFT;

	col.pszText  = _("Title");
	col.cx       = 170;
	col.iSubItem = 0;

	ListView_InsertColumn(List, 0, &col);

	col.pszText  = _("Status");
	col.cx       = 50;
	col.iSubItem = 1;

	ListView_InsertColumn(List, 1, &col);

	col.pszText  = _("Game ID");
	col.cx       = 90;
	col.iSubItem = 2;

	ListView_InsertColumn(List, 2, &col);

	col.pszText  = _("Game");
	col.cx       = 80;
	col.iSubItem = 3;

	ListView_InsertColumn(List, 3, &col);
}

int GetRGB() {
    HDC scrDC, memDC;
    HBITMAP oldBmp = NULL; 
    HBITMAP curBmp = NULL;
    COLORREF oldColor;
    COLORREF curColor = RGB(255,255,255);
    int i, R, G, B;

    R = G = B = 1;
 
    scrDC = CreateDC("DISPLAY", NULL, NULL, NULL);
    memDC = CreateCompatibleDC(NULL); 
    curBmp = CreateCompatibleBitmap(scrDC, 1, 1);    
    oldBmp = (HBITMAP)SelectObject(memDC, curBmp);
        
    for (i = 255; i >= 0; --i) {
        oldColor = curColor;
        curColor = SetPixel(memDC, 0, 0, RGB(i, i, i));
 
        if (GetRValue(curColor) < GetRValue(oldColor)) ++R; 
        if (GetGValue(curColor) < GetGValue(oldColor)) ++G;
        if (GetBValue(curColor) < GetBValue(oldColor)) ++B;
    }
 
    DeleteObject(oldBmp);
    DeleteObject(curBmp);
    DeleteDC(scrDC);
    DeleteDC(memDC);
 
    return (R * G * B);
}
 
HICON GetIcon(short *icon) {
    ICONINFO iInfo;
    HDC hDC;
    char mask[16*16];
    int x, y, c, Depth;
  
    hDC = CreateIC("DISPLAY",NULL,NULL,NULL);
    Depth=GetDeviceCaps(hDC, BITSPIXEL);
    DeleteDC(hDC);
 
    if (Depth == 16) {
        if (GetRGB() == (32 * 32 * 32))        
            Depth = 15;
    }

    for (y=0; y<16; y++) {
        for (x=0; x<16; x++) {
            c = icon[y*16+x];
            if (Depth == 15 || Depth == 32)
				c = ((c&0x001f) << 10) | 
					((c&0x7c00) >> 10) | 
					((c&0x03e0)      );
			else
                c = ((c&0x001f) << 11) |
					((c&0x7c00) >>  9) |
					((c&0x03e0) <<  1);

            icon[y*16+x] = c;
        }
    }    

    iInfo.fIcon = TRUE;
    memset(mask, 0, 16*16);
    iInfo.hbmMask  = CreateBitmap(16, 16, 1, 1, mask);
    iInfo.hbmColor = CreateBitmap(16, 16, 1, 16, icon); 
 
    return CreateIconIndirect(&iInfo);
}

HICON hICON[2][3][15];
int aIover[2];                        
int ani[2];
 
void LoadMcdItems(int mcd, int idc) {
    HWND List = GetDlgItem(mcdDlg, idc);
    LV_ITEM item;
    HIMAGELIST iml = Iiml[mcd-1];
    int i, j;
    HICON hIcon;
    McdBlock *Info;
 
    aIover[mcd-1]=0;
    ani[mcd-1]=0;
 
    ListView_DeleteAllItems(List);

    for (i=0; i<15; i++) {
  
        item.mask      = LVIF_TEXT | LVIF_IMAGE;
        item.iItem       = i;
        item.iImage    = i;
        item.pszText  = LPSTR_TEXTCALLBACK;
        item.iSubItem = 0;
 
        IconC[mcd-1][i] = 0;
        Info = &Blocks[mcd-1][i];
 
        if ((Info->Flags & 0xF) == 1 && Info->IconCount != 0) {
            hIcon = GetIcon(Info->Icon);   
 
            if (Info->IconCount > 1) {
                for(j = 0; j < 3; j++)
                    hICON[mcd-1][j][i]=hIcon;
            }
        } else {
            hIcon = eICON; 
        }
 
        ImageList_ReplaceIcon(iml, -1, hIcon);
        ListView_InsertItem(List, &item);
    } 
}

void UpdateMcdItems(int mcd, int idc) {
    HWND List = GetDlgItem(mcdDlg, idc);
    LV_ITEM item;
    HIMAGELIST iml = Iiml[mcd-1];
    int i, j;
    McdBlock *Info;
    HICON hIcon;
 
    aIover[mcd-1]=0;
    ani[mcd-1]=0;
  
    for (i=0; i<15; i++) { 
 
        item.mask     = LVIF_TEXT | LVIF_IMAGE;
        item.iItem    = i;
        item.iImage   = i;
        item.pszText  = LPSTR_TEXTCALLBACK;
        item.iSubItem = 0;
 
        IconC[mcd-1][i] = 0; 
        Info = &Blocks[mcd-1][i];
 
        if ((Info->Flags & 0xF) == 1 && Info->IconCount != 0) {
            hIcon = GetIcon(Info->Icon);   
 
            if (Info->IconCount > 1) { 
                for(j = 0; j < 3; j++)
                    hICON[mcd-1][j][i]=hIcon;
            }
        } else { 
            hIcon = eICON; 
        }
              
        ImageList_ReplaceIcon(iml, i, hIcon);
        ListView_SetItem(List, &item);
    } 
    ListView_Update(List, -1);
}
 
void McdListGetDispInfo(int mcd, int idc, LPNMHDR pnmh) {
	LV_DISPINFO *lpdi = (LV_DISPINFO *)pnmh;
	McdBlock *Info;
	char buf[256];
	static char buftitle[256];

	Info = &Blocks[mcd - 1][lpdi->item.iItem];

	switch (lpdi->item.iSubItem) {
		case 0:
			switch (Info->Flags & 0xF) {
				case 1:
					if (MultiByteToWideChar(932, 0, (LPCSTR)Info->sTitle, -1, (LPWSTR)buf, sizeof(buf)) == 0) {
						lpdi->item.pszText = Info->Title;
					} else if (WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)buf, -1, (LPSTR)buftitle, sizeof(buftitle), NULL, NULL) == 0) {
						lpdi->item.pszText = Info->Title;
					} else {
						lpdi->item.pszText = buftitle;
					}
					break;
				case 2:
					lpdi->item.pszText = _("mid link block");
					break;
				case 3:
					lpdi->item.pszText = _("terminiting link block");
					break;
			}
			break;
		case 1:
			if ((Info->Flags & 0xF0) == 0xA0) {
				if ((Info->Flags & 0xF) >= 1 &&
					(Info->Flags & 0xF) <= 3) {
					lpdi->item.pszText = _("Deleted");
				} else lpdi->item.pszText = _("Free");
			} else if ((Info->Flags & 0xF0) == 0x50)
				lpdi->item.pszText = _("Used");
			else { lpdi->item.pszText = _("Free"); }
			break;
		case 2:
			if((Info->Flags & 0xF)==1)
				lpdi->item.pszText = Info->ID;
			break;
		case 3:
			if((Info->Flags & 0xF)==1)
				lpdi->item.pszText = Info->Name;
			break;
	}
}

void McdListNotify(int mcd, int idc, LPNMHDR pnmh) {
	switch (pnmh->code) {
		case LVN_GETDISPINFO: McdListGetDispInfo(mcd, idc, pnmh); break;
	}
}

void UpdateMcdDlg() {
	int i;

	for (i=1; i<16; i++) GetMcdBlockInfo(1, i, &Blocks[0][i-1]);
	for (i=1; i<16; i++) GetMcdBlockInfo(2, i, &Blocks[1][i-1]);
	UpdateMcdItems(1, IDC_LIST1);
	UpdateMcdItems(2, IDC_LIST2);
}

void LoadMcdDlg() {
	int i;

	for (i=1; i<16; i++) GetMcdBlockInfo(1, i, &Blocks[0][i-1]);
	for (i=1; i<16; i++) GetMcdBlockInfo(2, i, &Blocks[1][i-1]);
	LoadMcdItems(1, IDC_LIST1);
	LoadMcdItems(2, IDC_LIST2);
}

void UpdateMcdIcon(int mcd, int idc) {
    HWND List = GetDlgItem(mcdDlg, idc);
    HIMAGELIST iml = Iiml[mcd-1];
    int i;
    McdBlock *Info;
    int *count; 
 
    if(!aIover[mcd-1]) {
        ani[mcd-1]++; 
 
        for (i=0; i<15; i++) {
            Info = &Blocks[mcd-1][i];
            count = &IconC[mcd-1][i];
 
            if ((Info->Flags & 0xF) != 1) continue;
            if (Info->IconCount <= 1) continue;
 
            if (*count < Info->IconCount) {
                (*count)++;
                aIover[mcd-1]=0;
 
                if(ani[mcd-1] <= (Info->IconCount-1))  // last frame and below...
                    hICON[mcd-1][ani[mcd-1]][i] = GetIcon(&Info->Icon[(*count)*16*16]);
            } else {
                aIover[mcd-1]=1;
            }
        }

    } else { 
 
        if (ani[mcd-1] > 1) ani[mcd-1] = 0;  // 1st frame
        else ani[mcd-1]++;                       // 2nd, 3rd frame
 
        for(i=0;i<15;i++) {
            Info = &Blocks[mcd-1][i];
 
            if (((Info->Flags & 0xF) == 1) && (Info->IconCount > 1))
                ImageList_ReplaceIcon(iml, i, hICON[mcd-1][ani[mcd-1]][i]);
        }
        InvalidateRect(List,  NULL, FALSE);
    }
}

static int copy = 0, copymcd = 0;
//static int listsel = 0;

BOOL CALLBACK ConfigureMcdsDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char str[256];
	LPBYTE lpAND, lpXOR;
	LPBYTE lpA, lpX;
	int i, j;

	switch(uMsg) {
		case WM_INITDIALOG:
			mcdDlg = hW;

			SetWindowText(hW, _("Memcard Manager"));

			Button_SetText(GetDlgItem(hW, IDOK),        _("OK"));
			Button_SetText(GetDlgItem(hW, IDCANCEL),    _("Cancel"));
			Button_SetText(GetDlgItem(hW, IDC_MCDSEL1), _("Select Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_FORMAT1), _("Format Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_RELOAD1), _("Reload Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_MCDSEL2), _("Select Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_FORMAT2), _("Format Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_RELOAD2), _("Reload Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_COPYTO2), _("-> Copy ->"));
			Button_SetText(GetDlgItem(hW, IDC_COPYTO1), _("<- Copy <-"));
			Button_SetText(GetDlgItem(hW, IDC_PASTE),   _("Paste"));
			Button_SetText(GetDlgItem(hW, IDC_DELETE1), _("<- Un/Delete"));
			Button_SetText(GetDlgItem(hW, IDC_DELETE2), _("Un/Delete ->"));
 
			Static_SetText(GetDlgItem(hW, IDC_FRAMEMCD1), _("Memory Card 1"));
			Static_SetText(GetDlgItem(hW, IDC_FRAMEMCD2), _("Memory Card 2"));

			lpA=lpAND=(LPBYTE)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(16*16));
			lpX=lpXOR=(LPBYTE)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(16*16));

			for(i=0;i<16;i++)
			{
				for(j=0;j<16;j++)
				{
					*lpA++=0xff;
					*lpX++=0;
				}
			}
			eICON=CreateIcon(gApp.hInstance,16,16,1,1,lpAND,lpXOR);

			HeapFree(GetProcessHeap(),0,lpAND);
			HeapFree(GetProcessHeap(),0,lpXOR);

			if (!strlen(Config.Mcd1)) strcpy(Config.Mcd1, "memcards\\Mcd001.mcr");
			if (!strlen(Config.Mcd2)) strcpy(Config.Mcd2, "memcards\\Mcd002.mcr");
			Edit_SetText(GetDlgItem(hW,IDC_MCD1), Config.Mcd1);
			Edit_SetText(GetDlgItem(hW,IDC_MCD2), Config.Mcd2);

			CreateListView(IDC_LIST1);
			CreateListView(IDC_LIST2);
 
            Iiml[0] = ImageList_Create(16, 16, ILC_COLOR16, 0, 0);
            Iiml[1] = ImageList_Create(16, 16, ILC_COLOR16, 0, 0);
 
            ListView_SetImageList(GetDlgItem(mcdDlg, IDC_LIST1), Iiml[0], LVSIL_SMALL);
            ListView_SetImageList(GetDlgItem(mcdDlg, IDC_LIST2), Iiml[1], LVSIL_SMALL);

			Button_Enable(GetDlgItem(hW, IDC_PASTE), FALSE);

			LoadMcdDlg();

			SetTimer(hW, 1, 250, NULL);

			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_COPYTO1:
					copy = ListView_GetSelectionMark(GetDlgItem(mcdDlg, IDC_LIST2));
					copymcd = 1;

					Button_Enable(GetDlgItem(hW, IDC_PASTE), TRUE);
					return TRUE;
				case IDC_COPYTO2:
					copy = ListView_GetSelectionMark(GetDlgItem(mcdDlg, IDC_LIST1));
					copymcd = 2;

					Button_Enable(GetDlgItem(hW, IDC_PASTE), TRUE);
					return TRUE;
				case IDC_PASTE:
					if (MessageBox(hW, _("Are you sure you want to paste this selection?"), _("Confirmation"), MB_YESNO) == IDNO) return TRUE;

					if (copymcd == 1) {
						Edit_GetText(GetDlgItem(hW,IDC_MCD1), str, 256);
						i = ListView_GetSelectionMark(GetDlgItem(mcdDlg, IDC_LIST1));

						// save dir data + save data
						memcpy(Mcd1Data + (i+1) * 128, Mcd2Data + (copy+1) * 128, 128);
						SaveMcd(str, Mcd1Data, (i+1) * 128, 128);
						memcpy(Mcd1Data + (i+1) * 1024 * 8, Mcd2Data + (copy+1) * 1024 * 8, 1024 * 8);
						SaveMcd(str, Mcd1Data, (i+1) * 1024 * 8, 1024 * 8);
					} else { // 2
						Edit_GetText(GetDlgItem(hW,IDC_MCD2), str, 256);
						i = ListView_GetSelectionMark(GetDlgItem(mcdDlg, IDC_LIST2));

						// save dir data + save data
						memcpy(Mcd2Data + (i+1) * 128, Mcd1Data + (copy+1) * 128, 128);
						SaveMcd(str, Mcd2Data, (i+1) * 128, 128);
						memcpy(Mcd2Data + (i+1) * 1024 * 8, Mcd1Data + (copy+1) * 1024 * 8, 1024 * 8);
						SaveMcd(str, Mcd2Data, (i+1) * 1024 * 8, 1024 * 8);
					}

					UpdateMcdDlg();

					return TRUE;
				case IDC_DELETE1:
				{
					McdBlock *Info;
					int mcd = 1;
					int i, xor = 0, j;
					unsigned char *data, *ptr;

					Edit_GetText(GetDlgItem(hW,IDC_MCD1), str, 256);
					i = ListView_GetSelectionMark(GetDlgItem(mcdDlg, IDC_LIST1));
					data = Mcd1Data;

					i++;

					ptr = data + i * 128;

					Info = &Blocks[mcd-1][i-1];

					if ((Info->Flags & 0xF0) == 0xA0) {
						if ((Info->Flags & 0xF) >= 1 &&
							(Info->Flags & 0xF) <= 3) { // deleted
							*ptr = 0x50 | (Info->Flags & 0xF);
						} else return TRUE;
					} else if ((Info->Flags & 0xF0) == 0x50) { // used
						*ptr = 0xA0 | (Info->Flags & 0xF);
					} else { return TRUE; }

					for (j=0; j<127; j++) xor^=*ptr++;
					*ptr = xor;

					SaveMcd(str, data, i * 128, 128);
					UpdateMcdDlg();
				}

					return TRUE;
				case IDC_DELETE2:
				{
					McdBlock *Info;
					int mcd = 2;
					int i, xor = 0, j;
					unsigned char *data, *ptr;

					Edit_GetText(GetDlgItem(hW,IDC_MCD2), str, 256);
					i = ListView_GetSelectionMark(GetDlgItem(mcdDlg, IDC_LIST2));
					data = Mcd2Data;

					i++;

					ptr = data + i * 128;

					Info = &Blocks[mcd-1][i-1];

					if ((Info->Flags & 0xF0) == 0xA0) {
						if ((Info->Flags & 0xF) >= 1 &&
							(Info->Flags & 0xF) <= 3) { // deleted
							*ptr = 0x50 | (Info->Flags & 0xF);
						} else return TRUE;
					} else if ((Info->Flags & 0xF0) == 0x50) { // used
						*ptr = 0xA0 | (Info->Flags & 0xF);
					} else { return TRUE; }

					for (j=0; j<127; j++) xor^=*ptr++;
					*ptr = xor;

					SaveMcd(str, data, i * 128, 128);
					UpdateMcdDlg();
				}

					return TRUE;

				case IDC_MCDSEL1: 
					Open_Mcd_Proc(hW, 1); 
					return TRUE;
				case IDC_MCDSEL2: 
					Open_Mcd_Proc(hW, 2); 
					return TRUE;
				case IDC_RELOAD1: 
					Edit_GetText(GetDlgItem(hW,IDC_MCD1), str, 256);
					LoadMcd(1, str);
					UpdateMcdDlg();
					return TRUE;
				case IDC_RELOAD2: 
					Edit_GetText(GetDlgItem(hW,IDC_MCD2), str, 256);
					LoadMcd(2, str);
					UpdateMcdDlg();
					return TRUE;
				case IDC_FORMAT1:
					if (MessageBox(hW, _("Are you sure you want to format this Memory Card?"), _("Confirmation"), MB_YESNO) == IDNO) return TRUE;
					Edit_GetText(GetDlgItem(hW,IDC_MCD1), str, 256);
					CreateMcd(str);
					LoadMcd(1, str);
					UpdateMcdDlg();
					return TRUE;
				case IDC_FORMAT2:
					if (MessageBox(hW, _("Are you sure you want to format this Memory Card?"), _("Confirmation"), MB_YESNO) == IDNO) return TRUE;
					Edit_GetText(GetDlgItem(hW,IDC_MCD2), str, 256);
					CreateMcd(str);
					LoadMcd(2, str);
					UpdateMcdDlg();
					return TRUE;
       			case IDCANCEL:
					LoadMcds(Config.Mcd1, Config.Mcd2);

					EndDialog(hW,FALSE);

					return TRUE;
       			case IDOK:
					Edit_GetText(GetDlgItem(hW,IDC_MCD1), Config.Mcd1, 256);
					Edit_GetText(GetDlgItem(hW,IDC_MCD2), Config.Mcd2, 256);

					LoadMcds(Config.Mcd1, Config.Mcd2);
					SaveConfig();

					EndDialog(hW,TRUE);

					return TRUE;
			}
		case WM_NOTIFY:
			switch (wParam) {
				case IDC_LIST1: McdListNotify(1, IDC_LIST1, (LPNMHDR)lParam); break;
				case IDC_LIST2: McdListNotify(2, IDC_LIST2, (LPNMHDR)lParam); break;
			}
			return TRUE;
		case WM_TIMER:
			UpdateMcdIcon(1, IDC_LIST1);
			UpdateMcdIcon(2, IDC_LIST2);
			return TRUE;
		case WM_DESTROY:
			DestroyIcon(eICON);
			//KillTimer(hW, 1);
			return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK ConfigureCpuDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	long tmp;

	switch(uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Cpu Config"));

			Button_SetText(GetDlgItem(hW,IDOK),        _("OK"));
			Button_SetText(GetDlgItem(hW,IDCANCEL),    _("Cancel"));

			Button_SetText(GetDlgItem(hW,IDC_XA),      _("Disable Xa Decoding"));
			Button_SetText(GetDlgItem(hW,IDC_SIO),     _("Sio Irq Always Enabled"));
			Button_SetText(GetDlgItem(hW,IDC_MDEC),    _("Black && White Movies"));
			Button_SetText(GetDlgItem(hW,IDC_CDDA),    _("Disable Cd audio"));
			Button_SetText(GetDlgItem(hW,IDC_PSXAUTO), _("Autodetect"));
			Button_SetText(GetDlgItem(hW,IDC_CPU),     _("Enable Interpreter Cpu"));
			Button_SetText(GetDlgItem(hW,IDC_PSXOUT),  _("Enable Console Output"));
			Button_SetText(GetDlgItem(hW,IDC_DEBUG),   _("Enable Debugger"));
			Button_SetText(GetDlgItem(hW,IDC_SPUIRQ),  _("Spu Irq Always Enabled"));
			Button_SetText(GetDlgItem(hW,IDC_RCNTFIX), _("Parasite Eve 2, Vandal Hearts 1/2 Fix"));
			Button_SetText(GetDlgItem(hW,IDC_VSYNCWA), _("InuYasha Sengoku Battle Fix"));

			Static_SetText(GetDlgItem(hW,IDC_MISCOPT), _("Options"));
			Static_SetText(GetDlgItem(hW,IDC_SELPSX),  _("Psx System Type"));

			Button_SetCheck(GetDlgItem(hW,IDC_XA),      Config.Xa);
			Button_SetCheck(GetDlgItem(hW,IDC_SIO),     Config.Sio);
			Button_SetCheck(GetDlgItem(hW,IDC_MDEC),    Config.Mdec);
			Button_SetCheck(GetDlgItem(hW,IDC_CDDA),    Config.Cdda);
	   		Button_SetCheck(GetDlgItem(hW,IDC_PSXAUTO), Config.PsxAuto);
	   		Button_SetCheck(GetDlgItem(hW,IDC_CPU),     (Config.Cpu == CPU_INTERPRETER));
	   		Button_SetCheck(GetDlgItem(hW,IDC_PSXOUT),  Config.PsxOut);
			Button_SetCheck(GetDlgItem(hW,IDC_DEBUG),   Config.Debug);
	   		Button_SetCheck(GetDlgItem(hW,IDC_SPUIRQ),  Config.SpuIrq);
	   		Button_SetCheck(GetDlgItem(hW,IDC_RCNTFIX), Config.RCntFix);
	   		Button_SetCheck(GetDlgItem(hW,IDC_VSYNCWA), Config.VSyncWA);
	   		ComboBox_AddString(GetDlgItem(hW,IDC_PSXTYPES), "NTSC");
	   		ComboBox_AddString(GetDlgItem(hW,IDC_PSXTYPES), "PAL");
	   		ComboBox_SetCurSel(GetDlgItem(hW,IDC_PSXTYPES),Config.PsxType);

			if (Config.Cpu == CPU_DYNAREC) {
				Config.Debug = 0;
				Button_SetCheck(GetDlgItem(hW, IDC_DEBUG), FALSE);
				EnableWindow(GetDlgItem(hW, IDC_DEBUG), FALSE);
			}

			EnableWindow(GetDlgItem(hW,IDC_PSXTYPES), !Config.PsxAuto);
			break;

		case WM_COMMAND: {
			switch (LOWORD(wParam)) {
       			case IDCANCEL: EndDialog(hW, FALSE); return TRUE;
        		case IDOK:
					tmp = ComboBox_GetCurSel(GetDlgItem(hW,IDC_PSXTYPES));
					if (tmp == 0) Config.PsxType = 0;
					else Config.PsxType = 1;

					Config.Xa      = Button_GetCheck(GetDlgItem(hW,IDC_XA));
					Config.Sio     = Button_GetCheck(GetDlgItem(hW,IDC_SIO));
					Config.Mdec    = Button_GetCheck(GetDlgItem(hW,IDC_MDEC));
					Config.Cdda    = Button_GetCheck(GetDlgItem(hW,IDC_CDDA));
					Config.PsxAuto = Button_GetCheck(GetDlgItem(hW,IDC_PSXAUTO));
					tmp = Config.Cpu;
					Config.Cpu     = (Button_GetCheck(GetDlgItem(hW,IDC_CPU)) ? CPU_INTERPRETER : CPU_DYNAREC);
					if (tmp != Config.Cpu) {
						psxCpu->Shutdown();
						if (Config.Cpu == CPU_INTERPRETER) psxCpu = &psxInt;
						else psxCpu = &psxRec;
						if (psxCpu->Init() == -1) {
							SysClose();
							exit(1);
						}
						psxCpu->Reset();
					}
					Config.PsxOut  = Button_GetCheck(GetDlgItem(hW,IDC_PSXOUT));
					Config.SpuIrq  = Button_GetCheck(GetDlgItem(hW,IDC_SPUIRQ));
					Config.RCntFix = Button_GetCheck(GetDlgItem(hW,IDC_RCNTFIX));
					Config.VSyncWA = Button_GetCheck(GetDlgItem(hW,IDC_VSYNCWA));
					tmp = Config.Debug;
					Config.Debug   = Button_GetCheck(GetDlgItem(hW,IDC_DEBUG));
					if (tmp != Config.Debug) {
						if (Config.Debug) StartDebugger();
						else StopDebugger();
					}

					SaveConfig();

					EndDialog(hW,TRUE);

					if (Config.PsxOut) OpenConsole();
					else CloseConsole();

					return TRUE;

				case IDC_CPU:
					if (Button_GetCheck(GetDlgItem(hW,IDC_CPU))) {
						EnableWindow(GetDlgItem(hW,IDC_DEBUG), TRUE);
					} else {
						Button_SetCheck(GetDlgItem(hW,IDC_DEBUG), FALSE);
						EnableWindow(GetDlgItem(hW,IDC_DEBUG), FALSE);
					}
					break;

				case IDC_PSXAUTO:
					if (Button_GetCheck(GetDlgItem(hW,IDC_PSXAUTO))) {
						EnableWindow(GetDlgItem(hW,IDC_PSXTYPES), FALSE);
					} else {
						EnableWindow(GetDlgItem(hW,IDC_PSXTYPES), TRUE);
					}
					break;
			}
		}
	}
	return FALSE;
}

void Open_Mcd_Proc(HWND hW, int mcd) {
	OPENFILENAME ofn;
	char szFileName[MAXPATHLEN];
	char szFileTitle[MAXPATHLEN];
	char szFilter[1024];
	char *str;

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));
	memset(&szFilter,    0, sizeof(szFilter));

	strcpy(szFilter, _("Psx Mcd Format (*.mcr;*.mc;*.mem;*.vgs;*.mcd;*.gme;*.ddf)"));
	str = szFilter + strlen(szFilter) + 1; 
	strcpy(str, "*.mcr;*.mcd;*.mem;*.gme;*.mc;*.ddf");

	str+= strlen(str) + 1;
	strcpy(str, _("Psx Memory Card (*.mcr;*.mc)"));
	str+= strlen(str) + 1;
	strcpy(str, "*.mcr;0*.mc");

	str+= strlen(str) + 1;
	strcpy(str, _("CVGS Memory Card (*.mem;*.vgs)"));
	str+= strlen(str) + 1;
	strcpy(str, "*.mem;*.vgs");

	str+= strlen(str) + 1;
	strcpy(str, _("Bleem Memory Card (*.mcd)"));
	str+= strlen(str) + 1;
	strcpy(str, "*.mcd");

	str+= strlen(str) + 1;
	strcpy(str, _("DexDrive Memory Card (*.gme)"));
	str+= strlen(str) + 1;
	strcpy(str, "*.gme");

	str+= strlen(str) + 1;
	strcpy(str, _("DataDeck Memory Card (*.ddf)"));
	str+= strlen(str) + 1;
	strcpy(str, "*.ddf");

	str+= strlen(str) + 1;
	strcpy(str, _("All Files"));
	str+= strlen(str) + 1;
	strcpy(str, "*.*");

    ofn.lStructSize			= sizeof(OPENFILENAME);
    ofn.hwndOwner			= hW;
    ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter	= NULL;
    ofn.nMaxCustFilter		= 0;
    ofn.nFilterIndex		= 1;
    ofn.lpstrFile			= szFileName;
    ofn.nMaxFile			= MAXPATHLEN;
    ofn.lpstrInitialDir		= "memcards";
    ofn.lpstrFileTitle		= szFileTitle;
    ofn.nMaxFileTitle		= MAXPATHLEN;
    ofn.lpstrTitle			= NULL;
    ofn.lpstrDefExt			= "MCR";
    ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) {
		Edit_SetText(GetDlgItem(hW,mcd == 1 ? IDC_MCD1 : IDC_MCD2), szFileName);
		LoadMcd(mcd, szFileName);
		UpdateMcdDlg();
	}
}

int Open_File_Proc(char *file) {
	OPENFILENAME ofn;
	char szFileName[MAXPATHLEN];
	char szFileTitle[MAXPATHLEN];
	char szFilter[256];

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));
	memset(&szFilter,    0, sizeof(szFilter));

    ofn.lStructSize			= sizeof(OPENFILENAME);
    ofn.hwndOwner			= gApp.hWnd;

	strcpy(szFilter, _("Psx Exe Format"));
	strcatz(szFilter, "*.*");

    ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter	= NULL;
    ofn.nMaxCustFilter		= 0;
    ofn.nFilterIndex		= 1;
    ofn.lpstrFile			= szFileName;
    ofn.nMaxFile			= MAXPATHLEN;
    ofn.lpstrInitialDir		= NULL;
    ofn.lpstrFileTitle		= szFileTitle;
    ofn.nMaxFileTitle		= MAXPATHLEN;
    ofn.lpstrTitle			= NULL;
    ofn.lpstrDefExt			= "EXE";
    ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) {
		strcpy(file, szFileName);
		return 1;
	} else
		return 0;
}

int Open_Iso_Proc(char *file) {
	OPENFILENAME ofn;
	char szFileName[MAXPATHLEN];
	char szFileTitle[MAXPATHLEN];
	char szFilter[256];
	char *str;

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));
	memset(&szFilter,    0, sizeof(szFilter));

    ofn.lStructSize			= sizeof(OPENFILENAME);
    ofn.hwndOwner			= gApp.hWnd;

	strcpy(szFilter, _("Psx Isos (*.iso;*.mdf;*.img;*.bin)"));
	str = szFilter + strlen(szFilter) + 1; 
	strcpy(str, "*.iso;*.mdf;*.img;*.bin");

	str += strlen(str) + 1;
	strcpy(str, _("All Files"));
	str += strlen(str) + 1;
	strcpy(str, "*.*");

    ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter	= NULL;
    ofn.nMaxCustFilter		= 0;
    ofn.nFilterIndex		= 1;
    ofn.lpstrFile			= szFileName;
    ofn.nMaxFile			= MAXPATHLEN;
    ofn.lpstrInitialDir		= NULL;
    ofn.lpstrFileTitle		= szFileTitle;
    ofn.nMaxFileTitle		= MAXPATHLEN;
    ofn.lpstrTitle			= NULL;
    ofn.lpstrDefExt			= "ISO";
    ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) {
		strcpy(file, szFileName);
		return 1;
	} else
		return 0;
}

#define _ADDSUBMENU(menu, menun, string) \
	submenu[menun] = CreatePopupMenu(); \
	AppendMenu(menu, MF_STRING | MF_POPUP, (UINT)submenu[menun], string);

#define ADDSUBMENU(menun, string) \
	_ADDSUBMENU(gApp.hMenu, menun, string);

#define ADDSUBMENUS(submn, menun, string) \
	submenu[menun] = CreatePopupMenu(); \
	InsertMenu(submenu[submn], 0, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT)submenu[menun], string);

#define ADDMENUITEM(menun, string, id) \
	item.fType = MFT_STRING; \
	item.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID; \
	item.fState = MFS_ENABLED; \
	item.wID = id; \
	sprintf(buf, string); \
	InsertMenuItem(submenu[menun], 0, TRUE, &item);

#define ADDMENUITEMC(menun, string, id) \
	item.fType = MFT_STRING; \
	item.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID; \
	item.fState = MFS_ENABLED | MFS_CHECKED; \
	item.wID = id; \
	sprintf(buf, string); \
	InsertMenuItem(submenu[menun], 0, TRUE, &item);

#define ADDSEPARATOR(menun) \
	item.fMask = MIIM_TYPE; \
	item.fType = MFT_SEPARATOR; \
	InsertMenuItem(submenu[menun], 0, TRUE, &item);

void CreateMainMenu() {
	MENUITEMINFO item;
	HMENU submenu[256];
	char buf[256];
#ifdef ENABLE_NLS
	char *lang;
	int i;
#endif

	item.cbSize = sizeof(MENUITEMINFO);
	item.dwTypeData = buf;
	item.cch = 256;

	gApp.hMenu = CreateMenu();

	ADDSUBMENU(0, _("&File"));
	ADDMENUITEM(0, _("E&xit"), ID_FILE_EXIT);
	ADDSEPARATOR(0);
	ADDMENUITEM(0, _("Run &EXE..."), ID_FILE_RUN_EXE);
	ADDMENUITEM(0, _("Run &BIOS"), ID_FILE_RUNBIOS);
	ADDMENUITEM(0, _("Run &ISO..."), ID_FILE_RUN_ISO);
	ADDMENUITEM(0, _("Run &CD"), ID_FILE_RUN_CD);

	ADDSUBMENU(0, _("&Emulator"));
	ADDSUBMENUS(0, 1, _("&States"));
	ADDSEPARATOR(0);
	ADDMENUITEM(0, _("S&witch ISO..."), ID_EMULATOR_SWITCH_ISO);
	ADDSEPARATOR(0);
	ADDMENUITEM(0, _("Re&set"), ID_EMULATOR_RESET);
	ADDMENUITEM(0, _("&Run"), ID_EMULATOR_RUN);
	ADDSUBMENUS(1, 3, _("&Save"));
	ADDSUBMENUS(1, 2, _("&Load"));
	ADDMENUITEM(2, _("&Other..."), ID_FILE_STATES_LOAD_OTHER);
	ADDMENUITEM(2, _("Slot &5"), ID_FILE_STATES_LOAD_SLOT5);
	ADDMENUITEM(2, _("Slot &4"), ID_FILE_STATES_LOAD_SLOT4);
	ADDMENUITEM(2, _("Slot &3"), ID_FILE_STATES_LOAD_SLOT3);
	ADDMENUITEM(2, _("Slot &2"), ID_FILE_STATES_LOAD_SLOT2);
	ADDMENUITEM(2, _("Slot &1"), ID_FILE_STATES_LOAD_SLOT1);
	ADDMENUITEM(3, _("&Other..."), ID_FILE_STATES_SAVE_OTHER);
	ADDMENUITEM(3, _("Slot &5"), ID_FILE_STATES_SAVE_SLOT5);
	ADDMENUITEM(3, _("Slot &4"), ID_FILE_STATES_SAVE_SLOT4);
	ADDMENUITEM(3, _("Slot &3"), ID_FILE_STATES_SAVE_SLOT3);
	ADDMENUITEM(3, _("Slot &2"), ID_FILE_STATES_SAVE_SLOT2);
	ADDMENUITEM(3, _("Slot &1"), ID_FILE_STATES_SAVE_SLOT1);

	ADDSUBMENU(0, _("&Configuration"));
	ADDMENUITEM(0, _("Cheat &Search..."), ID_CONFIGURATION_CHEATSEARCH);
	ADDMENUITEM(0, _("Ch&eat Code..."), ID_CONFIGURATION_CHEATLIST);
	ADDSEPARATOR(0);
#ifdef ENABLE_NLS
	ADDSUBMENUS(0, 1, _("&Language"));

	if (langs != langs) free(langs);
	langs = (_langs*)malloc(sizeof(_langs));
	strcpy(langs[0].lang, "English");
	InitLanguages(); i=1;
	while ((lang = GetLanguageNext()) != NULL) {
		langs = (_langs*)realloc(langs, sizeof(_langs)*(i+1));
		strcpy(langs[i].lang, lang);
		if (!strcmp(Config.Lang, lang)) {
			ADDMENUITEMC(1, ParseLang(langs[i].lang), ID_LANGS + i);
		} else {
			ADDMENUITEM(1, ParseLang(langs[i].lang), ID_LANGS + i);
		}
		i++;
	}
	CloseLanguages();
	langsMax = i;
	if (!strcmp(Config.Lang, "English")) {
		ADDMENUITEMC(1, _("English"), ID_LANGS);
	} else {
		ADDMENUITEM(1, _("English"), ID_LANGS);
	}
	ADDSEPARATOR(0);
#endif
	ADDMENUITEM(0, _("&Memory cards..."), ID_CONFIGURATION_MEMORYCARDMANAGER);
	ADDMENUITEM(0, _("C&PU..."), ID_CONFIGURATION_CPU);
	ADDSEPARATOR(0);
	ADDMENUITEM(0, _("&NetPlay..."), ID_CONFIGURATION_NETPLAY);
	ADDSEPARATOR(0);
	ADDMENUITEM(0, _("&Controllers..."), ID_CONFIGURATION_CONTROLLERS);
	ADDMENUITEM(0, _("CD-&ROM..."), ID_CONFIGURATION_CDROM);
	ADDMENUITEM(0, _("&Sound..."), ID_CONFIGURATION_SOUND);
	ADDMENUITEM(0, _("&Graphics..."), ID_CONFIGURATION_GRAPHICS);
	ADDSEPARATOR(0);
	ADDMENUITEM(0, _("&Plugins && Bios..."), ID_CONFIGURATION);

	ADDSUBMENU(0, _("&Help"));
	ADDMENUITEM(0, _("&About..."), ID_HELP_ABOUT);

	if (CdromId[0] != '\0') {
		EnableMenuItem(gApp.hMenu, ID_CONFIGURATION_NETPLAY, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_CONFIGURATION_CONTROLLERS, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_CONFIGURATION_CDROM, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_CONFIGURATION_SOUND, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_CONFIGURATION_GRAPHICS, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_CONFIGURATION, MF_BYCOMMAND | MF_GRAYED);
		if (!UsingIso()) {
			EnableMenuItem(gApp.hMenu, ID_EMULATOR_SWITCH_ISO, MF_BYCOMMAND | MF_GRAYED);
		}
	} else {
		EnableMenuItem(gApp.hMenu, ID_EMULATOR_RESET, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_EMULATOR_RUN, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_EMULATOR_SWITCH_ISO, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_SLOT1, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_SLOT2, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_SLOT3, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_SLOT4, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_SLOT5, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_OTHER, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_SAVE_SLOT1, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_SAVE_SLOT2, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_SAVE_SLOT3, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_SAVE_SLOT4, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_SAVE_SLOT5, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_FILE_STATES_SAVE_OTHER, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gApp.hMenu, ID_CONFIGURATION_CHEATSEARCH, MF_BYCOMMAND | MF_GRAYED);
	}
}

void CreateMainWindow(int nCmdShow) {
	WNDCLASS wc;
	HWND hWnd;

	wc.lpszClassName = "PCSX Main";
	wc.lpfnWndProc = MainWndProc;
	wc.style = 0;
	wc.hInstance = gApp.hInstance;
	wc.hIcon = LoadIcon(gApp.hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_MENUTEXT);
	wc.lpszMenuName = 0;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;

	RegisterClass(&wc);

	hWnd = CreateWindow("PCSX Main",
						"PCSX",
						WS_CAPTION | WS_POPUPWINDOW | WS_MINIMIZEBOX,
						CW_USEDEFAULT,
						0,
						350,
						220,
						NULL,
						NULL,
						gApp.hInstance,
						NULL);

	gApp.hWnd = hWnd;
	ResetMenuSlots();

	CreateMainMenu();
	SetMenu(gApp.hWnd, gApp.hMenu);

	ShowWindow(hWnd, nCmdShow);
}

#ifdef ENABLE_NLS

WIN32_FIND_DATA lFindData;
HANDLE lFind;
int lFirst;

void InitLanguages() {
	lFind = FindFirstFile("Langs\\*", &lFindData);
	lFirst = 1;
}

char *GetLanguageNext() {
	for (;;) {
		if (!strcmp(lFindData.cFileName, ".")) {
			if (FindNextFile(lFind, &lFindData) == FALSE)
				return NULL;
			continue;
		}
		if (!strcmp(lFindData.cFileName, "..")) {
			if (FindNextFile(lFind, &lFindData) == FALSE)
				return NULL;
			continue;
		}
		break;
	}
	if (lFirst == 0) {
		if (FindNextFile(lFind, &lFindData) == FALSE)
			return NULL;
	} else lFirst = 0;
	if (lFind==INVALID_HANDLE_VALUE) return NULL;

	return lFindData.cFileName;
}

void CloseLanguages() {
	if (lFind != INVALID_HANDLE_VALUE) FindClose(lFind);
}

void ChangeLanguage(char *lang) {
	strcpy(Config.Lang, lang);
	SaveConfig();
	LoadConfig();
}

#endif

int SysInit() {
	if (Config.PsxOut) OpenConsole();

	if (EmuInit() == -1) return -1;

#ifdef EMU_LOG
	emuLog = fopen("emuLog.txt","w");
	setvbuf(emuLog, NULL,  _IONBF, 0);
#endif

	while (LoadPlugins(0) == -1) {
		CancelQuit = 1;
		ConfigurePlugins(gApp.hWnd);
		CancelQuit = 0;
	}
	LoadMcds(Config.Mcd1, Config.Mcd2);

	if (Config.Debug) StartDebugger();

	return 0;
}

void SysReset() {
	EmuReset();
}

void SysClose() {
	EmuShutdown();
	ReleasePlugins();

	StopDebugger();

	if (Config.PsxOut) CloseConsole();

	if (emuLog != NULL) fclose(emuLog);
}

void SysPrintf(const char *fmt, ...) {
	va_list list;
	char msg[512];
	DWORD tmp;

	if (!hConsole) return;

	va_start(list,fmt);
	vsprintf(msg,fmt,list);
	va_end(list);

	WriteConsole(hConsole, msg, (DWORD)strlen(msg), &tmp, 0);
#ifdef EMU_LOG
#ifndef LOG_STDOUT
	if (emuLog != NULL) fprintf(emuLog, "%s", msg);
#endif
#endif
}

void SysMessage(const char *fmt, ...) {
	va_list list;
	char tmp[512];

	va_start(list,fmt);
	vsprintf(tmp,fmt,list);
	va_end(list);
	MessageBox(0, tmp, _("Pcsx Msg"), 0);
}

static char *err = N_("Error Loading Symbol");
static int errval;

void *SysLoadLibrary(const char *lib) {
	return LoadLibrary(lib);
}

void *SysLoadSym(void *lib, const char *sym) {
	void *tmp = GetProcAddress((HINSTANCE)lib, sym);
	if (tmp == NULL) errval = 1;
	else errval = 0;
	return tmp;
}

const char *SysLibError() {
	if (errval) { errval = 0; return err; }
	return NULL;
}

void SysCloseLibrary(void *lib) {
	FreeLibrary((HINSTANCE)lib);
}

void SysUpdate() {
	MSG msg;

	while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void SysRunGui() {
	RestoreWindow();
	RunGui();
}

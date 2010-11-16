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

#ifndef __WIN32_H__
#define __WIN32_H__

typedef struct {
	HWND hWnd;           // Main window handle
	HINSTANCE hInstance; // Application instance
	HMENU hMenu;         // Main window menu
} AppData;

AppData gApp;
HANDLE hConsole;

extern int StatesC;
extern int AccBreak;
extern int ConfPlug;
extern int CancelQuit;
extern char cfgfile[256];
extern int Running;
extern char PcsxDir[256];

void strcatz(char *dst, char *src);

LRESULT WINAPI MainWndProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK ConfigureMcdsDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ConfigureCpuDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ConfigureNetPlayDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI CheatDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI CheatSearchDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ConfigurePlugins(HWND hWnd);

int  Open_File_Proc(char *file);
int  Open_Iso_Proc(char *file);
void Open_Mcd_Proc(HWND hW, int MCDID);
void CreateMainWindow(int nCmdShow);
void RunGui();
void PADhandleKey(int key);

int  LoadConfig();
void SaveConfig();

void UpdateMenuSlots();
void ResetMenuSlots();

void InitLanguages();
char *GetLanguageNext();
void CloseLanguages();
void ChangeLanguage(char *lang);

#endif /* __WIN32_H__ */

/*  Cheat Support for PCSX-Reloaded
 *  Copyright (C) 2009, Wei Mingzhi <whistler_wmz@users.sf.net>.
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
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif
#include <commctrl.h>
#include <stdio.h>
#include "psxcommon.h"
#include "psxmem.h"
#include "cheat.h"
#include "resource.h"
#include "Win32.h"

static void UpdateCheatDlg(HWND hW) {
    HWND		List;
    LV_ITEM		item;
	int			i;

	List = GetDlgItem(hW, IDC_CODELIST);

	ListView_DeleteAllItems(List);

	for (i = 0; i < NumCheats; i++) {
		memset(&item, 0, sizeof(item));

		item.mask		= LVIF_TEXT;
		item.iItem		= i;
		item.pszText	= Cheats[i].Descr;
		item.iSubItem	= 0;

		SendMessage(List, LVM_INSERTITEM, 0, (LPARAM)&item);

		item.pszText	= (Cheats[i].Enabled ? _("Yes") : _("No"));
		item.iSubItem	= 1;

		SendMessage(List, LVM_SETITEM, 0, (LPARAM)&item);
	}
}

static int		iEditItem = -1;
static char		szDescr[256], szCode[1024];

static LRESULT WINAPI CheatEditDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int		i;

	switch (uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Edit Cheat"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_DESCR), _("Description:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_CODE), _("Cheat Code:"));
			Button_SetText(GetDlgItem(hW, IDOK), _("OK"));
			Button_SetText(GetDlgItem(hW, IDCANCEL), _("Cancel"));

			assert(iEditItem != -1 && iEditItem < NumCheats);

			Edit_SetText(GetDlgItem(hW, IDC_DESCR), Cheats[iEditItem].Descr);

			szCode[0] = '\0';
			for (i = Cheats[iEditItem].First; i < Cheats[iEditItem].First + Cheats[iEditItem].n; i++) {
				sprintf(szDescr, "%.8X %.4X\r\n", CheatCodes[i].Addr, CheatCodes[i].Val);
				strcat(szCode, szDescr);
			}
			Edit_SetText(GetDlgItem(hW, IDC_CODE), szCode);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					Edit_GetText(GetDlgItem(hW, IDC_DESCR), szDescr, 256);
					Edit_GetText(GetDlgItem(hW, IDC_CODE), szCode, 1024);

					if (EditCheat(iEditItem, szDescr, szCode) != 0) {
						SysMessage(_("Invalid cheat code!"));
					}
					else {
                        EndDialog(hW, TRUE);
                        return TRUE;
					}
					break;

				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;
			}
			break;

		case WM_CLOSE:
			EndDialog(hW, FALSE);
			return TRUE;
	}

	return FALSE;
}

static LRESULT WINAPI CheatAddDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Add New Cheat"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_DESCR), _("Description:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_CODE), _("Cheat Code:"));
			Button_SetText(GetDlgItem(hW, IDOK), _("OK"));
			Button_SetText(GetDlgItem(hW, IDCANCEL), _("Cancel"));
			Edit_SetText(GetDlgItem(hW, IDC_DESCR), szDescr);
			Edit_SetText(GetDlgItem(hW, IDC_CODE), szCode);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					Edit_GetText(GetDlgItem(hW, IDC_DESCR), szDescr, 256);
					Edit_GetText(GetDlgItem(hW, IDC_CODE), szCode, 1024);

					if (AddCheat(szDescr, szCode) != 0) {
						SysMessage(_("Invalid cheat code!"));
					}
					else {
                        EndDialog(hW, TRUE);
                        return TRUE;
					}
					break;

				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;
			}
			break;

		case WM_CLOSE:
			EndDialog(hW, FALSE);
			return TRUE;
	}

	return FALSE;
}

LRESULT WINAPI CheatDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HWND			List;
	LV_COLUMN		col;
	LV_ITEM			item;
	int				i;
	OPENFILENAME	ofn;
	char			szFileName[256];
	char			szFileTitle[256];
	char			szFilter[256];

	switch (uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Edit Cheat Codes"));

			Button_SetText(GetDlgItem(hW, IDC_ADDCODE), _("&Add Code"));
			Button_SetText(GetDlgItem(hW, IDC_EDITCODE), _("&Edit Code"));
			Button_SetText(GetDlgItem(hW, IDC_REMOVECODE), _("&Remove Code"));
			Button_SetText(GetDlgItem(hW, IDC_TOGGLECODE), _("&Enable/Disable"));
			Button_SetText(GetDlgItem(hW, IDC_LOADCODE), _("&Load..."));
			Button_SetText(GetDlgItem(hW, IDC_SAVECODE), _("&Save As..."));
			Button_SetText(GetDlgItem(hW, IDCANCEL), _("&Close"));

			List = GetDlgItem(hW, IDC_CODELIST);

            SendMessage(List, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

			memset(&col, 0, sizeof(col));

			col.mask	= LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			col.fmt		= LVCFMT_LEFT;

			col.pszText		= _("Description");
			col.cx			= 400;

			SendMessage(List, LVM_INSERTCOLUMN, 0, (LPARAM)&col);

			col.pszText		= _("Enabled");
			col.cx			= 55;

			SendMessage(List, LVM_INSERTCOLUMN, 1, (LPARAM)&col);

			UpdateCheatDlg(hW);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;

				case IDC_ADDCODE:
					i = NumCheats;
					szDescr[0] = '\0';
					szCode[0] = '\0';

					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATEDIT), hW, (DLGPROC)CheatAddDlgProc);

					if (NumCheats > i) {
						// new cheat added
						List = GetDlgItem(hW, IDC_CODELIST);
						memset(&item, 0, sizeof(item));

						item.mask		= LVIF_TEXT;
						item.iItem		= i;
						item.pszText	= Cheats[i].Descr;
						item.iSubItem	= 0;

						SendMessage(List, LVM_INSERTITEM, 0, (LPARAM)&item);

						item.pszText	= (Cheats[i].Enabled ? _("Yes") : _("No"));
						item.iSubItem	= 1;

						SendMessage(List, LVM_SETITEM, 0, (LPARAM)&item);
					}
					break;

				case IDC_EDITCODE:
					List = GetDlgItem(hW, IDC_CODELIST);
					iEditItem = ListView_GetSelectionMark(List);

					if (iEditItem != -1) {
						DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATEDIT), hW, (DLGPROC)CheatEditDlgProc);

						memset(&item, 0, sizeof(item));

						item.mask		= LVIF_TEXT;
						item.iItem		= iEditItem;
						item.pszText	= Cheats[iEditItem].Descr;
						item.iSubItem	= 0;

						SendMessage(List, LVM_SETITEM, 0, (LPARAM)&item);
					}
					break;

				case IDC_REMOVECODE:
					List = GetDlgItem(hW, IDC_CODELIST);
					i = ListView_GetSelectionMark(List);

					if (i != -1) {
						RemoveCheat(i);
						ListView_DeleteItem(List, i);
						ListView_SetSelectionMark(List, -1);
					}
					break;

				case IDC_TOGGLECODE:
					List = GetDlgItem(hW, IDC_CODELIST);
					i = ListView_GetSelectionMark(List);

					if (i != -1) {
						Cheats[i].Enabled ^= 1;

						memset(&item, 0, sizeof(item));

						item.mask		= LVIF_TEXT;
						item.iItem		= i;
						item.pszText	= (Cheats[i].Enabled ? _("Yes") : _("No"));
						item.iSubItem	= 1;

						SendMessage(List, LVM_SETITEM, 0, (LPARAM)&item);
					}
					break;

				case IDC_LOADCODE:
					memset(&szFileName,  0, sizeof(szFileName));
					memset(&szFileTitle, 0, sizeof(szFileTitle));
					memset(&szFilter,    0, sizeof(szFilter));

					strcpy(szFilter, _("PCSX Cheat Code Files"));
					strcatz(szFilter, "*.*");

					ofn.lStructSize			= sizeof(OPENFILENAME);
					ofn.hwndOwner			= hW;
					ofn.lpstrFilter			= szFilter;
					ofn.lpstrCustomFilter	= NULL;
					ofn.nMaxCustFilter		= 0;
					ofn.nFilterIndex		= 1;
					ofn.lpstrFile			= szFileName;
					ofn.nMaxFile			= 256;
					ofn.lpstrInitialDir		= ".\\Cheats";
					ofn.lpstrFileTitle		= szFileTitle;
					ofn.nMaxFileTitle		= 256;
					ofn.lpstrTitle			= NULL;
					ofn.lpstrDefExt			= "CHT";
					ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

					if (GetOpenFileName((LPOPENFILENAME)&ofn)) {
						LoadCheats(szFileName);
						UpdateCheatDlg(hW);
					}
					break;

				case IDC_SAVECODE:
					memset(&szFileName,  0, sizeof(szFileName));
					memset(&szFileTitle, 0, sizeof(szFileTitle));
					memset(&szFilter,    0, sizeof(szFilter));

					strcpy(szFilter, _("PCSX Cheat Code Files"));
					strcatz(szFilter, "*.*");

					ofn.lStructSize			= sizeof(OPENFILENAME);
					ofn.hwndOwner			= hW;
					ofn.lpstrFilter			= szFilter;
					ofn.lpstrCustomFilter	= NULL;
					ofn.nMaxCustFilter		= 0;
					ofn.nFilterIndex		= 1;
					ofn.lpstrFile			= szFileName;
					ofn.nMaxFile			= 256;
					ofn.lpstrInitialDir		= ".\\Cheats";
					ofn.lpstrFileTitle		= szFileTitle;
					ofn.nMaxFileTitle		= 256;
					ofn.lpstrTitle			= NULL;
					ofn.lpstrDefExt			= "CHT";
					ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

					if (GetOpenFileName((LPOPENFILENAME)&ofn)) {
						SaveCheats(szFileName);
					}
					break;
			}
			break;

		case WM_NOTIFY:
			switch (LOWORD(wParam)) {
				case IDC_CODELIST:
					List = GetDlgItem(hW, IDC_CODELIST);
					i = ListView_GetSelectionMark(List);

					if (i != -1) {
						Button_Enable(GetDlgItem(hW, IDC_EDITCODE), TRUE);
						Button_Enable(GetDlgItem(hW, IDC_REMOVECODE), TRUE);
						Button_Enable(GetDlgItem(hW, IDC_TOGGLECODE), TRUE);
					}
					else {
						Button_Enable(GetDlgItem(hW, IDC_EDITCODE), FALSE);
						Button_Enable(GetDlgItem(hW, IDC_REMOVECODE), FALSE);
						Button_Enable(GetDlgItem(hW, IDC_TOGGLECODE), FALSE);
					}

					Button_Enable(GetDlgItem(hW, IDC_SAVECODE), (NumCheats > 0));
					break;
			}
			break;

		case WM_CLOSE:
			EndDialog(hW, FALSE);
			return TRUE;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////

#define SEARCH_EQUALVAL				0
#define SEARCH_NOTEQUALVAL			1
#define SEARCH_RANGE				2
#define SEARCH_INCBY				3
#define SEARCH_DECBY				4
#define SEARCH_INC					5
#define SEARCH_DEC					6
#define SEARCH_DIFFERENT			7
#define SEARCH_NOCHANGE				8

#define SEARCHTYPE_8BIT				0
#define SEARCHTYPE_16BIT			1
#define SEARCHTYPE_32BIT			2

#define SEARCHBASE_DEC				0
#define SEARCHBASE_HEX				1

static char current_search			= SEARCH_EQUALVAL;
static char current_searchtype		= SEARCHTYPE_8BIT;
static char current_searchbase		= SEARCHBASE_DEC;
static uint32_t current_valuefrom	= 0;
static uint32_t current_valueto		= 0;

static void UpdateCheatSearchDlg(HWND hW) {
	char		buf[256];
	int			i;

	SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_RESETCONTENT, 0, 0);
	SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_RESETCONTENT, 0, 0);

	Button_Enable(GetDlgItem(hW, IDC_FREEZE), FALSE);
	Button_Enable(GetDlgItem(hW, IDC_MODIFY), FALSE);
	Button_Enable(GetDlgItem(hW, IDC_COPY), FALSE);

	SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Equal Value"));
	SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Not Equal Value"));
	SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Range"));

	if (prevM != NULL) {
		SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Increased By"));
		SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Decreased By"));
		SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Increased"));
		SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Decreased"));
		SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("Different"));
		SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_ADDSTRING, 0, (LPARAM)_("No Change"));

		ComboBox_Enable(GetDlgItem(hW, IDC_DATATYPE), FALSE);
	}
	else {
		ComboBox_Enable(GetDlgItem(hW, IDC_DATATYPE), TRUE);
	}

	SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_SETCURSEL, (WPARAM)current_search, 0);

	if (current_search == SEARCH_RANGE) {
		ShowWindow(GetDlgItem(hW, IDC_LABEL_TO), SW_SHOW);
		ShowWindow(GetDlgItem(hW, IDC_VALUETO), SW_SHOW);
	}
	else {
		ShowWindow(GetDlgItem(hW, IDC_LABEL_TO), SW_HIDE);
		ShowWindow(GetDlgItem(hW, IDC_VALUETO), SW_HIDE);
	}

	SendMessage(GetDlgItem(hW, IDC_DATATYPE), CB_SETCURSEL, (WPARAM)current_searchtype, 0);
	SendMessage(GetDlgItem(hW, IDC_DATABASE), CB_SETCURSEL, (WPARAM)current_searchbase, 0);

	if (current_searchbase == SEARCHBASE_HEX) {
		sprintf(buf, "%X", current_valuefrom);
		SetWindowText(GetDlgItem(hW, IDC_VALUEFROM), buf);
		sprintf(buf, "%X", current_valueto);
		SetWindowText(GetDlgItem(hW, IDC_VALUETO), buf);
	}
	else {
		sprintf(buf, "%u", current_valuefrom);
		SetWindowText(GetDlgItem(hW, IDC_VALUEFROM), buf);
		sprintf(buf, "%u", current_valueto);
		SetWindowText(GetDlgItem(hW, IDC_VALUETO), buf);
	}

	if (prevM == NULL) {
		SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_ADDSTRING, (WPARAM)0, (LPARAM)_("Enter the values and start your search."));
		EnableWindow(GetDlgItem(hW, IDC_RESLIST), FALSE);
	}
	else {
		if (NumSearchResults == 0) {
			SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_ADDSTRING, (WPARAM)0, (LPARAM)_("No addresses found."));
			EnableWindow(GetDlgItem(hW, IDC_RESLIST), FALSE);
		}
		else if (NumSearchResults > 100) {
			SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_ADDSTRING, (WPARAM)0, (LPARAM)_("Too many addresses found."));
			EnableWindow(GetDlgItem(hW, IDC_RESLIST), FALSE);
		}
		else {
			for (i = 0; i < NumSearchResults; i++) {
				u32 addr = SearchResults[i];

				switch (current_searchtype) {
					case SEARCHTYPE_8BIT:
						sprintf(buf, _("%.8X    Current: %u (%.2X), Previous: %u (%.2X)"),
							addr, PSXMu8(addr), PSXMu8(addr), PrevMu8(addr), PrevMu8(addr));
						break;

					case SEARCHTYPE_16BIT:
						sprintf(buf, _("%.8X    Current: %u (%.4X), Previous: %u (%.4X)"),
							addr, PSXMu16(addr), PSXMu16(addr), PrevMu16(addr), PrevMu16(addr));
						break;

					case SEARCHTYPE_32BIT:
						sprintf(buf, _("%.8X    Current: %u (%.8X), Previous: %u (%.8X)"),
							addr, PSXMu32(addr), PSXMu32(addr), PrevMu32(addr), PrevMu32(addr));
						break;

					default:
						assert(FALSE); // impossible
						break;
				}

				SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_ADDSTRING, (WPARAM)0, (LPARAM)buf);
				SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_SETITEMDATA, i, (LPARAM)i);
			}
			EnableWindow(GetDlgItem(hW, IDC_RESLIST), TRUE);
		}
	}

	sprintf(buf, _("Founded Addresses: %d"), NumSearchResults);
	Static_SetText(GetDlgItem(hW, IDC_LABEL_RESULTSFOUND), buf);
}

static int iCurItem = 0;

static LRESULT WINAPI CheatFreezeProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	u32			val;
	char		buf[256];

	switch (uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Freeze"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_ADDRESS), _("Address:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_VALUE), _("Value:"));

			sprintf(buf, "%.8X", SearchResults[iCurItem]);
			SetWindowText(GetDlgItem(hW, IDC_ADDRESS), buf);

			switch (current_searchtype) {
				case SEARCHTYPE_8BIT:
					val = PSXMu8(SearchResults[iCurItem]);
					break;

				case SEARCHTYPE_16BIT:
					val = PSXMu16(SearchResults[iCurItem]);
					break;

				case SEARCHTYPE_32BIT:
					val = PSXMu32(SearchResults[iCurItem]);
					break;

				default:
					assert(FALSE); // should not reach here
					break;
			}

			sprintf(buf, "%u", val);
			SetWindowText(GetDlgItem(hW, IDC_VALUE), buf);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					val = 0;
					GetWindowText(GetDlgItem(hW, IDC_VALUE), buf, 255);
					sscanf(buf, "%u", &val);

					switch (current_searchtype) {
						case SEARCHTYPE_8BIT:
							if (val > (u32)0xFF) {
								val = 0xFF;
							}
							sprintf(szCode, "%.8X %.4X", (SearchResults[iCurItem] & 0x1FFFFF) | (CHEAT_CONST8 << 24), val);
							break;

						case SEARCHTYPE_16BIT:
							if (val > (u32)0xFFFF) {
								val = 0xFFFF;
							}
							sprintf(szCode, "%.8X %.4X", (SearchResults[iCurItem] & 0x1FFFFF) | (CHEAT_CONST16 << 24), val);
							break;

						case SEARCHTYPE_32BIT:
							sprintf(szCode, "%.8X %.4X\n%.8X %.4X",
								(SearchResults[iCurItem] & 0x1FFFFF) | (CHEAT_CONST16 << 24), val & 0xFFFF,
								((SearchResults[iCurItem] + 2) & 0x1FFFFF) | (CHEAT_CONST16 << 24), ((val & 0xFFFF0000) >> 16) & 0xFFFF);
							break;

						default:
							assert(FALSE); // should not reach here
							break;
					}

					sprintf(szDescr, _("Freeze %.8X"), SearchResults[iCurItem]);

					if (DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATEDIT), hW, (DLGPROC)CheatAddDlgProc)) {
						Cheats[NumCheats - 1].Enabled = 1;
						EndDialog(hW, TRUE);
						return TRUE;
					}
					break;

				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;
			}
			break;

		case WM_CLOSE:
			EndDialog(hW, FALSE);
			return TRUE;
	}

	return FALSE;
}

static LRESULT WINAPI CheatModifyProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	u32			val;
	char		buf[256];

	switch (uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Modify"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_ADDRESS), _("Address:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_VALUE), _("Value:"));

			sprintf(buf, "%.8X", SearchResults[iCurItem]);
			SetWindowText(GetDlgItem(hW, IDC_ADDRESS), buf);

			switch (current_searchtype) {
				case SEARCHTYPE_8BIT:
					val = PSXMu8(SearchResults[iCurItem]);
					break;

				case SEARCHTYPE_16BIT:
					val = PSXMu16(SearchResults[iCurItem]);
					break;

				case SEARCHTYPE_32BIT:
					val = PSXMu32(SearchResults[iCurItem]);
					break;

				default:
					assert(FALSE); // should not reach here
					break;
			}

			sprintf(buf, "%u", val);
			SetWindowText(GetDlgItem(hW, IDC_VALUE), buf);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					val = 0;
					GetWindowText(GetDlgItem(hW, IDC_VALUE), buf, 255);
					sscanf(buf, "%u", &val);

					switch (current_searchtype) {
						case SEARCHTYPE_8BIT:
							if (val > 0xFF) {
								val = 0xFF;
							}
							psxMemWrite8(SearchResults[iCurItem], (u8)val);
							break;

						case SEARCHTYPE_16BIT:
							if (val > 0xFFFF) {
								val = 0xFFFF;
							}
							psxMemWrite16(SearchResults[iCurItem], (u16)val);
							break;

						case SEARCHTYPE_32BIT:
							psxMemWrite32(SearchResults[iCurItem], (u32)val);
							break;

						default:
							assert(FALSE); // should not reach here
							break;
					}

					EndDialog(hW, TRUE);
					return TRUE;

				case IDCANCEL:
					EndDialog(hW, TRUE);
					return FALSE;
			}
			break;

		case WM_CLOSE:
			EndDialog(hW, TRUE);
			return FALSE;
	}

	return FALSE;
}

LRESULT WINAPI CheatSearchDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char			buf[256];
	uint32_t		i;

	switch (uMsg) {
		case WM_INITDIALOG:
			SetWindowText(hW, _("Cheat Search"));

			Static_SetText(GetDlgItem(hW, IDC_LABEL_SEARCHFOR), _("Search For:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_DATATYPE), _("Data Type:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_VALUE), _("Value:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_DATABASE), _("Data Base:"));
			Static_SetText(GetDlgItem(hW, IDC_LABEL_TO), _("To:"));
			Button_SetText(GetDlgItem(hW, IDC_FREEZE), _("&Freeze"));
			Button_SetText(GetDlgItem(hW, IDC_MODIFY), _("&Modify"));
			Button_SetText(GetDlgItem(hW, IDC_COPY), _("&Copy"));
			Button_SetText(GetDlgItem(hW, IDC_SEARCH), _("&Search"));
			Button_SetText(GetDlgItem(hW, IDC_NEWSEARCH), _("&New Search"));
			Button_SetText(GetDlgItem(hW, IDCANCEL), _("C&lose"));

			SendMessage(GetDlgItem(hW, IDC_DATATYPE), CB_ADDSTRING, 0, (LPARAM)_("8-bit"));
			SendMessage(GetDlgItem(hW, IDC_DATATYPE), CB_ADDSTRING, 0, (LPARAM)_("16-bit"));
			SendMessage(GetDlgItem(hW, IDC_DATATYPE), CB_ADDSTRING, 0, (LPARAM)_("32-bit"));
			SendMessage(GetDlgItem(hW, IDC_DATABASE), CB_ADDSTRING, 0, (LPARAM)_("Decimal"));
			SendMessage(GetDlgItem(hW, IDC_DATABASE), CB_ADDSTRING, 0, (LPARAM)_("Hexadecimal"));

			UpdateCheatSearchDlg(hW);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;

				case IDC_FREEZE:
					iCurItem = SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_GETCURSEL, 0, 0);
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATVALEDIT), hW, (DLGPROC)CheatFreezeProc);
					break;

				case IDC_MODIFY:
					iCurItem = SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_GETCURSEL, 0, 0);
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATVALEDIT), hW, (DLGPROC)CheatModifyProc);
					UpdateCheatSearchDlg(hW);
					break;

				case IDC_COPY:
					i = SendMessage(GetDlgItem(hW, IDC_RESLIST), LB_GETCURSEL, 0, 0);
					sprintf(buf, "%.8X", SearchResults[i]);

					if (OpenClipboard(gApp.hWnd)) {
						HGLOBAL hglbCopy = GlobalAlloc(GHND, 256);
						char *p;

						if (hglbCopy != NULL) {
							p = (char *)GlobalLock(hglbCopy);
							strcpy(p, buf);
							GlobalUnlock(p);

							EmptyClipboard();
							SetClipboardData(CF_TEXT, (HANDLE)hglbCopy);
						}

						CloseClipboard();
					}
					break;

				case IDC_SEARCH:
					current_search = SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_GETCURSEL, 0, 0);
					current_searchtype = SendMessage(GetDlgItem(hW, IDC_DATATYPE), CB_GETCURSEL, 0, 0);
					current_searchbase = SendMessage(GetDlgItem(hW, IDC_DATABASE), CB_GETCURSEL, 0, 0);
					current_valuefrom = 0;
					current_valueto = 0;

					if (current_searchbase == SEARCHBASE_DEC) {
						GetWindowText(GetDlgItem(hW, IDC_VALUEFROM), (LPTSTR)buf, 255);
						sscanf(buf, "%u", &current_valuefrom);
						GetWindowText(GetDlgItem(hW, IDC_VALUETO), (LPTSTR)buf, 255);
						sscanf(buf, "%u", &current_valueto);
					}
					else {
						GetWindowText(GetDlgItem(hW, IDC_VALUEFROM), (LPTSTR)buf, 255);
						sscanf(buf, "%x", &current_valuefrom);
						GetWindowText(GetDlgItem(hW, IDC_VALUETO), (LPTSTR)buf, 255);
						sscanf(buf, "%x", &current_valueto);
					}

					switch (current_searchtype) {
						case SEARCHTYPE_8BIT:
							if (current_valuefrom > (u32)0xFF) {
								current_valuefrom = 0xFF;
							}
							if (current_valueto > (u32)0xFF) {
								current_valueto = 0xFF;
							}
							break;

						case SEARCHTYPE_16BIT:
							if (current_valuefrom > (u32)0xFFFF) {
								current_valuefrom = 0xFFFF;
							}
							if (current_valueto > (u32)0xFFFF) {
								current_valueto = 0xFFFF;
							}
						break;
					}

					if (current_search == SEARCH_RANGE && current_valuefrom > current_valueto) {
						u32 t = current_valuefrom;
						current_valuefrom = current_valueto;
						current_valueto = t;
					}

					switch (current_search) {
						case SEARCH_EQUALVAL:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchEqual8((u8)current_valuefrom);
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchEqual16((u16)current_valuefrom);
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchEqual32((u32)current_valuefrom);
									break;
							}
							break;

						case SEARCH_NOTEQUALVAL:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchNotEqual8((u8)current_valuefrom);
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchNotEqual16((u16)current_valuefrom);
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchNotEqual32((u32)current_valuefrom);
									break;
							}
							break;

						case SEARCH_RANGE:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchRange8((u8)current_valuefrom, (u8)current_valueto);
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchRange16((u16)current_valuefrom, (u16)current_valueto);
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchRange32((u32)current_valuefrom, (u32)current_valueto);
									break;
							}
							break;

						case SEARCH_INCBY:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchIncreasedBy8((u8)current_valuefrom);
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchIncreasedBy16((u16)current_valuefrom);
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchIncreasedBy32((u32)current_valuefrom);
									break;
							}
							break;

						case SEARCH_DECBY:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchDecreasedBy8((u8)current_valuefrom);
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchDecreasedBy16((u16)current_valuefrom);
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchDecreasedBy32((u32)current_valuefrom);
									break;
							}
							break;

						case SEARCH_INC:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchIncreased8();
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchIncreased16();
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchIncreased32();
									break;
							}
							break;

						case SEARCH_DEC:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchDecreased8();
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchDecreased16();
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchDecreased32();
									break;
							}
							break;

						case SEARCH_DIFFERENT:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchDifferent8();
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchDifferent16();
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchDifferent32();
									break;
							}
							break;

						case SEARCH_NOCHANGE:
							switch (current_searchtype) {
								case SEARCHTYPE_8BIT:
									CheatSearchNoChange8();
									break;

								case SEARCHTYPE_16BIT:
									CheatSearchNoChange16();
									break;

								case SEARCHTYPE_32BIT:
									CheatSearchNoChange32();
									break;
							}
							break;

						default:
							assert(FALSE); // not possible
							break;
					}

					UpdateCheatSearchDlg(hW);
					break;

				case IDC_NEWSEARCH:
					FreeCheatSearchMem();
					FreeCheatSearchResults();

					current_search = SEARCH_EQUALVAL;
					current_searchtype = SEARCHTYPE_8BIT;
					current_searchbase = SEARCHBASE_DEC;
					current_valuefrom = 0;
					current_valueto = 0;

					UpdateCheatSearchDlg(hW);
					EnableWindow(GetDlgItem(hW, IDC_VALUEFROM), TRUE);
					break;

				case IDC_SEARCHFOR:
					EnableWindow(GetDlgItem(hW, IDC_VALUEFROM), TRUE);

					if (SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_GETCURSEL, 0, 0) == SEARCH_RANGE) {
						ShowWindow(GetDlgItem(hW, IDC_LABEL_TO), SW_SHOW);
						ShowWindow(GetDlgItem(hW, IDC_VALUETO), SW_SHOW);
					}
					else {
						ShowWindow(GetDlgItem(hW, IDC_LABEL_TO), SW_HIDE);
						ShowWindow(GetDlgItem(hW, IDC_VALUETO), SW_HIDE);

						if (SendMessage(GetDlgItem(hW, IDC_SEARCHFOR), CB_GETCURSEL, 0, 0) >= SEARCH_INC) {
							EnableWindow(GetDlgItem(hW, IDC_VALUEFROM), FALSE);
						}
					}
					break;

				case IDC_DATABASE:
					if (SendMessage(GetDlgItem(hW, IDC_DATABASE), CB_GETCURSEL, 0, 0) == SEARCHBASE_DEC) {
						if (current_searchbase == SEARCHBASE_HEX) {
							GetWindowText(GetDlgItem(hW, IDC_VALUEFROM), (LPTSTR)buf, 255);
							sscanf(buf, "%x", &i);
							sprintf(buf, "%u", i);
							SetWindowText(GetDlgItem(hW, IDC_VALUEFROM), (LPCTSTR)buf);

							GetWindowText(GetDlgItem(hW, IDC_VALUETO), (LPTSTR)buf, 255);
							sscanf(buf, "%x", &i);
							sprintf(buf, "%u", i);
							SetWindowText(GetDlgItem(hW, IDC_VALUETO), (LPCTSTR)buf);
						}
					}
					else if (current_searchbase == SEARCHBASE_DEC){
						GetWindowText(GetDlgItem(hW, IDC_VALUEFROM), (LPTSTR)buf, 255);
						sscanf(buf, "%u", &i);
						sprintf(buf, "%X", i);
						SetWindowText(GetDlgItem(hW, IDC_VALUEFROM), (LPCTSTR)buf);

						GetWindowText(GetDlgItem(hW, IDC_VALUETO), (LPTSTR)buf, 255);
						sscanf(buf, "%u", &i);
						sprintf(buf, "%X", i);
						SetWindowText(GetDlgItem(hW, IDC_VALUETO), (LPCTSTR)buf);
					}
					current_searchbase = SendMessage(GetDlgItem(hW, IDC_DATABASE), CB_GETCURSEL, 0, 0);
					break;

				case IDC_RESLIST:
					switch (HIWORD(wParam)) {
						case LBN_SELCHANGE:
							Button_Enable(GetDlgItem(hW, IDC_FREEZE), TRUE);
							Button_Enable(GetDlgItem(hW, IDC_MODIFY), TRUE);
							Button_Enable(GetDlgItem(hW, IDC_COPY), TRUE);
							break;

						case LBN_SELCANCEL:
							Button_Enable(GetDlgItem(hW, IDC_FREEZE), FALSE);
							Button_Enable(GetDlgItem(hW, IDC_MODIFY), FALSE);
							Button_Enable(GetDlgItem(hW, IDC_COPY), FALSE);
							break;
					}
					break;
			}
			break;

		case WM_CLOSE:
			EndDialog(hW, FALSE);
			return TRUE;
	}

	return FALSE;
}

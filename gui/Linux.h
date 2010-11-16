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

#ifndef __LINUX_H__
#define __LINUX_H__

#include "config.h"

#include "../libpcsxcore/psxcommon.h"
#include <gtk/gtk.h>

#include "Cheat.h"

#define DEFAULT_MEM_CARD_1 "/.pcsx/memcards/card1.mcd"
#define DEFAULT_MEM_CARD_2 "/.pcsx/memcards/card2.mcd"
#define MEMCARD_DIR "/.pcsx/memcards/"
#define PLUGINS_DIR "/.pcsx/plugins/"
#define PLUGINS_CFG_DIR "/.pcsx/plugins/cfg/"
#define PCSX_DOT_DIR "/.pcsx/"
#define BIOS_DIR "/.pcsx/bios/"
#define STATES_DIR "/.pcsx/sstates/"
#define CHEATS_DIR "/.pcsx/cheats/"
#define PATCHES_DIR "/.pcsx/patches/"

extern gboolean UseGui;
extern int StatesC;
char cfgfile[MAXPATHLEN];	/* ADB Comment this out - make a local var, or at least use gchar funcs */
char cfgfile_basename[MAXPATHLEN];	/* ADB Comment this out - make a local var, or at least use gchar funcs */

int LoadConfig();
void SaveConfig();

void StartGui();

void PADhandleKey(int key);

void UpdateMenuSlots();

gchar* get_state_filename(int i);

void state_save(gchar *state_filename);
void state_load(gchar *state_filename);

int match(const char* string, char* pattern);
int plugins_configured();

void UpdatePluginsBIOS();

void SysErrorMessage(gchar *primary, gchar *secondary);
void SysInfoMessage(gchar *primary, gchar *secondary);

#endif /* __LINUX_H__ */

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

extern char cfgfile_basename[MAXPATHLEN];

int get_state_filename(char *buf, int size, int i);

extern unsigned long gpuDisp;
extern int ready_to_go;

#endif /* __LINUX_H__ */

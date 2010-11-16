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

#ifndef CONFDLG_H
#define CONFDLG_H

// Helper Functions
void UpdatePluginsBIOS();

// Functions Callbacks
void OnConf_Graphics();
void OnConf_Sound();
void OnConf_CdRom();
void OnConf_Pad();
void OnConf_Cpu();
void OnConf_Net();

void ConfigurePlugins();

typedef struct {
	GtkWidget *Combo;
	GList *glist;
	char plist[255][255];	/* TODO Comment this out */
	int plugins;			/* TODO Comment this out and replace with glist count */
} PluginConf;

extern PluginConf GpuConfS;
extern PluginConf SpuConfS;
extern PluginConf CdrConfS;
extern PluginConf Pad1ConfS;
extern PluginConf Pad2ConfS;
extern PluginConf NetConfS;
extern PluginConf BiosConfS;

#endif

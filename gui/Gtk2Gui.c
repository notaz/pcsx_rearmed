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
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <signal.h>
#include <sys/time.h>
#include <regex.h>

#include "Linux.h"

#include "../libpcsxcore/plugins.h"
#include "../libpcsxcore/cheat.h"

#include "MemcardDlg.h"
#include "ConfDlg.h"
#include "DebugMemory.h"
#include "AboutDlg.h"

// Functions Callbacks
void OnFile_RunCd();
void OnFile_RunBios();
void OnFile_RunExe();
void OnFile_RunImage();
void OnEmu_Run();
void OnEmu_Reset();
void OnEmu_SwitchImage();
void OnHelp_Help();
void OnHelp_About();
void OnDestroy();
void OnFile_Exit();

void on_states_load(GtkWidget *widget, gpointer user_data);
void on_states_load_other();
void on_states_save(GtkWidget *widget, gpointer user_data);
void on_states_save_other();

GtkWidget *Window = NULL;

int destroy = 0;

#define MAX_SLOTS 5

/* TODO - If MAX_SLOTS changes, need to find a way to automatically set all positions */
int Slots[MAX_SLOTS] = { -1, -1, -1, -1, -1 };

void ResetMenuSlots(GladeXML *xml) {
	GtkWidget *widget;
	gchar *str;
	int i;

	if (CdromId[0] == '\0') {
		// disable state saving/loading if no CD is loaded
		for (i = 0; i < MAX_SLOTS; i++) {
			str = g_strdup_printf("GtkMenuItem_SaveSlot%d", i+1);
			widget = glade_xml_get_widget(xml, str);
			g_free(str);

			gtk_widget_set_sensitive(widget, FALSE);

			str = g_strdup_printf("GtkMenuItem_LoadSlot%d", i+1);
			widget = glade_xml_get_widget(xml, str);
			g_free(str);

			gtk_widget_set_sensitive(widget, FALSE);
		}

		// also disable certain menu/toolbar items
		widget = glade_xml_get_widget(xml, "other1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "other2");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "run1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "reset1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "search1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "SwitchImage");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "memorydump1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "toolbutton_run");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "toolbutton_switchimage");
		gtk_widget_set_sensitive(widget, FALSE);

		widget = glade_xml_get_widget(xml, "statusbar");
		gtk_statusbar_pop(GTK_STATUSBAR(widget), 1);
		gtk_statusbar_push(GTK_STATUSBAR(widget), 1, _("Ready"));
	}
	else {
		for (i = 0; i < MAX_SLOTS; i++) {
			str = g_strdup_printf("GtkMenuItem_LoadSlot%d", i+1);
			widget = glade_xml_get_widget (xml, str);
			g_free (str);

			if (Slots[i] == -1) 
				gtk_widget_set_sensitive(widget, FALSE);
			else
				gtk_widget_set_sensitive(widget, TRUE);
		}

		widget = glade_xml_get_widget(xml, "plugins_bios");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "graphics1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "sound1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "cdrom1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "pad1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "net1");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "SwitchImage");
		gtk_widget_set_sensitive(widget, UsingIso());
		widget = glade_xml_get_widget(xml, "toolbutton_switchimage");
		gtk_widget_set_sensitive(widget, UsingIso());
		widget = glade_xml_get_widget(xml, "toolbutton_graphics");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "toolbutton_sound");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "toolbutton_cdrom");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = glade_xml_get_widget(xml, "toolbutton_controllers");
		gtk_widget_set_sensitive(widget, FALSE);

		widget = glade_xml_get_widget(xml, "statusbar");
		gtk_statusbar_pop(GTK_STATUSBAR(widget), 1);
		gtk_statusbar_push(GTK_STATUSBAR(widget), 1, _("Emulation Paused."));
	}
}

int match(const char *string, char *pattern) {
	int    status;
	regex_t    re;

	if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
		return 0;
	}
	status = regexec(&re, string, (size_t) 0, NULL, 0);
	regfree(&re);
	if (status != 0) {
		return 0;
	}

	return 1;
}

gchar* get_state_filename(int i) {
	gchar *state_filename;
	char SStateFile[64];
	char trimlabel[33];
	int j;

	strncpy(trimlabel, CdromLabel, 32);
	trimlabel[32] = 0;
	for (j = 31; j >= 0; j--)
		if (trimlabel[j] == ' ')
			trimlabel[j] = 0;
		else
			continue;

	sprintf(SStateFile, "%.32s-%.9s.%3.3d", trimlabel, CdromId, i);
	state_filename = g_build_filename (getenv("HOME"), STATES_DIR, SStateFile, NULL);

	return state_filename;
}

void UpdateMenuSlots() {
	gchar *str;
	int i;

	for (i = 0; i < MAX_SLOTS; i++) {
		str = get_state_filename (i);
		Slots[i] = CheckState(str);
		g_free (str);
	}
}

void StartGui() {
	GladeXML *xml;
	GtkWidget *widget;

	/* If a plugin fails, the Window is not NULL, but is not initialised,
	   so the following causes a segfault
	if (Window != NULL) {
		gtk_window_present (GTK_WINDOW (Window));
		return;
	}*/

	xml = glade_xml_new(PACKAGE_DATA_DIR "pcsx.glade2", "MainWindow", NULL);

	if (!xml) {
		g_warning("We could not load the interface!");
		return;
	}

	Window = glade_xml_get_widget(xml, "MainWindow");
	gtk_window_set_title(GTK_WINDOW(Window), "PCSX");
	gtk_window_set_icon_from_file(GTK_WINDOW(Window), PIXMAPDIR "pcsx-icon.png", NULL);
	gtk_window_set_default_icon_from_file(PIXMAPDIR "pcsx-icon.png", NULL);
	ResetMenuSlots(xml);

	// Set up callbacks
	g_signal_connect_data(GTK_OBJECT(Window), "delete-event",
			GTK_SIGNAL_FUNC(OnDestroy), xml, (GClosureNotify)g_object_unref, G_CONNECT_AFTER);

	// File menu
	widget = glade_xml_get_widget(xml, "RunCd");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnFile_RunCd), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "RunBios");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnFile_RunBios), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "RunExe");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnFile_RunExe), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "RunImage");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnFile_RunImage), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "exit2");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnFile_Exit), NULL, NULL, G_CONNECT_AFTER);

	// States
	widget = glade_xml_get_widget(xml, "GtkMenuItem_LoadSlot1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_load), (gpointer) 0, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_LoadSlot2");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_load), (gpointer) 1, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_LoadSlot3");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_load), (gpointer) 2, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_LoadSlot4");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_load), (gpointer) 3, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_LoadSlot5");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_load), (gpointer) 4, NULL, G_CONNECT_AFTER);	
	widget = glade_xml_get_widget(xml, "other1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_load_other), NULL, NULL, G_CONNECT_AFTER);			

	widget = glade_xml_get_widget(xml, "GtkMenuItem_SaveSlot1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_save), (gpointer) 0, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_SaveSlot2");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_save), (gpointer) 1, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_SaveSlot3");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_save), (gpointer) 2, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_SaveSlot4");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_save), (gpointer) 3, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "GtkMenuItem_SaveSlot5");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_save), (gpointer) 4, NULL, G_CONNECT_AFTER);	
	widget = glade_xml_get_widget(xml, "other2");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(on_states_save_other), NULL, NULL, G_CONNECT_AFTER);

	// Emulation menu
	widget = glade_xml_get_widget(xml, "run1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnEmu_Run), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "reset1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnEmu_Reset), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "SwitchImage");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnEmu_SwitchImage), NULL, NULL, G_CONNECT_AFTER);

	// Configuration menu
	widget = glade_xml_get_widget(xml, "plugins_bios");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(ConfigurePlugins), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "graphics1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnConf_Graphics), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "sound1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnConf_Sound), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "cdrom1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnConf_CdRom), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "pad1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnConf_Pad), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "cpu1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnConf_Cpu), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "memory_cards1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnConf_Mcds), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "net1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnConf_Net), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "memorydump1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(RunDebugMemoryDialog), NULL, NULL, G_CONNECT_AFTER);

	// Cheat menu
	widget = glade_xml_get_widget(xml, "browse1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(RunCheatListDialog), NULL, NULL, G_CONNECT_AFTER);
	widget = glade_xml_get_widget(xml, "search1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(RunCheatSearchDialog), NULL, NULL, G_CONNECT_AFTER);

	// Help menu
	widget = glade_xml_get_widget(xml, "about_pcsx1");
	g_signal_connect_data(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(OnHelp_About), NULL, NULL, G_CONNECT_AFTER);

	// Toolbar
	widget = glade_xml_get_widget(xml, "toolbutton_runcd");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnFile_RunCd), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_runimage");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnFile_RunImage), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_run");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnEmu_Run), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_switchimage");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnEmu_SwitchImage), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_memcards");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConf_Mcds), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_graphics");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConf_Graphics), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_sound");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConf_Sound), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_cdrom");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConf_CdRom), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "toolbutton_controllers");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConf_Pad), NULL, NULL, G_CONNECT_AFTER);

	gtk_main();
}

void OnDestroy() {
	if (!destroy) OnFile_Exit();
}

void destroy_main_window () {
	destroy = 1;
	gtk_widget_destroy(Window);
	Window = NULL;
	destroy = 0;
	gtk_main_quit();
	while (gtk_events_pending()) gtk_main_iteration();
}

void OnFile_RunExe() {
	GtkWidget *file_chooser;

	if (plugins_configured() == FALSE) {
		ConfigurePlugins();
	} else {
		file_chooser = gtk_file_chooser_dialog_new(_("Select PSX EXE File"),
			NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

		// Add file filters
		GtkFileFilter *exefilter = gtk_file_filter_new ();
		gtk_file_filter_add_pattern (exefilter, "*.exe");
		gtk_file_filter_add_pattern (exefilter, "*.psx");
		gtk_file_filter_add_pattern (exefilter, "*.cpe");
		gtk_file_filter_add_pattern (exefilter, "*.EXE");
		gtk_file_filter_add_pattern (exefilter, "*.PSX");
		gtk_file_filter_add_pattern (exefilter, "*.CPE");
		gtk_file_filter_set_name (exefilter, _("PlayStation Executable Files"));
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), exefilter);
		GtkFileFilter *allfilter = gtk_file_filter_new ();
		gtk_file_filter_add_pattern (allfilter, "*");
		gtk_file_filter_set_name (allfilter, _("All Files"));
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), allfilter);

		// Set this to the config object and retain it - maybe LastUsedDir
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), getenv("HOME"));

		if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
			gchar *file;

			/* TODO Need to validate the file */

			file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));

			gtk_widget_destroy (file_chooser);
			destroy_main_window();

			SetIsoFile(NULL);
			LoadPlugins();
			NetOpened = FALSE;

			if (OpenPlugins() == -1) {
				g_free(file);
				SysRunGui();
			} else {
				SysReset();

				if (Load(file) == 0) {
					g_free(file);
					psxCpu->Execute();
				} else {
					g_free(file);
					ClosePlugins();
					SysErrorMessage(_("Not a valid PSX file"), _("The file does not appear to be a valid Playstation executable"));
					SysRunGui();
				}
			}
		} else
			gtk_widget_destroy(file_chooser);
	}
}

void OnFile_RunCd() {
	if (plugins_configured() == FALSE) { 
		ConfigurePlugins();
		return;
	}

	destroy_main_window();

	SetIsoFile(NULL);
	LoadPlugins();
	NetOpened = FALSE;

	if (OpenPlugins() == -1) {
		SysRunGui();
		return;
	}

	SysReset();

	if (CheckCdrom() == -1) {
		/* Only check the CD if we are starting the console with a CD */
		ClosePlugins();
		SysErrorMessage (_("CD ROM failed"), _("The CD does not appear to be a valid Playstation CD"));
		SysRunGui();
		return;
	}

	// Read main executable directly from CDRom and start it
	if (LoadCdrom() == -1) {
		ClosePlugins();
		SysErrorMessage(_("Could not load CD-ROM!"), _("The CD-ROM could not be loaded"));
		SysRunGui();
	}

	psxCpu->Execute();
}

void OnFile_RunBios() {
	if (plugins_configured() == FALSE) { 
		ConfigurePlugins();
		return;
	}

	if (strcmp(Config.Bios, "HLE") == 0) {
		SysErrorMessage (_("Could not run BIOS"), _("Running BIOS is not supported with Internal HLE BIOS."));
		return;
	}

	destroy_main_window();

	SetIsoFile(NULL);
	LoadPlugins();
	NetOpened = FALSE;

	if (OpenPlugins() == -1) {
		SysRunGui();
		return;
	}

	SysReset();

	CdromId[0] = '\0';
	CdromLabel[0] = '\0';

	psxCpu->Execute();
}

static gchar *Open_Iso_Proc() {
	GtkWidget *chooser;
	gchar *filename;
	GtkFileFilter *psxfilter, *allfilter;
	static char current_folder[MAXPATHLEN] = "";

	chooser = gtk_file_chooser_dialog_new (_("Open PSX Disc Image File"),
		NULL, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);

	if (current_folder[0] == '\0') {
		strcpy(current_folder, getenv("HOME"));
	}

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (chooser), current_folder);

	psxfilter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(psxfilter, "*.bin");
	gtk_file_filter_add_pattern(psxfilter, "*.img");
	gtk_file_filter_add_pattern(psxfilter, "*.mdf");
	gtk_file_filter_add_pattern(psxfilter, "*.iso");
	gtk_file_filter_add_pattern(psxfilter, "*.BIN");
	gtk_file_filter_add_pattern(psxfilter, "*.IMG");
	gtk_file_filter_add_pattern(psxfilter, "*.MDF");
	gtk_file_filter_add_pattern(psxfilter, "*.ISO");
	gtk_file_filter_set_name(psxfilter, _("PSX Image Files (*.bin, *.img, *.mdf, *.iso)"));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER (chooser), psxfilter);

	allfilter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(allfilter, "*");
	gtk_file_filter_set_name(allfilter, _("All Files"));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER (chooser), allfilter);

	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_OK) {
		gchar *path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(chooser));
		strcpy(current_folder, path);
		g_free(path);
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (chooser));
		gtk_widget_destroy(GTK_WIDGET(chooser));
		while (gtk_events_pending()) gtk_main_iteration();
		return filename;
	} else {
		gtk_widget_destroy (GTK_WIDGET(chooser));
		while (gtk_events_pending()) gtk_main_iteration();
		return NULL;
	}
}

void OnFile_RunImage() {
	gchar *filename;

	if (plugins_configured() == FALSE) { 
		ConfigurePlugins();
		return;
	}

	filename = Open_Iso_Proc();
	if (filename == NULL) {
		return;
	}

	destroy_main_window();

	SetIsoFile(filename);
	g_free(filename);

	LoadPlugins();
	NetOpened = FALSE;

	if (OpenPlugins() == -1) {
		SysRunGui();
		return;
	}

	SysReset();

	if (CheckCdrom() == -1) {
		// Only check the CD if we are starting the console with a CD
		ClosePlugins();
		SysErrorMessage (_("CD ROM failed"), _("The CD does not appear to be a valid Playstation CD"));
		SysRunGui();
		return;
	}

	// Read main executable directly from CDRom and start it
	if (LoadCdrom() == -1) {
		ClosePlugins();
		SysErrorMessage(_("Could not load CD-ROM!"), _("The CD-ROM could not be loaded"));
		SysRunGui();
	}

	psxCpu->Execute();
}

void OnEmu_Run() {
	if (plugins_configured() == FALSE) { 
		ConfigurePlugins();
		return;
	}

	destroy_main_window();

	if (OpenPlugins() == -1) {
		SysRunGui();
		return;
	}

	CheatSearchBackupMemory();
	psxCpu->Execute();
}

void OnEmu_Reset() {
	if (plugins_configured() == FALSE) { 
		ConfigurePlugins();
		return;
	}

	destroy_main_window();

	if (OpenPlugins() == -1) {
		SysRunGui();
		return;
	}

	SysReset();

	if (CheckCdrom() != -1) {
		LoadCdrom();
	}

	psxCpu->Execute();
}

void OnEmu_SwitchImage() {
	gchar *filename;

	if (plugins_configured() == FALSE) { 
		ConfigurePlugins();
		return;
	}

	filename = Open_Iso_Proc();
	if (filename == NULL) {
		return;
	}

	destroy_main_window();

	SetIsoFile(filename);
	g_free(filename);

	if (OpenPlugins() == -1) {
		SysRunGui();
		return;
	}

	SetCdOpenCaseTime(time(NULL) + 2);

	CheatSearchBackupMemory();
	psxCpu->Execute();
}

void OnFile_Exit() {
	DIR *dir;
	struct dirent *ent;
	void *Handle;
	gchar *plugin = NULL;
	gchar *dotdir;

	dotdir = g_build_filename(getenv("HOME"), PLUGINS_DIR, NULL);

	// with this the problem with plugins that are linked with the pthread
	// library is solved

	dir = opendir(dotdir);
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			plugin = g_build_filename(dotdir, ent->d_name, NULL);

			if (strstr(plugin, ".so") == NULL && strstr(plugin, ".dylib") == NULL)
				continue;
			Handle = dlopen(plugin, RTLD_NOW);
			if (Handle == NULL)
				continue;

			g_free(plugin);
		}
	}
	g_free(dotdir);

	bind_textdomain_codeset(PACKAGE_NAME, "");
	if (UseGui)
		gtk_main_quit();
	SysClose();
	if (UseGui)
		gtk_exit (0);
	else
		exit(0);
}

void state_load(gchar *state_filename) {
	int ret;
	char Text[MAXPATHLEN + 20];
	FILE *fp;

	// check if the state file actually exists
	fp = fopen(state_filename, "rb");
	if (fp == NULL) {
		// file does not exist
		return;
	}

	fclose(fp);

	// If the window exists, then we are loading the state from within
	// within the PCSX GUI. We need to initialise the plugins first
	if (Window) {
		destroy_main_window();

		if (OpenPlugins() == -1) {
			SysRunGui();
			return;
		}
	}

	ret = CheckState(state_filename);

	if (ret == 0) {
		SysReset();
		ret = LoadState(state_filename);
	}

	if (ret == 0) {
		// Check the CD-ROM is valid
		if (CheckCdrom() == -1) {
			ClosePlugins();
			SysRunGui();
			return;
		}

		sprintf(Text, _("Loaded state %s."), state_filename);
		GPU_displayText(Text);
	} else {
		sprintf(Text, _("Error loading state %s!"), state_filename);
		GPU_displayText(Text);
	}
}

void state_save(gchar *state_filename) {
	char Text[MAXPATHLEN + 20];

	GPU_updateLace();

	if (SaveState(state_filename) == 0)
		sprintf(Text, _("Saved state %s."), state_filename);
	else
		sprintf(Text, _("Error saving state %s!"), state_filename);

	GPU_displayText(Text);
}

void on_states_load (GtkWidget *widget, gpointer user_data) {
	gchar *state_filename;
	gint state = (int)user_data;

	state_filename = get_state_filename(state);

	state_load(state_filename);

	g_free(state_filename);

	psxCpu->Execute();
}

void on_states_save (GtkWidget *widget, gpointer user_data) {
	gchar *state_filename;
	gint state = (int)user_data;

	state_filename = get_state_filename(state);

	state_save(state_filename);

	g_free(state_filename);
}

void on_states_load_other() {
	GtkWidget *file_chooser;
	gchar *SStateFile;

	SStateFile = g_strconcat(getenv("HOME"), STATES_DIR, NULL);

	file_chooser = gtk_file_chooser_dialog_new(_("Select State File"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (file_chooser), SStateFile);
	g_free(SStateFile);

	if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
		gtk_widget_destroy(file_chooser);

		state_load(filename);

		g_free(filename);

		psxCpu->Execute();
	} else
		gtk_widget_destroy(file_chooser);
} 

void on_states_save_other() {
	GtkWidget *file_chooser;
	gchar *SStateFile;

	SStateFile = g_strconcat (getenv("HOME"), STATES_DIR, NULL);

	file_chooser = gtk_file_chooser_dialog_new(_("Select State File"),
			NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_OK,
			NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), SStateFile);
	g_free(SStateFile);

	if (gtk_dialog_run (GTK_DIALOG(file_chooser)) == GTK_RESPONSE_OK) {
		gchar *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (file_chooser));
		gtk_widget_destroy(file_chooser);

		state_save(filename);

		g_free(filename);
	}
	else
		gtk_widget_destroy(file_chooser);
} 

void OnHelp_About(GtkWidget *widget, gpointer user_data) {
	RunAboutDialog();
}

void SysMessage(const char *fmt, ...) {
	GtkWidget *Txt, *MsgDlg;
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg) - 1] == '\n')
		msg[strlen(msg) - 1] = 0;

	if (!UseGui) {
		fprintf(stderr, "%s\n", msg);
		return;
	}

	MsgDlg = gtk_dialog_new_with_buttons(_("Notice"), NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);

	gtk_window_set_position (GTK_WINDOW(MsgDlg), GTK_WIN_POS_CENTER);

	Txt = gtk_label_new (msg);
	gtk_label_set_line_wrap(GTK_LABEL(Txt), TRUE);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(MsgDlg)->vbox), Txt);

	gtk_widget_show (Txt);
	gtk_widget_show_all (MsgDlg);
	gtk_dialog_run (GTK_DIALOG(MsgDlg));
	gtk_widget_destroy (MsgDlg);
}

void SysErrorMessage(gchar *primary, gchar *secondary) {
	GtkWidget *message_dialog;	
	if (!UseGui)
		printf ("%s - %s\n", primary, secondary);
	else {
		message_dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				primary,
				NULL);
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message_dialog),
				secondary);

		gtk_dialog_run(GTK_DIALOG(message_dialog));
		gtk_widget_destroy(message_dialog);
	}
}

void SysInfoMessage(gchar *primary, gchar *secondary) {
	GtkWidget *message_dialog;	
	if (!UseGui)
		printf ("%s - %s\n", primary, secondary);
	else {
		message_dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				primary,
				NULL);
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message_dialog),
				secondary);

		gtk_dialog_run(GTK_DIALOG(message_dialog));
		gtk_widget_destroy(message_dialog);
	}
}

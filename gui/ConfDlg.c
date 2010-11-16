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
#include "ConfDlg.h"

#include "../libpcsxcore/plugins.h"

static void OnBiosPath_Changed(GtkWidget *wdg, gpointer data);
static void OnConf_Clicked(GtkDialog *dialog, gint arg1, gpointer user_data);
static void OnPluginPath_Changed(GtkWidget *wdg, gpointer data);
static void OnConfConf_Pad1About(GtkWidget *widget, gpointer user_data);
static void OnConfConf_Pad2About(GtkWidget *widget, gpointer user_data);
static void OnConfConf_Pad1Conf(GtkWidget *widget, gpointer user_data);
static void OnConfConf_Pad2Conf(GtkWidget *widget, gpointer user_data);
static void OnNet_Conf(GtkWidget *widget, gpointer user_data);
static void OnNet_About(GtkWidget *widget, gpointer user_data);
static void on_configure_plugin(GtkWidget *widget, gpointer user_data);
static void on_about_plugin(GtkWidget *widget, gpointer user_data);
static void UpdatePluginsBIOS_UpdateGUI(GladeXML *xml);
static void FindNetPlugin(GladeXML *xml);

PSEgetLibType		PSE_getLibType = NULL;
PSEgetLibVersion	PSE_getLibVersion = NULL;
PSEgetLibName		PSE_getLibName = NULL;

GtkWidget *ConfDlg = NULL;
GtkWidget *NetDlg = NULL;
GtkWidget *controlwidget = NULL;

PluginConf GpuConfS;
PluginConf SpuConfS;
PluginConf CdrConfS;
PluginConf Pad1ConfS;
PluginConf Pad2ConfS;
PluginConf NetConfS;
PluginConf BiosConfS;

#define FindComboText(combo, list, conf) \
	if (strlen(conf) > 0) { \
		int i; \
		for (i = 2; i < 255; i += 2) { \
			if (!strcmp(conf, list[i - 2])) { \
				gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i / 2 - 1); \
				break; \
			} \
		} \
	}

#define GetComboText(combo, list, conf) \
	{ \
		int row; \
		row = gtk_combo_box_get_active(GTK_COMBO_BOX(combo)); \
		strcpy(conf, (char *)list[row * 2]); \
	}

void ConfigurePlugins() {
	if (!UseGui) {
		/* How do we get here if we're not running the GUI? */
		/* Ryan: we're going to imagine that someday, there will be a way
		 * to configure plugins from the commandline */
		printf("ERROR: Plugins cannot be configured without the GUI.");
		return;
	}

	GladeXML *xml;
	GtkWidget *widget;

	gchar *path;

	UpdatePluginsBIOS();

	xml = glade_xml_new(PACKAGE_DATA_DIR "pcsx.glade2", "ConfDlg", NULL);

	if (!xml) {
		g_warning(_("Error: Glade interface could not be loaded!"));
		return;
	}

	UpdatePluginsBIOS_UpdateGUI(xml);

	ConfDlg = glade_xml_get_widget(xml, "ConfDlg");

	gtk_window_set_title(GTK_WINDOW(ConfDlg), _("Configure PCSX"));

	/* Set the paths in the file choosers to be based on the saved configurations */
	widget = glade_xml_get_widget(xml, "GtkFileChooser_Bios");
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget), Config.BiosDir);

	widget = glade_xml_get_widget(xml, "GtkFileChooser_Plugin");
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget), Config.PluginsDir);

	if (strlen(Config.PluginsDir) == 0) {
		if((path = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (widget))) != NULL) {
			strcpy(Config.PluginsDir, path);
			g_free(path);
		}
	}

	widget = glade_xml_get_widget(xml, "btn_ConfGpu");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(on_configure_plugin), (gpointer) PSE_LT_GPU, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_ConfSpu");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(on_configure_plugin), (gpointer) PSE_LT_SPU, NULL, G_CONNECT_AFTER);

	/* ADB TODO Does pad 1 and 2 need to be different? */
	widget = glade_xml_get_widget(xml, "btn_ConfPad1");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
						  GTK_SIGNAL_FUNC(OnConfConf_Pad1Conf), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_ConfPad2");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConfConf_Pad2Conf), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_ConfCdr");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(on_configure_plugin), (gpointer) PSE_LT_CDR, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_AboutGpu");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(on_about_plugin), (gpointer) PSE_LT_GPU, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_AboutSpu");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(on_about_plugin), (gpointer) PSE_LT_SPU, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_AboutPad1");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConfConf_Pad1About), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_AboutPad2");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnConfConf_Pad2About), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_AboutCdr");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(on_about_plugin), (gpointer) PSE_LT_CDR, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "GtkFileChooser_Bios");
	g_signal_connect_data(GTK_OBJECT(widget), "current_folder_changed",
			GTK_SIGNAL_FUNC(OnBiosPath_Changed), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "GtkFileChooser_Plugin");
	g_signal_connect_data(GTK_OBJECT(widget), "current_folder_changed",
			GTK_SIGNAL_FUNC(OnPluginPath_Changed), xml, NULL, G_CONNECT_AFTER);

	g_signal_connect_data(GTK_OBJECT(ConfDlg), "response",
			GTK_SIGNAL_FUNC(OnConf_Clicked), xml, (GClosureNotify)g_object_unref, G_CONNECT_AFTER);
}

void OnNet_Clicked(GtkDialog *dialog, gint arg1, gpointer user_data) {
	GetComboText(NetConfS.Combo, NetConfS.plist, Config.Net);
	SaveConfig();
	gtk_widget_destroy(GTK_WIDGET(dialog));
	NetDlg = NULL;
}

void OnConf_Net() {
	GladeXML *xml;
	GtkWidget *widget;

	if (NetDlg != NULL) {
		gtk_window_present (GTK_WINDOW (NetDlg));
		return;
	}

	xml = glade_xml_new(PACKAGE_DATA_DIR "pcsx.glade2", "NetDlg", NULL);

	if (!xml) {
		g_warning(_("Error: Glade interface could not be loaded!"));
		return;
	}

	NetDlg = glade_xml_get_widget(xml, "NetDlg");

	FindNetPlugin(xml);

	/* Setup a handler for when Close or Cancel is clicked */
	g_signal_connect_data(GTK_OBJECT(NetDlg), "response",
			GTK_SIGNAL_FUNC(OnNet_Clicked), xml, (GClosureNotify)g_object_unref, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_ConfNet");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnNet_Conf), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_AboutNet");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnNet_About), xml, NULL, G_CONNECT_AFTER);
}

void OnConf_Graphics() {
	void *drv;
	GPUconfigure conf;
	char Plugin[MAXPATHLEN];

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Gpu);
	drv = SysLoadLibrary(Plugin);
	if (drv == NULL) { printf("Error with file %s\n", Plugin); return; }

	while (gtk_events_pending()) gtk_main_iteration();

	conf = (GPUconfigure)SysLoadSym(drv, "GPUconfigure");
	if (conf != NULL) {
		conf();
	}
	else
		SysInfoMessage (_("No configuration required"), _("This plugin doesn't need to be configured."));

	SysCloseLibrary(drv);
}

void OnConf_Sound() {
	void *drv;
	SPUconfigure conf;
	char Plugin[MAXPATHLEN];

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Spu);
	drv = SysLoadLibrary(Plugin);
	if (drv == NULL) { printf("Error with file %s\n", Plugin); return; }

	while (gtk_events_pending()) gtk_main_iteration();

	conf = (GPUconfigure)SysLoadSym(drv, "SPUconfigure");
	if (conf != NULL) {
		conf();
	}
	else
		SysInfoMessage (_("No configuration required"), _("This plugin doesn't need to be configured."));

	SysCloseLibrary(drv);
}

void OnConf_CdRom() {
	void *drv;
	CDRconfigure conf;
	char Plugin[MAXPATHLEN];

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Cdr);
	drv = SysLoadLibrary(Plugin);
	if (drv == NULL) { printf("Error with file %s\n", Plugin); return; }

	while (gtk_events_pending()) gtk_main_iteration();

	conf = (GPUconfigure)SysLoadSym(drv, "CDRconfigure");
	if (conf != NULL) {
		conf();
	}
	else
		SysInfoMessage (_("No configuration required"), _("This plugin doesn't need to be configured."));

	SysCloseLibrary(drv);
}

void OnConf_Pad() {
	void *drv;
	PADconfigure conf;
	char Plugin[MAXPATHLEN];

	sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Pad1);
	drv = SysLoadLibrary(Plugin);
	if (drv == NULL) { printf("Error with file %s\n", Plugin); return; }

	while (gtk_events_pending()) gtk_main_iteration();

	conf = (GPUconfigure)SysLoadSym(drv, "PADconfigure");
	if (conf != NULL) {
		conf();
	}
	else
		SysInfoMessage (_("No configuration required"), _("This plugin doesn't need to be configured."));

	SysCloseLibrary(drv);

	if (strcmp(Config.Pad1, Config.Pad2) != 0) {
		sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Pad2);
		drv = SysLoadLibrary(Plugin);
		if (drv == NULL) { printf("Error with file %s\n", Plugin); return; }

		while (gtk_events_pending()) gtk_main_iteration();

		conf = (GPUconfigure)SysLoadSym(drv, "PADconfigure");
		if (conf != NULL) {
			conf();
		}

		SysCloseLibrary(drv);
	}
}

static int all_config_set() {
	int retval;

	if ((strlen(Config.Gpu) != 0) &&
	    (strlen(Config.Spu) != 0) &&
	    (strlen(Config.Cdr) != 0) &&
	    (strlen(Config.Pad1) != 0) &&
	    (strlen(Config.Pad2) != 0))
		retval = TRUE;
	else
		retval = FALSE;

	return retval;
}

/* TODO Check whether configuration is required when we choose the plugin, and set the state of the
    button appropriately. New gtk tooltip API should allow us to put a tooltip explanation for
    disabled widgets */
/* TODO If combo screen hasn't been opened and the user chooses the menu config option, confs.Combo will be null and cause a segfault */
#define ConfPlugin(src, confs, plugin, name, parent)  { \
	void *drv; \
	src conf; \
	gchar *filename; \
 \
	GetComboText(confs.Combo, confs.plist, plugin); \
	filename = g_build_filename (getenv("HOME"), PLUGINS_DIR, plugin, NULL); \
	/*printf("Configuring plugin %s\n", filename);*/ \
	drv = SysLoadLibrary(filename); \
	if (drv == NULL) {printf("Error with file %s\n", filename);return; } \
\
	while (gtk_events_pending()) gtk_main_iteration(); \
	conf = (src) SysLoadSym(drv, name); \
	if (conf) { \
		conf(); \
	} else \
		SysInfoMessage (_("No configuration required"), _("This plugin doesn't need to be configured.")); \
	SysCloseLibrary(drv); \
	g_free (filename); \
}

static void on_configure_plugin(GtkWidget *widget, gpointer user_data) {
	gint plugin_type = (int) user_data;

	while (gtk_events_pending())
		gtk_main_iteration();
	if (all_config_set() == TRUE) {
		switch (plugin_type) {
			case PSE_LT_GPU:
				ConfPlugin(GPUconfigure, GpuConfS, Config.Gpu, "GPUconfigure", ConfDlg);
				break;
			case PSE_LT_SPU:
				ConfPlugin(SPUconfigure, SpuConfS, Config.Spu, "SPUconfigure", ConfDlg);
				break;
			case PSE_LT_CDR:
				ConfPlugin(CDRconfigure, CdrConfS, Config.Cdr, "CDRconfigure", ConfDlg);
				break;
		}
	} else
		ConfigurePlugins();
}

static void on_about_plugin(GtkWidget *widget, gpointer user_data) {
	gint plugin_type = (int) user_data;

	while (gtk_events_pending())
		gtk_main_iteration();
	if (all_config_set() == TRUE) {
		switch (plugin_type) {
			case PSE_LT_GPU:
				ConfPlugin(GPUconfigure, GpuConfS, Config.Gpu, "GPUabout", ConfDlg);
				break;
			case PSE_LT_SPU:
				ConfPlugin(SPUconfigure, SpuConfS, Config.Spu, "SPUabout", ConfDlg);
				break;
			case PSE_LT_CDR:
				ConfPlugin(CDRconfigure, CdrConfS, Config.Cdr, "CDRabout", ConfDlg);
				break;
		}
	} else
		ConfigurePlugins();
}

static void OnConfConf_Pad1About(GtkWidget *widget, gpointer user_data) {
	ConfPlugin(PADabout, Pad1ConfS, Config.Pad1, "PADabout", ConfDlg);
}

static void OnConfConf_Pad2About(GtkWidget *widget, gpointer user_data) {
	ConfPlugin(PADabout, Pad2ConfS, Config.Pad2, "PADabout", ConfDlg);
}

static void OnConfConf_Pad1Conf(GtkWidget *widget, gpointer user_data) {
	ConfPlugin(PADabout, Pad1ConfS, Config.Pad1, "PADconfigure", ConfDlg);
}

static void OnConfConf_Pad2Conf(GtkWidget *widget, gpointer user_data) {
	ConfPlugin(PADabout, Pad2ConfS, Config.Pad2, "PADconfigure", ConfDlg);
}

static void OnNet_Conf(GtkWidget *widget, gpointer user_data) {
	ConfPlugin(NETconfigure, NetConfS, Config.Net, "NETconfigure", NetDlg);
}

static void OnNet_About(GtkWidget *widget, gpointer user_data) {
	ConfPlugin(NETabout, NetConfS, Config.Net, "NETabout", NetDlg);
}

static void OnPluginPath_Changed(GtkWidget *wdg, gpointer data) {
	gchar *path;

	path = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (wdg));
	strcpy(Config.PluginsDir, path);
	UpdatePluginsBIOS();
	UpdatePluginsBIOS_UpdateGUI(data);

	g_free(path);
}

static void OnBiosPath_Changed(GtkWidget *wdg, gpointer data) {
	gchar *foldername;

	foldername = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (wdg));
	strcpy(Config.BiosDir, foldername);

	UpdatePluginsBIOS();
	UpdatePluginsBIOS_UpdateGUI(data);

	g_free(foldername);
}

void OnConf_Clicked(GtkDialog *dialog, gint arg1, gpointer user_data) {
	GetComboText(GpuConfS.Combo, GpuConfS.plist, Config.Gpu);
	GetComboText(SpuConfS.Combo, SpuConfS.plist, Config.Spu);
	GetComboText(CdrConfS.Combo, CdrConfS.plist, Config.Cdr);
	GetComboText(Pad1ConfS.Combo, Pad1ConfS.plist, Config.Pad1);
	GetComboText(Pad2ConfS.Combo, Pad2ConfS.plist, Config.Pad2);
	GetComboText(BiosConfS.Combo, BiosConfS.plist, Config.Bios);

	SaveConfig();

	gtk_widget_destroy(ConfDlg);
	ConfDlg = NULL;
}

#define ComboAddPlugin(type) { \
	type##ConfS.plugins += 2; \
	strcpy(type##ConfS.plist[type##ConfS.plugins - 1], name); \
	strcpy(type##ConfS.plist[type##ConfS.plugins - 2], ent->d_name); \
	type##ConfS.glist = g_list_append(type##ConfS.glist, type##ConfS.plist[type##ConfS.plugins-1]); \
}

void populate_combo_box(GtkWidget *widget, GList *list) {
	GtkListStore *store;
	GtkCellRenderer *renderer;
	store = gtk_list_store_new(1, G_TYPE_STRING);

	// Clear existing data from combo box
	gtk_cell_layout_clear(GTK_CELL_LAYOUT(widget));

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget), renderer, FALSE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(widget), renderer, "text", 0);

	while (list != NULL) {
		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, (char *)list->data, -1);
		list = list->next;
	}

	gtk_combo_box_set_model(GTK_COMBO_BOX(widget), GTK_TREE_MODEL(store));
}

#define ConfCreatePConf(name, type) \
	/* Populate the relevant combo widget with the list of plugins. \
	   If no plugins available, disable the combo and its controls. \
	   Note that the Bios plugin has no About/Conf control. */ \
	type##ConfS.Combo = glade_xml_get_widget(xml, "GtkCombo_" name); \
	if (type##ConfS.glist != NULL) { \
		populate_combo_box (type##ConfS.Combo, type##ConfS.glist); \
		FindComboText(type##ConfS.Combo, type##ConfS.plist, Config.type); \
		gtk_widget_set_sensitive (type##ConfS.Combo, TRUE); \
		if (g_ascii_strcasecmp (name, "Bios") != 0) { \
			controlwidget = glade_xml_get_widget(xml, "btn_Conf" name); \
			gtk_widget_set_sensitive (controlwidget, TRUE); \
			controlwidget = glade_xml_get_widget(xml, "btn_About" name); \
			gtk_widget_set_sensitive (controlwidget, TRUE); \
		} \
	} else { \
		if (g_ascii_strcasecmp (name, "Bios") != 0) { \
			gtk_cell_layout_clear (GTK_CELL_LAYOUT (type##ConfS.Combo)); \
			gtk_widget_set_sensitive (type##ConfS.Combo, FALSE); \
			controlwidget = glade_xml_get_widget(xml, "btn_Conf" name); \
			gtk_widget_set_sensitive (controlwidget, FALSE); \
			controlwidget = glade_xml_get_widget(xml, "btn_About" name); \
			gtk_widget_set_sensitive (controlwidget, FALSE); \
		} \
	}

int plugin_is_available(gchar *plugin) {
	int retval;
	gchar *pluginfile;
	struct stat stbuf;

	pluginfile = g_strconcat(getenv("HOME"), PLUGINS_DIR, plugin, NULL);

	if (stat(pluginfile, &stbuf) == -1)
		retval = FALSE;
	else
		retval = TRUE;

	g_free(pluginfile);

	return retval;
}

int plugins_configured() {
	// make sure there are choices for all of the plugins!!
	if (all_config_set() == FALSE)
		return FALSE;

	// and make sure they can all be accessed
	// if they can't be, wipe the variable and return FALSE
	if (plugin_is_available (Config.Gpu) == FALSE) { Config.Gpu[0] = '\0'; return FALSE; }
	if (plugin_is_available (Config.Spu) == FALSE) { Config.Spu[0] = '\0'; return FALSE; }
	if (plugin_is_available (Config.Cdr) == FALSE) { Config.Cdr[0] = '\0'; return FALSE; }
	if (plugin_is_available (Config.Pad1) == FALSE) { Config.Pad1[0] = '\0'; return FALSE; }
	if (plugin_is_available (Config.Pad2) == FALSE) { Config.Pad2[0] = '\0'; return FALSE; }

	// if everything is happy, return TRUE
	return TRUE;
}

int is_valid_bios_file(gchar *filename) {
	int valid;
	struct stat buf;

	if ((stat(filename, &buf) == -1) || (buf.st_size != (1024*512)))
		valid = FALSE;
	else {
		valid = TRUE;
	}

	return valid;
}

// Add the name of the BIOS file to the drop-down list. This will
// be the filename, not the full path to the file
void add_bios_to_list(gchar *bios_name, gchar *internal_name) {
	BiosConfS.plugins += 2;
	strcpy(BiosConfS.plist[BiosConfS.plugins - 1], bios_name);
	strcpy(BiosConfS.plist[BiosConfS.plugins - 2], internal_name);
	BiosConfS.glist = g_list_append(BiosConfS.glist, BiosConfS.plist[BiosConfS.plugins - 1]);
}

void scan_bios_dir(gchar *dirname) {
	DIR *dir;
	struct dirent *ent;
	gchar *filename;

	dir = opendir(dirname);
	if (dir == NULL) {
		SysMessage(_("Could not open BIOS directory: '%s'\n"), dirname);
		return;
	}

	while ((ent = readdir(dir)) != NULL) {
		filename = g_build_filename(dirname, ent->d_name, NULL);
		if (is_valid_bios_file(filename))
			add_bios_to_list(g_path_get_basename(filename), g_path_get_basename (filename));
		g_free(filename);
	}
	closedir(dir);
}

void UpdatePluginsBIOS() {
	DIR *dir;
	struct dirent *ent;
	void *Handle;
	char name[256];
	gchar *linkname;

	GpuConfS.plugins  = 0; SpuConfS.plugins  = 0; CdrConfS.plugins  = 0;
	Pad1ConfS.plugins = 0; Pad2ConfS.plugins = 0; BiosConfS.plugins = 0;
	GpuConfS.glist  = NULL; SpuConfS.glist  = NULL; CdrConfS.glist  = NULL;
	Pad1ConfS.glist = NULL; Pad2ConfS.glist = NULL; BiosConfS.glist = NULL;
	GpuConfS.plist[0][0]  = '\0'; SpuConfS.plist[0][0]  = '\0'; CdrConfS.plist[0][0]  = '\0';
	Pad1ConfS.plist[0][0] = '\0'; Pad2ConfS.plist[0][0] = '\0'; BiosConfS.plist[0][0] = '\0';

	// Load and get plugin info
	dir = opendir(Config.PluginsDir);
	if (dir == NULL) {
		printf(_("Could not open directory: '%s'\n"), Config.PluginsDir);
		return;
	}
	while ((ent = readdir(dir)) != NULL) {
		long type, v;
		linkname = g_build_filename(Config.PluginsDir, ent->d_name, NULL);

		// only libraries past this point, not config tools
		if (strstr(linkname, ".so") == NULL && strstr(linkname, ".dylib") == NULL)
			continue;

		Handle = dlopen(linkname, RTLD_NOW);
		if (Handle == NULL) {
			printf("%s\n", dlerror());
			g_free(linkname);
			continue;
		}

		PSE_getLibType = (PSEgetLibType)dlsym(Handle, "PSEgetLibType");
		if (PSE_getLibType == NULL) {
			if (strstr(linkname, "gpu") != NULL) type = PSE_LT_GPU;
			else if (strstr(linkname, "cdr") != NULL) type = PSE_LT_CDR;
			else if (strstr(linkname, "spu") != NULL) type = PSE_LT_SPU;
			else if (strstr(linkname, "pad") != NULL) type = PSE_LT_PAD;
			else { g_free(linkname); continue; }
		}
		else type = PSE_getLibType();

		PSE_getLibName = (PSEgetLibName) dlsym(Handle, "PSEgetLibName");
		if (PSE_getLibName != NULL) {
			sprintf(name, "%s", PSE_getLibName());
			PSE_getLibVersion = (PSEgetLibVersion) dlsym(Handle, "PSEgetLibVersion");
			if (PSE_getLibVersion != NULL) {
				char ver[32];

				v = PSE_getLibVersion();
				sprintf(ver, " %ld.%ld.%ld", v >> 16, (v >> 8) & 0xff, v & 0xff);
				strcat(name, ver);
			}
		}
		else strcpy(name, ent->d_name);

		if (type & PSE_LT_CDR)
			ComboAddPlugin(Cdr);
		if (type & PSE_LT_GPU)
			ComboAddPlugin(Gpu);
		if (type & PSE_LT_SPU)
			ComboAddPlugin(Spu);
		if (type & PSE_LT_PAD) {
			PADquery query = (PADquery)dlsym(Handle, "PADquery");
			if (query() & 0x1) {
				ComboAddPlugin(Pad1);
			}
			if (query() & 0x2) {
				ComboAddPlugin(Pad2);
			}
		}
		g_free(linkname);
	}
	closedir(dir);

	scan_bios_dir(Config.BiosDir);

	// The BIOS list always contains the PCSX internal BIOS
	add_bios_to_list(_("Simulate PSX BIOS"), "HLE");
}

static void UpdatePluginsBIOS_UpdateGUI(GladeXML *xml) {
	// Populate the plugin combo boxes
	ConfCreatePConf("Gpu", Gpu);
	ConfCreatePConf("Spu", Spu);
	ConfCreatePConf("Pad1", Pad1);
	ConfCreatePConf("Pad2", Pad2);
	ConfCreatePConf("Cdr", Cdr);
	ConfCreatePConf("Bios", Bios);
}

static void FindNetPlugin(GladeXML *xml) {
	DIR *dir;
	struct dirent *ent;
	void *Handle;
	char plugin[MAXPATHLEN],name[MAXPATHLEN];

	NetConfS.plugins  = 0;
	NetConfS.glist = NULL; 

	NetConfS.plugins += 2;
	strcpy(NetConfS.plist[NetConfS.plugins - 1], "Disabled");
	strcpy(NetConfS.plist[NetConfS.plugins - 2], "Disabled");
	NetConfS.glist = g_list_append(NetConfS.glist, NetConfS.plist[NetConfS.plugins - 1]);

	dir = opendir(Config.PluginsDir);
	if (dir == NULL)
		SysMessage(_("Could not open directory: '%s'\n"), Config.PluginsDir);
	else {
		/* ADB TODO Replace the following with a function */
		while ((ent = readdir(dir)) != NULL) {
			long type, v;

			sprintf(plugin, "%s/%s", Config.PluginsDir, ent->d_name);

			if (strstr(plugin, ".so") == NULL && strstr(plugin, ".dylib") == NULL)
				continue;
			Handle = dlopen(plugin, RTLD_NOW);
			if (Handle == NULL) continue;

			PSE_getLibType = (PSEgetLibType) dlsym(Handle, "PSEgetLibType");
			if (PSE_getLibType == NULL) {
				if (strstr(plugin, "net") != NULL) type = PSE_LT_NET;
				else continue;
			}
			else type = PSE_getLibType();

			PSE_getLibName = (PSEgetLibName) dlsym(Handle, "PSEgetLibName");
			if (PSE_getLibName != NULL) {
				sprintf(name, "%s", PSE_getLibName());
				PSE_getLibVersion = (PSEgetLibVersion) dlsym(Handle, "PSEgetLibVersion");
				if (PSE_getLibVersion != NULL) {
					char ver[32];

					v = PSE_getLibVersion();
					sprintf(ver, " %ld.%ld.%ld",v>>16,(v>>8)&0xff,v&0xff);
					strcat(name, ver);
				}
			}
			else strcpy(name, ent->d_name);

			if (type & PSE_LT_NET) {
				ComboAddPlugin(Net);
			}
		}
		closedir(dir);

		ConfCreatePConf("Net", Net);
	}
}

GtkWidget *CpuDlg;
GtkWidget *PsxCombo;
GList *psxglist;
char *psxtypes[] = {
	"NTSC",
	"PAL"
};

// When the auto-detect CPU type is selected, disable the NTSC/PAL selection
static void OnCpu_PsxAutoClicked (GtkWidget *widget, gpointer user_data) {
	GtkWidget *combo;
	GladeXML *xml = user_data;
	combo = glade_xml_get_widget(xml, "GtkCombo_PsxType");

	gtk_widget_set_sensitive (combo,
			!(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))));
}

// When the interpreter core is deselected, disable the debugger checkbox
static void OnCpu_CpuClicked(GtkWidget *widget, gpointer user_data) {
	GtkWidget *check;
	GladeXML *xml = user_data;
	check = glade_xml_get_widget(xml, "GtkCheckButton_Dbg");

	// Debugger is only working with interpreter not recompiler, so let's set it
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);

	gtk_widget_set_sensitive (check,
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

void OnCpu_Clicked(GtkDialog *dialog, gint arg1, gpointer user_data) {
	GtkWidget *widget;
	GladeXML *xml = user_data;
	int tmp;
	long t;

	widget = glade_xml_get_widget(xml, "GtkCombo_PsxType");

	// If nothing chosen, default to NTSC
	tmp = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	if (tmp == -1)	
		tmp = PSX_TYPE_NTSC;

	if (!strcmp("NTSC", psxtypes[tmp]))
		Config.PsxType = PSX_TYPE_NTSC;
	else
		Config.PsxType = PSX_TYPE_PAL;

	Config.Xa = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Xa")));
	Config.Sio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Sio")));
	Config.Mdec = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Mdec")));
	Config.Cdda = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_CDDA")));
	Config.PsxAuto = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_PsxAuto")));

	t = Config.Debug;
	Config.Debug = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Dbg")));
	if (t != Config.Debug) {
		if (Config.Debug) StartDebugger();
		else StopDebugger();
	}

	t = Config.Cpu;
	Config.Cpu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Cpu")));
	if (t != Config.Cpu) {
		psxCpu->Shutdown();
#ifdef PSXREC
		if (Config.Cpu == CPU_INTERPRETER) {
			psxCpu = &psxInt;
		}
		else psxCpu = &psxRec;
#else
		psxCpu = &psxInt;
#endif
		if (psxCpu->Init() == -1) {
			SysClose();
			exit(1);
		}
		psxCpu->Reset();
	}

	Config.PsxOut = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_PsxOut")));
	Config.SpuIrq = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_SpuIrq")));
	Config.RCntFix = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_RCntFix")));
	Config.VSyncWA = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_VSyncWA")));

	SaveConfig();

	gtk_widget_destroy(CpuDlg);
	CpuDlg = NULL;
}

void OnConf_Cpu() {
	GladeXML *xml;

	xml = glade_xml_new(PACKAGE_DATA_DIR "pcsx.glade2", "CpuDlg", NULL);

	if (!xml) {
		g_warning("We could not load the interface!");
		return;
	}

	CpuDlg = glade_xml_get_widget(xml, "CpuDlg");

	PsxCombo = glade_xml_get_widget(xml, "GtkCombo_PsxType");
	gtk_combo_box_set_active(GTK_COMBO_BOX (PsxCombo), Config.PsxType);
	gtk_widget_set_sensitive(GTK_WIDGET (PsxCombo), !Config.PsxAuto);

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Xa")), Config.Xa);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Sio")), Config.Sio);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_Mdec")), Config.Mdec);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_CDDA")), Config.Cdda);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_PsxAuto")), Config.PsxAuto);

	g_signal_connect_data(GTK_OBJECT(glade_xml_get_widget(xml, "GtkCheckButton_PsxAuto")), "toggled",
			GTK_SIGNAL_FUNC(OnCpu_PsxAutoClicked), xml, NULL, G_CONNECT_AFTER);

#ifdef PSXREC
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON (glade_xml_get_widget(xml, "GtkCheckButton_Cpu")), Config.Cpu);

	g_signal_connect_data(GTK_OBJECT(glade_xml_get_widget(xml, "GtkCheckButton_Cpu")), "toggled",
			GTK_SIGNAL_FUNC(OnCpu_CpuClicked), xml, NULL, G_CONNECT_AFTER);
#else
	Config.Cpu = CPU_INTERPRETER;

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON (glade_xml_get_widget(xml, "GtkCheckButton_Cpu")), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET (glade_xml_get_widget(xml, "GtkCheckButton_Cpu")), FALSE);
#endif

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON (glade_xml_get_widget(xml, "GtkCheckButton_Dbg")), Config.Cpu && Config.Debug);
	gtk_widget_set_sensitive(GTK_WIDGET (glade_xml_get_widget(xml, "GtkCheckButton_Dbg")), Config.Cpu);

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_PsxOut")), Config.PsxOut);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_SpuIrq")), Config.SpuIrq);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_RCntFix")), Config.RCntFix);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "GtkCheckButton_VSyncWA")), Config.VSyncWA);

	// Setup a handler for when Close or Cancel is clicked
	g_signal_connect_data(GTK_OBJECT(CpuDlg), "response",
			GTK_SIGNAL_FUNC(OnCpu_Clicked), xml, (GClosureNotify)g_object_unref, G_CONNECT_AFTER);
}

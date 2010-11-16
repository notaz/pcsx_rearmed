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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "Linux.h"

#include "../libpcsxcore/cheat.h"
#include "../libpcsxcore/psxmem.h"

GtkWidget *CheatListDlg = NULL;
GtkWidget *CheatSearchDlg = NULL;

static void LoadCheatListItems(int index) {
	GtkListStore *store = gtk_list_store_new(2, G_TYPE_BOOLEAN, G_TYPE_STRING);
	GtkTreeIter iter;
	GtkWidget *widget;
	GladeXML *xml;

	int i;

	xml = glade_get_widget_tree(CheatListDlg);
	widget = glade_xml_get_widget(xml, "GtkCList_Cheat");

	for (i = 0; i < NumCheats; i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, Cheats[i].Enabled, 1, Cheats[i].Descr, -1);
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(widget), GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(widget), TRUE);
	gtk_widget_show(widget);

	if (index >= NumCheats) {
		index = NumCheats - 1;
	}

	if (index >= 0) {
		GtkTreePath *path;
		GtkTreeSelection *sel;

		path = gtk_tree_path_new_from_indices(index, -1);
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

		gtk_tree_selection_select_path(sel, path);
		gtk_tree_path_free(path);
	}
}

static void CheatList_TreeSelectionChanged(GtkTreeSelection *selection, gpointer user_data) {
	GladeXML *xml;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreePath *path;

	gboolean selected;
	int i;

	selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (selected) {
		path = gtk_tree_model_get_path(model, &iter);
		i = *gtk_tree_path_get_indices(path);
		gtk_tree_path_free(path);

		// If a row was selected, and the row is not blank, we can now enable
		// some of the disabled widgets
		xml = glade_get_widget_tree(CheatListDlg);

		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "editbutton1")), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "delbutton1")), TRUE);
	} else {
		xml = glade_get_widget_tree(CheatListDlg);

		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "editbutton1")), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "delbutton1")), FALSE);
	}

	gtk_widget_set_sensitive (GTK_WIDGET(glade_xml_get_widget(xml, "savebutton1")), NumCheats);
}

static void OnCheatListDlg_AddClicked(GtkWidget *widget, gpointer user_data) {
	GtkWidget *dlg;
	GtkWidget *box, *scroll, *label, *descr_edit, *code_edit;

	dlg = gtk_dialog_new_with_buttons(_("Add New Cheat"), GTK_WINDOW(CheatListDlg),
		GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	gtk_window_set_default_size(GTK_WINDOW(dlg), 350, 350);

	box = GTK_WIDGET(GTK_DIALOG(dlg)->vbox);

	label = gtk_label_new(_("Cheat Description:"));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	descr_edit = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), descr_edit, FALSE, FALSE, 5);
	gtk_widget_show(descr_edit);

	label = gtk_label_new(_("Cheat Code:"));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	code_edit = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(code_edit), GTK_WRAP_CHAR);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), code_edit);
	gtk_widget_show(code_edit);

	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 5);
	gtk_widget_show(scroll);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);

	gtk_widget_show_all(dlg);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
		GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(code_edit));
		GtkTextIter s, e;
		char *codetext;

		gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(b), &s, &e);
		codetext = strdup(gtk_text_buffer_get_text(GTK_TEXT_BUFFER(b), &s, &e, FALSE));

		if (AddCheat(gtk_entry_get_text(GTK_ENTRY(descr_edit)), codetext) != 0) {
			SysErrorMessage(_("Error"), _("Invalid cheat code!"));
		}

		LoadCheatListItems(NumCheats - 1);

		free(codetext);
	}

	gtk_widget_destroy(dlg);
}

static void OnCheatListDlg_EditClicked(GtkWidget *widget, gpointer user_data) {
	GtkWidget *dlg;
	GtkWidget *box, *scroll, *label, *descr_edit, *code_edit;
	GladeXML *xml;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreePath *path;

	gboolean selected;
	int index, i;
	char buf[8192];
	char *p = buf;

	xml = glade_get_widget_tree(CheatListDlg);
	widget = glade_xml_get_widget(xml, "GtkCList_Cheat");

	selected = gtk_tree_selection_get_selected(
		gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)),
		&model, &iter);

	if (!selected) {
		return;
	}

	path = gtk_tree_model_get_path(model, &iter);
	index = *gtk_tree_path_get_indices(path);
	gtk_tree_path_free(path);

	dlg = gtk_dialog_new_with_buttons(_("Edit Cheat"), GTK_WINDOW(CheatListDlg),
		GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	gtk_window_set_default_size(GTK_WINDOW(dlg), 350, 350);

	box = GTK_WIDGET(GTK_DIALOG(dlg)->vbox);

	label = gtk_label_new(_("Cheat Description:"));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	descr_edit = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(descr_edit), Cheats[index].Descr);
	gtk_box_pack_start(GTK_BOX(box), descr_edit, FALSE, FALSE, 5);
	gtk_widget_show(descr_edit);

	label = gtk_label_new(_("Cheat Code:"));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	code_edit = gtk_text_view_new();

	for (i = Cheats[index].First; i < Cheats[index].First + Cheats[index].n; i++) {
		sprintf(p, "%.8X %.4X\n", CheatCodes[i].Addr, CheatCodes[i].Val);
		p += 14;
		*p = '\0';
	}

	gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(code_edit)),
		buf, -1);

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(code_edit), GTK_WRAP_CHAR);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), code_edit);
	gtk_widget_show(code_edit);

	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 5);
	gtk_widget_show(scroll);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);

	gtk_widget_show_all(dlg);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
		GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(code_edit));
		GtkTextIter s, e;
		char *codetext;

		gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(b), &s, &e);
		codetext = strdup(gtk_text_buffer_get_text(GTK_TEXT_BUFFER(b), &s, &e, FALSE));

		if (EditCheat(index, gtk_entry_get_text(GTK_ENTRY(descr_edit)), codetext) != 0) {
			SysErrorMessage(_("Error"), _("Invalid cheat code!"));
		}

		LoadCheatListItems(index);

		free(codetext);		
	}

	gtk_widget_destroy(dlg);
}

static void OnCheatListDlg_DelClicked(GtkWidget *widget, gpointer user_data) {
	GladeXML *xml;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreePath *path;

	gboolean selected;
	int i = -1;

	xml = glade_get_widget_tree(CheatListDlg);
	widget = glade_xml_get_widget(xml, "GtkCList_Cheat");

	selected = gtk_tree_selection_get_selected(
		gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)),
		&model, &iter);

	if (selected) {
		path = gtk_tree_model_get_path(model, &iter);
		i = *gtk_tree_path_get_indices(path);
		gtk_tree_path_free(path);

		RemoveCheat(i);
	}

	LoadCheatListItems(i); // FIXME: should remove it from the list directly
	                       // rather than regenerating the whole list
}

static void OnCheatListDlg_EnableToggled(GtkWidget *widget, gchar *path, gpointer user_data) {
	int i = atoi(path);

	assert(i >= 0 && i < NumCheats);
	Cheats[i].Enabled ^= 1;

	LoadCheatListItems(i); // FIXME: should modify it in the list directly
	                       // rather than regenerating the whole list
}

static void OnCheatListDlg_OpenClicked(GtkWidget *widget, gpointer user_data) {
	GtkWidget *chooser;
	gchar *filename;

	GtkFileFilter *filter;

	chooser = gtk_file_chooser_dialog_new (_("Open Cheat File"),
		NULL, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	filename = g_build_filename(getenv("HOME"), CHEATS_DIR, NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), filename);
	g_free(filename);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.cht");
	gtk_file_filter_set_name (filter, _("PCSX Cheat Code Files (*.cht)"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK) {
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		gtk_widget_destroy (GTK_WIDGET (chooser));
		while (gtk_events_pending()) gtk_main_iteration();
	} else {
		gtk_widget_destroy (GTK_WIDGET (chooser));
		while (gtk_events_pending()) gtk_main_iteration();
		return;
	}

	LoadCheats(filename);

	g_free(filename);

	LoadCheatListItems(-1);
}

static void OnCheatListDlg_SaveClicked(GtkWidget *widget, gpointer user_data) {
	GtkWidget *chooser;
	gchar *filename;
	GtkFileFilter *filter;

	chooser = gtk_file_chooser_dialog_new(_("Save Cheat File"),
		NULL, GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	filename = g_build_filename(getenv("HOME"), CHEATS_DIR, NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), filename);
	g_free(filename);

	filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter, "*.cht");
	gtk_file_filter_set_name(filter, _("PCSX Cheat Code Files (*.cht)"));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_filter_set_name(filter, _("All Files (*.*)"));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_OK) {
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		gtk_widget_destroy (GTK_WIDGET(chooser));
		while (gtk_events_pending()) gtk_main_iteration();
	} else {
		gtk_widget_destroy (GTK_WIDGET(chooser));
		while (gtk_events_pending()) gtk_main_iteration();
		return;
	}

	SaveCheats(filename);

	g_free(filename);
}

static void OnCheatListDlg_CloseClicked() {
	gtk_widget_destroy(CheatListDlg);
	CheatListDlg = NULL;
}

// run the cheat list dialog
void RunCheatListDialog() {
	GladeXML *xml;
	GtkWidget *widget;
	GtkTreeSelection *treesel;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	xml = glade_xml_new(PACKAGE_DATA_DIR "pcsx.glade2", "CheatListDlg", NULL);
	if (!xml) {
		g_warning(_("Error: Glade interface could not be loaded!"));
		return;
	}

	CheatListDlg = glade_xml_get_widget(xml, "CheatListDlg");
	gtk_window_set_title(GTK_WINDOW(CheatListDlg), _("Cheat Codes"));

	widget = glade_xml_get_widget(xml, "GtkCList_Cheat");

	// column for enable
	renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(_("Enable"),
		renderer, "active", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

	g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(OnCheatListDlg_EnableToggled), 0);

	// column for description
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Description"),
		renderer, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

	LoadCheatListItems(-1);

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);
	g_signal_connect_data(G_OBJECT (treesel), "changed",
						  G_CALLBACK (CheatList_TreeSelectionChanged),
						  NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "addbutton1");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnCheatListDlg_AddClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "editbutton1");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnCheatListDlg_EditClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "delbutton1");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnCheatListDlg_DelClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "loadbutton1");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnCheatListDlg_OpenClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "savebutton1");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
			GTK_SIGNAL_FUNC(OnCheatListDlg_SaveClicked), xml, NULL, G_CONNECT_AFTER);

	// Setup a handler for when Close or Cancel is clicked
	g_signal_connect_data(GTK_OBJECT(CheatListDlg), "response",
			GTK_SIGNAL_FUNC(OnCheatListDlg_CloseClicked), xml, (GClosureNotify)g_object_unref, G_CONNECT_AFTER);

	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "savebutton1")), NumCheats);
	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "editbutton1")), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "delbutton1")), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "editbutton1")), FALSE);
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

// update the cheat search dialog
static void UpdateCheatSearchDialog() {
	GladeXML		*xml;
	char			buf[256];
	int				i;
	u32				addr;
	GtkListStore	*store = gtk_list_store_new(1, G_TYPE_STRING);
	GtkTreeIter		iter;
	GtkWidget		*widget;

	xml = glade_get_widget_tree(CheatSearchDlg);
	widget = glade_xml_get_widget(xml, "GtkCList_Result");

	gtk_combo_box_set_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "combo_searchfor")), current_search);
	gtk_combo_box_set_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "combo_datatype")), current_searchtype);
	gtk_combo_box_set_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "combo_database")), current_searchbase);

	if (current_searchbase == SEARCHBASE_DEC) {
		sprintf(buf, "%u", current_valuefrom);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value")), buf);
		sprintf(buf, "%u", current_valueto);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto")), buf);
	}
	else {
		sprintf(buf, "%X", current_valuefrom);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value")), buf);
		sprintf(buf, "%X", current_valueto);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto")), buf);
	}

	if (current_search == SEARCH_RANGE) {
		gtk_widget_show(GTK_WIDGET(glade_xml_get_widget(xml, "label_valueto")));
		gtk_widget_show(GTK_WIDGET(glade_xml_get_widget(xml, "entry_valueto")));
	}
	else {
		gtk_widget_hide(GTK_WIDGET(glade_xml_get_widget(xml, "label_valueto")));
		gtk_widget_hide(GTK_WIDGET(glade_xml_get_widget(xml, "entry_valueto")));
	}

	if (current_search >= SEARCH_INC) {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "entry_value")), FALSE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "entry_value")), TRUE);
	}

	if (current_search >= SEARCH_INCBY && prevM == NULL) {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_start")), FALSE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_start")), TRUE);
	}

	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_freeze")), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_modify")), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_copy")), FALSE);

	if (prevM != NULL) {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "combo_datatype")), FALSE);

		if (NumSearchResults > 100) {
			// too many results to be shown
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, _("Too many addresses found."), -1);
			gtk_widget_set_sensitive(widget, FALSE);
		}
		else {
			for (i = 0; i < NumSearchResults; i++) {
				addr = SearchResults[i];

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

				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter, 0, buf, -1);
			}
			gtk_widget_set_sensitive(widget, TRUE);
		}

		sprintf(buf, _("Founded Addresses: %d"), NumSearchResults);
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(xml, "label_resultsfound")), buf);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "combo_datatype")), TRUE);
		gtk_widget_set_sensitive(widget, FALSE);

		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(xml, "label_resultsfound")),
			_("Enter the values and start your search."));
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(widget), GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(widget), TRUE);
	gtk_widget_show(widget);
}

// get the current selected result index in the list
static int GetSelectedResultIndex() {
	GladeXML			*xml;
	GtkTreeSelection	*selection;
	GtkTreeIter			iter;
	GtkTreeModel		*model;
	GtkTreePath			*path;
	gboolean			selected;
	int					i;

	xml = glade_get_widget_tree(CheatSearchDlg);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(glade_xml_get_widget(xml, "GtkCList_Result")));
	selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (!selected) {
		return -1;
	}

	path = gtk_tree_model_get_path(model, &iter);
	i = *gtk_tree_path_get_indices(path);
	gtk_tree_path_free(path);

	assert(i < NumSearchResults);
	return i;
}

// add cheat code to freeze the value
static void OnCheatSearchDlg_FreezeClicked(GtkWidget *widget, gpointer user_data) {
	GtkWidget	   *dlg;
	GtkWidget	   *box, *hbox, *label, *descr_edit, *value_edit;
	char			buf[256];
	u32				addr, val = 0;

	addr = SearchResults[GetSelectedResultIndex()];

	dlg = gtk_dialog_new_with_buttons(_("Freeze value"), GTK_WINDOW(CheatListDlg),
		GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	box = GTK_WIDGET(GTK_DIALOG(dlg)->vbox);

	label = gtk_label_new(_("Description:"));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	descr_edit = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), descr_edit, FALSE, FALSE, 10);
	gtk_widget_show(descr_edit);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 15);

	label = gtk_label_new(_("Value:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	value_edit = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), value_edit, FALSE, FALSE, 10);
	gtk_widget_show(value_edit);

	switch (current_searchtype) {
		case SEARCHTYPE_8BIT:
			val = PSXMu8(addr);
			break;

		case SEARCHTYPE_16BIT:
			val = PSXMu16(addr);
			break;

		case SEARCHTYPE_32BIT:
			val = PSXMu32(addr);
			break;

		default:
			assert(FALSE); // should not reach here
			break;
	}

	sprintf(buf, "%u", val);
	gtk_entry_set_text(GTK_ENTRY(value_edit), buf);

	sprintf(buf, "%.8X", addr);
	gtk_entry_set_text(GTK_ENTRY(descr_edit), buf);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_widget_show_all(dlg);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
		val = 0;
		sscanf(gtk_entry_get_text(GTK_ENTRY(value_edit)), "%u", &val);

		switch (current_searchtype) {
			case SEARCHTYPE_8BIT:
				if (val > (u32)0xFF) {
					val = 0xFF;
				}
				sprintf(buf, "%.8X %.4X", (addr & 0x1FFFFF) | (CHEAT_CONST8 << 24), val);
				break;

			case SEARCHTYPE_16BIT:
				if (val > (u32)0xFFFF) {
					val = 0xFFFF;
				}
				sprintf(buf, "%.8X %.4X", (addr & 0x1FFFFF) | (CHEAT_CONST16 << 24), val);
				break;

			case SEARCHTYPE_32BIT:
				sprintf(buf, "%.8X %.4X\n%.8X %.4X",
					(addr & 0x1FFFFF) | (CHEAT_CONST16 << 24), val & 0xFFFF,
					((addr + 2) & 0x1FFFFF) | (CHEAT_CONST16 << 24), ((val & 0xFFFF0000) >> 16) & 0xFFFF);
				break;

			default:
				assert(FALSE); // should not reach here
				break;
		}

		if (AddCheat(gtk_entry_get_text(GTK_ENTRY(descr_edit)), buf) == 0) {
			Cheats[NumCheats - 1].Enabled = 1;
		}
	}

	gtk_widget_destroy(dlg);
}

// modify the value on the fly
static void OnCheatSearchDlg_ModifyClicked(GtkWidget *widget, gpointer user_data) {
	GtkWidget	   *dlg;
	GtkWidget	   *box, *hbox, *label, *value_edit;
	char			buf[256];
	u32				addr, val = 0;

	addr = SearchResults[GetSelectedResultIndex()];

	dlg = gtk_dialog_new_with_buttons(_("Modify value"), GTK_WINDOW(CheatListDlg),
		GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	box = GTK_WIDGET(GTK_DIALOG(dlg)->vbox);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);

	label = gtk_label_new(_("New value:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	value_edit = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), value_edit, FALSE, FALSE, 10);
	gtk_widget_show(value_edit);

	switch (current_searchtype) {
		case SEARCHTYPE_8BIT:
			val = PSXMu8(addr);
			break;

		case SEARCHTYPE_16BIT:
			val = PSXMu16(addr);
			break;

		case SEARCHTYPE_32BIT:
			val = PSXMu32(addr);
			break;

		default:
			assert(FALSE); // should not reach here
			break;
	}

	sprintf(buf, "%u", val);
	gtk_entry_set_text(GTK_ENTRY(value_edit), buf);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_widget_show_all(dlg);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
		val = 0;
		sscanf(gtk_entry_get_text(GTK_ENTRY(value_edit)), "%u", &val);

		switch (current_searchtype) {
			case SEARCHTYPE_8BIT:
				if (val > 0xFF) {
					val = 0xFF;
				}
				psxMemWrite8(addr, (u8)val);
				break;

			case SEARCHTYPE_16BIT:
				if (val > 0xFFFF) {
					val = 0xFFFF;
				}
				psxMemWrite16(addr, (u16)val);
				break;

			case SEARCHTYPE_32BIT:
				psxMemWrite32(addr, (u32)val);
				break;

			default:
				assert(FALSE); // should not reach here
				break;
		}

		UpdateCheatSearchDialog();
	}

	gtk_widget_destroy(dlg);
}

// copy the selected address to clipboard
static void OnCheatSearchDlg_CopyClicked(GtkWidget *widget, gpointer user_data) {
	int			i;
	char		buf[9];

	i = GetSelectedResultIndex();
	assert(i != -1);

	sprintf(buf, "%8X", SearchResults[i]);
	buf[8] = '\0';

	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), buf, 8);
}

// preform the search
static void OnCheatSearchDlg_SearchClicked(GtkWidget *widget, gpointer user_data) {
	GladeXML		*xml;

	xml = glade_get_widget_tree(CheatSearchDlg);

	current_search = gtk_combo_box_get_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "combo_searchfor")));
	current_searchtype = gtk_combo_box_get_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "combo_datatype")));
	current_searchbase = gtk_combo_box_get_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "combo_database")));
	current_valuefrom = 0;
	current_valueto = 0;

	if (current_searchbase == SEARCHBASE_DEC) {
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value"))), "%u", &current_valuefrom);
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto"))), "%u", &current_valueto);
	}
	else {
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value"))), "%x", &current_valuefrom);
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto"))), "%x", &current_valueto);
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

	UpdateCheatSearchDialog();
}

// restart the search
static void OnCheatSearchDlg_RestartClicked(GtkWidget *widget, gpointer user_data) {
	FreeCheatSearchResults();
	FreeCheatSearchMem();

	current_search = SEARCH_EQUALVAL;
	current_searchtype = SEARCHTYPE_8BIT;
	current_searchbase = SEARCHBASE_DEC;
	current_valuefrom = 0;
	current_valueto = 0;

	UpdateCheatSearchDialog();
}

// close the cheat search window
static void OnCheatSearchDlg_CloseClicked(GtkWidget *widget, gpointer user_data) {
	gtk_widget_destroy(CheatSearchDlg);
	CheatSearchDlg = NULL;
}

static void OnCheatSearchDlg_SearchForChanged(GtkWidget *widget, gpointer user_data) {
	GladeXML *xml;

	xml = glade_get_widget_tree(CheatSearchDlg);

	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) == SEARCH_RANGE) {
		gtk_widget_show(GTK_WIDGET(glade_xml_get_widget(xml, "label_valueto")));
		gtk_widget_show(GTK_WIDGET(glade_xml_get_widget(xml, "entry_valueto")));
	}
	else {
		gtk_widget_hide(GTK_WIDGET(glade_xml_get_widget(xml, "label_valueto")));
		gtk_widget_hide(GTK_WIDGET(glade_xml_get_widget(xml, "entry_valueto")));
	}

	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) >= SEARCH_INC) {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "entry_value")), FALSE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "entry_value")), TRUE);
	}

	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) >= SEARCH_INCBY && prevM == NULL) {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_start")), FALSE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_start")), TRUE);
	}
}

static void OnCheatSearchDlg_DataBaseChanged(GtkWidget *widget, gpointer user_data) {
	u32				val;
	char			buf[256];
	GladeXML		*xml;

	xml = glade_get_widget_tree(CheatSearchDlg);

	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) == SEARCHBASE_DEC) {
		val = 0;
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value"))), "%x", &val);
		sprintf(buf, "%u", val);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value")), buf);

		val = 0;
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto"))), "%x", &val);
		sprintf(buf, "%u", val);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto")), buf);
	}
	else {
		val = 0;
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value"))), "%u", &val);
		sprintf(buf, "%X", val);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_value")), buf);

		val = 0;
		sscanf(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto"))), "%u", &val);
		sprintf(buf, "%X", val);
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(xml, "entry_valueto")), buf);
	}
}

static void CheatSearch_TreeSelectionChanged(GtkTreeSelection *selection, gpointer user_data) {
	GladeXML			*xml;

	xml = glade_get_widget_tree(CheatSearchDlg);

	if (GetSelectedResultIndex() != -1) {
		// If a row was selected, we can now enable some of the disabled widgets
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_freeze")), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_modify")), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_copy")), TRUE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_freeze")), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_modify")), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btn_copy")), FALSE);
	}
}

// run the cheat search dialog
void RunCheatSearchDialog() {
	GladeXML *xml;
	GtkWidget *widget;
	GtkCellRenderer *renderer;
	GtkTreeSelection *treesel;
	GtkTreeViewColumn *column;

	xml = glade_xml_new(PACKAGE_DATA_DIR "pcsx.glade2", "CheatSearchDlg", NULL);
	if (!xml) {
		g_warning(_("Error: Glade interface could not be loaded!"));
		return;
	}

	CheatSearchDlg = glade_xml_get_widget(xml, "CheatSearchDlg");
	gtk_window_set_title(GTK_WINDOW(CheatSearchDlg), _("Cheat Search"));

	widget = glade_xml_get_widget(xml, "GtkCList_Result");

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes(_("Search Results"),
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	gtk_tree_selection_set_mode (treesel, GTK_SELECTION_SINGLE);
	g_signal_connect_data(G_OBJECT(treesel), "changed",
						  G_CALLBACK(CheatSearch_TreeSelectionChanged),
						  NULL, NULL, G_CONNECT_AFTER);

	UpdateCheatSearchDialog();

	widget = glade_xml_get_widget(xml, "btn_freeze");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_FreezeClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_modify");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_ModifyClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_copy");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_CopyClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_start");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_SearchClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btn_restart");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_RestartClicked), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "combo_searchfor");
	g_signal_connect_data(GTK_OBJECT(widget), "changed",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_SearchForChanged), xml, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "combo_database");
	g_signal_connect_data(GTK_OBJECT(widget), "changed",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_DataBaseChanged), xml, NULL, G_CONNECT_AFTER);

	g_signal_connect_data(GTK_OBJECT(CheatSearchDlg), "response",
		GTK_SIGNAL_FUNC(OnCheatSearchDlg_CloseClicked), xml, (GClosureNotify)g_object_unref, G_CONNECT_AFTER);
}

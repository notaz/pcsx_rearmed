/*
 * Copyright (c) 2009, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 */

#include "cfg.c"

#include <time.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

GtkWidget *MainWindow;

const int DPad[DKEY_TOTAL] = {
	DKEY_UP,
	DKEY_DOWN,
	DKEY_LEFT,
	DKEY_RIGHT,
	DKEY_CROSS,
	DKEY_CIRCLE,
	DKEY_SQUARE,
	DKEY_TRIANGLE,
	DKEY_L1,
	DKEY_R1,
	DKEY_L2,
	DKEY_R2,
	DKEY_SELECT,
	DKEY_START,
	DKEY_L3,
	DKEY_R3
};

const char *DPadText[DKEY_TOTAL] = {
	N_("D-Pad Up"),
	N_("D-Pad Down"),
	N_("D-Pad Left"),
	N_("D-Pad Right"),
	N_("Cross"),
	N_("Circle"),
	N_("Square"),
	N_("Triangle"),
	N_("L1"),
	N_("R1"),
	N_("L2"),
	N_("R2"),
	N_("Select"),
	N_("Start"),
	N_("L3"),
	N_("R3")
};

const char *AnalogText[] = {
	N_("L-Stick Right"),
	N_("L-Stick Left"),
	N_("L-Stick Down"),
	N_("L-Stick Up"),
	N_("R-Stick Right"),
	N_("R-Stick Left"),
	N_("R-Stick Down"),
	N_("R-Stick Up")
};

static int GetSelectedKeyIndex(int padnum) {
	GladeXML			*xml;
	GtkTreeSelection	*selection;
	GtkTreeIter			iter;
	GtkTreeModel		*model;
	GtkTreePath			*path;
	gboolean			selected;
	int					i;

	xml = glade_get_widget_tree(MainWindow);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(glade_xml_get_widget(xml, padnum == 0 ? "treeview1" : "treeview2")));
	selected = gtk_tree_selection_get_selected(selection, &model, &iter);

	if (!selected) {
		return -1;
	}

	path = gtk_tree_model_get_path(model, &iter);
	i = *gtk_tree_path_get_indices(path);
	gtk_tree_path_free(path);

	return i;
}

static void GetKeyDescription(char *buf, int joynum, int key) {
	const char *hatname[16] = {_("Centered"), _("Up"), _("Right"), _("Rightup"),
		_("Down"), "", _("Rightdown"), "", _("Left"), _("Leftup"), "", "",
		_("Leftdown"), "", "", ""};

	switch (g.cfg.PadDef[joynum].KeyDef[key].JoyEvType) {
		case BUTTON:
			sprintf(buf, _("Joystick: Button %d"), g.cfg.PadDef[joynum].KeyDef[key].J.Button);
			break;

		case AXIS:
			sprintf(buf, _("Joystick: Axis %d%c"), abs(g.cfg.PadDef[joynum].KeyDef[key].J.Axis) - 1,
				g.cfg.PadDef[joynum].KeyDef[key].J.Axis > 0 ? '+' : '-');
			break;

		case HAT:
			sprintf(buf, _("Joystick: Hat %d %s"), (g.cfg.PadDef[joynum].KeyDef[key].J.Hat >> 8),
				hatname[g.cfg.PadDef[joynum].KeyDef[key].J.Hat & 0x0F]);
			break;

		case NONE:
		default:
			buf[0] = '\0';
			break;
	}

	if (g.cfg.PadDef[joynum].KeyDef[key].Key != 0) {
		if (buf[0] != '\0') {
			strcat(buf, " / ");
		}

		strcat(buf, _("Keyboard:"));
		strcat(buf, " ");
		strcat(buf, XKeysymToString(g.cfg.PadDef[joynum].KeyDef[key].Key));
	} else if (buf[0] == '\0') {
		strcpy(buf, _("(Not Set)"));
	}
}

static void GetAnalogDescription(char *buf, int joynum, int analognum, int dir) {
	const char *hatname[16] = {_("Centered"), _("Up"), _("Right"), _("Rightup"),
		_("Down"), "", _("Rightdown"), "", _("Left"), _("Leftup"), "", "",
		_("Leftdown"), "", "", ""};

	switch (g.cfg.PadDef[joynum].AnalogDef[analognum][dir].JoyEvType) {
		case BUTTON:
			sprintf(buf, _("Joystick: Button %d"), g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Button);
			break;

		case AXIS:
			sprintf(buf, _("Joystick: Axis %d%c"), abs(g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Axis) - 1,
				g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Axis > 0 ? '+' : '-');
			break;

		case HAT:
			sprintf(buf, _("Joystick: Hat %d %s"), (g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Hat >> 8),
				hatname[g.cfg.PadDef[joynum].AnalogDef[analognum][dir].J.Hat & 0x0F]);
			break;

		case NONE:
		default:
			buf[0] = '\0';
			break;
	}

	if (g.cfg.PadDef[joynum].AnalogDef[analognum][dir].Key != 0) {
		if (buf[0] != '\0') {
			strcat(buf, " / ");
		}

		strcat(buf, _("Keyboard:"));
		strcat(buf, " ");
		strcat(buf, XKeysymToString(g.cfg.PadDef[joynum].AnalogDef[analognum][dir].Key));
	} else if (buf[0] == '\0') {
		strcpy(buf, _("(Not Set)"));
	}
}

static void UpdateKeyList() {
	const char *widgetname[2] = {"treeview1", "treeview2"};

	GladeXML *xml;
	GtkWidget *widget;
	GtkListStore *store;
	GtkTreeIter iter;
	int i, j;
	char buf[256];

	xml = glade_get_widget_tree(MainWindow);

	for (i = 0; i < 2; i++) {
		int total;

		if (g.cfg.PadDef[i].Type == PSE_PAD_TYPE_ANALOGPAD) {
			total = DKEY_TOTAL;
		} else {
			total = DKEY_TOTAL - 2;
		}

		widget = glade_xml_get_widget(xml, widgetname[i]);

		store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

		for (j = 0; j < total; j++) {
			gtk_list_store_append(store, &iter);
			GetKeyDescription(buf, i, DPad[j]);
			gtk_list_store_set(store, &iter, 0, _(DPadText[j]), 1, buf, -1);
		}

		if (g.cfg.PadDef[i].Type == PSE_PAD_TYPE_ANALOGPAD) {
			for (j = 0; j < 8; j++) {
				gtk_list_store_append(store, &iter);
				GetAnalogDescription(buf, i, j / 4, j % 4);
				gtk_list_store_set(store, &iter, 0, _(AnalogText[j]), 1, buf, -1);
			}
		}

		gtk_tree_view_set_model(GTK_TREE_VIEW(widget), GTK_TREE_MODEL(store));
		g_object_unref(G_OBJECT(store));
		gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(widget), TRUE);
		gtk_widget_show(widget);
	}
}

static void UpdateKey() {
	const char *widgetname[2] = {"treeview1", "treeview2"};
	int i, index;
	GladeXML *xml;
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue value = {0, };
	char buf[256];

	xml = glade_get_widget_tree(MainWindow);

	for (i = 0; i < 2; i++) {
		index = GetSelectedKeyIndex(i);
		if (index == -1) continue;

		widget = glade_xml_get_widget(xml, widgetname[i]);
		gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), &model, &iter);

		if (index < DKEY_TOTAL) {
			GetKeyDescription(buf, i, DPad[index]);
		} else {
			GetAnalogDescription(buf, i, (index - DKEY_TOTAL) / 4, (index - DKEY_TOTAL) % 4);
		}

		g_value_init(&value, G_TYPE_STRING);
		g_value_set_string(&value, buf);
		gtk_list_store_set_value(GTK_LIST_STORE(model), &iter, 1, &value);
	}
}

static void OnConfigExit(GtkWidget *widget, gpointer user_data) {
	SavePADConfig();

	gtk_widget_destroy(widget);
	SDL_Quit();

	gtk_exit(0);
}

static void TreeSelectionChanged(GtkTreeSelection *selection, gpointer user_data) {
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
		xml = glade_get_widget_tree(MainWindow);

		if ((int)user_data == 0) {
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnchange1")), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnreset1")), TRUE);
		} else {
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnchange2")), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnreset2")), TRUE);
		}
	} else {
		xml = glade_get_widget_tree(MainWindow);

		if ((int)user_data == 0) {
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnchange1")), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnreset1")), FALSE);
		} else {
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnchange2")), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(glade_xml_get_widget(xml, "btnreset2")), FALSE);
		}
	}
}

static void OnDeviceChanged(GtkWidget *widget, gpointer user_data) {
	int n = (int)user_data, current = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	current--;
	g.cfg.PadDef[n].DevNum = current;
}

static void OnTypeChanged(GtkWidget *widget, gpointer user_data) {
	int n = (int)user_data, current = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	g.cfg.PadDef[n].Type = (current == 0 ? PSE_PAD_TYPE_STANDARD : PSE_PAD_TYPE_ANALOGPAD);

	UpdateKeyList();
}

static void OnThreadedToggled(GtkWidget *widget, gpointer user_data) {
	g.cfg.Threaded = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void ReadDKeyEvent(int padnum, int key) {
	SDL_Joystick *js;
	time_t t;
	GdkEvent *ge;
	int i;
	Sint16 axis, numAxes = 0, InitAxisPos[256], PrevAxisPos[256];

	if (g.cfg.PadDef[padnum].DevNum >= 0) {
		js = SDL_JoystickOpen(g.cfg.PadDef[padnum].DevNum);
		SDL_JoystickEventState(SDL_IGNORE);

		SDL_JoystickUpdate();

		numAxes = SDL_JoystickNumAxes(js);
		if (numAxes > 256) numAxes = 256;

		for (i = 0; i < numAxes; i++) {
			InitAxisPos[i] = PrevAxisPos[i] = SDL_JoystickGetAxis(js, i);
		}
	} else {
		js = NULL;
	}

	t = time(NULL);

	while (time(NULL) < t + 10) {
		// check joystick events
		if (js != NULL) {
			SDL_JoystickUpdate();

			for (i = 0; i < SDL_JoystickNumButtons(js); i++) {
				if (SDL_JoystickGetButton(js, i)) {
					g.cfg.PadDef[padnum].KeyDef[key].JoyEvType = BUTTON;
					g.cfg.PadDef[padnum].KeyDef[key].J.Button = i;
					goto end;
				}
			}

			for (i = 0; i < numAxes; i++) {
				axis = SDL_JoystickGetAxis(js, i);
				if (abs(axis) > 16383 && (abs(axis - InitAxisPos[i]) > 4096 || abs(axis - PrevAxisPos[i]) > 4096)) {
					g.cfg.PadDef[padnum].KeyDef[key].JoyEvType = AXIS;
					g.cfg.PadDef[padnum].KeyDef[key].J.Axis = (i + 1) * (axis > 0 ? 1 : -1);
					goto end;
				}
				PrevAxisPos[i] = axis;
			}

			for (i = 0; i < SDL_JoystickNumHats(js); i++) {
				axis = SDL_JoystickGetHat(js, i);
				if (axis != SDL_HAT_CENTERED) {
					g.cfg.PadDef[padnum].KeyDef[key].JoyEvType = HAT;

					if (axis & SDL_HAT_UP) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_UP);
					} else if (axis & SDL_HAT_DOWN) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_DOWN);
					} else if (axis & SDL_HAT_LEFT) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_LEFT);
					} else if (axis & SDL_HAT_RIGHT) {
						g.cfg.PadDef[padnum].KeyDef[key].J.Hat = ((i << 8) | SDL_HAT_RIGHT);
					}

					goto end;
				}
			}
		}

		// check keyboard events
		while ((ge = gdk_event_get()) != NULL) {
			if (ge->type == GDK_KEY_PRESS) {
				if (ge->key.keyval != XK_Escape) {
					g.cfg.PadDef[padnum].KeyDef[key].Key = ge->key.keyval;
				}
				gdk_event_free(ge);
				goto end;
			}
			gdk_event_free(ge);
		}

		usleep(5000);
	}

end:
	if (js != NULL) {
		SDL_JoystickClose(js);
	}
}

static void ReadAnalogEvent(int padnum, int analognum, int analogdir) {
	SDL_Joystick *js;
	time_t t;
	GdkEvent *ge;
	int i;
	Sint16 axis, numAxes = 0, InitAxisPos[256], PrevAxisPos[256];

	if (g.cfg.PadDef[padnum].DevNum >= 0) {
		js = SDL_JoystickOpen(g.cfg.PadDef[padnum].DevNum);
		SDL_JoystickEventState(SDL_IGNORE);

		SDL_JoystickUpdate();

		numAxes = SDL_JoystickNumAxes(js);
		if (numAxes > 256) numAxes = 256;

		for (i = 0; i < SDL_JoystickNumAxes(js); i++) {
			InitAxisPos[i] = PrevAxisPos[i] = SDL_JoystickGetAxis(js, i);
		}
	} else {
		js = NULL;
	}

	t = time(NULL);

	while (time(NULL) < t + 10) {
		// check joystick events
		if (js != NULL) {
			SDL_JoystickUpdate();

			for (i = 0; i < SDL_JoystickNumButtons(js); i++) {
				if (SDL_JoystickGetButton(js, i)) {
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].JoyEvType = BUTTON;
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Button = i;
					goto end;
				}
			}

			for (i = 0; i < numAxes; i++) {
				axis = SDL_JoystickGetAxis(js, i);
				if (abs(axis) > 16383 && (abs(axis - InitAxisPos[i]) > 4096 || abs(axis - PrevAxisPos[i]) > 4096)) {
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].JoyEvType = AXIS;
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Axis = (i + 1) * (axis > 0 ? 1 : -1);
					goto end;
				}
				PrevAxisPos[i] = axis;
			}

			for (i = 0; i < SDL_JoystickNumHats(js); i++) {
				axis = SDL_JoystickGetHat(js, i);
				if (axis != SDL_HAT_CENTERED) {
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].JoyEvType = HAT;

					if (axis & SDL_HAT_UP) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_UP);
					} else if (axis & SDL_HAT_DOWN) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_DOWN);
					} else if (axis & SDL_HAT_LEFT) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_LEFT);
					} else if (axis & SDL_HAT_RIGHT) {
						g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].J.Hat = ((i << 8) | SDL_HAT_RIGHT);
					}

					goto end;
				}
			}
		}

		// check keyboard events
		while ((ge = gdk_event_get()) != NULL) {
			if (ge->type == GDK_KEY_PRESS) {
				if (ge->key.keyval != XK_Escape) {
					g.cfg.PadDef[padnum].AnalogDef[analognum][analogdir].Key = ge->key.keyval;
				}
				gdk_event_free(ge);
				goto end;
			}
			gdk_event_free(ge);
		}

		usleep(5000);
	}

end:
	if (js != NULL) {
		SDL_JoystickClose(js);
	}
}

static void OnChangeClicked(GtkWidget *widget, gpointer user_data) {
	int pad = (int)user_data;
	int index = GetSelectedKeyIndex(pad);

	if (index == -1) return;

	if (index < DKEY_TOTAL) {
		ReadDKeyEvent(pad, DPad[index]);
	} else {
		index -= DKEY_TOTAL;
		ReadAnalogEvent(pad, index / 4, index % 4);
	}

	UpdateKey();
}

static void OnResetClicked(GtkWidget *widget, gpointer user_data) {
	int pad = (int)user_data;
	int index = GetSelectedKeyIndex(pad);

	if (index == -1) return;

	if (index < DKEY_TOTAL) {
		g.cfg.PadDef[pad].KeyDef[DPad[index]].Key = 0;
		g.cfg.PadDef[pad].KeyDef[DPad[index]].JoyEvType = NONE;
		g.cfg.PadDef[pad].KeyDef[DPad[index]].J.Button = 0;
	} else {
		index -= DKEY_TOTAL;
		g.cfg.PadDef[pad].AnalogDef[index / 4][index % 4].Key = 0;
		g.cfg.PadDef[pad].AnalogDef[index / 4][index % 4].JoyEvType = NONE;
		g.cfg.PadDef[pad].AnalogDef[index / 4][index % 4].J.Button = 0;
	}

	UpdateKey();
}

static void PopulateDevList() {
	const char *widgetname[2] = {"combodev1", "combodev2"};
	int i, j, n;
	GtkWidget *widget;
	GladeXML *xml;
	GtkTreeIter iter;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	char buf[256];

	xml = glade_get_widget_tree(MainWindow);

	for (i = 0; i < 2; i++) {
		widget = glade_xml_get_widget(xml, widgetname[i]);

		renderer = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget), renderer, FALSE);
		gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(widget), renderer, "text", 0);

		store = gtk_list_store_new(1, G_TYPE_STRING);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, _("None"), -1);

		n = SDL_NumJoysticks();
		for (j = 0; j < n; j++) {
			sprintf(buf, "%d: %s", j + 1, SDL_JoystickName(j));
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, buf, -1);
		}

		gtk_combo_box_set_model(GTK_COMBO_BOX(widget), GTK_TREE_MODEL(store));

		n = g.cfg.PadDef[i].DevNum + 1;
		if (n > SDL_NumJoysticks()) {
			n = 0;
			g.cfg.PadDef[i].DevNum = -1;
		}

		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), n);
	}
}

long PADconfigure() {
	GladeXML *xml;
	GtkWidget *widget;
	GtkTreeSelection *treesel;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	if (SDL_Init(SDL_INIT_JOYSTICK) == -1) {
		fprintf(stderr, "Failed to initialize SDL!\n");
		return -1;
	}

	LoadPADConfig();

	xml = glade_xml_new(DATADIR "dfinput.glade2", "CfgWnd", NULL);
	if (xml == NULL) {
		g_warning("We could not load the interface!");
		return -1;
	}

	MainWindow = glade_xml_get_widget(xml, "CfgWnd");
	gtk_window_set_title(GTK_WINDOW(MainWindow), _("Gamepad/Keyboard Input Configuration"));

	widget = glade_xml_get_widget(xml, "treeview1");

	// column for key
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Key"),
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

	// column for button
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Button"),
		renderer, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);

	g_signal_connect_data(G_OBJECT(treesel), "changed",
		G_CALLBACK(TreeSelectionChanged), (gpointer)0, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "treeview2");

	// column for key
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Key"),
		renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

	// column for button
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Button"),
		renderer, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	gtk_tree_selection_set_mode(treesel, GTK_SELECTION_SINGLE);

	g_signal_connect_data(G_OBJECT(treesel), "changed",
		G_CALLBACK(TreeSelectionChanged), (gpointer)1, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "CfgWnd");
	g_signal_connect_data(GTK_OBJECT(widget), "delete_event",
		GTK_SIGNAL_FUNC(OnConfigExit), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btnclose");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnConfigExit), NULL, NULL, G_CONNECT_AFTER);

	PopulateDevList();
	UpdateKeyList();

	widget = glade_xml_get_widget(xml, "checkmt");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), g.cfg.Threaded);
	g_signal_connect_data(GTK_OBJECT(widget), "toggled",
		GTK_SIGNAL_FUNC(OnThreadedToggled), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "combodev1");
	g_signal_connect_data(GTK_OBJECT(widget), "changed",
		GTK_SIGNAL_FUNC(OnDeviceChanged), (gpointer)0, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "combodev2");
	g_signal_connect_data(GTK_OBJECT(widget), "changed",
		GTK_SIGNAL_FUNC(OnDeviceChanged), (gpointer)1, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "combotype1");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget),
		g.cfg.PadDef[0].Type == PSE_PAD_TYPE_ANALOGPAD ? 1 : 0);
	g_signal_connect_data(GTK_OBJECT(widget), "changed",
		GTK_SIGNAL_FUNC(OnTypeChanged), (gpointer)0, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "combotype2");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget),
		g.cfg.PadDef[1].Type == PSE_PAD_TYPE_ANALOGPAD ? 1 : 0);
	g_signal_connect_data(GTK_OBJECT(widget), "changed",
		GTK_SIGNAL_FUNC(OnTypeChanged), (gpointer)1, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btnchange1");
	gtk_widget_set_sensitive(widget, FALSE);
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnChangeClicked), (gpointer)0, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btnreset1");
	gtk_widget_set_sensitive(widget, FALSE);
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnResetClicked), (gpointer)0, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btnchange2");
	gtk_widget_set_sensitive(widget, FALSE);
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnChangeClicked), (gpointer)1, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "btnreset2");
	gtk_widget_set_sensitive(widget, FALSE);
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnResetClicked), (gpointer)1, NULL, G_CONNECT_AFTER);

	gtk_widget_show(MainWindow);
	gtk_main();

	return 0;
}

void PADabout() {
	const char *authors[]= {"Wei Mingzhi <weimingzhi@gmail.com>", NULL};
	GtkWidget *widget;

	widget = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(widget), "Gamepad/Keyboard Input");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(widget), "1.1");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(widget), authors);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(widget), "http://www.codeplex.com/pcsxr/");

	gtk_dialog_run(GTK_DIALOG(widget));
	gtk_widget_destroy(widget);
}

int main(int argc, char *argv[]) {
#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	gtk_set_locale();
	gtk_init(&argc, &argv);

	if (argc > 1 && !strcmp(argv[1], "-about")) {
		PADabout();
	} else {
		PADconfigure();
	}

	gtk_exit(0);
	return 0;
}

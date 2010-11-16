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

#include "Linux.h"
#include <gtk/gtk.h>

#if GTK_MAJOR_VERSION <= 2 && GTK_MINOR_VERSION < 12
#define gtk_about_dialog_set_program_name gtk_about_dialog_set_name
#endif

#define ABOUT_VERSION "1.9.92"

void RunAboutDialog(void) {
	GtkWidget *AboutDlg;

	const gchar *authors[] = {
		"PCSX-Reloaded Team:",
		"edgbla <edgbla@yandex.ru>",
		"Wei Mingzhi <whistler_wmz@users.sf.net>",
		"",
		"Contributors:",
		"Dario",
		"Firnis",
		"Gabriele Gorla",
		"Peter Collingbourne",
		"shalma",
		"Tristin Celestin",
		"(See the included AUTHORS file for more details.)",
		"",
		"PCSX-df Team:",
		"Ryan Schultz <schultz.ryan@gmail.com>",
		"Andrew Burton <adb@iinet.net.au>",
		"Stephen Chao <stephen@digitalnexus.org>",
		"Stefan Sikora <hoshy[AT]schrauberstube.de>",
		"",
		"PCSX Team:",
		"Linuzappz <linuzappz@hotmail.com>",
		"Shadow",
		"Pete Bernert",
		"NoComp",
		"Nik3d",
		NULL
	};

	const gchar *artists[] = {
		"Ryan Schultz <schultz.ryan@gmail.com>",
		"",
		"Icon Design:",
		"Romain Lafourcade",
		NULL
	};

	const gchar *documenters[] = {
		"Ryan Schultz <schultz.ryan@gmail.com>",
		NULL
	};

	const gchar *copyright = N_(
		"(C) 1999-2003 PCSX Team\n"
		"(C) 2005-2009 PCSX-df Team\n"
		"(C) 2009-2010 PCSX-Reloaded Team");

	const gchar *license = N_(
		"This program is free software; you can redistribute it and/or modify "
		"it under the terms of the GNU General Public License as published by "
		"the Free Software Foundation; either version 2 of the License, or "
		"(at your option) any later version.\n"
		"\n"
		"This program is distributed in the hope that it will be useful, "
		"but WITHOUT ANY WARRANTY; without even the implied warranty of "
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		"GNU General Public License for more details.\n"
		"\n"
		"You should have received a copy of the GNU General Public License "
		"along with this program; if not, write to the Free Software "
		"Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.");

	AboutDlg = gtk_about_dialog_new();
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(AboutDlg), "PCSX-Reloaded");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(AboutDlg), ABOUT_VERSION);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(AboutDlg), "http://pcsxr.codeplex.com/");
	gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(AboutDlg), "http://pcsxr.codeplex.com/");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(AboutDlg), authors);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(AboutDlg), _(copyright));
	gtk_about_dialog_set_documenters(GTK_ABOUT_DIALOG(AboutDlg), documenters);
	gtk_about_dialog_set_artists(GTK_ABOUT_DIALOG(AboutDlg), artists);
	gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(AboutDlg), _("translator-credits"));
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG (AboutDlg), _("A PlayStation emulator."));
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(AboutDlg), _(license));
	gtk_about_dialog_set_wrap_license(GTK_ABOUT_DIALOG(AboutDlg), TRUE);

	gtk_dialog_run(GTK_DIALOG(AboutDlg));
	gtk_widget_destroy(AboutDlg);
}

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glade/glade.h>
#include <gtk/gtk.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#endif

#define READBINARY "rb"
#define WRITEBINARY "wb"
#define CONFIG_FILENAME "dfsound.cfg"

void SaveConfig(GtkWidget *widget, gpointer user_datal);

/*	This function checks for the value being outside the accepted range,
	and returns the appropriate boundary value */
int set_limit (char *p, int len, int lower, int upper)
{
	int val = 0;

	if (p)
	    val = atoi(p + len);

	if (val < lower)
	    val = lower;
	if (val > upper)
	    val = upper;

    return val;
}

void on_about_clicked (GtkWidget *widget, gpointer user_data)
{
	gtk_widget_destroy (widget);
	exit (0);
}

void OnConfigClose(GtkWidget *widget, gpointer user_data)
{
	GladeXML *xml = (GladeXML *)user_data;

	gtk_widget_destroy(glade_xml_get_widget(xml, "CfgWnd"));
	gtk_exit(0);
}

int main(int argc, char *argv[])
{
    GtkWidget *widget;
    GladeXML *xml;
    FILE *in;
    char t[256];
    int len, val = 0;
    char *pB, *p;
    char cfg[255];

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    if (argc != 2) {
    	printf ("Usage: cfgDFSound {ABOUT | CFG}\n");
		return 0;
	}

    if (strcmp(argv[1], "CFG") != 0 && strcmp(argv[1], "ABOUT") != 0) {
		printf ("Usage: cfgDFSound {ABOUT | CFG}\n");
		return 0;
    }

    gtk_set_locale();
    gtk_init(&argc, &argv);

    if (strcmp(argv[1], "ABOUT") == 0) {
		const char *authors[]= {"Pete Bernert and the P.E.Op.S. team", "Ryan Schultz", "Andrew Burton", NULL};
		widget = gtk_about_dialog_new ();
		gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (widget), "dfsound PCSX Sound Plugin");
		gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (widget), "1.6");
		gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (widget), authors);
		gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (widget), "http://pcsx-df.sourceforge.net/");

		g_signal_connect_data(GTK_OBJECT(widget), "response",
			GTK_SIGNAL_FUNC(on_about_clicked), NULL, NULL, G_CONNECT_AFTER);

		gtk_widget_show (widget);
		gtk_main();

		return 0;
    }

    xml = glade_xml_new(DATADIR "dfsound.glade2", "CfgWnd", NULL);
    if (!xml) {
		g_warning("We could not load the interface!");
		return 255;
    }

    strcpy(cfg, CONFIG_FILENAME);

    in = fopen(cfg, READBINARY);
    if (in) {
		pB = (char *)malloc(32767);
		memset(pB, 0, 32767);
		len = fread(pB, 1, 32767, in);
		fclose(in);
    } else {
		pB = 0;
		printf ("Error - no configuration file\n");
		/* TODO Raise error - no configuration file */
    }

	/* ADB TODO Replace a lot of the following with common functions */
    if (pB) {
		strcpy(t, "\nVolume");
		p = strstr(pB, t);
		if (p) {
		    p = strstr(p, "=");
	    	len = 1;
		}
	    val = set_limit (p, len, 0, 4);
    } else val = 2;

    gtk_combo_box_set_active(GTK_COMBO_BOX (glade_xml_get_widget(xml, "cbVolume2")), val);

    if (pB) {
	strcpy(t, "\nUseInterpolation");
	p = strstr(pB, t);
	if (p) {
	    p = strstr(p, "=");
	    len = 1;
	}
	    val = set_limit (p, len, 0, 3);
    } else val = 2;

    gtk_combo_box_set_active(GTK_COMBO_BOX (glade_xml_get_widget(xml, "cbInterpolation2")), val);

    if (pB) {
		strcpy(t, "\nXAPitch");
		p = strstr(pB, t);
		if (p) {
		    p = strstr(p, "=");
	    	len = 1;
		}
		val = set_limit (p, len, 0, 1);
    } else val = 0;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (glade_xml_get_widget(xml, "chkXASpeed")), val);

    if (pB) {
		strcpy(t, "\nHighCompMode");
		p = strstr(pB, t);
		if (p) {
		    p = strstr(p, "=");
	    	len = 1;
		}
		val = set_limit (p, len, 0, 1);
    } else val = 0;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (glade_xml_get_widget(xml, "chkHiCompat")), val);

    if (pB) {
		strcpy(t, "\nSPUIRQWait");
		p = strstr(pB, t);
		if (p) {
		    p = strstr(p, "=");
		    len = 1;
		}

		val = set_limit (p, len, 0, 1);
    } else val = 1;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (glade_xml_get_widget(xml, "chkIRQWait")), val);

    if (pB) {
		strcpy(t, "\nDisStereo");
		p = strstr(pB, t);
		if (p) {
		    p = strstr(p, "=");
		    len = 1;
		}

		val = set_limit (p, len, 0, 1);
    } else val = 0;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (glade_xml_get_widget(xml, "chkDisStereo")), val);

    if (pB) {
		strcpy(t, "\nUseReverb");
		p = strstr(pB, t);
		if (p) {
		    p = strstr(p, "=");
		    len = 1;
		}
		val = set_limit (p, len, 0, 2);
    } else val = 2;

    gtk_combo_box_set_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "cbReverb2")), val);

    if (pB)
		free(pB);

	widget = glade_xml_get_widget(xml, "CfgWnd");
	g_signal_connect_data(GTK_OBJECT(widget), "destroy",
		GTK_SIGNAL_FUNC(SaveConfig), xml, NULL, 0);

	widget = glade_xml_get_widget(xml, "btn_close");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnConfigClose), xml, NULL, G_CONNECT_AFTER);

    gtk_main();
    return 0;
}

void SaveConfig(GtkWidget *widget, gpointer user_data)
{
	GladeXML *xml = (GladeXML *)user_data;
	FILE *fp;
	int val;

	fp = fopen(CONFIG_FILENAME, WRITEBINARY);
	if (fp == NULL) {
		fprintf(stderr, "Unable to write to configuration file %s!\n", CONFIG_FILENAME);
		gtk_exit(0);
	}

	val = gtk_combo_box_get_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "cbVolume2")));
	fprintf(fp, "\nVolume = %d\n", val);

	val = gtk_combo_box_get_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "cbInterpolation2")));
	fprintf(fp, "\nUseInterpolation = %d\n", val);

	val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "chkXASpeed")));
	fprintf(fp, "\nXAPitch = %d\n", val);

	val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "chkHiCompat")));
	fprintf(fp, "\nHighCompMode = %d\n", val);

	val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "chkIRQWait")));
	fprintf(fp, "\nSPUIRQWait = %d\n", val);

	val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "chkDisStereo")));
	fprintf(fp, "\nDisStereo = %d\n", val);

	val = gtk_combo_box_get_active(GTK_COMBO_BOX(glade_xml_get_widget(xml, "cbReverb2")));
	fprintf(fp, "\nUseReverb = %d\n", val);

	fclose(fp);
	gtk_exit(0);
}

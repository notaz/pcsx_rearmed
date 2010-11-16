//
// DF Netplay Plugin
//
// Based on netSock 0.2 by linuzappz.
// The Plugin is free source code.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <signal.h>

#include "cfg.c"

void cfgSysMessage(const char *fmt, ...) {
	GtkWidget *MsgDlg;
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg) - 1] == '\n') msg[strlen(msg) - 1] = 0;

	MsgDlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("NetPlay"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(MsgDlg), "%s", msg);

	gtk_dialog_run(GTK_DIALOG(MsgDlg));
	gtk_widget_destroy(MsgDlg);
}

void CFGconfigure() {
	cfgSysMessage(_("Nothing to configure"));
}

#ifdef __linux__

#include <sys/ioctl.h>
#include <linux/if.h>

#define MAXINTERFACES 16

void sockGetIP(char *IPAddress) {
	int fd, intrface;
	struct ifreq buf[MAXINTERFACES];
	struct ifconf ifc;
	struct sockaddr_in addr;

	strcpy(IPAddress, "127.0.0.1");

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = (caddr_t)buf;
		if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc)) {
			intrface = ifc.ifc_len / sizeof(struct ifreq);
			while (intrface-- > 0) {
				if (!(ioctl(fd, SIOCGIFADDR, (char *)&buf[intrface]))) {
					memcpy(&addr, &(buf[intrface].ifr_addr), sizeof(addr));
					strcpy(IPAddress, inet_ntoa(addr.sin_addr));
					break;
				}
			}
		}
		close(fd);
	}
}

#else

void sockGetIP(char *IPAddress) {
	struct hostent *host;
	char str[256];

	gethostname(str, 256);
	host = gethostbyname(str);

	if (host != NULL)
		strcpy(IPAddress, inet_ntoa(*((struct in_addr *)host->h_addr_list[0])));
	else strcpy(IPAddress, "127.0.0.1");
}

#endif

void OnCopyIP(GtkWidget *widget, gpointer user_data) {
	char str[256];

	sockGetIP(str);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), str, strlen(str));
	cfgSysMessage(_("IP %s"), str);
}

long CFGopen() {
	GladeXML *xml;
	GtkWidget *widget, *MainWindow;
	char buf[256];

	LoadConf();

	xml = glade_xml_new(DATADIR "dfnet.glade2", "dlgStart", NULL);
	if (xml == NULL) {
		g_warning("We could not load the interface!");
		return 0;
	}

	MainWindow = glade_xml_get_widget(xml, "dlgStart");
	gtk_window_set_title(GTK_WINDOW(MainWindow), _("NetPlay"));

	widget = glade_xml_get_widget(xml, "btnCopyIP");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnCopyIP), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "tbServerIP");
	gtk_entry_set_text(GTK_ENTRY(widget), conf.ipAddress);

	widget = glade_xml_get_widget(xml, "tbPort");
	sprintf(buf, "%d", conf.PortNum);
	gtk_entry_set_text(GTK_ENTRY(widget), buf);

	if (conf.PlayerNum == 1) {
		widget = glade_xml_get_widget(xml, "rbServer");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	} else {
		widget = glade_xml_get_widget(xml, "rbClient");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	}

	if (gtk_dialog_run(GTK_DIALOG(MainWindow)) == GTK_RESPONSE_OK) {
		widget = glade_xml_get_widget(xml, "tbServerIP");
		strcpy(conf.ipAddress, gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = glade_xml_get_widget(xml, "tbPort");
		conf.PortNum = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = glade_xml_get_widget(xml, "rbServer");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
			conf.PlayerNum = 1;
		} else {
			conf.PlayerNum = 2;
		}

		SaveConf();
		gtk_widget_destroy(MainWindow);
		return 1;
	}

	gtk_widget_destroy(MainWindow);

	return 0;
}

void OnWaitDialog_Abort() {
	kill(getppid(), SIGUSR2);
}

void CFGwait() {
	GtkWidget *WaitDlg;

	WaitDlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
		GTK_BUTTONS_CANCEL, _("Waiting for connection..."));

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(WaitDlg),
		_("The Client should now Start a Connection, waiting..."));

	gtk_dialog_run(GTK_DIALOG(WaitDlg));
	gtk_widget_destroy(WaitDlg);

	OnWaitDialog_Abort();
}

long CFGpause() {
	return 0;
}

void CFGabout() {
	const char *authors[]= {"linuzappz <linuzappz@hotmail.com>", "Wei Mingzhi <whistler_wmz@users.sf.net>", NULL};
	GtkWidget *widget;

	widget = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(widget), "Socket NetPlay Driver");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(widget), "0.21");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(widget), authors);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(widget), "http://www.codeplex.com/pcsxr/");

	gtk_dialog_run(GTK_DIALOG(widget));
	gtk_widget_destroy(widget);
}

long CFGmessage(char *args[], int num) {
	char msg[512];

	memset(msg, 0, sizeof(msg));
	while (num) {
		strcat(msg, *args); strcat(msg, " ");
		num--; args++;
	}
	cfgSysMessage(msg);

	return 0;
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

	if (!strcmp(argv[1], "configure")) {
		CFGconfigure();
	} else if (!strcmp(argv[1], "open")) {
		return CFGopen();
	} else if (!strcmp(argv[1], "wait")) {
		CFGwait();
	} else if (!strcmp(argv[1], "pause")) {
		return CFGpause();
	} else if (!strcmp(argv[1], "about")) {
		CFGabout();
	} else if (!strcmp(argv[1], "message")) {
		CFGmessage(&argv[2], argc - 2);
	}

	return 0;
}

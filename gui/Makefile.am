INCLUDES = -DPACKAGE_DATA_DIR=\"${datadir}/pcsx/\" \
	-DPIXMAPDIR=\"${datadir}/pixmaps/\" \
	-DLOCALE_DIR=\"${datadir}/locale/\" \
	$(GTK2_CFLAGS) $(GLADE2_CFLAGS) \
	-I$(top_srcdir)/libpcsxcore -I$(top_srcdir)/include \
	-DPSEMU_DATA_DIR=\"${datadir}/psemu\" \
	-DDEF_PLUGIN_DIR=\"${libdir}/games/psemu\"

bin_PROGRAMS = pcsx

pcsx_SOURCES = \
	LnxMain.c	\
	Plugin.c	\
	Config.c	\
	Gtk2Gui.c	\
	MemcardDlg.c	\
	ConfDlg.c	\
	Cheat.c		\
	DebugMemory.c	\
	AboutDlg.c

pcsx_LDADD = \
	$(GTK2_LIBS) $(GLADE2_LIBS) -lpthread -lz -lm -lXext -lXtst \
	../libpcsxcore/libpcsxcore.a

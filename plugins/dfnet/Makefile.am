bindir = @libdir@/games/psemu/
libdir = @libdir@/games/psemu/

lib_LTLIBRARIES = libDFNet.la

libDFNet_la_SOURCES = dfnet.c unix.c cfg.c
libDFNet_la_LDFLAGS = -module -avoid-version

INCLUDES = -DPIXMAPDIR=\"${datadir}/pixmaps/\" \
	-DLOCALE_DIR=\"${datadir}/locale/\" \
	-DDATADIR=\"${datadir}/psemu/\" \
	$(GTK2_CFLAGS) $(GLADE2_CFLAGS) \
	-I../../libpcsxcore -I../../include

bin_PROGRAMS = cfgDFNet
cfgDFNet_SOURCES = gui.c
cfgDFNet_LDADD = $(GTK2_LIBS) $(GLADE2_LIBS)

glade_DATA = dfnet.glade2
gladedir = $(datadir)/psemu/
EXTRA_DIST = $(glade_DATA)

bindir = @libdir@/games/psemu/
libdir = @libdir@/games/psemu/

lib_LTLIBRARIES = libDFCdrom.la

libDFCdrom_la_SOURCES = \
	cdr.c \
	cdr.h \
	cfg.c \
	cdr-linux.c \
	cdr-null.c \
	cdr-libcdio.c \
	util.c

libDFCdrom_la_CFLAGS =
libDFCdrom_la_LDFLAGS = -module -avoid-version -lpthread

if USE_LIBCDIO
libDFCdrom_la_CFLAGS += -DUSE_LIBCDIO=1 $(LIBCDIO_CFLAGS)
libDFCdrom_la_LDFLAGS += $(LIBCDIO_LIBS)
endif

INCLUDES = -DPIXMAPDIR=\"${datadir}/pixmaps/\" \
	-DLOCALE_DIR=\"${datadir}/locale/\" \
	-DDATADIR=\"${datadir}/psemu/\" \
	$(GTK2_CFLAGS) $(GLADE2_CFLAGS) \
	-I../../libpcsxcore -I../../include

bin_PROGRAMS = cfgDFCdrom
cfgDFCdrom_SOURCES = cdrcfg-0.1df/main.c
cfgDFCdrom_LDADD = $(GTK2_LIBS) $(GLADE2_LIBS)

glade_DATA = cdrcfg-0.1df/dfcdrom.glade2
gladedir = $(datadir)/psemu/
EXTRA_DIST = $(glade_DATA)

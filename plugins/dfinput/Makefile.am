bindir = @libdir@/games/psemu/
libdir = @libdir@/games/psemu/

lib_LTLIBRARIES = libDFInput.la

libDFInput_la_SOURCES = cfg.c pad.c pad.h sdljoy.c xkb.c analog.c
libDFInput_la_LDFLAGS = -module -avoid-version -lpthread -lX11 \
	`$(SDL_CONFIG) --libs` -L/usr/X11R6/lib64 -L/usr/X11R6/lib

INCLUDES = -DPIXMAPDIR=\"${datadir}/pixmaps/\" \
	-DLOCALE_DIR=\"${datadir}/locale/\" \
	-DDATADIR=\"${datadir}/psemu/\" \
	$(GTK2_CFLAGS) $(GLADE2_CFLAGS) -I/usr/X11R6/include \
	-I../../include -I../../libpcsxcore $(SDL_CFLAGS)

bin_PROGRAMS = cfgDFInput
cfgDFInput_SOURCES = cfg-gtk2.c pad.h
cfgDFInput_LDADD = $(GTK2_LIBS) $(GLADE2_LIBS)
cfgDFInput_LDFLAGS = $(SDL_LIBS) -L/usr/X11R6/lib64 -L/usr/X11R6/lib -lX11

glade_DATA = dfinput.glade2
gladedir = $(datadir)/psemu/
EXTRA_DIST = $(glade_DATA)

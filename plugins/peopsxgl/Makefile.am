INCLUDES = -DPIXMAPDIR=\"${datadir}/pixmaps/\" \
	-DLOCALE_DIR=\"${datadir}/locale/\" \
	-DDATADIR=\"${datadir}/psemu/\" \
	$(GTK2_CFLAGS) $(GLADE2_CFLAGS) -I/usr/X11R6/include \
	-I../../libpcsxcore -I../../include -fPIC

bindir = @libdir@/games/psemu/
libdir = @libdir@/games/psemu/

lib_LTLIBRARIES = libpeopsxgl.la

libpeopsxgl_la_SOURCES = cfg.c draw.c fps.c gpu.c key.c menu.c \
	prim.c soft.c texture.c
libpeopsxgl_la_LDFLAGS = -module -avoid-version \
	-L/usr/X11R6/lib64 -L/usr/X11R6/lib -lX11 -lXxf86vm -lGL -lm

bin_PROGRAMS = cfgpeopsxgl
cfgpeopsxgl_SOURCES = gpucfg/main.c gpucfg/callbacks.c gpucfg/interface.c \
	gpucfg/support.c
cfgpeopsxgl_LDADD = $(GTK2_LIBS) $(GLADE2_LIBS)

#glade_DATA = gpucfg-0.1df/peopsxgl.glade2
#gladedir = $(datadir)/psemu/
#EXTRA_DIST = $(glade_DATA)

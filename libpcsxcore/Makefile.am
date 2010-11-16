INCLUDES = -DLOCALE_DIR=\"${datadir}/locale/\" \
	-I$(top_srcdir)/include

noinst_LIBRARIES = libpcsxcore.a

libpcsxcore_a_SOURCES = \
	$(top_builddir)/libpcsxcore/psxbios.c	\
	$(top_builddir)/libpcsxcore/cdrom.c	\
	$(top_builddir)/libpcsxcore/psxcounters.c	\
	$(top_builddir)/libpcsxcore/psxdma.c	\
	$(top_builddir)/libpcsxcore/disr3000a.c	\
	$(top_builddir)/libpcsxcore/spu.c	\
	$(top_builddir)/libpcsxcore/sio.c	\
	$(top_builddir)/libpcsxcore/psxhw.c	\
	$(top_builddir)/libpcsxcore/mdec.c	\
	$(top_builddir)/libpcsxcore/psxmem.c	\
	$(top_builddir)/libpcsxcore/misc.c	\
	$(top_builddir)/libpcsxcore/plugins.c	\
	$(top_builddir)/libpcsxcore/decode_xa.c	\
	$(top_builddir)/libpcsxcore/r3000a.c	\
	$(top_builddir)/libpcsxcore/psxinterpreter.c	\
	$(top_builddir)/libpcsxcore/gte.c	\
	$(top_builddir)/libpcsxcore/psxhle.c	\
	$(top_builddir)/libpcsxcore/cdrom.h \
	$(top_builddir)/libpcsxcore/coff.h \
	$(top_builddir)/libpcsxcore/debug.c \
	$(top_builddir)/libpcsxcore/debug.h \
	$(top_builddir)/libpcsxcore/decode_xa.h \
	$(top_builddir)/libpcsxcore/gte.h \
	$(top_builddir)/libpcsxcore/gte_divider.h \
	$(top_builddir)/libpcsxcore/mdec.h \
	$(top_builddir)/libpcsxcore/misc.h \
	$(top_builddir)/libpcsxcore/plugins.h \
	$(top_builddir)/libpcsxcore/psemu_plugin_defs.h \
	$(top_builddir)/libpcsxcore/psxbios.h \
	$(top_builddir)/libpcsxcore/psxcommon.c \
	$(top_builddir)/libpcsxcore/psxcommon.h \
	$(top_builddir)/libpcsxcore/psxcounters.h \
	$(top_builddir)/libpcsxcore/psxdma.h \
	$(top_builddir)/libpcsxcore/psxhle.h \
	$(top_builddir)/libpcsxcore/psxhw.h \
	$(top_builddir)/libpcsxcore/psxmem.h \
	$(top_builddir)/libpcsxcore/r3000a.h \
	$(top_builddir)/libpcsxcore/sio.h \
	$(top_builddir)/libpcsxcore/sjisfont.h \
	$(top_builddir)/libpcsxcore/spu.h \
	$(top_builddir)/libpcsxcore/system.h \
	$(top_builddir)/libpcsxcore/cdriso.c \
	$(top_builddir)/libpcsxcore/cdriso.h \
	$(top_builddir)/libpcsxcore/cheat.c \
	$(top_builddir)/libpcsxcore/cheat.h \
	$(top_builddir)/libpcsxcore/socket.c \
	$(top_builddir)/libpcsxcore/socket.h \
	$(top_builddir)/libpcsxcore/ppf.c \
	$(top_builddir)/libpcsxcore/ppf.h

if ARCH_X86_64
libpcsxcore_a_SOURCES += \
	$(top_builddir)/libpcsxcore/ix86_64/iR3000A-64.c	\
	$(top_builddir)/libpcsxcore/ix86_64/ix86-64.c	\
	$(top_builddir)/libpcsxcore/ix86_64/ix86_cpudetect.c	\
	$(top_builddir)/libpcsxcore/ix86_64/ix86_fpu.c	\
	$(top_builddir)/libpcsxcore/ix86_64/ix86_3dnow.c	\
	$(top_builddir)/libpcsxcore/ix86_64/ix86_mmx.c	\
	$(top_builddir)/libpcsxcore/ix86_64/ix86_sse.c	\
	$(top_builddir)/libpcsxcore/
else		
if ARCH_X86
libpcsxcore_a_SOURCES += \
	$(top_builddir)/libpcsxcore/ix86/iR3000A.c	\
	$(top_builddir)/libpcsxcore/ix86/ix86.c	\
	$(top_builddir)/libpcsxcore/
endif
endif

if ARCH_PPC
libpcsxcore_a_SOURCES += \
	$(top_builddir)/libpcsxcore/ppc/pR3000A.c	\
	$(top_builddir)/libpcsxcore/ppc/ppc.c	\
	$(top_builddir)/libpcsxcore/ppc/reguse.c	\
	$(top_builddir)/libpcsxcore/ppc/pasm.s	\
	$(top_builddir)/libpcsxcore/
libpcsxcore_a_CCASFLAGS = -x assembler-with-cpp -mregnames
endif

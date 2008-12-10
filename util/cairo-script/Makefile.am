SUBDIRS = examples

lib_LTLIBRARIES = libcairo-script-interpreter.la
noinst_PROGRAMS = csi-replay csi-exec

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src

cairoincludedir=$(includedir)/cairo
cairoinclude_HEADERS = cairo-script-interpreter.h
libcairo_script_interpreter_la_SOURCES = \
	cairo-script-private.h \
	cairo-script-file.c \
	cairo-script-hash.c \
	cairo-script-interpreter.c \
	cairo-script-objects.c \
	cairo-script-operators.c \
	cairo-script-scanner.c \
	cairo-script-stack.c \
	$(NULL)
libcairo_script_interpreter_la_CFLAGS = $(CAIRO_CFLAGS)
libcairo_script_interpreter_la_LDFLAGS = -version-info $(CAIRO_LIBTOOL_VERSION_INFO) -no-undefined $(export_symbols)
libcairo_script_interpreter_la_LIBADD = $(top_builddir)/src/libcairo.la $(CAIRO_LIBS)

csi_replay_SOURCES = csi-replay.c
csi_replay_LDADD = libcairo-script-interpreter.la $(top_builddir)/src/libcairo.la $(CAIRO_LIBS)

csi_exec_SOURCES = csi-exec.c
csi_exec_LDADD = libcairo-script-interpreter.la $(top_builddir)/src/libcairo.la $(CAIRO_LIBS)

EXTRA_DIST = \
	COPYING

lib_LTLIBRARIES = libcairo-script-interpreter.la

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
libcairo_script_interpreter_la_CPPFLAGS = -I$(top_srcdir)/src
libcairo_script_interpreter_la_CFLAGS = $(CAIRO_CFLAGS)
libcairo_script_interpreter_la_LDFLAGS = -version-info $(CAIRO_LIBTOOL_VERSION_INFO) -no-undefined $(export_symbols)
libcairo_script_interpreter_la_LIBADD = -lz $(CAIRO_LIBS) -L$(top_builddir)/src -lcairo

EXTRA_DIST = \
	COPYING

cairolibdir = $(libdir)/cairo
cairooutdir = $(localstatedir)/lib/cairo-trace

bin_SCRIPTS = cairo-trace
cairolib_LTLIBRARIES = cairo-trace.la

AM_CPPFLAGS = -I$(top_srcdir)/src \
	      -I$(top_builddir)/src

cairo_trace_la_SOURCES = trace.c
cairo_trace_la_CPPFLAGS = -DCAIRO_TRACE_OUTDIR="\"$(cairooutdir)\"" \
			  $(AM_CPPFLAGS)
cairo_trace_la_CFLAGS = $(CAIRO_CFLAGS)
cairo_trace_la_LDFLAGS = -module -no-undefined

cairo_trace_la_LIBADD = -lz $(pthread_LIBS)
if CAIRO_HAS_DL
cairo_trace_la_LIBADD += -ldl
endif

if CAIRO_HAS_SYMBOL_LOOKUP
cairo_trace_la_SOURCES += \
			 lookup-symbol.c \
			 lookup-symbol.h
cairo_trace_la_LIBADD += $(BFD_LIBS)
endif


system-install: install
	-mkdir -p $(cairooutdir)
	-chmod 01777 $(cairooutdir)
	grep -sq $(cairolibdir)/cairo-trace.so /etc/ld.so.preload || echo $(cairolibdir)/cairo-trace.so >> /etc/ld.so.preload

system-uninstall: uninstall
	sed -e '/cairo-trace.so/d' < /etc/ld.so.preload > /tmp/ld.so.preload && mv /tmp/ld.so.preload /etc/ld.so.preload;

EXTRA_DIST = \
	COPYING \
	COPYING-GPL-3 \
	cairo-trace.in

# Note: All source files are listed in Makefile.sources.

include $(top_srcdir)/build/Makefile.am.common
include $(srcdir)/Makefile.am.features

EXTRA_DIST += Makefile.win32      Makefile.win32.features
#MAINTAINERCLEANFILES += $(srcdir)/Makefile.win32.features

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	$(CAIRO_CFLAGS) \
	$(NULL)
AM_LDFLAGS = $(CAIRO_LDFLAGS)

EXTRA_LTLIBRARIES += libcairoboilerplate.la

libcairoboilerplate_la_SOURCES = \
	$(enabled_cairo_boilerplate_headers) \
	$(enabled_cairo_boilerplate_private) \
	$(enabled_cairo_boilerplate_sources) \
	cairo-boilerplate-constructors.c \
	$(NULL)
libcairoboilerplate_la_LIBADD = $(top_builddir)/src/libcairo.la
if CAIRO_HAS_DL
libcairoboilerplate_la_LIBADD += -ldl
endif

if CAIRO_HAS_BEOS_SURFACE
# BeOS system headers trigger this warning
AM_CPPFLAGS += -Wno-multichar
endif

if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
libcairoboilerplate_la_LIBADD += $(GLITZ_AGL_LIBS)
endif
if CAIRO_CAN_TEST_GLITZ_EGL_SURFACE
libcairoboilerplate_la_LIBADD += $(GLITZ_EGL_LIBS)
endif
if CAIRO_CAN_TEST_GLITZ_GLX_SURFACE
libcairoboilerplate_la_LIBADD += $(GLITZ_GLX_LIBS)
endif
if CAIRO_CAN_TEST_GLITZ_WGL_SURFACE
libcairoboilerplate_la_LIBADD += $(GLITZ_WGL_LIBS)
endif

if CAIRO_HAS_WIN32_SURFACE
libcairoboilerplate_la_LIBADD += -lwinspool
endif

libcairoboilerplate_la_LIBADD += $(CAIROBOILERPLATE_LIBS)

cairo-boilerplate-constructors.c: Makefile $(enabled_cairo_boilerplate_sources) make-cairo-boilerplate-constructors.sh
	(cd $(srcdir) && sh ./make-cairo-boilerplate-constructors.sh $(enabled_cairo_boilerplate_sources)) > $@

BUILT_SOURCES += cairo-boilerplate-constructors.c
EXTRA_DIST += $(BUILT_SOURCES) make-cairo-boilerplate-constructors.sh
CLEANFILES += $(BUILT_SOURCES)

test: check

if CROSS_COMPILING
else
TESTS += check-link$(EXEEXT)
endif

check_PROGRAMS += check-link
check_link_LDADD = libcairoboilerplate.la

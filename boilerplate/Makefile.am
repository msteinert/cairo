EXTRA_LTLIBRARIES = libcairoboilerplate.la

libcairoboilerplate_la_SOURCES =\
cairo-boilerplate.c	\
cairo-boilerplate.h	\
xmalloc.c		\
xmalloc.h
libcairoboilerplate_la_LIBADD = $(top_builddir)/src/libcairo.la $(CAIRO_LDADD)

if CAIRO_HAS_BEOS_SURFACE
libcairoboilerplate_la_SOURCES += cairo-boilerplate-beos.cpp
libcairoboilerplate_la_SOURCES += cairo-boilerplate-beos-private.h
# BeOS system headers trigger this warning
libcairoboilerplate_la_CXXFLAGS = -Wno-multichar
endif

if CAIRO_HAS_DIRECTFB_SURFACE
libcairoboilerplate_la_SOURCES += cairo-boilerplate-directfb.c
libcairoboilerplate_la_SOURCES += cairo-boilerplate-directfb-private.h
endif

if CAIRO_HAS_PDF_SURFACE
libcairoboilerplate_la_SOURCES += cairo-boilerplate-pdf.c
libcairoboilerplate_la_SOURCES += cairo-boilerplate-pdf-private.h
endif

if CAIRO_HAS_PS_SURFACE
libcairoboilerplate_la_SOURCES += cairo-boilerplate-ps.c
libcairoboilerplate_la_SOURCES += cairo-boilerplate-ps-private.h
endif

if CAIRO_HAS_QUARTZ_SURFACE
libcairoboilerplate_la_SOURCES += cairo-boilerplate-quartz.c
libcairoboilerplate_la_SOURCES += cairo-boilerplate-quartz-private.h
endif

if CAIRO_HAS_SVG_SURFACE
libcairoboilerplate_la_SOURCES += cairo-boilerplate-svg.c
libcairoboilerplate_la_SOURCES += cairo-boilerplate-svg-private.h
endif

if CAIRO_HAS_XLIB_XRENDER_SURFACE
libcairoboilerplate_la_SOURCES += cairo-boilerplate-xlib.c
libcairoboilerplate_la_SOURCES += cairo-boilerplate-xlib-private.h
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

# We're using _GNU_SOURCE to get the prototype for asprintf. This may
# not be the most portable approach, but it is pragmatic and I'm
# willing to do something cleaner as soon as it causes someone a
# problem.
INCLUDES =					\
	-D_GNU_SOURCE				\
	-I$(srcdir)				\
	-I$(top_srcdir)/pixman/src		\
	-I$(top_builddir)/src			\
	-I$(top_srcdir)/src			\
	$(CAIRO_CFLAGS)

CLEANFILES =					\
	$(EXTRA_LTLIBRARIES)

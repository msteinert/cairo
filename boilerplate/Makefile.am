noinst_LTLIBRARIES = libcairotest.la

libcairotest_la_SOURCES =\
cairo-boilerplate.c	\
cairo-bolierplate.h	\
buffer-diff.c		\
buffer-diff.h		\
cairo-test.c		\
cairo-test.h		\
read-png.c		\
read-png.h		\
write-png.c		\
write-png.h		\
xmalloc.c		\
xmalloc.h

if CAIRO_HAS_BEOS_SURFACE
libcairotest_la_SOURCES += cairo-test-beos.cpp cairo-test-beos.h
# BeOS system headers trigger this warning
libcairotest_la_CXXFLAGS = -Wno-multichar
endif

if CAIRO_HAS_DIRECTFB_SURFACE
libcairotest_la_SOURCES += cairo-test-directfb.c cairo-test-directfb.h
endif

# We're using _GNU_SOURCE to get the prototype for asprintf. This may
# not be the most portable approach, but it is pragmatic and I'm
# willing to do something cleaner as soon as it causes someone a
# problem.
INCLUDES =					\
	-D_GNU_SOURCE				\
	-I$(srcdir)				\
	-I$(top_srcdir)/pixman/src		\
	-I$(top_srcdir)/src			\
	$(CAIRO_CFLAGS)

# We're using _GNU_SOURCE to get the prototype for asprintf. This may
# not be the most portable approach, but it is pragmatic and I'm
# willing to do something cleaner as soon as it causes someone a
# problem.
INCLUDES =					\
	-D_GNU_SOURCE				\
	-I$(srcdir)				\
	-I$(top_srcdir)/boilerplate		\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	$(CAIRO_CFLAGS)

noinst_PROGRAMS = cairo-perf cairo-perf-diff-files
EXTRA_DIST = cairo-perf-diff

cairo_perf_SOURCES =		\
	cairo-perf.c		\
	cairo-perf.h		\
	cairo-perf-cover.c	\
	cairo-stats.c		\
	cairo-stats.h		\
	box-outline.c		\
	fill.c			\
	long-lines.c		\
	mosaic.c		\
	mosaic.h		\
	paint.c			\
	pattern_create_radial.c \
	rectangles.c		\
	stroke.c		\
	subimage_copy.c		\
	tessellate.c		\
	unaligned-clip.c	\
	text.c			\
	world-map.c		\
	world-map.h		\
	zrusin.c		\
	zrusin-another.h

if CAIRO_HAS_WIN32_SURFACE
cairo_perf_SOURCES += cairo-perf-win32.c
else
cairo_perf_SOURCES += cairo-perf-posix.c
endif

cairo_perf_diff_files_SOURCES =	\
	cairo-perf-diff-files.c \
	cairo-stats.c		\
	cairo-stats.h

LDADD = $(top_builddir)/boilerplate/libcairoboilerplate.la \
	$(top_builddir)/src/libcairo.la

$(top_builddir)/boilerplate/libcairoboilerplate.la:
	cd $(top_builddir)/boilerplate && $(MAKE) $(AM_MAKEFLAGS) libcairoboilerplate.la

$(top_builddir)/src/libcairo.la:
	cd $(top_builddir)/src && $(MAKE) $(AM_MAKEFLAGS) libcairo.la

perf: cairo-perf FORCE
	./cairo-perf

FORCE:


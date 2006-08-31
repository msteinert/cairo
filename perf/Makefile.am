SUBDIRS=../boilerplate

# We're using _GNU_SOURCE to get the prototype for asprintf. This may
# not be the most portable approach, but it is pragmatic and I'm
# willing to do something cleaner as soon as it causes someone a
# problem.
INCLUDES =					\
	-D_GNU_SOURCE				\
	-I$(srcdir)				\
	-I$(top_srcdir)/boilerplate		\
	-I$(top_srcdir)/src			\
	$(CAIRO_CFLAGS)

noinst_PROGRAMS = cairo-perf

cairo_perf_SOURCES =	\
	cairo-perf.c	\
	cairo-perf.h	\
	timing.c	\
	timing.h	\
	timer-alarm.h	\
	paint.c

if CAIRO_HAS_WIN32_SURFACE
cairo_perf_SOURCES += timer-alarm-win32.c
else
cairo_perf_SOURCES += timer-alarm-posix.c
endif

LDADD = $(top_builddir)/boilerplate/libcairoboilerplate.la \
	$(top_builddir)/src/libcairo.la

perf: cairo-perf
	./cairo-perf


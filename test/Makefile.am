# All new test cases go here
TESTS = 		\
clip_twice		\
coverage		\
fill_rule		\
get_and_set		\
leaky_polygon		\
line_width		\
linear_gradient		\
move_to_show_surface	\
path_data		\
pixman_rotate		\
text_cache_crash	\
text_rotate		\
user_data

# And all new tests go here too. I really don't like having to repeat
# this list. Anyone know a good way to avoid it? Can I use a wildcard
# here?
EXTRA_DIST =			\
fill_rule-ref.png		\
leaky_polygon-ref.png		\
line_width-ref.png		\
linear_gradient-ref.png		\
move_to_show_surface-ref.png	\
coverage-ref.png		\
clip_twice-ref.png		\
path_data-ref.png		\
pixman_rotate-ref.png		\
romedalen.png

# Once we can draw the text_rotate.c test case correctly, we should
# create and add text_rotate-ref.png to the list of reference PNGs.

# This list is only for known bugs (not regressions). We do need to
# fix these before the next release, but they are expected to fail for
# now, so they don't need to hold up any new code commit.
#
# When new bugs are found in committed code they can be listed
# here. New failures due to local, uncommitted code changes are
# regression bugs that should not be listed here. Instead they should
# be fixed before the code is committed.
XFAIL_TESTS =		\
move_to_show_surface	\
pixman_rotate		\
text_rotate

check_PROGRAMS = $(TESTS)

# We're using _GNU_SOURCE to get the prototype for asprintf. This may
# not be the most portable approach, but it is pragmatic and I'm
# willing to do something cleaner as soon as it causes someone a
# problem.
INCLUDES = -D_GNU_SOURCE -I$(srcdir) $(CAIRO_CFLAGS) -I$(top_srcdir)/src

noinst_LTLIBRARIES = libcairotest.la

libcairotest_la_SOURCES =\
buffer_diff.c	\
buffer_diff.h	\
cairo_test.c	\
cairo_test.h	\
read_png.c	\
read_png.h	\
write_png.c	\
write_png.h	\
xmalloc.c	\
xmalloc.h

LDADDS = libcairotest.la $(top_builddir)/src/libcairo.la

# ARGH! I have to repeat the list of tests a third time. Maybe it's
# time to break down and auto-generate the Makefile.am or something
# from autogen.sh. My, but this is painful...
clip_twice_LDADD = $(LDADDS)
coverage_LDADD = $(LDADDS)
fill_rule_LDADD = $(LDADDS)
get_and_set_LDADD = $(LDADDS)
leaky_polygon_LDADD = $(LDADDS)
line_width_LDADD = $(LDADDS)
linear_gradient_LDADD = $(LDADDS)
move_to_show_surface_LDADD = $(LDADDS)
path_data_LDADD = $(LDADDS)
pixman_rotate_LDADD = $(LDADDS)
text_cache_crash_LDADD = $(LDADDS)
text_rotate_LDADD = $(LDADDS)
user_data_LDADD = $(LDADDS)

noinst_PROGRAMS = imagediff
imagediff_LDADD = $(LDADDS)

CLEANFILES = *-out.png *-diff.png *.log

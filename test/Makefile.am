# All new test cases go here
TESTS = 		\
clip-twice		\
coverage		\
create-for-png		\
fill-rule		\
get-and-set		\
leaky-polygon		\
line-width		\
linear-gradient		\
move-to-show-surface	\
path-data		\
pixman-rotate		\
text-cache-crash	\
text-rotate		\
transforms		\
user-data

# And all new tests go here too. I really don't like having to repeat
# this list. Anyone know a good way to avoid it? Can I use a wildcard
# here?
EXTRA_DIST =			\
create-for-png-ref.png		\
fill-rule-ref.png		\
leaky-polygon-ref.png		\
line-width-ref.png		\
linear-gradient-ref.png		\
move-to-show-surface-ref.png	\
coverage-ref.png		\
clip-twice-ref.png		\
path-data-ref.png		\
pixman-rotate-ref.png		\
romedalen.png			\
transforms-ref.png

# Once we can draw the text-rotate.c test case correctly, we should
# create and add text-rotate-ref.png to the list of reference PNGs.

# This list is only for known bugs (not regressions). We do need to
# fix these before the next release, but they are expected to fail for
# now, so they don't need to hold up any new code commit.
#
# When new bugs are found in committed code they can be listed
# here. New failures due to local, uncommitted code changes are
# regression bugs that should not be listed here. Instead they should
# be fixed before the code is committed.
XFAIL_TESTS =		\
pixman-rotate		\
text-rotate

check_PROGRAMS = $(TESTS)

# We're using _GNU_SOURCE to get the prototype for asprintf. This may
# not be the most portable approach, but it is pragmatic and I'm
# willing to do something cleaner as soon as it causes someone a
# problem.
INCLUDES = -D_GNU_SOURCE -I$(srcdir) $(CAIRO_CFLAGS) -I$(top_srcdir)/src

noinst_LTLIBRARIES = libcairotest.la

libcairotest_la_SOURCES =\
buffer-diff.c	\
buffer-diff.h	\
cairo-test.c	\
cairo-test.h	\
read-png.c	\
read-png.h	\
write-png.c	\
write-png.h	\
xmalloc.c	\
xmalloc.h

LDADDS = libcairotest.la $(top_builddir)/src/libcairo.la

# ARGH! I have to repeat the list of tests a third time. Maybe it's
# time to break down and auto-generate the Makefile.am or something
# from autogen.sh. My, but this is painful...
clip_twice_LDADD = $(LDADDS)
coverage_LDADD = $(LDADDS)
create_for_png_LDADD = $(LDADDS)
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
transforms_LDADD = $(LDADDS)
user_data_LDADD = $(LDADDS)

noinst_PROGRAMS = imagediff
imagediff_LDADD = $(LDADDS)

CLEANFILES = *-out.png *-diff.png *.log

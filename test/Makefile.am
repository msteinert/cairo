# All new test cases go here
TESTS = 		\
fill_rule		\
line_width		\
move_to_show_surface

# And all new test go here too. I really don't like having to repeat
# this list. Anyone know a good way to avoid it? Can I use a wildcard
# here?
EXTRA_DIST = \
fill_rule-ref.png \
line_width-ref.png \
move_to_show_surface-ref.png

# This list is only for known bugs (not regressions). We do need to
# fix these before the next release, but they are expected to fail for
# now, so they don't need to hold up any new code commit.
#
# When new bugs are found in committed code they can be listed
# here. New failures due to local, uncommitted code changes are
# regression bugs that should not be listed here. Instead they should
# be fixed before the code is committed.
XFAIL_TESTS = \
move_to_show_surface

check_PROGRAMS = $(TESTS)

# We're using _GNU_SOURCE to get the prototype for asprintf. This may
# not be the most portable approach, but it is pragmatic and I'm
# willing to do something cleaner as soon as it causes someone a
# problem.
INCLUDES = -D_GNU_SOURCE -I$(srcdir) $(CAIRO_CFLAGS) -I$(srcdir)/../src

AM_LDFLAGS = $(CAIRO_LIBS) ../src/libcairo.la

cairo_test_lib =\
cairo_test.c	\
cairo_test.h	\
read_png.c	\
read_png.h	\
write_png.c	\
write_png.h	\
xmalloc.c	\
xmalloc.h

# ARGH! I have to repeat the list of tests a third time. Maybe it's
# time to break down and auto-generate the Makefile.am or something
# from autogen.sh. My, but this is painful...
fill_rule_SOURCES = fill_rule.c $(cairo_test_lib)
line_width_SOURCES = line_width.c $(cairo_test_lib)
move_to_show_surface_SOURCES = move_to_show_surface.c $(cairo_test_lib)

CLEANFILES = *-out.png *-diff.png

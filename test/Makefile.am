TESTS = 		\
line_width		\
move_to_show_surface

# Known bugs (not regressions). We do need to fix these before the
# next release, but they are expected to fail for now, so they don't
# need to hold up any new code commit.
#
# When new bugs are found in committed code they can be listed
# here. New failures due to local, uncommitted code changes are
# regression bugs that should not be listed here. Instead they should
# be fixed before the code is committed.
XFAIL_TESTS = \
move_to_show_surface

check_PROGRAMS = $(TESTS)

INCLUDES = -I$(srcdir) $(CAIRO_CFLAGS) -I$(srcdir)/../src

AM_LDFLAGS = $(CAIRO_LIBS) -L../src -lcairo

cairo_test_lib = cairo_test.c read_png.c write_png.c xmalloc.c

line_width_SOURCES = line_width.c $(cairo_test_lib)
move_to_show_surface_SOURCES = move_to_show_surface.c $(cairo_test_lib)





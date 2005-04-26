# All test cases go here
TESTS = 		\
clip-twice		\
coverage		\
create-for-png		\
fill-and-stroke		\
fill-rule		\
get-and-set		\
gradient-alpha		\
leaky-polygon		\
line-width		\
linear-gradient		\
move-to-show-surface	\
paint			\
path-data		\
pdf-surface		\
pixman-rotate		\
select-font-no-show-text\
set-source		\
surface-finish-twice	\
surface-pattern		\
text-cache-crash	\
text-rotate		\
transforms		\
translate-show-surface	\
trap-clip		\
user-data

# All tests which have a reference image go here.
# I really don't like having to repeat this list. Anyone know a good
# way to avoid it? Can I use a wildcard here?
EXTRA_DIST =			\
create-for-png-ref.png		\
fill-and-stroke-ref.png		\
fill-rule-ref.png		\
gradient-alpha-ref.png		\
leaky-polygon-ref.png		\
line-width-ref.png		\
linear-gradient-ref.png		\
move-to-show-surface-ref.png	\
coverage-ref.png		\
clip-twice-ref.png		\
paint-ref.png			\
path-data-ref.png		\
pixman-rotate-ref.png		\
romedalen.png			\
set-source-ref.png		\
surface-pattern-ref.png		\
transforms-ref.png		\
translate-show-surface-ref.png	\
trap-clip-ref.png

# Any test for which the code committed to CVS is expected to fail
# should be listed here.
#
# This way, we can avoid being bothered by reports of bugs we are
# aware of, but users can still report when tests start behaving in
# unexpected ways on their system.
#
# Of course, before any "release" of cairo we should eliminate
# everything from this list by fixing the bugs. (We don't necessarily
# have to be that strict for "snapshots" though.)
#
# Also, any test listed here should call cairo_test_expect_failure and
# provide an explanation for the expected failure.
XFAIL_TESTS =		\
coverage		\
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
fill_and_stroke_LDADD = $(LDADDS)
fill_rule_LDADD = $(LDADDS)
get_and_set_LDADD = $(LDADDS)
gradient_alpha_LDADD = $(LDADDS)
leaky_polygon_LDADD = $(LDADDS)
line_width_LDADD = $(LDADDS)
linear_gradient_LDADD = $(LDADDS)
move_to_show_surface_LDADD = $(LDADDS)
paint_LDADD = $(LDADDS)
path_data_LDADD = $(LDADDS)
pdf_surface_LDADD = $(LDADDS)
pixman_rotate_LDADD = $(LDADDS)
select_font_no_show_text_LDADD = $(LDADDS)
set_source_LDADD = $(LDADDS)
surface_finish_twice_LDADD = $(LDADDS)
surface_pattern_LDADD = $(LDADDS)
text_cache_crash_LDADD = $(LDADDS)
text_rotate_LDADD = $(LDADDS)
transforms_LDADD = $(LDADDS)
translate_show_surface_LDADD = $(LDADDS)
trap_clip_LDADD = $(LDADDS)
user_data_LDADD = $(LDADDS)

noinst_PROGRAMS = imagediff
imagediff_LDADD = $(LDADDS)

CLEANFILES = *-out.png *-diff.png *.log

lib_LTLIBRARIES = libcairo.la
include_HEADERS = cairo.h

libcairo_la_SOURCES = \
	cairo.h \
	cairo.c \
	cairoint.h \
	cairo_color.c \
	cairo_font.c \
	cairo_gstate.c \
	cairo_matrix.c \
	cairo_misc.c \
	cairo_path.c \
	cairo_path_bounds.c \
	cairo_path_fill.c \
	cairo_path_stroke.c \
	cairo_pen.c \
	cairo_polygon.c \
	cairo_spline.c \
	cairo_surface.c \
	cairo_traps.c

libcairo_la_LDFLAGS = -version-info @VERSION_INFO@

INCLUDES = $(CAIRO_CFLAGS) $(X_CFLAGS)

libcairo_la_LIBADD = $(CAIRO_LIBS) -lm

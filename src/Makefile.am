lib_LTLIBRARIES = libcairo.la
include_HEADERS = cairo.h cairo-xlib.h

libcairo_la_SOURCES = 		\
	cairo.c			\
	cairo.h			\
	cairo_color.c		\
	cairo_fixed.c		\
	cairo_font.c		\
	cairo_ft_font.c		\
	cairo_gstate.c		\
	cairo_hull.c		\
	cairo_image_surface.c	\
	cairo_matrix.c		\
	cairo_path.c		\
	cairo_path_bounds.c	\
	cairo_path_fill.c	\
	cairo_path_stroke.c	\
	cairo_pen.c		\
	cairo_polygon.c		\
	cairo_ps_surface.c	\
	cairo_slope.c		\
	cairo_spline.c		\
	cairo_surface.c		\
	cairo_traps.c		\
	cairo_xlib_surface.c	\
	cairoint.h

libcairo_la_LDFLAGS = -version-info @VERSION_INFO@

INCLUDES = $(CAIRO_CFLAGS) $(X_CFLAGS)

libcairo_la_LIBADD = $(CAIRO_LIBS) -lm

lib_LTLIBRARIES = libcairo.la
include_HEADERS = cairo.h cairo-features.h

if CAIRO_HAS_PS_SURFACE
libcairo_ps_sources = cairo_ps_surface.c
endif

if CAIRO_HAS_PNG_SURFACE
libcairo_png_sources = cairo_png_surface.c
endif

if CAIRO_HAS_XLIB_SURFACE
libcairo_xlib_sources = cairo_xlib_surface.c
endif

if CAIRO_HAS_XCB_SURFACE
libcairo_xcb_sources = cairo_xcb_surface.c
endif

if CAIRO_HAS_GL_SURFACE
libcairo_gl_sources = cairo_gl_surface.c
endif

# These names match automake style variable definition conventions so
# without these lines, automake will complain during the handling of 
# the libcairo_la_LIBADD below.  (The INCLUDES is an autoconf only
# term and automake does not care about it)
FONTCONFIG_LIBS=@FONTCONFIG_LIBS@
XRENDER_LIBS=@XRENDER_LIBS@

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
	cairo_slope.c		\
	cairo_spline.c		\
	cairo_surface.c		\
	cairo_traps.c		\
	cairo_pattern.c		\
	$(libcairo_ps_sources)  \
	$(libcairo_png_sources) \
	$(libcairo_xlib_sources)\
	$(libcairo_xcb_sources) \
	$(libcairo_gl_sources)  \
	cairoint.h

libcairo_la_LDFLAGS = -version-info @VERSION_INFO@ -no-undefined

INCLUDES = -I$(srcdir) $(CAIRO_CFLAGS) $(FONTCONFIG_CFLAGS) $(XRENDER_CFLAGS) $(XCB_CFLAGS) $(PNG_CFLAGS) $(GL_CFLAGS)

libcairo_la_LIBADD = $(CAIRO_LIBS) $(FONTCONFIG_LIBS) $(XRENDER_LIBS) $(XCB_LIBS) $(PS_LIBS) $(PNG_LIBS) $(GL_LIBS) -lm

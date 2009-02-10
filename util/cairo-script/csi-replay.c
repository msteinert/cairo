#include <cairo.h>
#include <cairo-script-interpreter.h>

#include <stdio.h>
#include <stdlib.h>

static const cairo_user_data_key_t _key;

#if CAIRO_HAS_XLIB_SURFACE
#include <cairo-xlib.h>
static Display *
_get_display (void)
{
    static Display *dpy;

    if (dpy != NULL)
	return dpy;

    dpy = XOpenDisplay (NULL);
    if (dpy == NULL) {
	fprintf (stderr, "Failed to open display.\n");
	exit (1);
    }

    return dpy;
}

static void
_destroy_window (void *closure)
{
    XFlush (_get_display ());
    XDestroyWindow (_get_display(), (Window) closure);
}

static cairo_surface_t *
_xlib_surface_create (void *closure,
			 cairo_content_t content,
			 double width, double height)
{
    Display *dpy;
    XSetWindowAttributes attr;
    Visual *visual;
    int depth;
    Window w;
    cairo_surface_t *surface;

    dpy = _get_display ();

    visual = DefaultVisual (dpy, DefaultScreen (dpy));
    depth = DefaultDepth (dpy, DefaultScreen (dpy));
    attr.override_redirect = True;
    w = XCreateWindow (dpy, DefaultRootWindow (dpy), 0, 0,
		       width <= 0 ? 1 : width,
		       height <= 0 ? 1 : height,
		       0, depth,
		       InputOutput, visual, CWOverrideRedirect, &attr);
    XMapWindow (dpy, w);

    surface = cairo_xlib_surface_create (dpy, w, visual, width, height);
    cairo_surface_set_user_data (surface, &_key, (void *) w, _destroy_window);

    return surface;
}

#if CAIRO_HAS_XLIB_XRENDER_SURFACE
#include <cairo-xlib-xrender.h>

static void
_destroy_pixmap (void *closure)
{
    XFreePixmap (_get_display(), (Pixmap) closure);
}

static cairo_surface_t *
_xrender_surface_create (void *closure,
			 cairo_content_t content,
			 double width, double height)
{
    Display *dpy;
    Pixmap pixmap;
    XRenderPictFormat *xrender_format;
    cairo_surface_t *surface;

    dpy = _get_display ();

    content = CAIRO_CONTENT_COLOR_ALPHA;

    switch (content) {
    case CAIRO_CONTENT_COLOR_ALPHA:
	xrender_format = XRenderFindStandardFormat (dpy, PictStandardARGB32);
	break;
    case CAIRO_CONTENT_COLOR:
	xrender_format = XRenderFindStandardFormat (dpy, PictStandardRGB24);
	break;
    case CAIRO_CONTENT_ALPHA:
    default:
	xrender_format = XRenderFindStandardFormat (dpy, PictStandardA8);
    }

    pixmap = XCreatePixmap (dpy, DefaultRootWindow (dpy),
		       width, height, xrender_format->depth);

    surface = cairo_xlib_surface_create_with_xrender_format (dpy, pixmap,
							     DefaultScreenOfDisplay (dpy),
							     xrender_format,
							     width, height);
    cairo_surface_set_user_data (surface, &_key,
				 (void *) pixmap, _destroy_pixmap);

    return surface;
}
#endif
#endif

#if CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
static cairo_surface_t *
_pdf_surface_create (void *closure,
		     cairo_content_t content,
		     double width, double height)
{
    return cairo_pdf_surface_create_for_stream (NULL, NULL, width, height);
}
#endif

#if CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
static cairo_surface_t *
_ps_surface_create (void *closure,
		    cairo_content_t content,
		    double width, double height)
{
    return cairo_ps_surface_create_for_stream (NULL, NULL, width, height);
}
#endif

#if CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
static cairo_surface_t *
_svg_surface_create (void *closure,
		     cairo_content_t content,
		     double width, double height)
{
    return cairo_svg_surface_create_for_stream (NULL, NULL, width, height);
}
#endif

static cairo_surface_t *
_image_surface_create (void *closure,
		       cairo_content_t content,
		       double width, double height)
{
    return cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
}

int
main (int argc, char **argv)
{
    cairo_script_interpreter_t *csi;
    cairo_script_interpreter_hooks_t hooks = {
#if CAIRO_HAS_XLIB_XRENDER_SURFACE
	.surface_create = _xrender_surface_create
#elif CAIRO_HAS_XLIB_SURFACE
	.surface_create = _xlib_surface_create
#elif CAIRO_PDF_SURFACE
	.surface_create = _pdf_surface_create
#elif CAIRO_PS_SURFACE
	.surface_create = _ps_surface_create
#elif CAIRO_SVG_SURFACE
	.surface_create = _svg_surface_create
#else
	.surface_create = _image_surface_create
#endif
    };
    int i;
    const struct backends {
	const char *name;
	csi_surface_create_func_t create;
    } backends[] = {
	{ "--image", _image_surface_create },
#if CAIRO_HAS_XLIB_XRENDER_SURFACE
	{ "--xrender", _xrender_surface_create },
#endif
#if CAIRO_HAS_XLIB_SURFACE
	{ "--xlib", _xlib_surface_create },
#endif
#if CAIRO_HAS_PDF_SURFACE
	{ "--pdf", _pdf_surface_create },
#endif
#if CAIRO_HAS_PS_SURFACE
	{ "--ps", _ps_surface_create },
#endif
#if CAIRO_HAS_SVG_SURFACE
	{ "--svg", _svg_surface_create },
#endif
	{ NULL, NULL }
    };

    csi = cairo_script_interpreter_create ();
    cairo_script_interpreter_install_hooks (csi, &hooks);

    for (i = 1; i < argc; i++) {
	const struct backends *b;

	for (b = backends; b->name != NULL; b++) {
	    if (strcmp (b->name, argv[i]) == 0) {
		hooks.surface_create = b->create;
		cairo_script_interpreter_install_hooks (csi, &hooks);
		break;
	    }
	}

	if (b->name == NULL)
	    cairo_script_interpreter_run (csi, argv[i]);
    }

    return cairo_script_interpreter_destroy (csi);
}

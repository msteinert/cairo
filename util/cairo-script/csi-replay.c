#include <cairo.h>
#include <cairo-script-interpreter.h>

#include <stdio.h>
#include <stdlib.h>

static const cairo_user_data_key_t _key;

#if CAIRO_HAS_XLIB_XRENDER_SURFACE
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>

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
_destroy_pixmap (void *closure)
{
    XFreePixmap (_get_display(), (Pixmap) closure);
}

static void
_destroy_window (void *closure)
{
    XFlush (_get_display ());
    XDestroyWindow (_get_display(), (Window) closure);
}

static cairo_surface_t *
_surface_create (void *closure,
		 cairo_content_t content,
		 double width, double height)
{
    Display *dpy;
    XRenderPictFormat *xrender_format;
    cairo_surface_t *surface;

    dpy = _get_display ();

    content = CAIRO_CONTENT_COLOR_ALPHA;
    if (1) {
	Pixmap pixmap;

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
    } else {
	XSetWindowAttributes attr;
	Visual *visual;
	Window w;

	visual = DefaultVisual (dpy, DefaultScreen (dpy));
	xrender_format = XRenderFindVisualFormat (dpy, visual);
	if (xrender_format == NULL) {
	    fprintf (stderr, "X server does not have the Render extension.\n");
	    exit (1);
	}

	attr.override_redirect = True;
	w = XCreateWindow (dpy, DefaultRootWindow (dpy), 0, 0,
			   width <= 0 ? 1 : width,
			   height <= 0 ? 1 : height,
			   0, xrender_format->depth,
			   InputOutput, visual, CWOverrideRedirect, &attr);
	XMapWindow (dpy, w);

	surface = cairo_xlib_surface_create_with_xrender_format (dpy, w,
								 DefaultScreenOfDisplay (dpy),
								 xrender_format,
								 width, height);
	cairo_surface_set_user_data (surface, &_key, (void *) w, _destroy_window);
    }

    return surface;
}
#else
/* fallback: just use an image surface */
static cairo_surface_t *
_surface_create (void *closure,
		 cairo_content_t content,
		 double width, double height)
{
    return cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
}
#endif

int
main (int argc, char **argv)
{
    cairo_script_interpreter_t *csi;
    const cairo_script_interpreter_hooks_t hooks = {
	.surface_create = _surface_create
    };
    int i;

    csi = cairo_script_interpreter_create ();
    cairo_script_interpreter_install_hooks (csi, &hooks);
    for (i = 1; i < argc; i++)
	cairo_script_interpreter_run (csi, argv[i]);
    return cairo_script_interpreter_destroy (csi);
}

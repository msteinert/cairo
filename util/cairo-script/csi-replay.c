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
_destroy_window (void *closure)
{
    XFlush (_get_display ());
    XDestroyWindow (_get_display(), (Window) closure);
}

static cairo_surface_t *
_surface_create (void *closure,
		 double width, double height)
{
    Display *dpy;
    Visual *visual;
    XRenderPictFormat *xrender_format;
    XSetWindowAttributes attr;
    Window w;
    cairo_surface_t *surface;

    dpy = _get_display ();

    visual = DefaultVisual (dpy, DefaultScreen (dpy));
    xrender_format = XRenderFindVisualFormat (dpy, visual);
    if (xrender_format == NULL) {
	fprintf (stderr, "X server does not have the Render extension.\n");
	exit (1);
    }

    attr.override_redirect = True;
    w = XCreateWindow (dpy, DefaultRootWindow (dpy), 0, 0,
			width, height, 0, xrender_format->depth,
			InputOutput, visual, CWOverrideRedirect, &attr);
    XMapWindow (dpy, w);

    surface = cairo_xlib_surface_create_with_xrender_format (dpy, w,
							     DefaultScreenOfDisplay (dpy),
							     xrender_format,
							     width, height);
    cairo_surface_set_user_data (surface, &_key, (void *) w, _destroy_window);

    return surface;
}
#endif

int
main (int argc, char **argv)
{
    cairo_script_interpreter_t *csi;
    const cairo_script_interpreter_hooks_t hooks = {
	.surface_create = _surface_create
    };

    csi = cairo_script_interpreter_create ();
    cairo_script_interpreter_install_hooks (csi, &hooks);
    cairo_script_interpreter_run (csi, argv[1]);
    return cairo_script_interpreter_destroy (csi);
}

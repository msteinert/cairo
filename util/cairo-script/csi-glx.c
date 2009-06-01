#include <cairo-gl.h>
#include "cairo-script-interpreter.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static void
die (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);

    exit (EXIT_FAILURE);
}

static cairo_surface_t *
_get_fb (void)
{
    int rgb_attribs[] = { GLX_RGBA,
			  GLX_RED_SIZE, 1,
			  GLX_GREEN_SIZE, 1,
			  GLX_BLUE_SIZE, 1,
			  GLX_DOUBLEBUFFER,
			  None };
    XVisualInfo *vi;
    GLXContext ctx;
    Display *dpy;
    Screen *screen;
    Colormap cmap;
    XSetWindowAttributes swa;
    Window win;

    cairo_gl_context_t *context;
    cairo_surface_t *surface;

    dpy = XOpenDisplay (NULL);
    if (dpy == NULL)
	die ("Failed to open display\n");

    vi = glXChooseVisual (dpy, DefaultScreen (dpy), rgb_attribs);
    if (vi == NULL)
	die ("Failed to create RGB, double-buffered visual\n");

    ctx = glXCreateContext (dpy, vi, NULL, True);

    screen = ScreenOfDisplay (dpy, vi->screen);

    cmap = XCreateColormap (dpy,
			    RootWindowOfScreen (screen),
			    vi->visual,
			    AllocNone);
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.override_redirect = True;
    win = XCreateWindow (dpy, RootWindowOfScreen (screen),
			 0, 0,
			 WidthOfScreen (screen), HeightOfScreen (screen),
			 0,
			 vi->depth,
			 InputOutput,
			 vi->visual,
			 CWBorderPixel | CWColormap | CWOverrideRedirect,
			 &swa);
    XMapWindow (dpy, win);
    XFlush (dpy);
    XFree (vi);

    context = cairo_glx_context_create (dpy, ctx);
    surface = cairo_gl_surface_create_for_window (context, win,
	                                          WidthOfScreen (screen),
						  HeightOfScreen (screen));
    cairo_gl_context_destroy (context);

    if (cairo_surface_status (surface))
	die ("failed to create cairo surface\n");

    return surface;
}

static cairo_surface_t *
_glx_surface_create (void *closure,
		     cairo_content_t content,
		     double width, double height)
{
    return cairo_surface_create_similar (closure, content, width, height);
}

static struct list {
    struct list *next;
    cairo_t *context;
    cairo_surface_t *surface;
} *list;

static cairo_t *
_glx_context_create (void *closure, cairo_surface_t *surface)
{
    cairo_t *cr = cairo_create (surface);
    struct list *l = malloc (sizeof (*l));
    l->next = list;
    l->context = cr;
    l->surface = cairo_surface_reference (surface);
    list = l;
    return cr;
}

static void
_glx_context_destroy (void *closure, void *ptr)
{
    struct list *l, **prev = &list;
    while ((l = *prev) != NULL) {
	if (l->context == ptr) {
	    if (cairo_surface_status (l->surface) == CAIRO_STATUS_SUCCESS) {
		cairo_t *cr = cairo_create (closure);
		cairo_set_source_surface (cr, l->surface, 0, 0);
		cairo_paint (cr);
		cairo_destroy (cr);
		cairo_gl_surface_swapbuffers (closure);
	    }

	    cairo_surface_destroy (l->surface);
	    *prev = l->next;
	    free (l);
	    return;
	}
	prev = &l->next;
    }
}

int
main (int argc, char **argv)
{
    const cairo_script_interpreter_hooks_t hooks = {
	.closure = _get_fb (),
	.surface_create = _glx_surface_create,
	.context_create = _glx_context_create,
	.context_destroy = _glx_context_destroy
    };
    cairo_script_interpreter_t *csi;
    int i;

    csi = cairo_script_interpreter_create ();
    cairo_script_interpreter_install_hooks (csi, &hooks);
    for (i = 1; i < argc; i++)
	cairo_script_interpreter_run (csi, argv[i]);
    return cairo_script_interpreter_destroy (csi);
}

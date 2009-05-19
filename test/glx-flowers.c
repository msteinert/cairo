/*
 * Copyright © 2007 Michael Dominic K.
 * Copyright © 2009 Chris Wilson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#include <cairo-gl.h>

#include <math.h>
#include <stdio.h>
#include <sys/time.h>

typedef struct {
    float x;
    float y;
    float scale;
    float rotation;
    float r1, g1, b1, a1;
    float r2, b2, g2, a2;
} Flower;

#define N_FLOWERS 200
#define FLOWER_SIZE 128
static Flower flowers[N_FLOWERS];

#define WIDTH 640
#define HEIGHT 480

static cairo_surface_t *
_surface_create (void)
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
    Colormap cmap;
    XSetWindowAttributes swa;
    Window win;

    cairo_gl_context_t *context;
    cairo_surface_t *surface;

    dpy = XOpenDisplay (NULL);
    if (dpy == NULL) {
	fprintf (stderr, "Failed to open display\n");
	return NULL;
    }

    vi = glXChooseVisual (dpy, DefaultScreen (dpy), rgb_attribs);
    if (vi == NULL) {
	fprintf (stderr, "Failed to create RGB, double-buffered visual\n");
	XCloseDisplay (dpy);
	return NULL;
    }

    ctx = glXCreateContext (dpy, vi, NULL, True);

    cmap = XCreateColormap (dpy,
			    RootWindow (dpy, vi->screen),
			    vi->visual,
			    AllocNone);
    swa.colormap = cmap;
    swa.border_pixel = 0;
    win = XCreateWindow (dpy, RootWindow (dpy, vi->screen),
			 0, 0,
			 640, 480,
			 0,
			 vi->depth,
			 InputOutput,
			 vi->visual,
			 CWBorderPixel | CWColormap, &swa);
    XMapWindow (dpy, win);
    XFlush (dpy);
    XFree (vi);

    context = cairo_glx_context_create (dpy, ctx);
    surface = cairo_gl_surface_create_for_window (context, win, WIDTH, HEIGHT);
    cairo_gl_context_destroy (context);

    if (cairo_surface_status (surface)) {
	fprintf (stderr, "failed to create cairo surface\n");
	return NULL;
    }

    return surface;
}

static unsigned int
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static unsigned int x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static void
random_colour (float *r, float *g, float *b, float *a)
{
    unsigned int x = hars_petruska_f54_1_random ();
    *r = (x & 255) / 255.; x >>= 8;
    *g = (x & 255) / 255.; x >>= 8;
    *b = (x & 255) / 255.; x >>= 8;
    *a = x / 255.;
}

static void
randomize_flower (Flower *flower, int width, int height)
{
    flower->x = (hars_petruska_f54_1_random() & 8191) * width / 8191.;
    flower->y = (hars_petruska_f54_1_random() & 8191) * height / 8191.;
    flower->scale = 10 + (hars_petruska_f54_1_random() & 511) * 140 / 512.;
    flower->rotation = (hars_petruska_f54_1_random() & 511) * M_PI / 256;

    random_colour (&flower->r1, &flower->g1, &flower->b1, &flower->a1);
    random_colour (&flower->r2, &flower->g2, &flower->b2, &flower->a2);
}

static void
randomize_flowers (int width, int height)
{
    int i;

    for (i = 0; i < N_FLOWERS; i++)
        randomize_flower (&flowers [i], width, height);
}

static cairo_pattern_t *
create_flower (cairo_surface_t *target, int size)
{
    cairo_surface_t *surface;
    cairo_pattern_t *mask;
    cairo_t *cr;

    surface = cairo_surface_create_similar (target,
	                                    CAIRO_CONTENT_ALPHA, size, size);
    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    cairo_scale (cr, size/2, size/2);
    cairo_translate (cr, 1., 1.);
    cairo_move_to (cr, 0, 0);
    cairo_curve_to (cr, -0.9, 0, -0.9, -0.9, -0.9, -0.9);
    cairo_curve_to (cr, 0.0, -0.9, 0, 0, 0, 0);
    cairo_curve_to (cr, 0.9, 0.0, 0.9, -0.9, 0.9, -0.9);
    cairo_curve_to (cr, 0.0, -0.9, 0.0, 0.0, 0.0, 0.0);
    cairo_curve_to (cr, 0.9, 0.0, 0.9, 0.9, 0.9, 0.9);
    cairo_curve_to (cr, 0.0, 0.9, 0.0, 0.0, 0.0, 0.0);
    cairo_curve_to (cr, -0.9, 0.0, -0.9, 0.9, -0.9, 0.9);
    cairo_curve_to (cr, 0.0, 0.9, 0.0, 0.0, 0.0, 0.0);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_fill (cr);

    mask = cairo_pattern_create_for_surface (cairo_get_target (cr));
    cairo_destroy (cr);

    return mask;
}

static void
paint (cairo_surface_t *surface,
       cairo_pattern_t *mask,
       int mask_size)
{
    cairo_t *cr;
    int i;

    cr = cairo_create (surface);
    for (i = 0; i < N_FLOWERS; i++) {
        cairo_pattern_t *pattern;
	cairo_matrix_t matrix;

        cairo_matrix_init_identity (&matrix);
        cairo_matrix_translate (&matrix, flowers[i].x, flowers[i].y);
        cairo_matrix_scale (&matrix,
			    flowers[i].scale/mask_size,
			    flowers[i].scale/mask_size);
        cairo_matrix_rotate (&matrix, flowers[i].rotation);
        cairo_set_matrix (cr, &matrix);

        pattern = cairo_pattern_create_linear (0, -mask_size, 0, mask_size);
        cairo_pattern_add_color_stop_rgba (pattern, 0,
		flowers[i].r1, flowers[i].g1, flowers[i].b1, flowers[i].a1);
        cairo_pattern_add_color_stop_rgba (pattern, 1,
		flowers[i].r2, flowers[i].g2, flowers[i].b2, flowers[i].a2);
        cairo_set_source (cr, pattern);
        cairo_pattern_destroy (pattern);

	cairo_mask (cr, mask);
    }
    cairo_destroy (cr);
}

int
main (int argc, char *argv[])
{
    cairo_surface_t *surface;
    cairo_pattern_t *flower;
    int frame, frame_target = 5;
    struct timeval start, stop;

    surface = _surface_create ();
    if (surface == NULL) {
	fprintf (stderr, "Failed to create framebuffer\n");
	return 1;
    }

    flower = create_flower (surface, FLOWER_SIZE);
    frame = 0;
    gettimeofday (&start, NULL);
    while (1) {
	randomize_flowers (cairo_gl_surface_get_width (surface),
			   cairo_gl_surface_get_height (surface));
	paint (surface, flower, FLOWER_SIZE);
	cairo_gl_surface_swapbuffers (surface);

	if (++frame == frame_target) {
	    int ticks;

	    gettimeofday (&stop, NULL);

	    ticks = (stop.tv_sec - start.tv_sec) * 1000000;
	    ticks += (stop.tv_usec - start.tv_usec);
	    printf ("%.2f fps\n", frame * 1000000. / ticks);

	    /* rate-limit output to once every 5 seconds */
	    frame_target = (frame_target + 5000000 * frame / ticks + 1) / 2;

	    frame = 0;
	    start = stop;
	}
    }

    cairo_pattern_destroy (flower);
    cairo_surface_destroy (surface);

    return 0;
}

/*
 * Copyright © 2004 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "cairo-test.h"

#include "buffer-diff.h"
#include "read-png.h"
#include "write-png.h"
#include "xmalloc.h"

#define CAIRO_TEST_LOG_SUFFIX ".log"
#define CAIRO_TEST_PNG_SUFFIX "-out.png"
#define CAIRO_TEST_REF_SUFFIX "-ref.png"
#define CAIRO_TEST_DIFF_SUFFIX "-diff.png"

void
xasprintf (char **strp, const char *fmt, ...)
{
#ifdef HAVE_VASPRINTF    
    va_list va;
    int ret;
    
    va_start (va, fmt);
    ret = vasprintf (strp, fmt, va);
    va_end (va);

    if (ret < 0) {
	fprintf (stderr, "Out of memory\n");
	exit (1);
    }
#else /* !HAVE_VASNPRINTF */
#define BUF_SIZE 1024
    va_list va;
    char buffer[BUF_SIZE];
    int ret;
    
    va_start (va, fmt);
    ret = vsnprintf (buffer, sizeof(buffer), fmt, va);
    va_end (va);

    if (ret < 0) {
	fprintf (stderr, "Failure in vsnprintf\n");
	exit (1);
    }
    
    if (strlen (buffer) == sizeof(buffer) - 1) {
	fprintf (stderr, "Overflowed fixed buffer\n");
	exit (1);
    }
    
    *strp = strdup (buffer);
    if (!*strp) {
	fprintf (stderr, "Out of memory\n");
	exit (1);
    }
#endif /* !HAVE_VASNPRINTF */
}

static void
xunlink (const char *pathname)
{
    if (unlink (pathname) < 0 && errno != ENOENT) {
	fprintf (stderr, "  Error: Cannot remove %s: %s\n",
		 pathname, strerror (errno));
	exit (1);
    }
}

typedef cairo_test_status_t
(*cairo_test_set_target_t) (cairo_t *cr, int width, int height, void **closure);

typedef void
(*cairo_test_cleanup_target_t) (void *closure);

typedef struct _cairo_test_target
{
    const char		       *name;
    cairo_test_set_target_t	set_target;
    cairo_test_cleanup_target_t cleanup_target;
    void		       *closure;
} cairo_test_target_t;

static cairo_test_status_t
set_image_target (cairo_t *cr, int width, int height, void **closure)
{
    unsigned char *png_buf;
    int stride = 4 * width;

    png_buf = xcalloc (stride * height, 1);

    cairo_set_target_image (cr, png_buf, CAIRO_FORMAT_ARGB32,
			    width, height, stride);

    *closure = png_buf;

    return CAIRO_TEST_SUCCESS;
}

static void
cleanup_image_target (void *closure)
{
    unsigned char *png_buf = closure;

    free (png_buf);
}

/* XXX: Someone who knows glitz better than I do should fix this up to
 * work. */
#if 0 /* #ifdef CAIRO_HAS_GLITZ_SURFACE */
static cairo_test_status_t
set_glitz_target (cairo_t *cr, int width, int height, void **closure)
{
#error Not yet implemented
}

static void
cleanup_glitz_target (cairo_t *cr)
{
#error Not yet implemented
}
#endif

#ifdef CAIRO_HAS_QUARTZ_SURFACE
static cairo_test_status_t
set_quartz_target (cairo_t *cr, int width, int height, void **closure)
{
#error Not yet implemented
}

static void
cleanup_quartz_target (void *closure)
{
#error Not yet implemented
}
#endif

#ifdef CAIRO_HAS_WIN32_SURFACE
static cairo_test_status_t
set_win32_target (cairo_t *cr, int width, int height, void **closure)
{
#error Not yet implemented
}

static void
cleanup_win32_target (void *closure)
{
#error Not yet implemented
}
#endif

#ifdef CAIRO_HAS_XCB_SURFACE
static cairo_test_status_t
set_xcb_target (cairo_t *cr, int width, int height, void **closure)
{
#error Not yet implemented
}

static void
cleanup_xcb_target (void *closure)
{
#error Not yet implemented
}
#endif

#ifdef CAIRO_HAS_XLIB_SURFACE
typedef struct _xlib_target_closure
{
    Display *dpy;
    Pixmap pixmap;
} xlib_target_closure_t;

static cairo_test_status_t
set_xlib_target (cairo_t *cr, int width, int height, void **closure)
{
    xlib_target_closure_t *xtc;
    cairo_surface_t *surface;
    Display *dpy;

    *closure = xtc = xmalloc (sizeof (xlib_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    xtc->dpy = dpy = XOpenDisplay (0);
    if (xtc->dpy == NULL) {
	fprintf (stderr, "Failed to open display: %s\n", XDisplayName(0));
	return CAIRO_TEST_FAILURE;
    }

    xtc->pixmap = XCreatePixmap (dpy, DefaultRootWindow (dpy),
				 width, height, 32);

    surface = cairo_xlib_surface_create_for_pixmap (dpy, xtc->pixmap,
						    CAIRO_FORMAT_ARGB32);
    cairo_xlib_surface_set_size (surface, width, height);
    cairo_set_target_surface (cr, surface);

    return CAIRO_TEST_SUCCESS;
}

static void
cleanup_xlib_target (void *closure)
{
    xlib_target_closure_t *xtc = closure;

    XFreePixmap (xtc->dpy, xtc->pixmap);
    XCloseDisplay (xtc->dpy);
}
#endif

static cairo_test_status_t
cairo_test_for_target (cairo_test_t *test,
		       cairo_test_draw_function_t draw,
		       cairo_test_target_t	 *target)
{
    cairo_test_status_t status;
    cairo_t *cr;
    char *png_name, *ref_name, *diff_name;
    char *srcdir;
    int pixels_changed;
    cairo_test_status_t ret;

    /* Get the strings ready that we'll need. */
    srcdir = getenv ("srcdir");
    if (!srcdir)
	srcdir = ".";
    xasprintf (&png_name, "%s_%s%s", test->name,
	       target->name, CAIRO_TEST_PNG_SUFFIX);
    xasprintf (&ref_name, "%s/%s%s", srcdir, test->name,
	       CAIRO_TEST_REF_SUFFIX);
    xasprintf (&diff_name, "%s_%s%s", test->name,
	       target->name, CAIRO_TEST_DIFF_SUFFIX);

    /* Run the actual drawing code. */
    cr = cairo_create ();

    status = (target->set_target) (cr,
				   test->width, test->height,
				   &target->closure);
    if (status) {
	fprintf (stderr, "Error: Failed to set %s target\n", target->name);
	return CAIRO_TEST_FAILURE;
    }

    cairo_save (cr);
    cairo_set_source_rgba (cr, 0, 0, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SRC);
    cairo_paint (cr);
    cairo_restore (cr);

    status = (draw) (cr, test->width, test->height);

    /* Then, check all the different ways it could fail. */
    if (status) {
	fprintf (stderr, "Error: Function under test failed\n");
	return status;
    }

    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
	fprintf (stderr, "Error: Function under test left cairo status in an error state: %s\n", cairo_status_string (cr));
	return CAIRO_TEST_FAILURE;
    }

    /* Skip image check for tests with no image (width,height == 0,0) */
    if (test->width == 0 || test->height == 0) {
	cairo_destroy (cr);
	return CAIRO_TEST_SUCCESS;
    }

    cairo_surface_write_to_png (cairo_get_target_surface (cr), png_name);

    cairo_destroy (cr);

    target->cleanup_target (target->closure);

    pixels_changed = image_diff (png_name, ref_name, diff_name);

    if (pixels_changed) {
	ret = CAIRO_TEST_FAILURE;
	if (pixels_changed > 0)
	    fprintf (stderr, "Error: %d pixels differ from reference image %s\n",
		     pixels_changed, ref_name);
    } else {
	ret = CAIRO_TEST_SUCCESS;
    }

    free (png_name);
    free (ref_name);
    free (diff_name);

    return ret;
}

static cairo_test_status_t
cairo_test_real (cairo_test_t *test, cairo_test_draw_function_t draw)
{
    int i;
    FILE *stderr_saved = stderr;
    cairo_test_status_t status, ret;
    cairo_test_target_t targets[] = 
	{
	    { "image", set_image_target, cleanup_image_target}, 
#if 0 /* #ifdef CAIRO_HAS_GLITZ_SURFACE */
	    { "glitz", set_glitz_target, cleanup_glitz_target}, 
#endif
#ifdef CAIRO_HAS_QUARTZ_SURFACE
	    { "quartz", set_quartz_target, cleanup_quart_target},
#endif
#ifdef CAIRO_HAS_WIN32_SURFACE
	    { "win32", set_win32_target, cleanup_win32_target},
#endif
#ifdef CAIRO_HAS_XCB_SURFACE
	    { "xcb", set_xcb_target, cleanup_xcb_target},
#endif
#ifdef CAIRO_HAS_XLIB_SURFACE
	    { "xlib", set_xlib_target, cleanup_xlib_target},
#endif
	};
    char *log_name;

    xasprintf (&log_name, "%s%s", test->name, CAIRO_TEST_LOG_SUFFIX);
    xunlink (log_name);

    stderr = fopen (log_name, "a");

    ret = CAIRO_TEST_SUCCESS;
    for (i=0; i < sizeof(targets)/sizeof(targets[0]); i++) {
	cairo_test_target_t *target = &targets[i];
	fprintf (stderr, "Testing %s with %s target\n", test->name, target->name);
	printf ("%s_%s:\t", test->name, target->name);
	status = cairo_test_for_target (test, draw, target);
	if (status) {
	    printf ("FAIL\n");
	    ret = status;
	} else {
	    printf ("PASS\n");
	}
    }

    fclose (stderr);
    stderr = stderr_saved;

    return ret;
}

cairo_test_status_t
cairo_test_expect_failure (cairo_test_t		      *test, 
			   cairo_test_draw_function_t  draw,
			   const char		      *because)
{
    printf ("\n%s is expected to fail:\n\t%s\n", test->name, because);
    return cairo_test_real (test, draw);
}

cairo_test_status_t
cairo_test (cairo_test_t *test, cairo_test_draw_function_t draw)
{
    printf ("\n");
    return cairo_test_real (test, draw);
}

cairo_pattern_t *
cairo_test_create_png_pattern (cairo_t *cr, const char *filename)
{
    cairo_surface_t *image;
    cairo_pattern_t *pattern;
    unsigned char *buffer;
    unsigned int w, h, stride;
    read_png_status_t status;
    char *srcdir = getenv ("srcdir");

    status = read_png_argb32 (filename, &buffer, &w,&h, &stride);
    if (status != READ_PNG_SUCCESS) {
	if (srcdir) {
	    char *srcdir_filename;
	    xasprintf (&srcdir_filename, "%s/%s", srcdir, filename);
	    status = read_png_argb32 (srcdir_filename, &buffer, &w,&h, &stride);
	    free (srcdir_filename);
	}
    }
    if (status != READ_PNG_SUCCESS)
	return NULL;

    image = cairo_surface_create_for_image (buffer, CAIRO_FORMAT_ARGB32,
					    w, h, stride);

    cairo_surface_set_repeat (image, 1);

    pattern = cairo_pattern_create_for_surface (image);

    return pattern;
}

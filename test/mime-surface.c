/*
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Author: Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairo-test.h"

#include <stdio.h>
#include <errno.h>

/* Basic test to exercise the new mime-surface callback. */

#define WIDTH 200
#define HEIGHT 80

/* Lazy way of determining PNG dimensions... */
static void
png_dimensions (const char *filename,
		cairo_content_t *content, int *width, int *height)
{
    cairo_surface_t *surface;

    surface = cairo_image_surface_create_from_png (filename);
    *content = cairo_surface_get_content (surface);
    *width = cairo_image_surface_get_width (surface);
    *height = cairo_image_surface_get_height (surface);
    cairo_surface_destroy (surface);
}

static cairo_surface_t *
png_acquire (cairo_surface_t *mime_surface, void *closure,
	     cairo_surface_t *target, const cairo_rectangle_int_t *roi,
	     cairo_rectangle_int_t *extents)
{
    cairo_surface_t *image;

    image = cairo_image_surface_create_from_png (closure);
    extents->x = extents->y = 0;
    extents->width = cairo_image_surface_get_width (image);
    extents->height = cairo_image_surface_get_height (image);
    return image;
}

static cairo_surface_t *
red_acquire (cairo_surface_t *mime_surface, void *closure,
	     cairo_surface_t *target, const cairo_rectangle_int_t *roi,
	     cairo_rectangle_int_t *extents)
{
    cairo_surface_t *image;
    cairo_t *cr;

    image = cairo_surface_create_similar_image (target,
						CAIRO_FORMAT_RGB24,
						roi->width, roi->height);
    cr = cairo_create (image);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    *extents = *roi;
    return image;
}

static void
release (cairo_surface_t *mime_surface, void *closure, cairo_surface_t *image)
{
    cairo_surface_destroy (image);
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    const char *png_filename = "png.png";
    cairo_surface_t *png, *red;
    cairo_content_t content;
    int png_width, png_height;
    int i, j;

    png_dimensions (png_filename, &content, &png_width, &png_height);

    png = cairo_mime_surface_create ((void*)png_filename, content, png_width, png_height);
    cairo_mime_surface_set_acquire (png, png_acquire, release);

    red = cairo_mime_surface_create (NULL, CAIRO_CONTENT_COLOR, WIDTH, HEIGHT);
    cairo_mime_surface_set_acquire (red, red_acquire, release);

    cairo_set_source_rgb (cr, 0, 0, 1);
    cairo_paint (cr);

    cairo_translate (cr, 0, (HEIGHT-png_height)/2);
    for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) {
	    cairo_surface_t *source;
	    if ((i ^ j) & 1)
		source = red;
	    else
		source = png;
	    cairo_set_source_surface (cr, source, 0, 0);
	    cairo_rectangle (cr, i * WIDTH/4, j * png_height/4, WIDTH/4, png_height/4);
	    cairo_fill (cr);
	}
    }

    cairo_surface_destroy (red);
    cairo_surface_destroy (png);

    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
check_status (const cairo_test_context_t *ctx,
	      cairo_status_t status,
	      cairo_status_t expected)
{
    if (status == expected)
	return CAIRO_TEST_SUCCESS;

    cairo_test_log (ctx,
		    "Error: Expected status value %d (%s), received %d (%s)\n",
		    expected,
		    cairo_status_to_string (expected),
		    status,
		    cairo_status_to_string (status));
    return CAIRO_TEST_FAILURE;
}

static cairo_test_status_t
preamble (cairo_test_context_t *ctx)
{
    cairo_surface_t *mime;
    cairo_status_t status;
    cairo_t *cr;

    /* drawing to a mime-surface is verboten */

    mime = cairo_mime_surface_create (NULL, CAIRO_CONTENT_COLOR, 0, 0);
    cr = cairo_create (mime);
    cairo_surface_destroy (mime);
    status = cairo_status (cr);
    cairo_destroy (cr);
    status = check_status (ctx, status, CAIRO_STATUS_WRITE_ERROR);
    if (status)
	return status;

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (mime_surface,
	    "Check that the mime-surface embedding works",
	    "api", /* keywords */
	    NULL, /* requirements */
	    WIDTH, HEIGHT,
	    preamble, draw)

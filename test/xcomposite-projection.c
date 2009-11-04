/*
 * Copyright 2009 Benjamin Otte
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Kai-Uwe Behrmann not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Kai-Uwe Behrmann makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * BENJAMIN OTTE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL KAI_UWE BEHRMANN BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 */

#include "cairo-test.h"

static const char png_filename[] = "romedalen.png";

static cairo_surface_t *
get_red_surface (void)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  
  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 256, 192);
  
  cr = cairo_create (surface);
  cairo_set_source_rgb (cr, 0.75, 0.25, 0.25);
  cairo_paint (cr);
  cairo_destroy (cr);

  return surface;
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_surface_t *image;

    image = get_red_surface ();

    /* we don't want to debug antialiasing artifacts */
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

    /* dark grey background */
    cairo_set_source_rgb (cr, 0.25, 0.25, 0.25);
    cairo_paint (cr);

    /* magic transform */
    cairo_translate (cr, 10, -40);
    cairo_rotate (cr, -0.05);

    /* place the image on our surface */
    cairo_set_source_surface (cr, image, 0, 0);

    /* paint the image */
    cairo_rectangle (cr, 0, 0, cairo_image_surface_get_width (image), cairo_image_surface_get_height (image));
    cairo_fill (cr);

    cairo_surface_destroy (image);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (xcomposite_projection,
	    "Test a bug with XRenderComposite reference computation when projecting the first trapezoid onto 16.16 space",
	    "xlib", /* keywords */
	    NULL, /* requirements */
	    300, 150,
	    NULL, draw)

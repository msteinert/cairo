/*
 * Copyright © 2006 M Joonas Pihlaja
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
 * Author: M Joonas Pihlaja <jpihlaja@cc.helsinki.fi>
 */

/* Bug history
 *
 * 2006-12-05  M Joonas Pihlaja <jpihlaja@cc.helsinki.fi>
 *
 *  The cairo_in_fill () function can sometimes produce false
 *  positives when the tessellator produces empty trapezoids
 *  and the query point lands exactly on a trapezoid edge.
 */

#include "cairo-test.h"

static cairo_test_draw_function_t draw;

cairo_test_t test = {
    "in-fill-empty-trapezoid",
    "Tests that the tessellator doesn't produce obviously empty trapezoids",
    10, 10,
    draw
};

static cairo_test_status_t
draw (cairo_t *cr_orig, int width, int height)
{
    int x,y;
    cairo_surface_t *surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create (surf);
    cairo_set_source_rgb (cr_orig, 1, 0, 0);

    /* Empty horizontal trapezoid. */
    cairo_move_to (cr, 0, height/3);
    cairo_line_to (cr, width, height/3);
    cairo_close_path (cr);

    /* Empty non-horizontal trapezoid #1. */
    cairo_move_to (cr, 0, 0);
    cairo_line_to (cr, width, height/2);
    cairo_close_path (cr);

    /* Empty non-horizontal trapezoid #2 intersecting #1. */
    cairo_move_to (cr, 0, height/2);
    cairo_line_to (cr, width, 0);
    cairo_close_path (cr);

    /* Point sample the tessellated path. The x and y starting offsets
     * are chosen to hit the nasty bits while still being able to do a
     * relatively sparse sampling. */
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    if (cairo_in_fill (cr, x, y)) {
		cairo_rectangle(cr_orig, x, y, 1, 1);
		cairo_fill (cr_orig);
	    }
	}
    }
    cairo_destroy (cr);
    cairo_surface_destroy (surf);
    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test);
}

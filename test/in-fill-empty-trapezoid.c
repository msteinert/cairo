/*
 * Copyright © 2006 M Joonas Pihlaja
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Joonas Pihlaja
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * Joonas Pihlaja makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * JOONAS PIHLAJA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL JOONAS PIHLAJA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

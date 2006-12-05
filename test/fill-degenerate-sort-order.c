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
 *   There's currently a regression bug in the tessellation code from
 *   switching to the "new tessellator".  The bug is caused by
 *   confusion in the comparator used to order events when there are
 *   degenerate edges.
 */

#include "cairo-test.h"

static cairo_test_draw_function_t draw;

cairo_test_t test = {
    "fill-degenerate-sort-order",
    "Tests the tessellator's event comparator with degenerate input",
    190, 120,
    draw
};

/* Derived from zrusin's "another" polygon in the performance suite. */
static cairo_test_status_t
draw (cairo_t *cr_orig, int width, int height)
{
    /* XXX: I wanted to be able to simply fill the nasty path to the
     * surface and then use a reference image to catch bugs, but the
     * renderer used when testing the postscript backend gets the
     * degeneracy wrong, thus leading to an (unfixable?) test case
     * failure.  Are external renderer bugs our bugs too?  Instead,
     * tessellate the polygon and render to the surface the results of
     * point sampling the tessellated path.  If there would be a way
     * to XFAIL only some backends we could do that for the .ps
     * backend only. */
    int x,y;
    int sample_stride;
    cairo_surface_t *surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create (surf);
    cairo_set_source_rgb (cr_orig, 1, 0, 0);

    /* The polygon uses (43,103) as its "base point".  Closed
     * subpaths are simulated by going from the base point to the
     * subpath's first point, doing the subpath, and returning to the
     * base point.  The moving to and from the base point causes
     * degenerate edges which shouldn't result in anything visible. */
    cairo_move_to (cr, 43, 103);

    /* First subpath. */
    cairo_line_to (cr, 91, 101);
    cairo_line_to (cr, 0, 112);
    cairo_line_to (cr, 60, 0);
    cairo_line_to (cr, 91, 101);

    cairo_line_to (cr, 43, 103);

    /* Second subpath. */
    cairo_line_to (cr, 176, 110);
    cairo_line_to (cr, 116, 100);
    cairo_line_to (cr, 176, 0);
    cairo_line_to (cr, 176, 110);

    cairo_close_path (cr);

    /* Point sample the tessellated path. The x and y starting offsets
     * are chosen to hit the nasty bits while still being able to do a
     * relatively sparse sampling. */
    sample_stride = 4;
    for (y = 0; y < height; y += sample_stride) {
	for (x = 0; x < width; x += sample_stride) {
	    if (cairo_in_fill (cr, x, y)) {
		cairo_rectangle(cr_orig, x, y, sample_stride, sample_stride);
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

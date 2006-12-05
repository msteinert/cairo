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
 *  The tessellator has a regression where a trapezoid may continue
 *  below the end of a polygon edge (i.e. the bottom of the trapezoid
 *  is miscomputed.)  This can only happen if the right edge of a
 *  trapezoid stops earlier than the left edge and there is no start
 *  event at the end point of the right edge.
 */

#include "cairo-test.h"

static cairo_test_draw_function_t draw;
#define SIZE 50

cairo_test_t test = {
    "fill-missed-stop",
    "Tests that the tessellator doesn't miss stop events when generating trapezoids",
    SIZE+3, SIZE+3,
    draw
};

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_set_source_rgb (cr, 1, 0, 0);

    cairo_translate (cr, 1, 1);

    /* What it should look like, with # marking the filled areas:
     *
     * |\    |\
     * |#\   |#\
     * |##\__|##\
     *     \#|
     *      \|
     *
     * What it looke like with the bug, when the rightmost edge's end
     * is missed:
     *
     * |\    |\
     * |#\   |#\
     * |##\__|##\
     *     \#####|
     *      \####|
     */

    cairo_move_to (cr, 0, 0);
    cairo_line_to (cr, SIZE/2, SIZE);
    cairo_line_to (cr, SIZE/2, 0);
    cairo_line_to (cr, SIZE, SIZE/2);
    cairo_line_to (cr, 0, SIZE/2);
    cairo_close_path (cr);
    cairo_fill (cr);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test);
}

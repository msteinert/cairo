/*
 * Copyright © 2006 Jeff Muizelaar
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Jeff Muizelaar. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Jeff Muizelaar. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * JEFF MUIZELAAR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL JEFF MUIZELAAR BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Jeff Muizelaar <jeff@infidigm.net>
 */

#include "cairo-test.h"

#define IMAGE_WIDTH 19
#define IMAGE_HEIGHT 25

/* A test of the two extremes of dashing: a solid line
 * and an invisible one. Also test that capping works
 * on invisible lines.
 */

cairo_test_t test = {
    "dash-zero-length",
    "Tests cairo_set_dash with zero length",
    IMAGE_WIDTH, IMAGE_HEIGHT
};

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    double solid_line[] = { 4, 0 };
    double invisible_line[] = { 0, 4 };
    double dotted_line[] = { 0, 6 };
    double rounded_line[] = { 2, 6 };

    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_set_line_width (cr, 2);

    /* draw a solid line */
    cairo_set_dash (cr, solid_line, 2, 0);
    cairo_move_to (cr,  1, 2);
    cairo_line_to (cr, 18, 2);
    cairo_stroke (cr);

    /* draw an invisible line */
    cairo_set_dash (cr, invisible_line, 2, 0);
    cairo_move_to (cr,  1, 8);
    cairo_line_to (cr, 18, 8);
    cairo_stroke (cr);

    /* draw a dotted line */
    cairo_set_line_width (cr, 5);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_dash (cr, dotted_line, 2, 0);
    cairo_move_to (cr, 5, 13);
    cairo_line_to (cr, 18, 13);
    cairo_stroke (cr);

    /* draw a rounded line */
    cairo_set_line_width (cr, 5);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_dash (cr, rounded_line, 2, 2);
    cairo_move_to (cr, 5, 20);
    cairo_line_to (cr, 18, 20);
    cairo_stroke (cr);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test, draw);
}

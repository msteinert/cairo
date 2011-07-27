/*
 * Copyright 2011 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Chris Wilson not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Chris Wilson makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CHRIS WILSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CHRIS WILSON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Chris Wilson <chris@chris-wilson.co.uk>
 */

/* Exercises a bug spotted by Andrea Canciani where the polygon clipping
 * code was hopeless broken with horizontal edges.
 */

#include "cairo-test.h"

#define WIDTH 20
#define HEIGHT 20

#define BUG 1

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);
    cairo_set_source_rgb (cr, 0, 0, 0);

    cairo_move_to (cr, 0-256, 4 - BUG);
    cairo_line_to (cr, WIDTH + 256, 4 + BUG);
    cairo_line_to (cr, WIDTH + 256, 2 - BUG);
    cairo_line_to (cr, 0-256, 2 + BUG);
    cairo_fill (cr);

    cairo_move_to (cr, 0-256, 6 - BUG);
    cairo_line_to (cr, WIDTH + 256, 6 + BUG);
    cairo_line_to (cr, WIDTH + 256, 8 - BUG);
    cairo_line_to (cr, 0-256, 8 + BUG);
    cairo_fill (cr);

    cairo_move_to (cr, WIDTH+256, 12 - BUG);
    cairo_line_to (cr, 0-256, 12 + BUG);
    cairo_line_to (cr, 0-256, 14 - BUG);
    cairo_line_to (cr, WIDTH+256, 14 + BUG);
    cairo_fill (cr);

    cairo_move_to (cr, WIDTH+256, 18 - BUG);
    cairo_line_to (cr, 0-256, 18 + BUG);
    cairo_line_to (cr, 0-256, 16 - BUG);
    cairo_line_to (cr, WIDTH+256, 16 + BUG);
    cairo_fill (cr);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (horizontal_clip,
	    "Tests intersection of a nearly horizontal lines with a clipped polygon",
	    "clip, fill", /* keywords */
	    NULL, /* requirements */
	    WIDTH, HEIGHT,
	    NULL, draw)

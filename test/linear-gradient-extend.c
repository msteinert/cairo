/*
 * Copyright 2010 Andrea Canciani
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Andrea Canciani not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Andrea Canciani makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * ANDREA CANCIANI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL ANDREA CANCIANI BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Andrea Canciani <ranma42@gmail.com>
 */

#include "cairo-test.h"

#define NUM_EXTEND 4
#define HEIGHT 16
#define WIDTH 16
#define PAD 3

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_pattern_t *pattern;
    unsigned int i, j;

    cairo_extend_t extend[NUM_EXTEND] = {
	CAIRO_EXTEND_NONE,
	CAIRO_EXTEND_REPEAT,
	CAIRO_EXTEND_REFLECT,
	CAIRO_EXTEND_PAD
    };

    cairo_test_paint_checkered (cr);

    pattern = cairo_pattern_create_linear (0, 2*PAD, 0, HEIGHT - 2*PAD);

    cairo_pattern_add_color_stop_rgb (pattern, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgb (pattern, 1, 0, 0, 1);

    cairo_translate (cr, PAD, PAD);

    for (i = 0; i < 2; i++) {
        cairo_save (cr);
	
	for (j = 0; j < NUM_EXTEND; j++) {
	    cairo_pattern_set_extend (pattern, extend[j]);

	    cairo_reset_clip (cr);
	    cairo_rectangle (cr, 0, 0, WIDTH, HEIGHT);
	    cairo_clip (cr);

	    if (i & 1) {
	        cairo_set_source_rgb (cr, 0, 1, 0);
		cairo_mask (cr, pattern);
	    } else {
	        cairo_set_source (cr, pattern);
	        cairo_paint (cr);
	    }

	    cairo_translate (cr, WIDTH+PAD, 0);
	}

	cairo_restore (cr);
	cairo_translate (cr, 0, HEIGHT+PAD);
    }

    cairo_pattern_destroy (pattern);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (linear_gradient_extend,
	    "Tests gradient to solid reduction of linear gradients",
	    "radial, pattern, extend", /* keywords */
	    NULL, /* requirements */
	    (WIDTH+PAD) * NUM_EXTEND + PAD, 2*(HEIGHT + PAD) + PAD,
	    NULL, draw)

/*
 * Copyright 2009 Andrea Canciani
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

#define N_OPERATORS (CAIRO_OPERATOR_SATURATE + 1)
#define HEIGHT 16
#define WIDTH 16
#define PAD 3

static cairo_pattern_t*
_create_pattern (cairo_surface_t *target, cairo_content_t content, int width, int height)
{
    cairo_pattern_t *pattern;
    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_surface_create_similar (target, content, width, height);
    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_arc (cr, 0.5 * width, 0.5 * height, 0.45 * height, -M_PI / 4, 3 * M_PI / 4);
    cairo_fill (cr);

    pattern = cairo_pattern_create_for_surface (cairo_get_target (cr));
    cairo_destroy (cr);

    return pattern;
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_pattern_t *alpha_pattern, *color_alpha_pattern, *pattern;
    unsigned int n, i;

    alpha_pattern = _create_pattern (cairo_get_target (cr),
				     CAIRO_CONTENT_ALPHA,
				     0.9 * WIDTH, 0.9 * HEIGHT);
    color_alpha_pattern = _create_pattern (cairo_get_target (cr),
					   CAIRO_CONTENT_COLOR_ALPHA,
					   0.9 * WIDTH, 0.9 * HEIGHT);

    pattern = cairo_pattern_create_linear (WIDTH, 0, 0, HEIGHT);
    cairo_pattern_add_color_stop_rgba (pattern, 0.2, 0, 0, 1, 1);
    cairo_pattern_add_color_stop_rgba (pattern, 0.8, 0, 0, 1, 0);

    cairo_translate (cr, PAD, PAD);

    for (n = 0; n < N_OPERATORS; n++) {
	cairo_save (cr);
	for (i = 0; i < 4; i++) {
	    cairo_reset_clip (cr);
	    cairo_rectangle (cr, 0, 0, WIDTH, HEIGHT);
	    cairo_clip (cr);

	    cairo_set_source (cr, pattern);
	    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	    if (i & 2) {
	        cairo_paint (cr);
	    } else {
	        cairo_rectangle (cr, WIDTH/2, HEIGHT/2, WIDTH, HEIGHT);
		cairo_fill (cr);
	    }

	    cairo_set_source (cr, i & 1 ? alpha_pattern : color_alpha_pattern);
	    cairo_set_operator (cr, n);
	    if (i & 2) {
	        cairo_paint (cr);
	    } else {
	        cairo_rectangle (cr, WIDTH/2, HEIGHT/2, WIDTH, HEIGHT);
		cairo_fill (cr);
	    }

	    cairo_translate (cr, 0, HEIGHT+PAD);
	}
	cairo_restore (cr);

	cairo_translate (cr, WIDTH+PAD, 0);
    }

    cairo_pattern_destroy (pattern);
    cairo_pattern_destroy (alpha_pattern);
    cairo_pattern_destroy (color_alpha_pattern);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (surface_pattern_operator,
	    "Tests alpha-only and alpha-color sources with all operators",
	    "surface, pattern, operator", /* keywords */
	    NULL, /* requirements */
	    (WIDTH+PAD) * N_OPERATORS + PAD, 4*HEIGHT + 5*PAD,
	    NULL, draw)

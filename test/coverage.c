/*
 * Copyright 2010 Intel Corporation
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

/* Test the fidelity of the rasterisation, because Cairo is my favourite
 * driver test suite.
 */

#define WIDTH 256
#define HEIGHT 40

#include "../src/cairo-fixed-type-private.h"
#define PRECISION (1 << CAIRO_FIXED_FRAC_BITS)

static uint32_t state;

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    return state = (state ^ rol (state, 5) ^ rol (state, 24)) + 0x37798849;
#undef rol
}

static double
random_offset (int range)
{
    double x = hars_petruska_f54_1_random() / (double) UINT32_MAX * range / WIDTH;
    return floor (x * PRECISION) / PRECISION;
}

static cairo_test_status_t
rectangles (cairo_t *cr, int width, int height)
{
    int x, y, channel;

    state = 0x12345678;

    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    cairo_paint (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
    for (channel = 0; channel < 3; channel++) {
	switch (channel) {
	default:
	case 0: cairo_set_source_rgb (cr, 1.0, 0.0, 0.0); break;
	case 1: cairo_set_source_rgb (cr, 0.0, 1.0, 0.0); break;
	case 2: cairo_set_source_rgb (cr, 0.0, 0.0, 1.0); break;
	}

	for (x = 0; x < WIDTH; x++) {
	    for (y = 0; y < HEIGHT; y++) {
		double dx = random_offset (WIDTH - x);
		double dy = random_offset (WIDTH - x);
		cairo_rectangle (cr, x + dx, y + dy, x / (double) WIDTH, x / (double) WIDTH);
	    }
	}
	cairo_fill (cr);
    }

    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
triangles (cairo_t *cr, int width, int height)
{
    int x, y, channel;

    state = 0x12345678;

    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    cairo_paint (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
    for (channel = 0; channel < 3; channel++) {
	switch (channel) {
	default:
	case 0: cairo_set_source_rgb (cr, 1.0, 0.0, 0.0); break;
	case 1: cairo_set_source_rgb (cr, 0.0, 1.0, 0.0); break;
	case 2: cairo_set_source_rgb (cr, 0.0, 0.0, 1.0); break;
	}

	for (x = 0; x < WIDTH; x++) {
	    for (y = 0; y < HEIGHT; y++) {
		double dx = random_offset (WIDTH - x);
		double dy = random_offset (WIDTH - x);
		cairo_move_to (cr, x + dx, y + dy);
		cairo_rel_line_to (cr, x / (double) WIDTH, 0);
		cairo_rel_line_to (cr, 0, x / (double) WIDTH);
		cairo_close_path (cr);
	    }
	}
	cairo_fill (cr);
    }

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (coverage_rectangles,
	    "Check the fidelity of the rasterisation.",
	    NULL, /* keywords */
	    "target=raster", /* requirements */
	    WIDTH, HEIGHT,
	    NULL, rectangles)

CAIRO_TEST (coverage_triangles,
	    "Check the fidelity of the rasterisation.",
	    NULL, /* keywords */
	    "target=raster", /* requirements */
	    WIDTH, HEIGHT,
	    NULL, triangles)

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

/* Test the sampling stratagems of the rasterisers by creating pixels
 * containing minute holes and seeing how close to the expected
 * coverage each rasteriser approaches.
 */

#define SIZE 64

#include "../src/cairo-fixed-type-private.h"
#define SAMPLE (1 << CAIRO_FIXED_FRAC_BITS)

static uint32_t state;

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    return state = (state ^ rol (state, 5) ^ rol (state, 24)) + 0x37798849;
#undef rol
}

static double
uniform_random (void)
{
    return hars_petruska_f54_1_random() / (double) UINT32_MAX;
}

/* coverage is given in [0,65535] */
static void
compute_occupancy (uint8_t *occupancy, int coverage)
{
    int i, c;

    if (coverage < SAMPLE*SAMPLE/2) {
	memset (occupancy, 0, SAMPLE*SAMPLE);
	for (i = c = 0; i < SAMPLE*SAMPLE; i++) {
	    if ((SAMPLE*SAMPLE - i) * uniform_random() < coverage - c) {
		occupancy[i] = 0xff;
		if (++c == coverage)
		    return;
	    }
	}
    } else {
	coverage = SAMPLE*SAMPLE - coverage;
	memset (occupancy, 0xff, SAMPLE*SAMPLE);
	for (i = c = 0; i < SAMPLE*SAMPLE; i++) {
	    if ((SAMPLE*SAMPLE - i) * uniform_random() < coverage - c) {
		occupancy[i] = 0;
		if (++c == coverage)
		    return;
	    }
	}
    }
}

static cairo_test_status_t
reference (cairo_t *cr, int width, int height)
{
    int i;

    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    cairo_paint (cr);

    for (i = 0; i < SIZE*SIZE; i++) {
	cairo_set_source_rgba (cr, 1., 1., 1.,
			       i / (double) (SIZE * SIZE));
	cairo_rectangle (cr, i % SIZE, i / SIZE, 1, 1);
	cairo_fill (cr);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_test_status_t
rectangles (cairo_t *cr, int width, int height)
{
    uint8_t *occupancy;
    int i, j, channel;

    state = 0x12345678;
    occupancy = xmalloc (SAMPLE*SAMPLE);

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

	for (i = 0; i < SIZE*SIZE; i++) {
	    int xs, ys;

	    compute_occupancy (occupancy, SAMPLE*SAMPLE * i / (SIZE * SIZE));

	    xs = i % SIZE * SAMPLE;
	    ys = i / SIZE * SAMPLE;
	    for (j = 0; j < SAMPLE*SAMPLE; j++) {
		if (occupancy[j]) {
		    cairo_rectangle (cr,
				     (j % SAMPLE + xs) / (double) SAMPLE,
				     (j / SAMPLE + ys) / (double) SAMPLE,
				     1 / (double) SAMPLE,
				     1 / (double) SAMPLE);
		}
	    }
	    cairo_fill (cr);
	}
    }

    free (occupancy);

    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
triangles (cairo_t *cr, int width, int height)
{
    uint8_t *occupancy;
    int i, j, channel;

    state = 0x12345678;
    occupancy = xmalloc (SAMPLE*SAMPLE);

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

	for (i = 0; i < SIZE*SIZE; i++) {
	    int xs, ys;

	    compute_occupancy (occupancy, SAMPLE*SAMPLE * i / (SIZE * SIZE));

	    xs = i % SIZE * SAMPLE;
	    ys = i / SIZE * SAMPLE;
	    for (j = 0; j < SAMPLE*SAMPLE; j++) {
		if (occupancy[j]) {
		    int x = j % SAMPLE + xs;
		    int y = j / SAMPLE + ys;
		    cairo_move_to (cr, x / (double) SAMPLE, y / (double) SAMPLE);
		    cairo_line_to (cr, (x+1) / (double) SAMPLE, (y+1) / (double) SAMPLE);
		    cairo_line_to (cr, (x+1) / (double) SAMPLE, y / (double) SAMPLE);
		    cairo_close_path (cr);
		}
	    }
	    cairo_fill (cr);
	}
    }

    free (occupancy);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (partial_coverage_rectangles,
	    "Check the fidelity of the rasterisation.",
	    "coverage raster", /* keywords */
	    "raster", /* requirements */
	    SIZE, SIZE,
	    NULL, rectangles)

CAIRO_TEST (partial_coverage_triangles,
	    "Check the fidelity of the rasterisation.",
	    "coverage raster", /* keywords */
	    "raster", /* requirements */
	    SIZE, SIZE,
	    NULL, triangles)

CAIRO_TEST (partial_coverage_reference,
	    "Check the fidelity of this test.",
	    "coverage raster", /* keywords */
	    "raster", /* requirements */
	    SIZE, SIZE,
	    NULL, reference)

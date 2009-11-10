/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright 2009 Andrea Canciani
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *      Andrea Canciani <ranma42@gmail.com>
 */

#include "cairo-test.h"

#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

/* Lengths of the dashes of the dash patterns */
static const double dashes[] = { 2, 2, 4, 4 };
/* Dash offset in userspace units
 * They always grow by 2, so the dash pattern is
 * should be shifted by the same amount each time */
static const double frac_offset[] = { 0, 2, 4, 6 };
/* Dash offset relative to the whole dash pattern
 * This corresponds to the non-inverted part only if
 * the dash pattern has odd length, so the expected result
 * is the same for every int_offset if the pattern has
 * even lenght, and inverted each time (or shifted by half
 * period, which is the same) if the pattern has odd length. */
static const double int_offset[] = { -2, -1, 0, 1, 2 };

#define PAD 6
#define STROKE_LENGTH 32
#define IMAGE_WIDTH (PAD + (STROKE_LENGTH + PAD) * ARRAY_SIZE(dashes))
#define IMAGE_HEIGHT (PAD + PAD * ARRAY_SIZE(int_offset) + PAD * ARRAY_SIZE(frac_offset) * ARRAY_SIZE(int_offset))


static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    double total;
    size_t i, j, k;

    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);

    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_line_width (cr, 2);

    total = 0.0;
    for (k = 0; k < ARRAY_SIZE(dashes); ++k) {
	total += dashes[k];
	for (i = 0; i < ARRAY_SIZE(frac_offset); ++i) {
	    for (j = 0; j < ARRAY_SIZE(int_offset); ++j) {
		cairo_set_dash (cr, dashes, k + 1, frac_offset[i] + total * int_offset[j]);
		cairo_move_to (cr, (STROKE_LENGTH + PAD) * k + PAD, PAD * (i + j + ARRAY_SIZE(frac_offset) * j + 1));
		cairo_line_to (cr, (STROKE_LENGTH + PAD) * (k + 1), PAD * (i + j + ARRAY_SIZE(frac_offset) * j + 1));
		cairo_stroke (cr);
	    }
	}
    }

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (dash_offset,
	    "Tests dashes of different length with various offsets",
	    "stroke, dash", /* keywords */
	    NULL, /* requirements */
	    IMAGE_WIDTH, IMAGE_HEIGHT,
	    NULL, draw)

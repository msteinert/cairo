/*
 * Copyright 2010 Red Hat Inc.
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
 * Author: Benjamin Otte <otte@gnome.org>
 */

#include "cairo-test.h"

#define DEVICE_OFFSET -10
#define CLIP_OFFSET 25
#define CLIP_SIZE 20

#define WIDTH 50
#define HEIGHT 50

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_surface_t *similar;
    cairo_t *similar_cr;

    similar = cairo_surface_create_similar (cairo_get_target (cr), 
                                            cairo_surface_get_content (cairo_get_target (cr)),
                                            width, height);
    cairo_surface_set_device_offset (similar, DEVICE_OFFSET, DEVICE_OFFSET);

    similar_cr = cairo_create (similar);

    /* Neutral gray background */
    cairo_set_source_rgb (similar_cr, 0.51613, 0.55555, 0.51613);
    cairo_paint (similar_cr);

    /* add a rectangle */
    cairo_rectangle (similar_cr, CLIP_OFFSET, CLIP_OFFSET, CLIP_SIZE, CLIP_SIZE);

    /* clip to the rectangle */
    cairo_clip_preserve (similar_cr);

    /* push a group. We now have a device offset. */
    cairo_push_group (similar_cr);

    /* push a group again. This is where the bug used to happen. */

    /* draw something */
    cairo_set_source_rgb (similar_cr, 1, 0, 0);
    cairo_fill (similar_cr);

    /* make sure the stuff we drew ends up on the output */
    cairo_pop_group_to_source (similar_cr);
    cairo_paint (similar_cr);

    cairo_pop_group_to_source (similar_cr);
    cairo_paint (similar_cr);

    cairo_destroy (similar_cr);

    cairo_set_source_surface (cr, similar, DEVICE_OFFSET, DEVICE_OFFSET);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (push_group_path_offset,
	    "Exercises a bug in Cairo 1.9 where existing paths applied the target's"
            " device offset twice when cairo_push_group() was called.",
	    "group, path", /* keywords */
	    NULL, /* requirements */
	    WIDTH, HEIGHT,
	    NULL, draw)

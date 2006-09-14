/*
 * Copyright © 2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include "cairo-perf.h"

static int
iters_for_size (int size)
{
    if (size <= 64)
	return 8;
    else if (size <= 128)
	return 4;
    else if (size <= 256)
	return 2;
    else
	return 1;
}

static cairo_perf_ticks_t
do_paint (cairo_t *cr, int size)
{
    int i;
    int iters = iters_for_size (size);

    cairo_perf_timer_start ();

    for (i=0; i < iters; i++)
	cairo_paint (cr);

    cairo_perf_timer_stop ();

    return cairo_perf_timer_elapsed ();
}

/*
 * paint with solid color
 */

cairo_perf_ticks_t
paint_over_solid (cairo_t *cr, int width, int height)
{
    cairo_set_source_rgb (cr, 0.2, 0.6, 0.9);

    return do_paint (cr, width);
}

cairo_perf_ticks_t
paint_over_solid_alpha (cairo_t *cr, int width, int height)
{
    cairo_set_source_rgba (cr, 0.2, 0.6, 0.9, 0.7);

    return do_paint (cr, width);
}

cairo_perf_ticks_t
paint_source_solid (cairo_t *cr, int width, int height)
{
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb (cr, 0.2, 0.6, 0.9);

    return do_paint (cr, width);
}

cairo_perf_ticks_t
paint_source_solid_alpha (cairo_t *cr, int width, int height)
{
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba (cr, 0.2, 0.6, 0.9, 0.7);

    return do_paint (cr, width);
}

/*
 * paint with surface
 */


int cached_surface_width = 0;
int cached_surface_height = 0;
cairo_content_t cached_surface_content = 0;
cairo_surface_t *cached_surface = NULL;

void
ensure_cached_surface (cairo_t *cr, cairo_content_t content, int w, int h)
{
    cairo_surface_t *target_surface = cairo_get_target (cr);

    cairo_t *cr2;

    if (w == cached_surface_width && h == cached_surface_height &&
        content == cached_surface_content &&
        cached_surface &&
        cairo_surface_get_type (target_surface) == cairo_surface_get_type (cached_surface))
    {
        return;
    }

    if (cached_surface)
        cairo_surface_destroy (cached_surface);
    
    cached_surface = cairo_surface_create_similar (target_surface, content, w, h);

    cached_surface_width = w;
    cached_surface_height = h;
    cached_surface_content = content;

    /* Fill it with something known */
    cr2 = cairo_create (cached_surface);
    cairo_set_operator (cr2, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr2);

    cairo_set_operator (cr2, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb (cr2, 0, 0, 1);
    cairo_paint (cr2);

    cairo_set_source_rgba (cr2, 1, 0, 0, 0.5);
    cairo_new_path (cr2);
    cairo_rectangle (cr2, 0, 0, w/2.0, h/2.0);
    cairo_rectangle (cr2, w/2.0, h/2.0, w/2.0, h/2.0);
    cairo_fill (cr2);
    cairo_destroy (cr2);
}

cairo_perf_ticks_t
paint_over_surface_rgb24 (cairo_t *cr, int width, int height)
{
    ensure_cached_surface (cr, CAIRO_CONTENT_COLOR, width, height);

    cairo_set_source_surface (cr, cached_surface, 0, 0);

    return do_paint (cr, width);
}

cairo_perf_ticks_t
paint_over_surface_argb32 (cairo_t *cr, int width, int height)
{
    ensure_cached_surface (cr, CAIRO_CONTENT_COLOR_ALPHA, width, height);

    cairo_set_source_surface (cr, cached_surface, 0, 0);

    return do_paint (cr, width);
}

cairo_perf_ticks_t
paint_source_surface_rgb24 (cairo_t *cr, int width, int height)
{
    ensure_cached_surface (cr, CAIRO_CONTENT_COLOR, width, height);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface (cr, cached_surface, 0, 0);

    return do_paint (cr, width);
}

cairo_perf_ticks_t
paint_source_surface_argb32 (cairo_t *cr, int width, int height)
{
    ensure_cached_surface (cr, CAIRO_CONTENT_COLOR_ALPHA, width, height);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface (cr, cached_surface, 0, 0);

    return do_paint (cr, width);
}

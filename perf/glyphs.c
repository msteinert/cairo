/*
 * Copyright © 2006 Red Hat, Inc.
 * Copyright © 2009 Chris Wilson
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
 *         Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairo-perf.h"

static cairo_perf_ticks_t
do_glyphs (cairo_t *cr, int width, int height)
{
    const char text[] = "the jay, pig, fox, zebra and my wolves quack";
    cairo_scaled_font_t *scaled_font;
    cairo_glyph_t *glyphs = NULL, *glyphs_copy;
    cairo_text_extents_t extents;
    cairo_status_t status;
    double x, y;
    int num_glyphs, n;

    cairo_set_font_size (cr, 9);
    scaled_font = cairo_get_scaled_font (cr);
    status = cairo_scaled_font_text_to_glyphs (scaled_font, 0., 0.,
					       text, -1,
					       &glyphs, &num_glyphs,
					       NULL, NULL,
					       NULL);
    if (status)
	return 0;

    glyphs_copy = cairo_glyph_allocate (num_glyphs);
    if (glyphs_copy == NULL) {
	cairo_glyph_free (glyphs);
	return 0;
    }

    cairo_scaled_font_glyph_extents (scaled_font,
				     glyphs, num_glyphs,
				     &extents);
    y = 0;

    cairo_perf_timer_start ();

    do {
	x = 0;
	do {
	    for (n = 0; n < num_glyphs; n++) {
		glyphs_copy[n] = glyphs[n];
		glyphs_copy[n].x += x;
		glyphs_copy[n].y += y;
	    }
	    cairo_show_glyphs (cr, glyphs_copy, num_glyphs);
	    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
		goto out;

	    x += extents.width;
	} while (x < width);
	y += extents.height;
    } while (y < height);
out:

    cairo_perf_timer_stop ();

    cairo_glyph_free (glyphs);
    cairo_glyph_free (glyphs_copy);

    return cairo_perf_timer_elapsed ();
}

void
glyphs (cairo_perf_t *perf, cairo_t *cr, int width, int height)
{
    if (! cairo_perf_can_run (perf, "glyphs"))
	return;

    cairo_perf_cover_sources_and_operators (perf, "glyphs", do_glyphs);
}

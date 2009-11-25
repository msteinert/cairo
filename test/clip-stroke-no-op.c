/*
 * Copyright 2009 Benjamin Otte
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Benjamin Otte not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Benjamin Otte makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * BENJAMIN OTTE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL BENJAMIN OTTE BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 */

#include "cairo-test.h"

#define WIDTH 50
#define HEIGHT 50

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    /* Neutral gray background */
    cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
    cairo_paint (cr);

    /* remove this clip operation and everything works */
    cairo_rectangle (cr, 10, 10, 30, 30);
    cairo_clip (cr);

    /* remove this no-op and everything works */
    cairo_stroke (cr);

    /* make the y coordinates integers and everything works */
    cairo_move_to (cr, 20, 20.101562);
    cairo_line_to (cr, 30, 20.101562);

    /* This clip operation should fail to work. But with cairo 1.9, if all the 
     * 3 cases above happen, the clip will not work and the paint will happen.
     */
    cairo_save (cr); {
	cairo_set_source_rgba (cr, 1, 0.5, 0.5, 1);
	cairo_clip_preserve (cr);
	cairo_paint (cr);
    } cairo_restore (cr);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (clip_stroke_no_op,
	    "Exercises a bug found by Benjamin Otte whereby a no-op clip is nullified by a stroke",
	    "clip, stroke", /* keywords */
	    NULL, /* requirements */
	    WIDTH, HEIGHT,
	    NULL, draw)

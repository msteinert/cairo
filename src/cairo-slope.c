/*
 * Copyright © 2002 USC, Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include "cairoint.h"

void
_cairo_slope_init (cairo_slope_t *slope, cairo_point_t *a, cairo_point_t *b)
{
    slope->dx = b->x - a->x;
    slope->dy = b->y - a->y;
}

/* Is a clockwise of b?
 *
 * NOTE: The strict equality here is not significant in and of itself,
 * but there are functions up above that are sensitive to it,
 * (cf. _cairo_pen_find_active_cw_vertex_index).
 */
int
_cairo_slope_clockwise (cairo_slope_t *a, cairo_slope_t *b)
{
    return ((cairo_fixed_48_16_t) b->dy * (cairo_fixed_48_16_t) a->dx 
	    > (cairo_fixed_48_16_t) a->dy * (cairo_fixed_48_16_t) b->dx);
}

int
_cairo_slope_counter_clockwise (cairo_slope_t *a, cairo_slope_t *b)
{
    return ! _cairo_slope_clockwise (a, b);
}





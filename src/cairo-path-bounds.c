/*
 * Copyright © 2003 USC, Information Sciences Institute
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

typedef struct cairo_path_bounder {
    int has_pt;

    cairo_fixed_t min_x;
    cairo_fixed_t min_y;
    cairo_fixed_t max_x;
    cairo_fixed_t max_y;
} cairo_path_bounder_t;

static void
_cairo_path_bounder_init (cairo_path_bounder_t *bounder);

static void
_cairo_path_bounder_fini (cairo_path_bounder_t *bounder);

static cairo_status_t
_cairo_path_bounder_add_point (cairo_path_bounder_t *bounder, cairo_point_t *pt);

static cairo_status_t
_cairo_path_bounder_add_edge (void *closure, cairo_point_t *p1, cairo_point_t *p2);

static cairo_status_t
_cairo_path_bounder_add_spline (void *closure,
				cairo_point_t *a, cairo_point_t *b,
				cairo_point_t *c, cairo_point_t *d);

static cairo_status_t
_cairo_path_bounder_done_sub_path (void *closure, cairo_sub_path_done_t done);

static cairo_status_t
_cairo_path_bounder_done_path (void *closure);

static void
_cairo_path_bounder_init (cairo_path_bounder_t *bounder)
{
    bounder->has_pt = 0;
}

static void
_cairo_path_bounder_fini (cairo_path_bounder_t *bounder)
{
    bounder->has_pt = 0;
}

static cairo_status_t
_cairo_path_bounder_add_point (cairo_path_bounder_t *bounder, cairo_point_t *pt)
{
    if (bounder->has_pt) {
	if (pt->x < bounder->min_x)
	    bounder->min_x = pt->x;
	
	if (pt->y < bounder->min_y)
	    bounder->min_y = pt->y;
	
	if (pt->x > bounder->max_x)
	    bounder->max_x = pt->x;
	
	if (pt->y > bounder->max_y)
	    bounder->max_y = pt->y;
    } else {
	bounder->min_x = pt->x;
	bounder->min_y = pt->y;
	bounder->max_x = pt->x;
	bounder->max_y = pt->y;

	bounder->has_pt = 1;
    }
	
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_add_edge (void *closure, cairo_point_t *p1, cairo_point_t *p2)
{
    cairo_path_bounder_t *bounder = closure;

    _cairo_path_bounder_add_point (bounder, p1);
    _cairo_path_bounder_add_point (bounder, p2);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_add_spline (void *closure,
				cairo_point_t *a, cairo_point_t *b,
				cairo_point_t *c, cairo_point_t *d)
{
    cairo_path_bounder_t *bounder = closure;

    _cairo_path_bounder_add_point (bounder, a);
    _cairo_path_bounder_add_point (bounder, b);
    _cairo_path_bounder_add_point (bounder, c);
    _cairo_path_bounder_add_point (bounder, d);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_done_sub_path (void *closure, cairo_sub_path_done_t done)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_done_path (void *closure)
{
    return CAIRO_STATUS_SUCCESS;
}

/* XXX: Perhaps this should compute a PixRegion rather than 4 doubles */
cairo_status_t
_cairo_path_bounds (cairo_path_t *path, double *x1, double *y1, double *x2, double *y2)
{
    cairo_status_t status;
    static cairo_path_callbacks_t const cb = {
	_cairo_path_bounder_add_edge,
	_cairo_path_bounder_add_spline,
	_cairo_path_bounder_done_sub_path,
	_cairo_path_bounder_done_path
    };

    cairo_path_bounder_t bounder;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_interpret (path, cairo_path_direction_forward, &cb, &bounder);
    if (status) {
	*x1 = *y1 = *x2 = *y2 = 0.0;
	_cairo_path_bounder_fini (&bounder);
	return status;
    }

    *x1 = XFixedToDouble (bounder.min_x);
    *y1 = XFixedToDouble (bounder.min_y);
    *x2 = XFixedToDouble (bounder.max_x);
    *y2 = XFixedToDouble (bounder.max_y);

    _cairo_path_bounder_fini (&bounder);

    return CAIRO_STATUS_SUCCESS;
}

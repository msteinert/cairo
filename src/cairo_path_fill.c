/*
 * Copyright © 2002 University of Southern California
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

typedef struct cairo_filler {
    cairo_gstate_t *gstate;
    cairo_traps_t *traps;

    cairo_point_t current_point;

    cairo_polygon_t polygon;
} cairo_filler_t;

static void
_cairo_filler_init (cairo_filler_t *filler, cairo_gstate_t *gstate, cairo_traps_t *traps);

static void
_cairo_filler_fini (cairo_filler_t *filler);

static cairo_status_t
_cairo_filler_move_to (void *closure, cairo_point_t *point);

static cairo_status_t
_cairo_filler_line_to (void *closure, cairo_point_t *point);

static cairo_status_t
_cairo_filler_curve_to (void *closure,
			cairo_point_t *b,
			cairo_point_t *c,
			cairo_point_t *d);

static cairo_status_t
_cairo_filler_close_path (void *closure);

static void
_cairo_filler_init (cairo_filler_t *filler, cairo_gstate_t *gstate, cairo_traps_t *traps)
{
    filler->gstate = gstate;
    filler->traps = traps;

    filler->current_point.x = 0;
    filler->current_point.y = 0;

    _cairo_polygon_init (&filler->polygon);
}

static void
_cairo_filler_fini (cairo_filler_t *filler)
{
    _cairo_polygon_fini (&filler->polygon);
}

static cairo_status_t
_cairo_filler_move_to (void *closure, cairo_point_t *point)
{
    cairo_status_t status;
    cairo_filler_t *filler = closure;
    cairo_polygon_t *polygon = &filler->polygon;

    status = _cairo_polygon_close (polygon);
    if (status)
	return status;
      
    status = _cairo_polygon_move_to (polygon, point);
    if (status)
	return status;

    filler->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_filler_line_to (void *closure, cairo_point_t *point)
{
    cairo_status_t status;
    cairo_filler_t *filler = closure;
    cairo_polygon_t *polygon = &filler->polygon;

    status = _cairo_polygon_line_to (polygon, point);
    if (status)
	return status;

    filler->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_filler_curve_to (void *closure,
			cairo_point_t *b,
			cairo_point_t *c,
			cairo_point_t *d)
{
    int i;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_filler_t *filler = closure;
    cairo_polygon_t *polygon = &filler->polygon;
    cairo_gstate_t *gstate = filler->gstate;
    cairo_spline_t spline;

    status = _cairo_spline_init (&spline, &filler->current_point, b, c, d);

    if (status == CAIRO_INT_STATUS_DEGENERATE)
	return CAIRO_STATUS_SUCCESS;

    _cairo_spline_decompose (&spline, gstate->tolerance);
    if (status)
	goto CLEANUP_SPLINE;

    for (i = 1; i < spline.num_points; i++) {
	status = _cairo_polygon_line_to (polygon, &spline.points[i]);
	if (status)
	    break;
    }

  CLEANUP_SPLINE:
    _cairo_spline_fini (&spline);

    filler->current_point = *d;

    return status;
}

static cairo_status_t
_cairo_filler_close_path (void *closure)
{
    cairo_status_t status;
    cairo_filler_t *filler = closure;
    cairo_polygon_t *polygon = &filler->polygon;

    status = _cairo_polygon_close (polygon);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fill_to_traps (cairo_path_t *path, cairo_gstate_t *gstate, cairo_traps_t *traps)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_filler_t filler;

    _cairo_filler_init (&filler, gstate, traps);

    status = _cairo_path_interpret (path,
				    CAIRO_DIRECTION_FORWARD,
				    _cairo_filler_move_to,
				    _cairo_filler_line_to,
				    _cairo_filler_curve_to,
				    _cairo_filler_close_path,
				    &filler);
    if (status)
	goto BAIL;

    status = _cairo_polygon_close (&filler.polygon);
    if (status)
	goto BAIL;

    status = _cairo_traps_tessellate_polygon (filler.traps,
					      &filler.polygon,
					      filler.gstate->fill_rule);
    if (status)
	goto BAIL;

BAIL:
    _cairo_filler_fini (&filler);

    return status;
}


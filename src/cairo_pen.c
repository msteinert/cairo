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

static int
_cairo_pen_vertices_needed (double radius, double tolerance, cairo_matrix_t *matrix);

static void
_cairo_pen_compute_slopes (cairo_pen_t *pen);

static int
_slope_clockwise (cairo_slope_fixed_t *a, cairo_slope_fixed_t *b);

static int
_slope_counter_clockwise (cairo_slope_fixed_t *a, cairo_slope_fixed_t *b);

static int
_cairo_pen_vertex_compare_by_theta (const void *a, const void *b);

static cairo_status_t
_cairo_pen_stroke_spline_half (cairo_pen_t *pen, cairo_spline_t *spline, cairo_pen_stroke_direction_t dir, cairo_polygon_t *polygon);

cairo_status_t
_cairo_pen_init_empty (cairo_pen_t *pen)
{
    pen->radius = 0;
    pen->tolerance = 0;
    pen->vertex = NULL;
    pen->num_vertices = 0;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_pen_init (cairo_pen_t *pen, double radius, cairo_gstate_t *gstate)
{
    int i;
    cairo_pen_vertex *v;
    double dx, dy;

    if (pen->num_vertices) {
	/* XXX: It would be nice to notice that the pen is already properly constructed.
	   However, this test would also have to account for possible changes in the transformation
	   matrix.
	   if (pen->radius == radius && pen->tolerance == tolerance)
	   return CAIRO_STATUS_SUCCESS;
	*/
	_cairo_pen_fini (pen);
    }

    pen->radius = radius;
    pen->tolerance = gstate->tolerance;

    pen->num_vertices = _cairo_pen_vertices_needed (radius, gstate->tolerance, &gstate->ctm);
    /* number of vertices must be even */
    if (pen->num_vertices % 2)
	pen->num_vertices++;

    pen->vertex = malloc (pen->num_vertices * sizeof (cairo_pen_vertex));
    if (pen->vertex == NULL) {
	return CAIRO_STATUS_NO_MEMORY;
    }

    for (i=0; i < pen->num_vertices; i++) {
	v = &pen->vertex[i];
	v->theta = 2 * M_PI * i / (double) pen->num_vertices;
	dx = radius * cos (v->theta);
	dy = radius * sin (v->theta);
	cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);
	v->pt.x = XDoubleToFixed (dx);
	v->pt.y = XDoubleToFixed (dy);
	/* Recompute theta in device space */
	v->theta = atan2 (v->pt.y, v->pt.x);
	if (v->theta < 0)
	    v->theta += 2 * M_PI;
    }

    _cairo_pen_compute_slopes (pen);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pen_fini (cairo_pen_t *pen)
{
    free (pen->vertex);
    _cairo_pen_init_empty (pen);
}

cairo_status_t
_cairo_pen_init_copy (cairo_pen_t *pen, cairo_pen_t *other)
{
    *pen = *other;

    if (pen->num_vertices) {
	pen->vertex = malloc (pen->num_vertices * sizeof (cairo_pen_vertex));
	if (pen->vertex == NULL) {
	    return CAIRO_STATUS_NO_MEMORY;
	}
	memcpy (pen->vertex, other->vertex, pen->num_vertices * sizeof (cairo_pen_vertex));
    }

    return CAIRO_STATUS_SUCCESS;
}

static int
_cairo_pen_vertex_compare_by_theta (const void *a, const void *b)
{
    double diff;
    const cairo_pen_vertex *va = a;
    const cairo_pen_vertex *vb = b;

    diff = va->theta - vb->theta;
    if (diff < 0)
	return -1;
    else if (diff > 0)
	return 1;
    else
	return 0;
}

cairo_status_t
_cairo_pen_add_points (cairo_pen_t *pen, XPointFixed *pt, int num_pts)
{
    int i, j;
    cairo_pen_vertex *v, *v_next, *new_vertex;

    pen->num_vertices += num_pts;
    new_vertex = realloc (pen->vertex, pen->num_vertices * sizeof (cairo_pen_vertex));
    if (new_vertex == NULL) {
	pen->num_vertices -= num_pts;
	return CAIRO_STATUS_NO_MEMORY;
    }
    pen->vertex = new_vertex;

    /* initialize new vertices */
    for (i=0; i < num_pts; i++) {
	v = &pen->vertex[pen->num_vertices-(i+1)];
	v->pt = pt[i];
	v->theta = atan2 (v->pt.y, v->pt.x);
	if (v->theta < 0)
	    v->theta += 2 * M_PI;
    }

    qsort (pen->vertex, pen->num_vertices, sizeof (cairo_pen_vertex), _cairo_pen_vertex_compare_by_theta);

    /* eliminate any duplicate vertices */
    for (i=0; i < pen->num_vertices - 1; i++ ) {
	v = &pen->vertex[i];
	v_next = &pen->vertex[i+1];
	if (v->pt.x == v_next->pt.x && v->pt.y == v_next->pt.y) {
	    for (j=i+1; j < pen->num_vertices - 1; j++)
		pen->vertex[j] = pen->vertex[j+1];
	    pen->num_vertices--;
	    /* There may be more of the same duplicate, check again */
	    i--;
	}
    }

    _cairo_pen_compute_slopes (pen);

    return CAIRO_STATUS_SUCCESS;
}

static int
_cairo_pen_vertices_needed (double radius, double tolerance, cairo_matrix_t *matrix)
{
    double expansion, theta;

    /* The determinant represents the area expansion factor of the
       transform. In the worst case, this is entirely in one
       dimension, which is what we assume here. */

    _cairo_matrix_compute_determinant (matrix, &expansion);

    if (tolerance > expansion*radius) {
	return 4;
    }

    theta = acos (1 - tolerance/(expansion * radius));
    return ceil (M_PI / theta);
}

static void
_cairo_pen_compute_slopes (cairo_pen_t *pen)
{
    int i, i_prev;
    cairo_pen_vertex *prev, *v, *next;

    for (i=0, i_prev = pen->num_vertices - 1;
	 i < pen->num_vertices;
	 i_prev = i++) {
	prev = &pen->vertex[i_prev];
	v = &pen->vertex[i];
	next = &pen->vertex[(i + 1) % pen->num_vertices];

	_compute_slope (&prev->pt, &v->pt, &v->slope_cw);
	_compute_slope (&v->pt, &next->pt, &v->slope_ccw);
    }
}

/* Is a clockwise of b?
 *
 * NOTE: The strict equality here is not significant in and of itself,
 * but there are functions up above that are sensitive to it,
 * (cf. _cairo_pen_find_active_cw_vertex_index).
 */
static int
_slope_clockwise (cairo_slope_fixed_t *a, cairo_slope_fixed_t *b)
{
    double a_dx = XFixedToDouble (a->dx);
    double a_dy = XFixedToDouble (a->dy);
    double b_dx = XFixedToDouble (b->dx);
    double b_dy = XFixedToDouble (b->dy);

    return b_dy * a_dx > a_dy * b_dx;
}

static int
_slope_counter_clockwise (cairo_slope_fixed_t *a, cairo_slope_fixed_t *b)
{
    return ! _slope_clockwise (a, b);
}

/* Find active pen vertex for clockwise edge of stroke at the given slope.
 *
 * NOTE: The behavior of this function is sensitive to the sense of
 * the inequality within _slope_clockwise/_slope_counter_clockwise.
 *
 * The issue is that the slope_ccw member of one pen vertex will be
 * equivalent to the slope_cw member of the next pen vertex in a
 * counterclockwise order. However, for this function, we care
 * strongly about which vertex is returned.
 */
cairo_status_t
_cairo_pen_find_active_cw_vertex_index (cairo_pen_t *pen,
					cairo_slope_fixed_t *slope,
					int *active)
{
    int i;

    for (i=0; i < pen->num_vertices; i++) {
	if (_slope_clockwise (slope, &pen->vertex[i].slope_ccw)
	    && _slope_counter_clockwise (slope, &pen->vertex[i].slope_cw))
	    break;
    }

    *active = i;

    return CAIRO_STATUS_SUCCESS;
}

/* Find active pen vertex for counterclockwise edge of stroke at the given slope.
 *
 * NOTE: The behavior of this function is sensitive to the sense of
 * the inequality within _slope_clockwise/_slope_counter_clockwise.
 */
cairo_status_t
_cairo_pen_find_active_ccw_vertex_index (cairo_pen_t *pen,
					 cairo_slope_fixed_t *slope,
					 int *active)
{
    int i;
    cairo_slope_fixed_t slope_reverse;

    slope_reverse = *slope;
    slope_reverse.dx = -slope_reverse.dx;
    slope_reverse.dy = -slope_reverse.dy;

    for (i=pen->num_vertices-1; i >= 0; i--) {
	if (_slope_counter_clockwise (&pen->vertex[i].slope_ccw, &slope_reverse)
	    && _slope_clockwise (&pen->vertex[i].slope_cw, &slope_reverse))
	    break;
    }

    *active = i;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pen_stroke_spline_half (cairo_pen_t *pen,
			       cairo_spline_t *spline,
			       cairo_pen_stroke_direction_t dir,
			       cairo_polygon_t *polygon)
{
    int i;
    cairo_status_t status;
    int start, stop, step;
    int active = 0;
    XPointFixed hull_pt;
    cairo_slope_fixed_t slope, initial_slope, final_slope;
    XPointFixed *pt = spline->pts;
    int num_pts = spline->num_pts;

    if (dir == cairo_pen_stroke_direction_forward) {
	start = 0;
	stop = num_pts;
	step = 1;
	initial_slope = spline->initial_slope;
	final_slope = spline->final_slope;
    } else {
	start = num_pts - 1;
	stop = -1;
	step = -1;
	initial_slope = spline->final_slope;
	initial_slope.dx = -initial_slope.dx;
	initial_slope.dy = -initial_slope.dy;
	final_slope = spline->initial_slope;
	final_slope.dx = -final_slope.dx; 
	final_slope.dy = -final_slope.dy; 
    }

    _cairo_pen_find_active_cw_vertex_index (pen, &initial_slope, &active);

    i = start;
    while (i != stop) {
	hull_pt.x = pt[i].x + pen->vertex[active].pt.x;
	hull_pt.y = pt[i].y + pen->vertex[active].pt.y;
	status = _cairo_polygon_add_point (polygon, &hull_pt);
	if (status)
	    return status;

	if (i + step == stop)
	    slope = final_slope;
	else
	    _compute_slope (&pt[i], &pt[i+step], &slope);
	if (_slope_counter_clockwise (&slope, &pen->vertex[active].slope_ccw)) {
	    if (++active == pen->num_vertices)
		active = 0;
	} else if (_slope_clockwise (&slope, &pen->vertex[active].slope_cw)) {
	    if (--active == -1)
		active = pen->num_vertices - 1;
	} else {
	    i += step;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Compute outline of a given spline using the pen.
   The trapezoids needed to fill that outline will be added to traps
*/
cairo_status_t
_cairo_pen_stroke_spline (cairo_pen_t		*pen,
			  cairo_spline_t	*spline,
			  double		tolerance,
			  cairo_traps_t		*traps)
{
    cairo_status_t status;
    cairo_polygon_t polygon;

    _cairo_polygon_init (&polygon);

    status = _cairo_spline_decompose (spline, tolerance);
    if (status)
	return status;

    status = _cairo_pen_stroke_spline_half (pen, spline, cairo_pen_stroke_direction_forward, &polygon);
    if (status)
	return status;

    status = _cairo_pen_stroke_spline_half (pen, spline, cairo_pen_stroke_direction_reverse, &polygon);
    if (status)
	return status;

    _cairo_polygon_close (&polygon);
    cairo_traps_tessellate_polygon (traps, &polygon, CAIRO_FILL_RULE_WINDING);
    _cairo_polygon_fini (&polygon);
    
    return CAIRO_STATUS_SUCCESS;
}

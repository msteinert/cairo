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

static int
_cairo_pen_vertices_needed (double radius, double tolerance, double expansion);

static void
_cairo_pen_compute_slopes (cairo_pen_t *pen);

static cairo_status_t
_cairo_pen_stroke_spline_half (cairo_pen_t *pen, cairo_spline_t *spline, cairo_direction_t dir, cairo_polygon_t *polygon);

cairo_status_t
_cairo_pen_init_empty (cairo_pen_t *pen)
{
    pen->radius = 0;
    pen->tolerance = 0;
    pen->vertices = NULL;
    pen->num_vertices = 0;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_pen_init (cairo_pen_t *pen, double radius, cairo_gstate_t *gstate)
{
    int i;
    int reflect;
    double det, expansion;

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

    /* The determinant represents the area expansion factor of the
       transform. In the worst case, this is entirely in one
       dimension, which is what we assume here. */

    _cairo_matrix_compute_determinant (&gstate->ctm, &det);
    if (det >= 0) {
	reflect = 0;
	expansion = det;
    } else {
	reflect = 1;
	expansion = -det;
    }
    
    pen->num_vertices = _cairo_pen_vertices_needed (radius, gstate->tolerance, expansion);
    /* number of vertices must be even */
    if (pen->num_vertices % 2)
	pen->num_vertices++;

    pen->vertices = malloc (pen->num_vertices * sizeof (cairo_pen_vertex_t));
    if (pen->vertices == NULL) {
	return CAIRO_STATUS_NO_MEMORY;
    }

    /*
     * Compute pen coordinates.  To generate the right ellipse, compute points around
     * a circle in user space and transform them to device space.  To get a consistent
     * orientation in device space, flip the pen if the transformation matrix
     * is reflecting
     */
    for (i=0; i < pen->num_vertices; i++) {
	double theta = 2 * M_PI * i / (double) pen->num_vertices;
	double dx = radius * cos (reflect ? -theta : theta);
	double dy = radius * sin (reflect ? -theta : theta);
	cairo_pen_vertex_t *v = &pen->vertices[i];
	cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);
	v->point.x = _cairo_fixed_from_double (dx);
	v->point.y = _cairo_fixed_from_double (dy);
    }

    _cairo_pen_compute_slopes (pen);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pen_fini (cairo_pen_t *pen)
{
    free (pen->vertices);
    pen->vertices = NULL;

    _cairo_pen_init_empty (pen);
}

cairo_status_t
_cairo_pen_init_copy (cairo_pen_t *pen, cairo_pen_t *other)
{
    *pen = *other;

    if (pen->num_vertices) {
	pen->vertices = malloc (pen->num_vertices * sizeof (cairo_pen_vertex_t));
	if (pen->vertices == NULL) {
	    return CAIRO_STATUS_NO_MEMORY;
	}
	memcpy (pen->vertices, other->vertices, pen->num_vertices * sizeof (cairo_pen_vertex_t));
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_pen_add_points (cairo_pen_t *pen, cairo_point_t *point, int num_points)
{
    cairo_pen_vertex_t *vertices;
    int num_vertices;
    int i;

    num_vertices = pen->num_vertices + num_points;
    vertices = realloc (pen->vertices, num_vertices * sizeof (cairo_pen_vertex_t));
    if (vertices == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    pen->vertices = vertices;
    pen->num_vertices = num_vertices;

    /* initialize new vertices */
    for (i=0; i < num_points; i++)
	pen->vertices[pen->num_vertices-num_points+i].point = point[i];

    _cairo_hull_compute (pen->vertices, &pen->num_vertices);

    _cairo_pen_compute_slopes (pen);

    return CAIRO_STATUS_SUCCESS;
}

static int
_cairo_pen_vertices_needed (double radius, double tolerance, double expansion)
{
    double theta;

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
    cairo_pen_vertex_t *prev, *v, *next;

    for (i=0, i_prev = pen->num_vertices - 1;
	 i < pen->num_vertices;
	 i_prev = i++) {
	prev = &pen->vertices[i_prev];
	v = &pen->vertices[i];
	next = &pen->vertices[(i + 1) % pen->num_vertices];

	_cairo_slope_init (&v->slope_cw, &prev->point, &v->point);
	_cairo_slope_init (&v->slope_ccw, &v->point, &next->point);
    }
}

/* Find active pen vertex for clockwise edge of stroke at the given slope.
 *
 * NOTE: The behavior of this function is sensitive to the sense of
 * the inequality within _cairo_slope_clockwise/_cairo_slope_counter_clockwise.
 *
 * The issue is that the slope_ccw member of one pen vertex will be
 * equivalent to the slope_cw member of the next pen vertex in a
 * counterclockwise order. However, for this function, we care
 * strongly about which vertex is returned.
 */
cairo_status_t
_cairo_pen_find_active_cw_vertex_index (cairo_pen_t *pen,
					cairo_slope_t *slope,
					int *active)
{
    int i;

    for (i=0; i < pen->num_vertices; i++) {
	if (_cairo_slope_clockwise (slope, &pen->vertices[i].slope_ccw)
	    && _cairo_slope_counter_clockwise (slope, &pen->vertices[i].slope_cw))
	    break;
    }

    *active = i;

    return CAIRO_STATUS_SUCCESS;
}

/* Find active pen vertex for counterclockwise edge of stroke at the given slope.
 *
 * NOTE: The behavior of this function is sensitive to the sense of
 * the inequality within _cairo_slope_clockwise/_cairo_slope_counter_clockwise.
 */
cairo_status_t
_cairo_pen_find_active_ccw_vertex_index (cairo_pen_t *pen,
					 cairo_slope_t *slope,
					 int *active)
{
    int i;
    cairo_slope_t slope_reverse;

    slope_reverse = *slope;
    slope_reverse.dx = -slope_reverse.dx;
    slope_reverse.dy = -slope_reverse.dy;

    for (i=pen->num_vertices-1; i >= 0; i--) {
	if (_cairo_slope_counter_clockwise (&pen->vertices[i].slope_ccw, &slope_reverse)
	    && _cairo_slope_clockwise (&pen->vertices[i].slope_cw, &slope_reverse))
	    break;
    }

    *active = i;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pen_stroke_spline_half (cairo_pen_t *pen,
			       cairo_spline_t *spline,
			       cairo_direction_t dir,
			       cairo_polygon_t *polygon)
{
    int i;
    cairo_status_t status;
    int start, stop, step;
    int active = 0;
    cairo_point_t hull_point;
    cairo_slope_t slope, initial_slope, final_slope;
    cairo_point_t *point = spline->points;
    int num_points = spline->num_points;

    if (dir == CAIRO_DIRECTION_FORWARD) {
	start = 0;
	stop = num_points;
	step = 1;
	initial_slope = spline->initial_slope;
	final_slope = spline->final_slope;
    } else {
	start = num_points - 1;
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
	hull_point.x = point[i].x + pen->vertices[active].point.x;
	hull_point.y = point[i].y + pen->vertices[active].point.y;
	status = _cairo_polygon_line_to (polygon, &hull_point);
	if (status)
	    return status;

	if (i + step == stop)
	    slope = final_slope;
	else
	    _cairo_slope_init (&slope, &point[i], &point[i+step]);
	if (_cairo_slope_counter_clockwise (&slope, &pen->vertices[active].slope_ccw)) {
	    if (++active == pen->num_vertices)
		active = 0;
	} else if (_cairo_slope_clockwise (&slope, &pen->vertices[active].slope_cw)) {
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

    /* If the line width is so small that the pen is reduced to a
       single point, then we have nothing to do. */
    if (pen->num_vertices <= 1)
	return CAIRO_STATUS_SUCCESS;

    _cairo_polygon_init (&polygon);

    status = _cairo_spline_decompose (spline, tolerance);
    if (status)
	return status;

    status = _cairo_pen_stroke_spline_half (pen, spline, CAIRO_DIRECTION_FORWARD, &polygon);
    if (status)
	return status;

    status = _cairo_pen_stroke_spline_half (pen, spline, CAIRO_DIRECTION_REVERSE, &polygon);
    if (status)
	return status;

    _cairo_polygon_close (&polygon);
    _cairo_traps_tessellate_polygon (traps, &polygon, CAIRO_FILL_RULE_WINDING);
    _cairo_polygon_fini (&polygon);
    
    return CAIRO_STATUS_SUCCESS;
}

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

static cairo_status_t
_cairo_spline_grow_by (cairo_spline_t *spline, int additional);

static cairo_status_t
_cairo_spline_add_point (cairo_spline_t *spline, cairo_point_t *point);

static void
_lerp_half (cairo_point_t *a, cairo_point_t *b, cairo_point_t *result);

static void
_de_casteljau (cairo_spline_t *spline, cairo_spline_t *s1, cairo_spline_t *s2);

static double
_cairo_spline_error_squared (cairo_spline_t *spline);

static cairo_status_t
_cairo_spline_decompose_into (cairo_spline_t *spline, double tolerance_squared, cairo_spline_t *result);

#define CAIRO_SPLINE_GROWTH_INC 100

cairo_int_status_t
_cairo_spline_init (cairo_spline_t *spline,
		    cairo_point_t *a, cairo_point_t *b,
		    cairo_point_t *c, cairo_point_t *d)
{
    spline->a = *a;
    spline->b = *b;
    spline->c = *c;
    spline->d = *d;

    if (a->x != b->x || a->y != b->y) {
	_cairo_slope_init (&spline->initial_slope, &spline->a, &spline->b);
    } else if (a->x != c->x || a->y != c->y) {
	_cairo_slope_init (&spline->initial_slope, &spline->a, &spline->c);
    } else if (a->x != d->x || a->y != d->y) {
	_cairo_slope_init (&spline->initial_slope, &spline->a, &spline->d);
    } else {
	return CAIRO_INT_STATUS_DEGENERATE;
    }

    if (c->x != d->x || c->y != d->y) {
	_cairo_slope_init (&spline->final_slope, &spline->c, &spline->d);
    } else if (b->x != d->x || b->y != d->y) {
	_cairo_slope_init (&spline->final_slope, &spline->b, &spline->d);
    } else {
	_cairo_slope_init (&spline->final_slope, &spline->a, &spline->d);
    }

    spline->num_points = 0;
    spline->points_size = 0;
    spline->points = NULL;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_spline_fini (cairo_spline_t *spline)
{
    spline->num_points = 0;
    spline->points_size = 0;
    free (spline->points);
    spline->points = NULL;
}

static cairo_status_t
_cairo_spline_grow_by (cairo_spline_t *spline, int additional)
{
    cairo_point_t *new_points;
    int old_size = spline->points_size;
    int new_size = spline->num_points + additional;

    if (new_size <= spline->points_size)
	return CAIRO_STATUS_SUCCESS;

    spline->points_size = new_size;
    new_points = realloc (spline->points, spline->points_size * sizeof (cairo_point_t));

    if (new_points == NULL) {
	spline->points_size = old_size;
	return CAIRO_STATUS_NO_MEMORY;
    }

    spline->points = new_points;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_spline_add_point (cairo_spline_t *spline, cairo_point_t *point)
{
    cairo_status_t status;
    cairo_point_t *prev;

    if (spline->num_points) {
	prev = &spline->points[spline->num_points - 1];
	if (prev->x == point->x && prev->y == point->y)
	    return CAIRO_STATUS_SUCCESS;
    }

    if (spline->num_points >= spline->points_size) {
	status = _cairo_spline_grow_by (spline, CAIRO_SPLINE_GROWTH_INC);
	if (status)
	    return status;
    }

    spline->points[spline->num_points] = *point;
    spline->num_points++;

    return CAIRO_STATUS_SUCCESS;
}

static void
_lerp_half (cairo_point_t *a, cairo_point_t *b, cairo_point_t *result)
{
    result->x = a->x + ((b->x - a->x) >> 1);
    result->y = a->y + ((b->y - a->y) >> 1);
}

static void
_de_casteljau (cairo_spline_t *spline, cairo_spline_t *s1, cairo_spline_t *s2)
{
    cairo_point_t ab, bc, cd;
    cairo_point_t abbc, bccd;
    cairo_point_t final;

    _lerp_half (&spline->a, &spline->b, &ab);
    _lerp_half (&spline->b, &spline->c, &bc);
    _lerp_half (&spline->c, &spline->d, &cd);
    _lerp_half (&ab, &bc, &abbc);
    _lerp_half (&bc, &cd, &bccd);
    _lerp_half (&abbc, &bccd, &final);

    s1->a = spline->a;
    s1->b = ab;
    s1->c = abbc;
    s1->d = final;

    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = spline->d;
}

static double
_PointDistanceSquaredToPoint (cairo_point_t *a, cairo_point_t *b)
{
    double dx = _cairo_fixed_to_double (b->x - a->x);
    double dy = _cairo_fixed_to_double (b->y - a->y);

    return dx*dx + dy*dy;
}

static double
_PointDistanceSquaredToSegment (cairo_point_t *p, cairo_point_t *p1, cairo_point_t *p2)
{
    double u;
    double dx, dy;
    double pdx, pdy;
    cairo_point_t px;

    /* intersection point (px):

       px = p1 + u(p2 - p1)
       (p - px) . (p2 - p1) = 0

       Thus:

       u = ((p - p1) . (p2 - p1)) / (||(p2 - p1)|| ^ 2);
    */

    dx = _cairo_fixed_to_double (p2->x - p1->x);
    dy = _cairo_fixed_to_double (p2->y - p1->y);

    if (dx == 0 && dy == 0)
	return _PointDistanceSquaredToPoint (p, p1);

    pdx = _cairo_fixed_to_double (p->x - p1->x);
    pdy = _cairo_fixed_to_double (p->y - p1->y);

    u = (pdx * dx + pdy * dy) / (dx*dx + dy*dy);

    if (u <= 0)
	return _PointDistanceSquaredToPoint (p, p1);
    else if (u >= 1)
	return _PointDistanceSquaredToPoint (p, p2);

    px.x = p1->x + u * (p2->x - p1->x);
    px.y = p1->y + u * (p2->y - p1->y);

    return _PointDistanceSquaredToPoint (p, &px);
}

/* Return an upper bound on the error (squared) that could result from approximating
   a spline as a line segment connecting the two endpoints */
static double
_cairo_spline_error_squared (cairo_spline_t *spline)
{
    double berr, cerr;

    berr = _PointDistanceSquaredToSegment (&spline->b, &spline->a, &spline->d);
    cerr = _PointDistanceSquaredToSegment (&spline->c, &spline->a, &spline->d);

    if (berr > cerr)
	return berr;
    else
	return cerr;
}

static cairo_status_t
_cairo_spline_decompose_into (cairo_spline_t *spline, double tolerance_squared, cairo_spline_t *result)
{
    cairo_status_t status;
    cairo_spline_t s1, s2;

    if (_cairo_spline_error_squared (spline) < tolerance_squared) {
	return _cairo_spline_add_point (result, &spline->a);
    }

    _de_casteljau (spline, &s1, &s2);

    status = _cairo_spline_decompose_into (&s1, tolerance_squared, result);
    if (status)
	return status;
    
    status = _cairo_spline_decompose_into (&s2, tolerance_squared, result);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_spline_decompose (cairo_spline_t *spline, double tolerance)
{
    cairo_status_t status;

    if (spline->points_size) {
	_cairo_spline_fini (spline);
    }

    status = _cairo_spline_decompose_into (spline, tolerance * tolerance, spline);
    if (status)
	return status;

    status = _cairo_spline_add_point (spline, &spline->d);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}


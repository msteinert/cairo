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

static cairo_status_t
_cairo_spline_grow_by (cairo_spline_t *spline, int additional);

static cairo_status_t
_cairo_spline_add_point (cairo_spline_t *spline, XPointFixed *pt);

static void
_lerp_half (XPointFixed *a, XPointFixed *b, XPointFixed *result);

static void
_de_casteljau (cairo_spline_t *spline, cairo_spline_t *s1, cairo_spline_t *s2);

static double
_cairo_spline_error_squared (cairo_spline_t *spline);

static cairo_status_t
_cairo_spline_decompose_into (cairo_spline_t *spline, double tolerance_squared, cairo_spline_t *result);

#define CAIRO_SPLINE_GROWTH_INC 100

cairo_int_status
_cairo_spline_init (cairo_spline_t *spline, XPointFixed *a,  XPointFixed *b,  XPointFixed *c,  XPointFixed *d)
{
    spline->a = *a;
    spline->b = *b;
    spline->c = *c;
    spline->d = *d;

    if (a->x != b->x || a->y != b->y) {
	_compute_slope (&spline->a, &spline->b, &spline->initial_slope);
    } else if (a->x != c->x || a->y != c->y) {
	_compute_slope (&spline->a, &spline->c, &spline->initial_slope);
    } else if (a->x != d->x || a->y != d->y) {
	_compute_slope (&spline->a, &spline->d, &spline->initial_slope);
    } else {
	return cairo_int_status_degenerate;
    }

    if (c->x != d->x || c->y != d->y) {
	_compute_slope (&spline->c, &spline->d, &spline->final_slope);
    } else if (b->x != d->x || b->y != d->y) {
	_compute_slope (&spline->b, &spline->d, &spline->final_slope);
    } else {
	_compute_slope (&spline->a, &spline->d, &spline->final_slope);
    }

    spline->num_pts = 0;
    spline->pts_size = 0;
    spline->pts = NULL;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_spline_fini (cairo_spline_t *spline)
{
    spline->num_pts = 0;
    spline->pts_size = 0;
    free (spline->pts);
    spline->pts = NULL;
}

static cairo_status_t
_cairo_spline_grow_by (cairo_spline_t *spline, int additional)
{
    XPointFixed *new_pts;
    int old_size = spline->pts_size;
    int new_size = spline->num_pts + additional;

    if (new_size <= spline->pts_size)
	return CAIRO_STATUS_SUCCESS;

    spline->pts_size = new_size;
    new_pts = realloc (spline->pts, spline->pts_size * sizeof (XPointFixed));

    if (new_pts == NULL) {
	spline->pts_size = old_size;
	return CAIRO_STATUS_NO_MEMORY;
    }

    spline->pts = new_pts;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_spline_add_point (cairo_spline_t *spline, XPointFixed *pt)
{
    cairo_status_t status;

    if (spline->num_pts >= spline->pts_size) {
	status = _cairo_spline_grow_by (spline, CAIRO_SPLINE_GROWTH_INC);
	if (status)
	    return status;
    }

    spline->pts[spline->num_pts] = *pt;
    spline->num_pts++;

    return CAIRO_STATUS_SUCCESS;
}

static void
_lerp_half (XPointFixed *a, XPointFixed *b, XPointFixed *result)
{
    result->x = a->x + ((b->x - a->x) >> 1);
    result->y = a->y + ((b->y - a->y) >> 1);
}

static void
_de_casteljau (cairo_spline_t *spline, cairo_spline_t *s1, cairo_spline_t *s2)
{
    XPointFixed ab, bc, cd;
    XPointFixed abbc, bccd;
    XPointFixed final;

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
_PointDistanceSquaredToPoint (XPointFixed *a, XPointFixed *b)
{
    double dx = XFixedToDouble (b->x - a->x);
    double dy = XFixedToDouble (b->y - a->y);

    return dx*dx + dy*dy;
}

static double
_PointDistanceSquaredToSegment (XPointFixed *p, XPointFixed *p1, XPointFixed *p2)
{
    double u;
    double dx, dy;
    double pdx, pdy;
    XPointFixed px;

    /* intersection point (px):

       px = p1 + u(p2 - p1)
       (p - px) . (p2 - p1) = 0

       Thus:

       u = ((p - p1) . (p2 - p1)) / (||(p2 - p1)|| ^ 2);
    */

    dx = XFixedToDouble (p2->x - p1->x);
    dy = XFixedToDouble (p2->y - p1->y);

    if (dx == 0 && dy == 0)
	return _PointDistanceSquaredToPoint (p, p1);

    pdx = XFixedToDouble (p->x - p1->x);
    pdy = XFixedToDouble (p->y - p1->y);

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

    if (spline->pts_size) {
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


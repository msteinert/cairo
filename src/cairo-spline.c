/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

cairo_bool_t
_cairo_spline_init (cairo_spline_t *spline,
		    cairo_spline_add_point_func_t add_point_func,
		    void *closure,
		    const cairo_point_t *a, const cairo_point_t *b,
		    const cairo_point_t *c, const cairo_point_t *d)
{
    spline->add_point_func = add_point_func;
    spline->closure = closure;

    spline->knots.a = *a;
    spline->knots.b = *b;
    spline->knots.c = *c;
    spline->knots.d = *d;

    if (a->x != b->x || a->y != b->y)
	_cairo_slope_init (&spline->initial_slope, &spline->knots.a, &spline->knots.b);
    else if (a->x != c->x || a->y != c->y)
	_cairo_slope_init (&spline->initial_slope, &spline->knots.a, &spline->knots.c);
    else if (a->x != d->x || a->y != d->y)
	_cairo_slope_init (&spline->initial_slope, &spline->knots.a, &spline->knots.d);
    else
	return FALSE;

    if (c->x != d->x || c->y != d->y)
	_cairo_slope_init (&spline->final_slope, &spline->knots.c, &spline->knots.d);
    else if (b->x != d->x || b->y != d->y)
	_cairo_slope_init (&spline->final_slope, &spline->knots.b, &spline->knots.d);
    else
	_cairo_slope_init (&spline->final_slope, &spline->knots.a, &spline->knots.d);

    return TRUE;
}

static cairo_status_t
_cairo_spline_add_point (cairo_spline_t *spline, cairo_point_t *point)
{
    cairo_point_t *prev;

    prev = &spline->last_point;
    if (prev->x == point->x && prev->y == point->y)
	return CAIRO_STATUS_SUCCESS;

    spline->last_point = *point;
    return spline->add_point_func (spline->closure, point);
}

static void
_lerp_half (const cairo_point_t *a, const cairo_point_t *b, cairo_point_t *result)
{
    result->x = a->x + ((b->x - a->x) >> 1);
    result->y = a->y + ((b->y - a->y) >> 1);
}

static void
_de_casteljau (cairo_spline_knots_t *s1, cairo_spline_knots_t *s2)
{
    cairo_point_t ab, bc, cd;
    cairo_point_t abbc, bccd;
    cairo_point_t final;

    _lerp_half (&s1->a, &s1->b, &ab);
    _lerp_half (&s1->b, &s1->c, &bc);
    _lerp_half (&s1->c, &s1->d, &cd);
    _lerp_half (&ab, &bc, &abbc);
    _lerp_half (&bc, &cd, &bccd);
    _lerp_half (&abbc, &bccd, &final);

    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = s1->d;

    s1->b = ab;
    s1->c = abbc;
    s1->d = final;
}

/* Return an upper bound on the error (squared) that could result from
 * approximating a spline as a line segment connecting the two endpoints. */
static double
_cairo_spline_error_squared (const cairo_spline_knots_t *knots)
{
    double bdx, bdy, berr;
    double cdx, cdy, cerr;

    /* Intersection point (px):
     *	    px = p1 + u(p2 - p1)
     *	    (p - px) ∙ (p2 - p1) = 0
     * Thus:
     *	    u = ((p - p1) ∙ (p2 - p1)) / ∥p2 - p1∥²;
     */
    bdx = _cairo_fixed_to_double (knots->b.x - knots->a.x);
    bdy = _cairo_fixed_to_double (knots->b.y - knots->a.y);

    cdx = _cairo_fixed_to_double (knots->c.x - knots->a.x);
    cdy = _cairo_fixed_to_double (knots->c.y - knots->a.y);

    if (knots->a.x != knots->d.x || knots->a.y != knots->d.y) {
	double dx, dy, u, v;

	dx = _cairo_fixed_to_double (knots->d.x - knots->a.x);
	dy = _cairo_fixed_to_double (knots->d.y - knots->a.y);
	 v = dx * dx + dy * dy;

	u = bdx * dx + bdy * dy;
	if (u <= 0) {
	    /* bdx -= 0;
	     * bdy -= 0;
	     */
	} else if (u >= v) {
	    bdx -= dx;
	    bdy -= dy;
	} else {
	    bdx -= u/v * dx;
	    bdy -= u/v * dy;
	}

	u = cdx * dx + cdy * dy;
	if (u <= 0) {
	    /* cdx -= 0;
	     * cdy -= 0;
	     */
	} else if (u >= v) {
	    cdx -= dx;
	    cdy -= dy;
	} else {
	    cdx -= u/v * dx;
	    cdy -= u/v * dy;
	}
    }

    berr = bdx * bdx + bdy * bdy;
    cerr = cdx * cdx + cdy * cdy;
    if (berr > cerr)
	return berr;
    else
	return cerr;
}

static cairo_status_t
_cairo_spline_decompose_into (cairo_spline_knots_t *s1, double tolerance_squared, cairo_spline_t *result)
{
    cairo_spline_knots_t s2;
    cairo_status_t status;

    if (_cairo_spline_error_squared (s1) < tolerance_squared)
	return _cairo_spline_add_point (result, &s1->a);

    _de_casteljau (s1, &s2);

    status = _cairo_spline_decompose_into (s1, tolerance_squared, result);
    if (unlikely (status))
	return status;

    return _cairo_spline_decompose_into (&s2, tolerance_squared, result);
}

cairo_status_t
_cairo_spline_decompose (cairo_spline_t *spline, double tolerance)
{
    cairo_spline_knots_t s1;
    cairo_status_t status;

    s1 = spline->knots;
    spline->last_point = s1.a;
    status = _cairo_spline_decompose_into (&s1, tolerance * tolerance, spline);
    if (unlikely (status))
	return status;

    return _cairo_spline_add_point (spline, &spline->knots.d);
}

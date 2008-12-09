/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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

typedef struct cairo_path_bounder {
    cairo_point_t move_to_point;
    cairo_bool_t has_move_to_point;
    cairo_bool_t has_point;

    cairo_fixed_t min_x;
    cairo_fixed_t min_y;
    cairo_fixed_t max_x;
    cairo_fixed_t max_y;
} cairo_path_bounder_t;

static void
_cairo_path_bounder_init (cairo_path_bounder_t *bounder)
{
    bounder->has_move_to_point = FALSE;
    bounder->has_point = FALSE;
}

static void
_cairo_path_bounder_fini (cairo_path_bounder_t *bounder)
{
    bounder->has_move_to_point = FALSE;
    bounder->has_point = FALSE;
}

static void
_cairo_path_bounder_add_point (cairo_path_bounder_t *bounder,
			       const cairo_point_t *point)
{
    if (bounder->has_point) {
	if (point->x < bounder->min_x)
	    bounder->min_x = point->x;

	if (point->y < bounder->min_y)
	    bounder->min_y = point->y;

	if (point->x > bounder->max_x)
	    bounder->max_x = point->x;

	if (point->y > bounder->max_y)
	    bounder->max_y = point->y;
    } else {
	bounder->min_x = point->x;
	bounder->min_y = point->y;
	bounder->max_x = point->x;
	bounder->max_y = point->y;

	bounder->has_point = TRUE;
    }
}

static cairo_status_t
_cairo_path_bounder_move_to (void *closure,
			     const cairo_point_t *point)
{
    cairo_path_bounder_t *bounder = closure;

    bounder->move_to_point = *point;
    bounder->has_move_to_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_line_to (void *closure,
			     const cairo_point_t *point)
{
    cairo_path_bounder_t *bounder = closure;

    if (bounder->has_move_to_point) {
	_cairo_path_bounder_add_point (bounder,
				       &bounder->move_to_point);
	bounder->has_move_to_point = FALSE;
    }

    _cairo_path_bounder_add_point (bounder, point);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_curve_to (void *closure,
			      const cairo_point_t *b,
			      const cairo_point_t *c,
			      const cairo_point_t *d)
{
    cairo_path_bounder_t *bounder = closure;

    if (bounder->has_move_to_point) {
	_cairo_path_bounder_add_point (bounder,
				       &bounder->move_to_point);
	bounder->has_move_to_point = FALSE;
    }

    _cairo_path_bounder_add_point (bounder, b);
    _cairo_path_bounder_add_point (bounder, c);
    _cairo_path_bounder_add_point (bounder, d);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_close_path (void *closure)
{
    return CAIRO_STATUS_SUCCESS;
}

/* This computes the extents of all the points in the path, not those of
 * the damage area (i.e it does not consider winding and it only inspects
 * the control points of the curves, not the flattened path).
 */
void
_cairo_path_fixed_approximate_extents (cairo_path_fixed_t *path,
				       cairo_rectangle_int_t *extents)
{
    cairo_path_bounder_t bounder;
    cairo_status_t status;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_fixed_interpret (path, CAIRO_DIRECTION_FORWARD,
					  _cairo_path_bounder_move_to,
					  _cairo_path_bounder_line_to,
					  _cairo_path_bounder_curve_to,
					  _cairo_path_bounder_close_path,
					  &bounder);
    assert (status == CAIRO_STATUS_SUCCESS);

    if (bounder.has_point) {
	extents->x = _cairo_fixed_integer_floor (bounder.min_x);
	extents->y = _cairo_fixed_integer_floor (bounder.min_y);
	extents->width =
	    _cairo_fixed_integer_ceil (bounder.max_x) - extents->x;
	extents->height =
	    _cairo_fixed_integer_ceil (bounder.max_y) - extents->y;
    } else {
	extents->x = extents->y = 0;
	extents->width = extents->height = 0;
    }

    _cairo_path_bounder_fini (&bounder);
}

void
_cairo_path_fixed_bounds (cairo_path_fixed_t *path,
			  double *x1, double *y1,
			  double *x2, double *y2,
			  double tolerance)
{
    cairo_path_bounder_t bounder;
    cairo_status_t status;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_fixed_interpret_flat (path, CAIRO_DIRECTION_FORWARD,
					       _cairo_path_bounder_move_to,
					       _cairo_path_bounder_line_to,
					       _cairo_path_bounder_close_path,
					       &bounder,
					       tolerance);
    assert (status == CAIRO_STATUS_SUCCESS);

    if (bounder.has_point) {
	*x1 = _cairo_fixed_to_double (bounder.min_x);
	*y1 = _cairo_fixed_to_double (bounder.min_y);
	*x2 = _cairo_fixed_to_double (bounder.max_x);
	*y2 = _cairo_fixed_to_double (bounder.max_y);
    } else {
	*x1 = 0.0;
	*y1 = 0.0;
	*x2 = 0.0;
	*y2 = 0.0;
    }

    _cairo_path_bounder_fini (&bounder);
}

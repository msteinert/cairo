/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@redhat.com>
 */

#include "cairo-path-data-private.h"

cairo_path_t
_cairo_path_nil = { NULL, 0 };

/* Closure for path interpretation. */
typedef struct cairo_path_data_count {
    int count;
} cpdc_t;

static void
_cpdc_move_to (void *closure, double x, double y)
{
    cpdc_t *cpdc = closure;

    cpdc->count += 2;
}

static void
_cpdc_line_to (void *closure, double x, double y)
{
    cpdc_t *cpdc = closure;

    cpdc->count += 2;
}

static void
_cpdc_curve_to (void *closure,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3)
{
    cpdc_t *cpdc = closure;

    cpdc->count += 4;
}

static void
_cpdc_close_path (void *closure)
{
    cpdc_t *cpdc = closure;

    cpdc->count += 1;
}

static int
_cairo_path_data_count (cairo_gstate_t *gstate, cairo_bool_t flatten)
{
    cpdc_t cpdc;

    cpdc.count = 0;

    _cairo_gstate_interpret_path (gstate,
				  _cpdc_move_to,
				  _cpdc_line_to,
				  flatten ? NULL : _cpdc_curve_to,
				  _cpdc_close_path,
				  &cpdc);

    return cpdc.count;
}

/* Closure for path interpretation. */
typedef struct cairo_path_data_populate {
    cairo_path_data_t *data;
} cpdp_t;

static void
_cpdp_move_to (void *closure, double x, double y)
{
    cpdp_t *cpdp = closure;
    cairo_path_data_t *data = cpdp->data;

    data->header.type = CAIRO_PATH_MOVE_TO;
    data->header.length = 2;

    /* We index from 1 to leave room for data->header */
    data[1].point.x = x;
    data[1].point.y = y;

    cpdp->data += data->header.length;
}

static void
_cpdp_line_to (void *closure, double x, double y)
{
    cpdp_t *cpdp = closure;
    cairo_path_data_t *data = cpdp->data;

    data->header.type = CAIRO_PATH_LINE_TO;
    data->header.length = 2;

    /* We index from 1 to leave room for data->header */
    data[1].point.x = x;
    data[1].point.y = y;

    cpdp->data += data->header.length;
}

static void
_cpdp_curve_to (void *closure,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3)
{
    cpdp_t *cpdp = closure;
    cairo_path_data_t *data = cpdp->data;

    data->header.type = CAIRO_PATH_CURVE_TO;
    data->header.length = 4;

    /* We index from 1 to leave room for data->header */
    data[1].point.x = x1;
    data[1].point.y = y1;

    data[2].point.x = x2;
    data[2].point.y = y2;

    data[3].point.x = x3;
    data[3].point.y = y3;

    cpdp->data += data->header.length;
}

static void
_cpdp_close_path (void *closure)
{
    cpdp_t *cpdp = closure;
    cairo_path_data_t *data = cpdp->data;

    data->header.type = CAIRO_PATH_CLOSE_PATH;
    data->header.length = 1;

    cpdp->data += data->header.length;
}

static void
_cairo_path_data_populate (cairo_path_t   *path,
			   cairo_gstate_t *gstate,
			   cairo_bool_t	   flatten)
{
    cpdp_t cpdp;

    cpdp.data = path->data;

    _cairo_gstate_interpret_path (gstate,
				  _cpdp_move_to,
				  _cpdp_line_to,
				  flatten ? NULL : _cpdp_curve_to,
				  _cpdp_close_path,
				  &cpdp);

    /* Sanity check the count */
    assert (cpdp.data - path->data == path->num_data);
}

static cairo_path_t *
_cairo_path_data_create_real (cairo_gstate_t *gstate, cairo_bool_t flatten)
{
    cairo_path_t *path;

    path = malloc (sizeof (cairo_path_t));
    if (path == NULL)
	return &_cairo_path_nil;

    path->num_data = _cairo_path_data_count (gstate, flatten);

    path->data = malloc (path->num_data * sizeof (cairo_path_data_t));
    if (path->data == NULL) {
	free (path);
	return &_cairo_path_nil;
    }

    _cairo_path_data_populate (path, gstate, flatten);

    return path;
}

void
cairo_path_destroy (cairo_path_t *path)
{
    free (path->data);
    path->num_data = 0;
    free (path);
}

cairo_path_t *
_cairo_path_data_create (cairo_gstate_t *gstate)
{
    return _cairo_path_data_create_real (gstate, FALSE);
}

cairo_path_t *
_cairo_path_data_create_flat (cairo_gstate_t *gstate)
{
    return _cairo_path_data_create_real (gstate, TRUE);
}

cairo_status_t
_cairo_path_data_append_to_context (cairo_path_t *path,
				    cairo_t	 *cr)
{
    int i;
    cairo_path_data_t *p;

    for (i=0; i < path->num_data; i += path->data[i].header.length) {
	p = &path->data[i];
	switch (p->header.type) {
	case CAIRO_PATH_MOVE_TO:
	    cairo_move_to (cr,
			   p[1].point.x, p[1].point.y);
	    if (p->header.length != 2)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    break;
	case CAIRO_PATH_LINE_TO:
	    cairo_line_to (cr,
			   p[1].point.x, p[1].point.y);
	    if (p->header.length != 2)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    break;
	case CAIRO_PATH_CURVE_TO:
	    cairo_curve_to (cr,
			    p[1].point.x, p[1].point.y,
			    p[2].point.x, p[2].point.y,
			    p[3].point.x, p[3].point.y);
	    if (p->header.length != 4)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    break;
	case CAIRO_PATH_CLOSE_PATH:
	    cairo_close_path (cr);
	    if (p->header.length != 1)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    break;
	default:
	    return CAIRO_STATUS_INVALID_PATH_DATA;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2009,2010,2011 Intel Corporation
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-compositor-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-spans-compositor-private.h"

typedef struct _cairo_image_span_renderer {
    cairo_span_renderer_t base;

    pixman_image_compositor_t *compositor;
    pixman_image_t *src;
    float opacity;
    cairo_rectangle_int_t extents;
} cairo_image_span_renderer_t;

static cairo_status_t
_cairo_image_span_renderer_init (cairo_abstract_span_renderer_t *_r,
				 cairo_surface_t *dst,
				 cairo_operator_t op,
				 cairo_surface_t *src,
				 int src_x, int src_y;
				 float opacity,
				 const cairo_composite_rectangles_t *composite,
				 cairo_bool_t needs_clip)
{
    cairo_image_span_renderer_t *r = (cairo_image_span_renderer_t *)_r;
    cairo_pixman_source_t *src = (cairo_pixman_source_t *)_src;
    int src_x, src_y;

    if (op == CAIRO_OPERATOR_CLEAR) {
	op = CAIRO_OPERATOR_DEST_OUT;
	pattern = NULL;
    }

    r->src = ((cairo_pixman_source_t *) src)->pixman_image;
    r->opacity = opacity;

    if (composite->is_bounded) {
	if (opacity == 1.)
	    r->base.render_rows = _cairo_image_bounded_opaque_spans;
	else
	    r->base.render_rows = _cairo_image_bounded_spans;
	r->base.finish = NULL;
    } else {
	if (needs_clip)
	    r->base.render_rows = _cairo_image_clipped_spans;
	else
	    r->base.render_rows = _cairo_image_unbounded_spans;
        r->base.finish =      _cairo_image_finish_unbounded_spans;
	r->extents = composite->unbounded;
	r->extents.height += r->extents.y;

    }
    r->compositor =
	pixman_image_create_compositor (_pixman_operator (op),
					r->src, NULL, dst->pixman_image,
					composite->bounded.x + src_x,
					composite->bounded.y + src_y,
					0, 0,
					composite->bounded.x,
					composite->bounded.y,
					composite->bounded.width,
					composite->bounded.height);
    if (unlikely (r->compositor == NULL))
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_span_renderer_fini (cairo_abstract_span_renderer_t *_r)
{
    cairo_image_span_renderer_t *r = (cairo_image_span_renderer_t *) r;
    pixman_image_compositor_destroy (r->compositor);
}

const cairo_compositor_t *
_cairo_image_spans_compositor_get (void)
{
    static cairo_spans_compositor_t compositor;

    if (compositor.base.delegate == NULL) {
	/* Can't fallback to the mask compositor as that will simply
	 * call the spans-compositor again to render the mask!
	 */
	_cairo_spans_compositor_init (&compositor,
				      _cairo_image_traps_compositor_get());

    }

    return &compositor.base;
}

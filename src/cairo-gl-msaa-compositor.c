/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2011 Intel Corporation
 * Copyright © 2011 Samsung Electronics
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
 *	Henry Song <hsong@sisa.samsung.com>
 *	Martin Robinson <mrobinson@igalia.com>
 */

#include "cairoint.h"

#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-gl-private.h"
#include "cairo-traps-private.h"

static cairo_int_status_t
_draw_trap (cairo_gl_context_t		*ctx,
	    cairo_gl_composite_t	*setup,
	    cairo_trapezoid_t		*trap)
{
    cairo_point_t quad[4];

    quad[0].x = _cairo_edge_compute_intersection_x_for_y (&trap->left.p1,
							  &trap->left.p2,
							  trap->top);
    quad[0].y = trap->top;

    quad[1].x = _cairo_edge_compute_intersection_x_for_y (&trap->left.p1,
						      &trap->left.p2,
						      trap->bottom);
    quad[1].y = trap->bottom;

    quad[2].x = _cairo_edge_compute_intersection_x_for_y (&trap->right.p1,
						      &trap->right.p2,
						      trap->bottom);
    quad[2].y = trap->bottom;

    quad[3].x = _cairo_edge_compute_intersection_x_for_y (&trap->right.p1,
						      &trap->right.p2,
						      trap->top);
    quad[3].y = trap->top;
    return _cairo_gl_composite_emit_quad_as_tristrip (ctx, setup, quad);
}

static cairo_int_status_t
_draw_traps (cairo_gl_context_t		*ctx,
	     cairo_gl_composite_t	*setup,
	     cairo_traps_t		*traps)
{
    int i;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    for (i = 0; i < traps->num_traps; i++) {
	cairo_trapezoid_t *trap = traps->traps + i;
	if (unlikely ((status = _draw_trap (ctx, setup, trap))))
	    return status;
    }

   return status;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_paint (const cairo_compositor_t	*compositor,
				 cairo_composite_rectangles_t	*composite)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_mask (const cairo_compositor_t	*compositor,
				cairo_composite_rectangles_t	*composite)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_stroke (const cairo_compositor_t	*compositor,
				  cairo_composite_rectangles_t	*composite,
				  const cairo_path_fixed_t	*path,
				  const cairo_stroke_style_t	*style,
				  const cairo_matrix_t		*ctm,
				  const cairo_matrix_t		*ctm_inverse,
				  double			 tolerance,
				  cairo_antialias_t		 antialias)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_fill (const cairo_compositor_t	*compositor,
				cairo_composite_rectangles_t	*composite,
				const cairo_path_fixed_t	*path,
				cairo_fill_rule_t		 fill_rule,
				double				 tolerance,
				cairo_antialias_t		 antialias)
{
    cairo_gl_composite_t setup;
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_gl_context_t *ctx = NULL;
    cairo_int_status_t status;
    cairo_traps_t traps;

    if (antialias != CAIRO_ANTIALIAS_NONE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_composite_rectangles_can_reduce_clip (composite,
						       composite->clip))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_to_traps (path, fill_rule, tolerance, &traps);
    if (unlikely (status))
	goto cleanup_traps;

    status = _cairo_gl_composite_init (&setup,
				       composite->op,
				       dst,
				       FALSE, /* assume_component_alpha */
				       &composite->bounded);
    if (unlikely (status))
	goto cleanup_traps;

    status = _cairo_gl_composite_set_source (&setup,
					     &composite->source_pattern.base,
					     composite->bounded.x,
					     composite->bounded.y,
					     composite->bounded.x,
					     composite->bounded.y,
					     composite->bounded.width,
					     composite->bounded.height);
    if (unlikely (status))
	goto cleanup_setup;

    status = _cairo_gl_composite_begin_tristrip (&setup, &ctx);
    if (unlikely (status))
	goto cleanup_setup;

    status = _draw_traps (ctx, &setup, &traps);
    if (unlikely (status))
        goto cleanup_setup;

    _cairo_gl_composite_flush (ctx);
cleanup_setup:
    _cairo_gl_composite_fini (&setup);
    if (ctx)
	status = _cairo_gl_context_release (ctx, status);
cleanup_traps:
    _cairo_traps_fini (&traps);

    return status;
}

static void
_cairo_gl_msaa_compositor_init (cairo_compositor_t	 *compositor,
				const cairo_compositor_t *delegate)
{
    compositor->delegate = delegate;

    compositor->paint = _cairo_gl_msaa_compositor_paint;
    compositor->mask = _cairo_gl_msaa_compositor_mask;
    compositor->fill = _cairo_gl_msaa_compositor_fill;
    compositor->stroke = _cairo_gl_msaa_compositor_stroke;
    /* always fallback to common glyph cache implmentation */
}

const cairo_compositor_t *
_cairo_gl_msaa_compositor_get (void)
{
    static cairo_compositor_t compositor;
    if (compositor.delegate == NULL)
	_cairo_gl_msaa_compositor_init (&compositor,
					_cairo_gl_span_compositor_get ());

    return &compositor;
}

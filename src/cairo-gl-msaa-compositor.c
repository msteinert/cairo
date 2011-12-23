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

#include "cairo-clip-inline.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-gl-private.h"
#include "cairo-traps-private.h"

static void
_scissor_to_rectangle (cairo_gl_surface_t	*surface,
		       const cairo_rectangle_int_t	*rect);

struct _tristrip_composite_info {
    cairo_gl_composite_t	setup;
    cairo_gl_context_t		*ctx;
};

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
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    int i;

    for (i = 0; i < traps->num_traps; i++) {
	cairo_trapezoid_t *trap = traps->traps + i;
	if (unlikely ((status = _draw_trap (ctx, setup, trap))))
	    return status;
    }

   return status;
}

static cairo_int_status_t
_draw_int_rect (cairo_gl_context_t	*ctx,
		cairo_gl_composite_t	*setup,
		cairo_rectangle_int_t	*rect)
{
    cairo_box_t box;
    cairo_point_t quad[4];

    _cairo_box_from_rectangle (&box, rect);
    quad[0].x = box.p1.x;
    quad[0].y = box.p1.y;
    quad[1].x = box.p1.x;
    quad[1].y = box.p2.y;
    quad[2].x = box.p2.x;
    quad[2].y = box.p2.y;
    quad[3].x = box.p2.x;
    quad[3].y = box.p1.y;

    return _cairo_gl_composite_emit_quad_as_tristrip (ctx, setup, quad);
}

static cairo_int_status_t
_draw_triangle_fan (cairo_gl_context_t		*ctx,
		    cairo_gl_composite_t	*setup,
		    const cairo_point_t		*midpt,
		    const cairo_point_t		*points,
		    int				 npoints)
{
    int i;

    /* Our strategy here is to not even try to build a triangle fan, but to
       draw each triangle as if it was an unconnected member of a triangle strip. */
    for (i = 1; i < npoints; i++) {
	cairo_int_status_t status;
	cairo_point_t triangle[3];

	triangle[0] = *midpt;
	triangle[1] = points[i - 1];
	triangle[2] = points[i];

	status = _cairo_gl_composite_emit_triangle_as_tristrip (ctx, setup, triangle);
	if (unlikely (status))
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_draw_clip (cairo_gl_context_t		*ctx,
	    cairo_gl_composite_t	*setup,
	    cairo_clip_t		*clip)
{
    cairo_int_status_t status;
    cairo_traps_t traps;

    cairo_polygon_t polygon;
    cairo_antialias_t antialias;
    cairo_fill_rule_t fill_rule;

    status = _cairo_clip_get_polygon (clip, &polygon, &fill_rule, &antialias);
    if (unlikely (status))
	return status;

    /* We ignore the antialias mode of the clip here, since the user requested
     * unantialiased rendering of their path and we expect that this stencil
     * based rendering of the clip to be a reasonable approximation to
     * the intersection between that clip and the path.
     *
     * In other words, what the user expects when they try to perform
     * a geometric intersection between an unantialiased polygon and an
     * antialiased polygon is open to interpretation. And we choose the fast
     * option.
     */

    _cairo_traps_init (&traps);
    status = _cairo_bentley_ottmann_tessellate_polygon (&traps,
							&polygon,
							fill_rule);
    _cairo_polygon_fini (&polygon);
    if (unlikely (status))
	return status;

    status = _draw_traps (ctx, setup, &traps);

    _cairo_traps_fini (&traps);
    return status;
}

static void
_disable_stencil_buffer (void)
{
    glDisable (GL_STENCIL_TEST);
    glDepthMask (GL_FALSE);
}

static cairo_int_status_t
_draw_clip_to_stencil_buffer (cairo_gl_context_t	*ctx,
			      cairo_gl_composite_t	*setup,
			      cairo_clip_t		*clip)
{
    cairo_int_status_t status;

    assert (! _cairo_clip_is_all_clipped (clip));

    if (! _cairo_gl_ensure_stencil (ctx, setup->dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    glDepthMask (GL_TRUE);
    glEnable (GL_STENCIL_TEST);
    glClearStencil (0);
    glClear (GL_STENCIL_BUFFER_BIT);
    glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilFunc (GL_EQUAL, 1, 0xffffffff);
    glColorMask (0, 0, 0, 0);

    status = _draw_clip (ctx, setup, clip);
    if (unlikely (status)) {
	glColorMask (1, 1, 1, 1);
	_disable_stencil_buffer ();
	return status;
    }

    /* We want to only render to the stencil buffer, so draw everything now. */
    _cairo_gl_composite_flush (ctx);

    glColorMask (1, 1, 1, 1);
    glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc (GL_EQUAL, 1, 0xffffffff);

    return status;;
}

static void
_scissor_to_rectangle (cairo_gl_surface_t *surface,
		       const cairo_rectangle_int_t *r)
{
    int y = r->y;
    if (_cairo_gl_surface_is_texture (surface) == FALSE)
	y = surface->height - (r->y + r->height);
    glScissor (r->x, y, r->width, r->height);
    glEnable (GL_SCISSOR_TEST);
}

static cairo_int_status_t
_scissor_and_clip (cairo_gl_context_t		*ctx,
		   cairo_gl_composite_t		*setup,
		   cairo_composite_rectangles_t	*composite,
		   cairo_bool_t			*used_stencil_buffer)
{
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_rectangle_int_t *bounds = &composite->unbounded;
    cairo_clip_t *clip = composite->clip;
    cairo_rectangle_int_t r;

    *used_stencil_buffer = FALSE;

    if (_cairo_composite_rectangles_can_reduce_clip (composite, clip)) {
	_scissor_to_rectangle (dst, bounds);
	return CAIRO_INT_STATUS_SUCCESS;
    }

    /* If we cannot reduce the clip to a rectangular region,
       we scissor and clip using the stencil buffer */
    if (clip->num_boxes > 1 || clip->path != NULL) {
	*used_stencil_buffer = TRUE;
	_scissor_to_rectangle (dst, bounds);
	return _draw_clip_to_stencil_buffer (ctx, setup, clip);
    }

    /* Always interpret the clip path as ANTIALIAS_NONE */
    _cairo_box_round_to_rectangle (clip->boxes, &r);
    _scissor_to_rectangle (dst, &r);
    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_bool_t
_should_use_unbounded_surface (cairo_composite_rectangles_t *composite)
{
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_rectangle_int_t *source = &composite->source;

    if (composite->is_bounded)
	return FALSE;

    /* This isn't just an optimization. It also detects when painting is used
       to paint back the unbounded surface, preventing infinite recursion. */
    return ! (source->x <= 0 && source->y <= 0 &&
              source->height + source->y >= dst->height &&
              source->width + source->x >= dst->width);
}

static cairo_surface_t*
_prepare_unbounded_surface (cairo_gl_surface_t *dst)
{

    cairo_surface_t* surface = cairo_gl_surface_create (dst->base.device,
							dst->base.content,
							dst->width,
							dst->height);
    if (surface == NULL)
        return NULL;
    if (unlikely (surface->status)) {
        cairo_surface_destroy (surface);
        return NULL;
    }
    return surface;
}

static cairo_int_status_t
_paint_back_unbounded_surface (const cairo_compositor_t		*compositor,
			       cairo_composite_rectangles_t	*composite,
			       cairo_surface_t			*surface)
{
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_int_status_t status;

    cairo_pattern_t *pattern = cairo_pattern_create_for_surface (surface);
    if (unlikely (pattern->status)) {
	status = pattern->status;
	goto finish;
    }

    status = _cairo_compositor_paint (compositor, &dst->base,
				      composite->op, pattern,
				      composite->clip);

finish:
    cairo_pattern_destroy (pattern);
    cairo_surface_destroy (surface);
    return status;
}

static cairo_bool_t
should_fall_back (cairo_gl_surface_t *surface,
		  cairo_antialias_t antialias)
{
    if (antialias == CAIRO_ANTIALIAS_FAST)
	return TRUE;
    if (antialias == CAIRO_ANTIALIAS_NONE)
	return FALSE;
    return ! surface->supports_msaa;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_mask (const cairo_compositor_t	*compositor,
				cairo_composite_rectangles_t	*composite)
{
    cairo_gl_composite_t setup;
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_gl_context_t *ctx = NULL;
    cairo_bool_t used_stencil_buffer;
    cairo_int_status_t status;
    cairo_operator_t op = composite->op;

    if (should_fall_back (dst, CAIRO_ANTIALIAS_GOOD))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* GL compositing operators cannot properly represent a mask operation
       using the SOURCE compositing operator in one pass. This only matters if
       there actually is a mask (there isn't in a paint operation) and if the
       mask isn't totally opaque. */
    if (op == CAIRO_OPERATOR_SOURCE &&
	 composite->original_mask_pattern != NULL &&
	! _cairo_pattern_is_opaque (&composite->mask_pattern.base,
				    &composite->mask_sample_area)) {

       /* If the source is opaque the operation reduces to OVER. */
	if (_cairo_pattern_is_opaque (&composite->source_pattern.base,
				      &composite->source_sample_area))
	    op = CAIRO_OPERATOR_OVER;
	else
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (_should_use_unbounded_surface (composite)) {
	cairo_surface_t* surface = _prepare_unbounded_surface (dst);

	if (unlikely (surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	/* This may be a paint operation. */
	if (composite->original_mask_pattern == NULL) {
	    status = _cairo_compositor_paint (compositor, surface,
					      CAIRO_OPERATOR_SOURCE,
					      &composite->source_pattern.base,
					      NULL);
	} else {
	    status = _cairo_compositor_mask (compositor, surface,
					     CAIRO_OPERATOR_SOURCE,
					     &composite->source_pattern.base,
					     &composite->mask_pattern.base,
					     NULL);
	}

	if (unlikely (status)) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	return _paint_back_unbounded_surface (compositor, composite, surface);
    }

    status = _cairo_gl_composite_init (&setup,
				       op,
				       dst,
				       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	return status;

    status = _cairo_gl_composite_set_source (&setup,
					     &composite->source_pattern.base,
					     &composite->source_sample_area,
					     &composite->bounded);
    if (unlikely (status))
	goto finish;

    if (composite->original_mask_pattern != NULL) {
	status = _cairo_gl_composite_set_mask (&setup,
					       &composite->mask_pattern.base,
					       &composite->mask_sample_area,
					       &composite->bounded);
    }
    if (unlikely (status))
	goto finish;

    /* We always use multisampling here, because we do not yet have the smarts
       to calculate when the clip or the source requires it. */
    status = _cairo_gl_composite_begin_multisample (&setup, &ctx, TRUE);
    if (unlikely (status))
	goto finish;

    status = _scissor_and_clip (ctx, &setup, composite, &used_stencil_buffer);
    if (unlikely (status))
	goto finish;

    _draw_int_rect (ctx, &setup, &composite->bounded);
    _cairo_gl_composite_flush (ctx);

finish:
    _cairo_gl_composite_fini (&setup);

    if (ctx) {
	glDisable (GL_SCISSOR_TEST);
	_disable_stencil_buffer ();
	status = _cairo_gl_context_release (ctx, status);
    }

    return status;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_paint (const cairo_compositor_t	*compositor,
				 cairo_composite_rectangles_t	*composite)
{
    return _cairo_gl_msaa_compositor_mask (compositor, composite);
}

static cairo_status_t
_stroke_shaper_add_triangle (void			*closure,
			     const cairo_point_t	 triangle[3])
{
    struct _tristrip_composite_info *info = closure;
    return _cairo_gl_composite_emit_triangle_as_tristrip (info->ctx,
							  &info->setup,
							  triangle);
}

static cairo_status_t
_stroke_shaper_add_triangle_fan (void			*closure,
				 const cairo_point_t	*midpoint,
				 const cairo_point_t	*points,
				 int			 npoints)
{
    struct _tristrip_composite_info *info = closure;
    return _draw_triangle_fan (info->ctx, &info->setup,
			       midpoint, points, npoints);
}

static cairo_status_t
_stroke_shaper_add_quad (void			*closure,
			 const cairo_point_t	 quad[4])
{
    struct _tristrip_composite_info *info = closure;
    return _cairo_gl_composite_emit_quad_as_tristrip (info->ctx, &info->setup,
						      quad);
}

static cairo_int_status_t
_prevent_overlapping_drawing (cairo_gl_context_t *ctx,
			      cairo_gl_composite_t *setup,
			      cairo_composite_rectangles_t *composite,
			      cairo_bool_t used_stencil_buffer_for_clip)
{
    if (! _cairo_gl_ensure_stencil (ctx, setup->dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (_cairo_pattern_is_opaque (&composite->source_pattern.base,
				  &composite->source_sample_area))
	return CAIRO_INT_STATUS_SUCCESS;

   if (used_stencil_buffer_for_clip == FALSE) {
	/* Enable the stencil buffer, even if we have no clip so that
	   we can use it below to prevent overlapping shapes. We initialize
	   it all to one here which represents infinite clip. */
	glDepthMask (GL_TRUE);
	glEnable (GL_STENCIL_TEST);
	glClearStencil (1);
	glClear (GL_STENCIL_BUFFER_BIT);
	glStencilFunc (GL_EQUAL, 1, 1);
    }

    /* This means that once we draw to a particular pixel nothing else can
       be drawn there until the stencil buffer is reset or the stencil test
       is disabled. */
    glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO);
    return CAIRO_INT_STATUS_SUCCESS;
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
    cairo_int_status_t status;
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    struct _tristrip_composite_info info;
    cairo_bool_t used_stencil_buffer_for_clip;

    if (should_fall_back (dst, antialias))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (composite->is_bounded == FALSE) {
	cairo_surface_t* surface = _prepare_unbounded_surface (dst);

	if (unlikely (surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	status = _cairo_compositor_stroke (compositor, surface,
					   CAIRO_OPERATOR_SOURCE,
					   &composite->source_pattern.base,
					   path, style, ctm, ctm_inverse,
					   tolerance, antialias, NULL);
	if (unlikely (status)) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	return _paint_back_unbounded_surface (compositor, composite, surface);
    }

    status = _cairo_gl_composite_init (&info.setup,
				       composite->op,
				       dst,
				       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	return status;

    info.ctx = NULL;

    status = _cairo_gl_composite_set_source (&info.setup,
					     &composite->source_pattern.base,
					     &composite->source_sample_area,
					     &composite->bounded);
    if (unlikely (status))
	goto finish;

    status = _cairo_gl_composite_begin_multisample (&info.setup, &info.ctx,
	antialias != CAIRO_ANTIALIAS_NONE);
    if (unlikely (status))
	goto finish;

    status = _scissor_and_clip (info.ctx, &info.setup, composite,
				&used_stencil_buffer_for_clip);
    if (unlikely (status))
	goto finish;

    status = _prevent_overlapping_drawing (info.ctx, &info.setup, composite,
					   used_stencil_buffer_for_clip);
    if (unlikely (status))
	goto finish;

    status = _cairo_path_fixed_stroke_to_shaper ((cairo_path_fixed_t *) path,
						 style,
						 ctm,
						 ctm_inverse,
						 tolerance,
						 _stroke_shaper_add_triangle,
						 _stroke_shaper_add_triangle_fan,
						 _stroke_shaper_add_quad,
						 &info);
    if (unlikely (status))
	goto finish;

    _cairo_gl_composite_flush (info.ctx);

finish:
    _cairo_gl_composite_fini (&info.setup);

    if (info.ctx) {
	glDisable (GL_SCISSOR_TEST);
	_disable_stencil_buffer ();
	status = _cairo_gl_context_release (info.ctx, status);
    }

    return status;
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
    cairo_bool_t used_stencil_buffer;

    if (should_fall_back (dst, antialias))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (composite->is_bounded == FALSE) {
	cairo_surface_t* surface = _prepare_unbounded_surface (dst);

	if (unlikely (surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;


	status = _cairo_compositor_fill (compositor, surface,
					 CAIRO_OPERATOR_SOURCE,
					 &composite->source_pattern.base,
					 path, fill_rule, tolerance,
					 antialias, NULL);

	if (unlikely (status)) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	return _paint_back_unbounded_surface (compositor, composite, surface);
    }

    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_to_traps (path, fill_rule, tolerance, &traps);
    if (unlikely (status))
	goto cleanup_traps;

    status = _cairo_gl_composite_init (&setup,
				       composite->op,
				       dst,
				       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	goto cleanup_traps;

    status = _cairo_gl_composite_set_source (&setup,
					     &composite->source_pattern.base,
					     &composite->source_sample_area,
					     &composite->bounded);
    if (unlikely (status))
	goto cleanup_setup;

    status = _cairo_gl_composite_begin_multisample (&setup, &ctx,
	antialias != CAIRO_ANTIALIAS_NONE);
    if (unlikely (status))
	goto cleanup_setup;

    status = _scissor_and_clip (ctx, &setup, composite,
				&used_stencil_buffer);
    if (unlikely (status))
	goto cleanup_setup;

    status = _draw_traps (ctx, &setup, &traps);
    if (unlikely (status))
        goto cleanup_setup;

    _cairo_gl_composite_flush (ctx);
cleanup_setup:
    _cairo_gl_composite_fini (&setup);

    if (ctx) {
	glDisable (GL_SCISSOR_TEST);
	_disable_stencil_buffer ();
	status = _cairo_gl_context_release (ctx, status);
    }
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

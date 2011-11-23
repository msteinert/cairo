/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
 * Copyright © 2011 Intel Corporation
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 */

#include "cairoint.h"

#include "cairo-gl-private.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-spans-compositor-private.h"
#include "cairo-surface-backend-private.h"

typedef struct _cairo_gl_span_renderer {
    cairo_span_renderer_t base;

    cairo_gl_composite_t setup;
    double opacity;

    int xmin, xmax;
    int ymin, ymax;

    cairo_gl_context_t *ctx;
} cairo_gl_span_renderer_t;

static cairo_status_t
_cairo_gl_bounded_opaque_spans (void *abstract_renderer,
				int y, int height,
				const cairo_half_open_span_t *spans,
				unsigned num_spans)
{
    cairo_gl_span_renderer_t *r = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (spans[0].coverage) {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           spans[0].x, y,
                                           spans[1].x, y + height,
                                           spans[0].coverage);
	}

	spans++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_bounded_spans (void *abstract_renderer,
			 int y, int height,
			 const cairo_half_open_span_t *spans,
			 unsigned num_spans)
{
    cairo_gl_span_renderer_t *r = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (spans[0].coverage) {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           spans[0].x, y,
                                           spans[1].x, y + height,
                                           r->opacity * spans[0].coverage);
	}

	spans++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_unbounded_spans (void *abstract_renderer,
			   int y, int height,
			   const cairo_half_open_span_t *spans,
			   unsigned num_spans)
{
    cairo_gl_span_renderer_t *r = abstract_renderer;

    if (y > r->ymin) {
        _cairo_gl_composite_emit_rect (r->ctx,
                                       r->xmin, r->ymin,
                                       r->xmax, y,
                                       0);
    }

    if (num_spans == 0) {
        _cairo_gl_composite_emit_rect (r->ctx,
                                       r->xmin, y,
                                       r->xmax, y + height,
                                       0);
    } else {
        if (spans[0].x != r->xmin) {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           r->xmin, y,
                                           spans[0].x,     y + height,
                                           0);
        }

        do {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           spans[0].x, y,
                                           spans[1].x, y + height,
                                           r->opacity * spans[0].coverage);
            spans++;
        } while (--num_spans > 1);

        if (spans[0].x != r->xmax) {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           spans[0].x,     y,
                                           r->xmax, y + height,
                                           0);
        }
    }

    r->ymin = y + height;
    return CAIRO_STATUS_SUCCESS;
}

/* XXX */
static cairo_status_t
_cairo_gl_clipped_spans (void *abstract_renderer,
			   int y, int height,
			   const cairo_half_open_span_t *spans,
			   unsigned num_spans)
{
    cairo_gl_span_renderer_t *r = abstract_renderer;

    if (y > r->ymin) {
        _cairo_gl_composite_emit_rect (r->ctx,
                                       r->xmin, r->ymin,
                                       r->xmax, y,
                                       0);
    }

    if (num_spans == 0) {
        _cairo_gl_composite_emit_rect (r->ctx,
                                       r->xmin, y,
                                       r->xmax, y + height,
                                       0);
    } else {
        if (spans[0].x != r->xmin) {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           r->xmin, y,
                                           spans[0].x,     y + height,
                                           0);
        }

        do {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           spans[0].x, y,
                                           spans[1].x, y + height,
                                           r->opacity * spans[0].coverage);
            spans++;
        } while (--num_spans > 1);

        if (spans[0].x != r->xmax) {
            _cairo_gl_composite_emit_rect (r->ctx,
                                           spans[0].x,     y,
                                           r->xmax, y + height,
                                           0);
        }
    }

    r->ymin = y + height;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_finish_unbounded_spans (void *abstract_renderer)
{
    cairo_gl_span_renderer_t *r = abstract_renderer;

    if (r->ymax > r->ymin) {
        _cairo_gl_composite_emit_rect (r->ctx,
                                       r->xmin, r->ymin,
                                       r->xmax, r->ymax,
                                       0);
    }

    return _cairo_gl_context_release (r->ctx, CAIRO_STATUS_SUCCESS);
}

static cairo_status_t
_cairo_gl_finish_bounded_spans (void *abstract_renderer)
{
    cairo_gl_span_renderer_t *r = abstract_renderer;

    return _cairo_gl_context_release (r->ctx, CAIRO_STATUS_SUCCESS);
}

static void
emit_aligned_boxes (cairo_gl_context_t *ctx,
		    const cairo_boxes_t *boxes)
{
    const struct _cairo_boxes_chunk *chunk;
    int i;

    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
	    int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
	    int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
	    int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);
	    _cairo_gl_composite_emit_rect (ctx, x1, y1, x2, y2, 0);
	}
    }
}

static cairo_int_status_t
fill_boxes (void		*_dst,
	    cairo_operator_t	 op,
	    const cairo_color_t	*color,
	    cairo_boxes_t	*boxes)
{
    cairo_gl_composite_t setup;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    status = _cairo_gl_composite_init (&setup, op, _dst, FALSE, NULL);
    if (unlikely (status))
        goto FAIL;

   _cairo_gl_composite_set_solid_source (&setup, color);

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
        goto FAIL;

    emit_aligned_boxes (ctx, boxes);
    status = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);

FAIL:
    _cairo_gl_composite_fini (&setup);
    return status;
}

static cairo_status_t
_cairo_gl_source_finish (void *abstract_surface)
{
    cairo_gl_source_t *source = abstract_surface;

    _cairo_gl_operand_destroy (&source->operand);
    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_gl_source_backend = {
    CAIRO_SURFACE_TYPE_GL,
    _cairo_gl_source_finish,
    NULL, /* read-only wrapper */
};

static cairo_surface_t *
pattern_to_surface (cairo_surface_t *dst,
		    const cairo_pattern_t *pattern,
		    cairo_bool_t is_mask,
		    const cairo_rectangle_int_t *extents,
		    const cairo_rectangle_int_t *sample,
		    int *src_x, int *src_y)
{
    cairo_gl_source_t *source;
    cairo_int_status_t status;

    source = malloc (sizeof (*source));
    if (unlikely (source == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&source->base,
			 &cairo_gl_source_backend,
			 NULL, /* device */
			 CAIRO_CONTENT_COLOR_ALPHA);

    *src_x = *src_y = 0;
    status = _cairo_gl_operand_init (&source->operand, pattern, (cairo_gl_surface_t *)dst,
				     extents->x, extents->y,
				     extents->x, extents->y,
				     extents->width, extents->height);
    if (unlikely (status)) {
	cairo_surface_destroy (&source->base);
	return _cairo_surface_create_in_error (status);
    }

    return &source->base;
}

cairo_surface_t *
_cairo_gl_white_source (void)
{
    cairo_gl_source_t *source;

    source = malloc (sizeof (*source));
    if (unlikely (source == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&source->base,
			 &cairo_gl_source_backend,
			 NULL, /* device */
			 CAIRO_CONTENT_COLOR_ALPHA);

    _cairo_gl_solid_operand_init (&source->operand, CAIRO_COLOR_WHITE);

    return &source->base;
}

static inline cairo_gl_operand_t *
source_to_operand (cairo_surface_t *surface)
{
    cairo_gl_source_t *source = (cairo_gl_source_t *)surface;
    return source ? &source->operand : NULL;
}

static cairo_int_status_t
composite_boxes (void			*_dst,
		 cairo_operator_t	op,
		 cairo_surface_t	*abstract_src,
		 cairo_surface_t	*abstract_mask,
		 int			src_x,
		 int			src_y,
		 int			mask_x,
		 int			mask_y,
		 int			dst_x,
		 int			dst_y,
		 cairo_boxes_t		*boxes,
		 const cairo_rectangle_int_t  *extents)
{
    cairo_gl_composite_t setup;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    status = _cairo_gl_composite_init (&setup, op, _dst, FALSE, extents);
    if (unlikely (status))
        goto FAIL;

    _cairo_gl_composite_set_source_operand (&setup,
					    source_to_operand (abstract_src));

    _cairo_gl_composite_set_mask_operand (&setup,
					  source_to_operand (abstract_mask));

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
        goto FAIL;

    emit_aligned_boxes (ctx, boxes);
    status = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);

FAIL:
    _cairo_gl_composite_fini (&setup);
    return status;
}

static cairo_int_status_t
_cairo_gl_span_renderer_init (cairo_abstract_span_renderer_t	*_r,
			      const cairo_composite_rectangles_t *composite,
			      cairo_bool_t			 needs_clip)
{
    cairo_gl_span_renderer_t *r = (cairo_gl_span_renderer_t *)_r;
    const cairo_pattern_t *source = &composite->source_pattern.base;
    cairo_operator_t op = composite->op;
    cairo_int_status_t status;

    /* XXX earlier! */
    if (op == CAIRO_OPERATOR_CLEAR) {
	source = &_cairo_pattern_white.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    } else if (composite->surface->is_clear &&
	       (op == CAIRO_OPERATOR_SOURCE ||
		op == CAIRO_OPERATOR_OVER ||
		op == CAIRO_OPERATOR_ADD)) {
	op = CAIRO_OPERATOR_SOURCE;
    } else if (op == CAIRO_OPERATOR_SOURCE) {
	/* no lerp equivalent without some major PITA */
	return CAIRO_INT_STATUS_UNSUPPORTED;
    } else if (! _cairo_gl_operator_is_supported (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_gl_composite_init (&r->setup,
                                       op, (cairo_gl_surface_t *)composite->surface,
                                       FALSE, &composite->unbounded);
    if (unlikely (status))
        goto FAIL;

    status = _cairo_gl_composite_set_source (&r->setup, source,
					     composite->unbounded.x,
					     composite->unbounded.y,
					     composite->unbounded.x,
					     composite->unbounded.y,
					     composite->unbounded.width,
					     composite->unbounded.height);
    if (unlikely (status))
        goto FAIL;

    r->opacity = 1.0;
    if (composite->mask_pattern.base.type == CAIRO_PATTERN_TYPE_SOLID) {
	r->opacity = composite->mask_pattern.solid.color.alpha;
    } else {
	status = _cairo_gl_composite_set_mask (&r->setup,
					       &composite->mask_pattern.base,
					       composite->unbounded.x,
					       composite->unbounded.y,
					       composite->unbounded.x,
					       composite->unbounded.y,
					       composite->unbounded.width,
					       composite->unbounded.height);
	if (unlikely (status))
	    goto FAIL;
    }

    _cairo_gl_composite_set_spans (&r->setup);

    status = _cairo_gl_composite_begin (&r->setup, &r->ctx);
    if (unlikely (status))
        goto FAIL;

    if (composite->is_bounded) {
	if (r->opacity == 1.)
	    r->base.render_rows = _cairo_gl_bounded_opaque_spans;
	else
	    r->base.render_rows = _cairo_gl_bounded_spans;
        r->base.finish = _cairo_gl_finish_bounded_spans;
    } else {
	if (needs_clip)
	    r->base.render_rows = _cairo_gl_clipped_spans;
	else
	    r->base.render_rows = _cairo_gl_unbounded_spans;
        r->base.finish = _cairo_gl_finish_unbounded_spans;
	r->xmin = composite->unbounded.x;
	r->xmax = composite->unbounded.x + composite->unbounded.width;
	r->ymin = composite->unbounded.y;
	r->ymax = composite->unbounded.y + composite->unbounded.height;
    }

    return CAIRO_STATUS_SUCCESS;

FAIL:
    return status;
}

static void
_cairo_gl_span_renderer_fini (cairo_abstract_span_renderer_t *_r,
			      cairo_int_status_t status)
{
    cairo_gl_span_renderer_t *r = (cairo_gl_span_renderer_t *) _r;

    if (status == CAIRO_INT_STATUS_SUCCESS)
	r->base.finish (r);

    _cairo_gl_composite_fini (&r->setup);
}

const cairo_compositor_t *
_cairo_gl_span_compositor_get (void)
{
    static cairo_spans_compositor_t compositor;

    if (compositor.base.delegate == NULL) {
	/* The fallback to traps here is essentially just for glyphs... */
	_cairo_spans_compositor_init (&compositor,
				      _cairo_gl_traps_compositor_get());

	compositor.fill_boxes = fill_boxes;
	//compositor.check_composite_boxes = check_composite_boxes;
	compositor.pattern_to_surface = pattern_to_surface;
	compositor.composite_boxes = composite_boxes;
	//compositor.check_span_renderer = check_span_renderer;
	compositor.renderer_init = _cairo_gl_span_renderer_init;
	compositor.renderer_fini = _cairo_gl_span_renderer_fini;
    }

    return &compositor.base;
}

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2013 EchoStar Corporation
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
 * The Initial Developer of the Original Code is Mike Steinert
 *
 * Contributor(s):
 *	Mike Steinert <mike.steinert@gmail.com>
 */

#include "cairoint.h"

#include "cairo-clip-inline.h"
#include "cairo-compositor-private.h"
#include "cairo-directfb.h"
#include "directfb/cairo-directfb-private.h"

/**
 * _cairo_dfb_compositor_draw_boxes:
 * @self: a #cairo_compositor_t
 * @extents: a #cairo_composite_rectangles_t
 * @boxes: a #cairo_boxes_t
 *
 * Performs a drawing operation.
 *
 * Returns: %CAIRO_INT_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_int_status_t
_cairo_dfb_compositor_draw_boxes (const cairo_compositor_t	*self,
				  cairo_composite_rectangles_t	*extents,
				  cairo_boxes_t			*boxes)
{
    cairo_int_status_t status;
    if (! boxes->num_boxes) {
	return CAIRO_INT_STATUS_SUCCESS;
    }
    status = _cairo_dfb_surface_set_clip (extents->surface, extents->clip);
    if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	return status;
    }
    status = _cairo_dfb_surface_fill_boxes (extents->surface,
					    extents->op,
					    &extents->source_pattern.base,
					    boxes);
    _cairo_dfb_surface_reset_clip (extents->surface);
    return status;
}

/**
 * _cairo_dfb_compositor_mask_boxes:
 * @self: a #cairo_compositor_t
 * @extents: a #cairo_composite_rectangles_t
 * @boxes: a #cairo_boxes_t
 *
 * Classifies a mask operation and performs the drawing operation.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_int_status_t
_cairo_dfb_compositor_mask_boxes (const cairo_compositor_t	*self,
				  cairo_composite_rectangles_t	*extents,
				  cairo_boxes_t			*boxes)
{
    const cairo_pattern_t *mask_pattern;
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (! boxes->num_boxes) {
	return CAIRO_INT_STATUS_SUCCESS;
    }
    mask_pattern = &extents->mask_pattern.base;
    if (CAIRO_PATTERN_TYPE_SURFACE == mask_pattern->type) {
	const cairo_pattern_t *src_pattern;
	src_pattern = &extents->source_pattern.base;
	if (CAIRO_PATTERN_TYPE_SURFACE == src_pattern->type) {
	    const cairo_surface_pattern_t *mask;
	    mask = (const cairo_surface_pattern_t *) mask_pattern;
	    status = _cairo_dfb_surface_set_mask (extents->surface,
						  mask->surface);
	    if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
		return status;
	    }
	    status = _cairo_dfb_compositor_draw_boxes (self, extents, boxes);
	    _cairo_dfb_surface_reset_mask (extents->surface);
	}
    }
    return status;
}

/**
 * _cairo_dfb_compositor_draw_glyphs:
 * @self: a #cairo_compositor_t
 * @extents: a #cairo_composite_rectangles_t
 * @scaled_font: a #cairo_scaled_font_t
 * @glyphs: an array of #cairo_glyph_t
 * @num_glyphs: the number of glyphs in @glyphs
 *
 * Classifies a glyph operation and then draws the glyphs.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_int_status_t
_cairo_dfb_compositor_draw_glyphs (const cairo_compositor_t	*self,
				   cairo_composite_rectangles_t	*extents,
				   cairo_scaled_font_t		*scaled_font,
				   cairo_glyph_t		*glyphs,
				   int				 num_glyphs)
{
    const cairo_pattern_t *src_pattern;
    const cairo_color_t *color;
    cairo_int_status_t status;
    src_pattern = &extents->source_pattern.base;
    if (CAIRO_PATTERN_TYPE_SOLID != src_pattern->type) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    status = _cairo_dfb_surface_set_clip (extents->surface, extents->clip);
    if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	return status;
    }
    color = &((cairo_solid_pattern_t *) src_pattern)->color;
    status = _cairo_dfb_surface_draw_glyphs (extents->surface,
					     extents->op,
					     color,
					     scaled_font,
					     glyphs,
					     num_glyphs);
    _cairo_dfb_surface_reset_clip (extents->surface);
    return status;
}

/* high-level compositor interface */

static cairo_int_status_t
_cairo_dfb_compositor_paint (const cairo_compositor_t		*self,
			     cairo_composite_rectangles_t	*extents)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_clip_is_region (extents->clip) &&
	_cairo_operator_bounded_by_source (extents->op)) {
	cairo_boxes_t boxes;
	_cairo_clip_steal_boxes (extents->clip, &boxes);
	status = _cairo_dfb_compositor_draw_boxes (self, extents, &boxes);
	_cairo_clip_unsteal_boxes (extents->clip, &boxes);
    }
    return status;
}

static cairo_int_status_t
_cairo_dfb_compositor_mask (const cairo_compositor_t		*self,
			    cairo_composite_rectangles_t	*extents)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_clip_is_region (extents->clip) &&
	_cairo_operator_bounded_by_mask (extents->op)) {
	cairo_boxes_t boxes;
	_cairo_clip_steal_boxes (extents->clip, &boxes);
	status = _cairo_dfb_compositor_mask_boxes (self, extents, &boxes);
	_cairo_clip_unsteal_boxes (extents->clip, &boxes);
    }
    return status;
}

static cairo_int_status_t
_cairo_dfb_compositor_stroke (const cairo_compositor_t		*self,
			      cairo_composite_rectangles_t	*extents,
			      const cairo_path_fixed_t		*path,
			      const cairo_stroke_style_t	*style,
			      const cairo_matrix_t		*ctm,
			      const cairo_matrix_t		*ctm_inverse,
			      double				 tolerance,
			      cairo_antialias_t			 antialias)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_clip_is_region (extents->clip) &&
	_cairo_operator_bounded_by_source (extents->op) &&
	_cairo_path_fixed_stroke_is_rectilinear (path)) {
	cairo_boxes_t boxes;
	_cairo_boxes_init_with_clip (&boxes, extents->clip);
	status = _cairo_path_fixed_stroke_rectilinear_to_boxes (path,
								style,
								ctm,
								antialias,
								&boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS)) {
	    status = _cairo_dfb_compositor_draw_boxes (self, extents, &boxes);
	}
	_cairo_boxes_fini (&boxes);
    }
    return status;
}

static cairo_int_status_t
_cairo_dfb_compositor_fill (const cairo_compositor_t		*self,
			    cairo_composite_rectangles_t	*extents,
			    const cairo_path_fixed_t		*path,
			    cairo_fill_rule_t			 fill_rule,
			    double				 tolerance,
			    cairo_antialias_t			 antialias)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_clip_is_region (extents->clip) &&
	_cairo_operator_bounded_by_source (extents->op) &&
	_cairo_path_fixed_fill_is_rectilinear (path)) {
	cairo_boxes_t boxes;
	_cairo_boxes_init_with_clip (&boxes, extents->clip);
	status = (cairo_int_status_t)
		_cairo_path_fixed_fill_rectilinear_to_boxes (path,
							     fill_rule,
							     antialias,
							     &boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS)) {
	    status = _cairo_dfb_compositor_draw_boxes (self, extents, &boxes);
	}
	_cairo_boxes_fini (&boxes);
    }
    return status;
}

static cairo_int_status_t
_cairo_dfb_compositor_glyphs (const cairo_compositor_t		*self,
			      cairo_composite_rectangles_t	*extents,
			      cairo_scaled_font_t		*scaled_font,
			      cairo_glyph_t			*glyphs,
			      int				 num_glyphs,
			      cairo_bool_t			 overlap)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_clip_is_region (extents->clip) &&
	_cairo_operator_bounded_by_mask (extents->op)) {
	status = _cairo_dfb_compositor_draw_glyphs (self, extents, scaled_font,
						    glyphs, num_glyphs);
    }
    return status;
}

void
_cairo_dfb_compositor_get (cairo_compositor_t			*self,
			   cairo_directfb_acceleration_flags_t	 flags)
{
    self->delegate = &_cairo_fallback_compositor;
    self->paint = flags & CAIRO_DIRECTFB_ACCELERATION_FLAG_PAINT ?
	_cairo_dfb_compositor_paint : NULL;
    self->mask = flags & CAIRO_DIRECTFB_ACCELERATION_FLAG_MASK ?
	_cairo_dfb_compositor_mask : NULL;
    self->fill = flags & CAIRO_DIRECTFB_ACCELERATION_FLAG_FILL ?
	_cairo_dfb_compositor_fill : NULL;
    self->stroke = flags & CAIRO_DIRECTFB_ACCELERATION_FLAG_STROKE ?
	_cairo_dfb_compositor_stroke : NULL;
    self->glyphs = flags & CAIRO_DIRECTFB_ACCELERATION_FLAG_GLYPHS ?
	_cairo_dfb_compositor_glyphs : NULL;
}

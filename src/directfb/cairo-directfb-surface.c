/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2012 Intel Corporation
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
 * The Initial Developer of the Original Code is Chris Wilson
 *
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Mike Steinert <mike.steinert@gmail.com>
 */

/**
 * SECTION:cairo-directfb
 * @Title: DirectFB Surfaces
 * @Short_Description: Rendering to DirectFB surfaces
 * @See_Also: #cairo_surface_t
 *
 * The DirectFB surface is used to render Cairo graphics to DirectFB surfaces.
 **/

/**
 * CAIRO_HAS_DIRECTFB_SURFACE:
 *
 * Defined if the DirectFB backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 *
 * Since: 1.2
 **/

#include "cairoint.h"

#include "cairo-compositor-private.h"
#include "cairo-default-context-private.h"
#include "cairo-image-surface-inline.h"
#include "cairo-surface-subsurface-private.h"
#include "directfb/cairo-directfb-private.h"
#include <directfb.h>
#include <directfb_util.h>
#include <pixman.h>

slim_hidden_proto(cairo_directfb_surface_create);
slim_hidden_proto(cairo_directfb_surface_get_context);
slim_hidden_proto(cairo_directfb_surface_get_surface);
slim_hidden_proto(cairo_directfb_surface_get_width);
slim_hidden_proto(cairo_directfb_surface_get_height);
slim_hidden_proto(cairo_directfb_surface_set_acceleration);

typedef struct _cairo_dfb_surface {
    cairo_surface_t base;
    IDirectFB *dfb;
    IDirectFBSurface *surface;
    DFBSurfaceBlittingFlags mask;
    cairo_image_surface_t *image;
    size_t locked;
    cairo_compositor_t compositor;
} cairo_dfb_surface_t;

/**
 * _cairo_dfb_surface_set_color:
 * @base: a #cairo_dfb_surface_t
 * @color: a #cairo_color_t
 *
 * Sets the color for the next drawing operation.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_int_status_t
_cairo_dfb_surface_set_color (cairo_dfb_surface_t	*self,
			      const cairo_color_t	*color)
{
    DFBResult result;
    DFBSurfaceCapabilities caps;
    (void) self->surface->GetCapabilities (self->surface, &caps);
    if (caps & DSCAPS_PREMULTIPLIED) {
	result = self->surface->SetColor (self->surface,
					  color->red_short >> 8,
					  color->green_short >> 8,
					  color->blue_short >> 8,
					  color->alpha_short >> 8);
    } else {
	result = self->surface->SetColor (self->surface,
					  color->red * 0xff,
					  color->green * 0xff,
					  color->blue * 0xff,
					  color->alpha * 0xff);
    }
    if (unlikely (DFB_OK != result)) {
	return CAIRO_INT_STATUS_NO_MEMORY;
    }
    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 * _cairo_to_dfb_operators:
 * @op: a #cairo_operator_t
 * @src_blend: the source blend function
 * @dst_blend: the destination blend function
 *
 * Gets the blending operators for a drawing operation.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_int_status_t
_cairo_to_dfb_operators (cairo_operator_t		 op,
			 DFBSurfaceBlendFunction	*src_blend,
			 DFBSurfaceBlendFunction	*dst_blend)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	*src_blend = DSBF_ZERO;
	*dst_blend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_SOURCE:
	*src_blend = DSBF_ONE;
	*dst_blend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_OVER:
	*src_blend = DSBF_ONE;
	*dst_blend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_IN:
	*src_blend = DSBF_DESTALPHA;
	*dst_blend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_OUT:
	*src_blend = DSBF_INVDESTALPHA;
	*dst_blend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_ATOP:
	*src_blend = DSBF_DESTALPHA;
	*dst_blend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_DEST:
	*src_blend = DSBF_ZERO;
	*dst_blend = DSBF_ONE;
	break;
    case CAIRO_OPERATOR_DEST_OVER:
	*src_blend = DSBF_INVDESTALPHA;
	*dst_blend = DSBF_ONE;
	break;
    case CAIRO_OPERATOR_DEST_IN:
	*src_blend = DSBF_ZERO;
	*dst_blend = DSBF_SRCALPHA;
	break;
    case CAIRO_OPERATOR_DEST_OUT:
	*src_blend = DSBF_ZERO;
	*dst_blend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_DEST_ATOP:
	*src_blend = DSBF_INVDESTALPHA;
	*dst_blend = DSBF_SRCALPHA;
	break;
    case CAIRO_OPERATOR_XOR:
	*src_blend = DSBF_INVDESTALPHA;
	*dst_blend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_ADD:
	*src_blend = DSBF_ONE;
	*dst_blend = DSBF_ONE;
	break;
    case CAIRO_OPERATOR_SATURATE:
    case CAIRO_OPERATOR_MULTIPLY:
    case CAIRO_OPERATOR_SCREEN:
    case CAIRO_OPERATOR_OVERLAY:
    case CAIRO_OPERATOR_DARKEN:
    case CAIRO_OPERATOR_LIGHTEN:
    case CAIRO_OPERATOR_COLOR_DODGE:
    case CAIRO_OPERATOR_COLOR_BURN:
    case CAIRO_OPERATOR_HARD_LIGHT:
    case CAIRO_OPERATOR_SOFT_LIGHT:
    case CAIRO_OPERATOR_DIFFERENCE:
    case CAIRO_OPERATOR_EXCLUSION:
    case CAIRO_OPERATOR_HSL_HUE:
    case CAIRO_OPERATOR_HSL_SATURATION:
    case CAIRO_OPERATOR_HSL_COLOR:
    case CAIRO_OPERATOR_HSL_LUMINOSITY:
    default:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 * _box_to_rectangle:
 * @box: a #cairo_box_t
 * @rect: a #DFBRectangle
 *
 * Converts a Cairo box into a DirectFB rectangle.
 **/
static void
_box_to_rectangle (cairo_box_t *box, DFBRectangle *rect)
{
    rect->x = _cairo_fixed_integer_part (box->p1.x);
    rect->y = _cairo_fixed_integer_part (box->p1.y);
    rect->w = _cairo_fixed_integer_part (box->p2.x) - rect->x;
    rect->h = _cairo_fixed_integer_part (box->p2.y) - rect->y;
}

/**
 * _box_to_point:
 * @box: a #cairo_box_t
 * @point: a #DFBPoint
 *
 * Converts a Cairo box into a DirectFB point.
 **/
static void
_box_to_point (cairo_box_t *box, DFBPoint *point)
{
    point->x = _cairo_fixed_integer_part (box->p1.x);
    point->y = _cairo_fixed_integer_part (box->p1.y);
}

struct fill_boxes_data {
    cairo_dfb_surface_t *self;
    IDirectFBSurface *surface;
    const cairo_matrix_t *matrix;
};

/**
 * _fill_boxes:
 * @box: a #cairo_box_t
 * @closure: private data passed to _cairo_boxes_for_each_box()
 *
 * This function is a callback for _cairo_boxes_for_each_box().
 *
 * Calls Blit() to blend the rectangles in @boxes. Matrix transformations
 * will be applied.
 *
 * Returns: True if the operation succeeded, otherwise False
 **/
static cairo_bool_t
_fill_boxes (cairo_box_t	*box,
	     void		*closure)
{
    DFBPoint dst;
    DFBResult result;
    DFBRectangle src;
    struct fill_boxes_data *data = closure;
    _box_to_point (box, &dst);
    _cairo_matrix_transform_bounding_box_fixed (data->matrix, box, NULL);
    _box_to_rectangle (box, &src);
    if (src.w && src.h) {
	result = data->self->surface->Blit (data->self->surface,
					    data->surface, &src,
					    dst.x, dst.y);
	if (unlikely (DFB_OK != result)) {
	    return FALSE;
	}
    }
    return TRUE;
}

/**
 * _fill_scaled_boxes:
 * @box: a #cairo_box_t
 * @closure: private data passed to _cairo_boxes_for_each_box()
 *
 * This function is a callback for _cairo_boxes_for_each_box().
 *
 * Calls StretchBlit() to blend the rectangles in @boxes. Matrix
 * transformations will be applied.
 *
 * Returns: True if the operation succeeded, otherwise False
 **/
static cairo_bool_t
_fill_scaled_boxes (cairo_box_t	*box,
		    void	*closure)
{
    DFBResult result;
    DFBRectangle src, dst;
    struct fill_boxes_data *data = closure;
    _box_to_rectangle (box, &dst);
    _cairo_matrix_transform_bounding_box_fixed (data->matrix, box, NULL);
    _box_to_rectangle (box, &src);
    if (src.w && src.h && dst.w && dst.h) {
	result = data->self->surface->StretchBlit (data->self->surface,
						   data->surface,
						   &src, &dst);
	if (unlikely (DFB_OK != result)) {
	    return FALSE;
	}
    }
    return TRUE;
}

/**
 * _fill_tiled_boxes:
 * @box: a #cairo_box_t
 * @closure: private data passed to _cairo_boxes_for_each_box()
 *
 * This function is a callback for _cairo_boxes_for_each_box().
 *
 * Calls TileBlit() to blend the rectangles in @boxes. Matrix transformations
 * will be applied.
 *
 * Returns: True if the operation succeeded, otherwise False
 **/
static cairo_bool_t
_fill_tiled_boxes (cairo_box_t	*box,
		   void		*closure)
{
    DFBPoint point;
    DFBResult result;
    DFBRectangle src;
    struct fill_boxes_data *data = closure;
    _box_to_point (box, &point);
    _cairo_matrix_transform_bounding_box_fixed (data->matrix, box, NULL);
    _box_to_rectangle (box, &src);
    if (src.w && src.h) {
	DFBRectangle copy, rect = { 0, 0, 0, 0 };
	copy = src;
	(void) data->surface->GetSize (data->surface, &rect.w, &rect.h);
	if (dfb_rectangle_intersect (&copy, &rect)) {
	    result = data->self->surface->TileBlit (data->self->surface,
						    data->surface, &src,
						    point.x, point.y);
	} else {
	    result = data->self->surface->TileBlit (data->self->surface,
						    data->surface, NULL,
						    point.x, point.y);
	}
	if (unlikely (DFB_OK != result)) {
	    return FALSE;
	}
    }
    return TRUE;
}

/**
 * _fill_solid_boxes:
 * @box: a #cairo_box_t
 * @closure: private data passed to _cairo_boxes_for_each_box()
 *
 * This function is a callback for _cairo_boxes_for_each_box().
 *
 * Calls FillRectangle() to blend the rectangles in @boxes.
 *
 * Returns: True if the operation succeeded, otherwise False
 **/
static cairo_bool_t
_fill_solid_boxes (cairo_box_t	*box,
		   void		*closure)
{
    DFBResult result;
    DFBRectangle rect;
    struct fill_boxes_data *data = closure;
    _box_to_rectangle (box, &rect);
    if (rect.w > 0 && rect.h > 0) {
	result = data->self->surface->FillRectangle (data->self->surface,
						     rect.x, rect.y,
						     rect.w, rect.h);
	if (unlikely (DFB_OK != result)) {
	    return FALSE;
	}
    }
    return TRUE;
}

/**
 * _cairo_dfb_surface_fill_boxes:
 * @base: a #cairo_surface_t
 * @op: a #cairo_operator_t
 * @src: a #cairo_pattern_t
 * @boxes: a #cairo_boxes_t
 *
 * Performs a blit operation to fill @boxes with the surface pattern @src.
 * Scaling and translation matrices are supported.
 *
 * Returns: %CAIRO_INT_STATUS_SUCCESS if the operation succeeded
 **/
cairo_int_status_t
_cairo_dfb_surface_fill_boxes (cairo_surface_t		*base,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*src,
			       cairo_boxes_t		*boxes)
{
    cairo_bool_t ok;
    cairo_int_status_t status;
    const cairo_color_t *color;
    IDirectFBSurface *surface = NULL;
    const cairo_surface_pattern_t *pattern;
    DFBSurfaceBlendFunction src_blend, dst_blend;
    struct fill_boxes_data data = { NULL, NULL, NULL };
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) base;
    cairo_bool_t (*callback) (cairo_box_t *box, void *data) = NULL;
    if (! _cairo_matrix_is_scale (&src->matrix)) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    status = _cairo_to_dfb_operators (op, &src_blend, &dst_blend);
    if (CAIRO_INT_STATUS_SUCCESS != status) {
	return status;
    }
    switch (src->type) {
    case CAIRO_PATTERN_TYPE_SURFACE:
	pattern = (const cairo_surface_pattern_t *) src;
	if (cairo_surface_get_type (pattern->surface) != CAIRO_SURFACE_TYPE_DIRECTFB) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
	switch (src->extend) {
	case CAIRO_EXTEND_PAD:
	    if (1.0 == src->matrix.xx && 1.0 == src->matrix.yy) {
		callback = _fill_boxes;
	    } else {
		callback = _fill_scaled_boxes;
	    }
	    break;
	case CAIRO_EXTEND_REPEAT:
	    if (1.0 == src->matrix.xx && 1.0 == src->matrix.yy) {
		callback = _fill_tiled_boxes;
	    } else {
		return CAIRO_INT_STATUS_UNSUPPORTED;
	    }
	    break;
	case CAIRO_EXTEND_NONE:
	case CAIRO_EXTEND_REFLECT:
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
	if (DSBF_ONE == src_blend && DSBF_ZERO == dst_blend) {
	    DFBSurfaceBlittingFlags flags = DSBLIT_NOFX | self->mask;
	    (void) self->surface->SetBlittingFlags (self->surface, flags);
	} else {
	    DFBSurfaceBlittingFlags flags =
		DSBLIT_BLEND_ALPHACHANNEL | self->mask;
	    (void) self->surface->SetBlittingFlags (self->surface, flags);
	    (void) self->surface->SetSrcBlendFunction (self->surface,
						       src_blend);
	    (void) self->surface->SetDstBlendFunction (self->surface,
						       dst_blend);
	}
	if (pattern->surface->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *subsurface;
	    cairo_dfb_surface_t *source;
	    DFBRectangle rect;
	    DFBResult result;
	    subsurface = (cairo_surface_subsurface_t *) pattern->surface;
	    source = (cairo_dfb_surface_t *) subsurface->target;
	    rect.x = subsurface->extents.x;
	    rect.y = subsurface->extents.y;
	    rect.w = subsurface->extents.width;
	    rect.h = subsurface->extents.height;
	    result = source->surface->GetSubSurface (source->surface,
						     &rect,
						     &surface);
	    if (unlikely (status != DFB_OK)) {
		return CAIRO_INT_STATUS_NO_MEMORY;
	    }
	} else {
	    surface = ((cairo_dfb_surface_t *) pattern->surface)->surface;
	    (void) surface->AddRef (surface);
	}
	data.surface = surface;
	data.matrix = &src->matrix;
	break;
    case CAIRO_PATTERN_TYPE_SOLID:
	color = &((cairo_solid_pattern_t *) src)->color;
	status = _cairo_dfb_surface_set_color (self, color);
	if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	    return status;
	}
	if (CAIRO_COLOR_IS_OPAQUE (color)) {
	    if (src_blend == DSBF_SRCALPHA) {
		src_blend = DSBF_ONE;
	    } else if (src_blend == DSBF_INVSRCALPHA) {
		src_blend = DSBF_ZERO;
	    }
	    if (dst_blend == DSBF_SRCALPHA) {
		dst_blend = DSBF_ONE;
	    } else if (dst_blend == DSBF_INVSRCALPHA) {
		dst_blend = DSBF_ZERO;
	    }
	}
	if ((self->base.content & CAIRO_CONTENT_ALPHA) == 0) {
	    if (src_blend == DSBF_DESTALPHA) {
		src_blend = DSBF_ONE;
	    } else if (src_blend == DSBF_INVDESTALPHA) {
		src_blend = DSBF_ZERO;
	    }
	    if (dst_blend == DSBF_DESTALPHA) {
		dst_blend = DSBF_ONE;
	    } else if (dst_blend == DSBF_INVDESTALPHA) {
		dst_blend = DSBF_ZERO;
	    }
	}
	if (DSBF_ONE == src_blend && DSBF_ZERO == dst_blend) {
	    (void) self->surface->SetDrawingFlags (self->surface, DSDRAW_NOFX);
	} else {
	    (void) self->surface->SetDrawingFlags (self->surface, DSDRAW_BLEND);
	    (void) self->surface->SetSrcBlendFunction (self->surface,
						       src_blend);
	    (void) self->surface->SetDstBlendFunction (self->surface,
						       dst_blend);
	}
	callback = _fill_solid_boxes;
	break;
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_MESH:
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    data.self = self;
    ok = _cairo_boxes_for_each_box (boxes, callback, &data);
    if (surface) {
	(void) surface->Release (surface);
    }
    if (! ok) {
	return CAIRO_INT_STATUS_NO_MEMORY;
    }
    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 * _cairo_dfb_surface_set_clip:
 * @base: a #cairo_surface_t
 * @clip: a #cairo_clip_t
 *
 * Sets the clip region for the next drawing operation.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
cairo_int_status_t
_cairo_dfb_surface_set_clip (cairo_surface_t	*base,
			     cairo_clip_t	*clip)
{
    DFBResult result;
    DFBRegion clip_region;
    cairo_region_t *region;
    cairo_rectangle_int_t rect;
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) base;
    region = _cairo_clip_get_region (clip);
    if (! region) {
	return CAIRO_INT_STATUS_SUCCESS;
    }
    cairo_region_get_extents (region, &rect);
    clip_region.x1 = rect.x;
    clip_region.y1 = rect.y;
    clip_region.x2 = rect.x + rect.width - 1;
    clip_region.y2 = rect.y + rect.height - 1;
    result = self->surface->SetClip (self->surface, &clip_region);
    if (unlikely (DFB_OK != result)) {
	return CAIRO_INT_STATUS_NO_MEMORY;
    }
    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 * _cairo_dfb_surface_reset_clip:
 * @base: a #cairo_surface_t
 *
 * Resets the clip region.
 **/
void
_cairo_dfb_surface_reset_clip (cairo_surface_t *base)
{
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) base;
    (void) self->surface->SetClip (self->surface, NULL);
}

/**
 * _cairo_dfb_surface_set_mask:
 * @base: a #cairo_surface_t
 * @mask_base: a #cairo_surface_t
 *
 * Sets a source mask for for blit operations.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
cairo_int_status_t
_cairo_dfb_surface_set_mask (cairo_surface_t *base,
			     cairo_surface_t *mask_base)
{
    DFBResult result;
    IDirectFBSurface *surface;
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) base;
    if (cairo_surface_get_type (mask_base) != CAIRO_SURFACE_TYPE_DIRECTFB) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    if (mask_base->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	cairo_surface_subsurface_t *subsurface;
	cairo_dfb_surface_t *mask;
	DFBRectangle rect;
	subsurface = (cairo_surface_subsurface_t *) mask_base;
	mask = (cairo_dfb_surface_t *) subsurface->target;
	rect.x = subsurface->extents.x;
	rect.y = subsurface->extents.y;
	rect.w = subsurface->extents.width;
	rect.h = subsurface->extents.height;
	result = mask->surface->GetSubSurface (mask->surface, &rect, &surface);
	if (unlikely (result != DFB_OK)) {
	    return CAIRO_INT_STATUS_NO_MEMORY;
	}
    } else {
	cairo_dfb_surface_t *mask = (cairo_dfb_surface_t *) mask_base;
	surface = mask->surface;
	(void) surface->AddRef (surface);
    }
    result = self->surface->SetSourceMask (self->surface,
					   surface,
					   0, 0,
					   DSMF_NONE);
    (void) surface->Release (surface);
    if (unlikely (DFB_OK != result)) {
	return CAIRO_INT_STATUS_NO_MEMORY;
    }
    self->mask = DSBLIT_SRC_MASK_ALPHA;
    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 * _cairo_dfb_surface_reset_mask:
 * @base: a #cairo_surface_t
 *
 * Resets the source mask.
 **/
void
_cairo_dfb_surface_reset_mask (cairo_surface_t *base)
{
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) base;
    self->mask = DSBLIT_NOFX;
}

/**
 * _cairo_dfb_surface_draw_glyphs:
 * @base: a #cairo_surface_t
 * @op: a #cairo_operator_t
 * @scaled_font: a #cairo_scaled_font_t
 * @glyphs: an array of #cairo_glyph_t
 * @num_glyphs: then number of glyphs in @glyphs
 *
 * Draws @glyphs from @scaled_font with the source @color. Glyphs are cached
 * for future use.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
cairo_int_status_t
_cairo_dfb_surface_draw_glyphs (cairo_surface_t		*base,
				cairo_operator_t	 op,
				const cairo_color_t	*color,
				cairo_scaled_font_t	*scaled_font,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs)
{
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) base;
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    DFBSurfaceBlendFunction src_blend, dst_blend;
    static cairo_user_data_key_t key;
    DFBSurfaceBlittingFlags flags;
    DFBRectangle *rects = NULL;
    DFBPoint *points = NULL;
    IDirectFBSurface *cache;
    cairo_dfb_font_t *font;
    DFBResult result;
    status = _cairo_to_dfb_operators (op, &src_blend, &dst_blend);
    if (CAIRO_INT_STATUS_SUCCESS != status) {
	goto exit;
    }
    if (DSBF_DESTALPHA == src_blend || DSBF_INVDESTALPHA == src_blend) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    font = cairo_scaled_font_get_user_data (scaled_font, &key);
    if (unlikely (! font)) {
	font = _cairo_dfb_font_create (scaled_font, self->dfb);
	if (unlikely (! font)) {
	    status = CAIRO_INT_STATUS_NO_MEMORY;
	    goto exit;
	}
	status = (cairo_int_status_t)
		cairo_scaled_font_set_user_data (scaled_font, &key, font,
						 (cairo_destroy_func_t)
						 _cairo_dfb_font_destroy);
	if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	    goto exit;
	}
    }
    rects = _cairo_malloc_ab (num_glyphs, sizeof (*rects));
    if (unlikely (! rects)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
	goto exit;
    }
    points = _cairo_malloc_ab (num_glyphs, sizeof (*points));
    if (unlikely (! points)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
	goto exit;
    }
    status = _cairo_dfb_font_get_rectangles (font, glyphs, num_glyphs,
					     &rects, &points, &cache);
    if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	goto exit;
    }
    status = _cairo_dfb_surface_set_color (self, color);
    if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	goto exit;
    }
    flags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE;
    if (! CAIRO_COLOR_IS_OPAQUE (color)) {
	flags |= DSBLIT_BLEND_COLORALPHA;
    }
    (void) self->surface->SetBlittingFlags (self->surface, flags);
    (void) self->surface->SetSrcBlendFunction (self->surface, src_blend);
    (void) self->surface->SetDstBlendFunction (self->surface, dst_blend);
    result = self->surface->BatchBlit (self->surface, cache,
				       rects, points, num_glyphs);
    if (unlikely (DFB_OK != result)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
    }
exit:
    free (rects);
    free (points);
    return status;
}

/**
 * _cairo_to_dfb_format:
 * @format: a #cairo_format_t
 *
 * Get the equivalent DirectFB pixel format for @format.
 *
 * Returns: A #DFBSurfacePixelFormat or -1 if no equivalent exists
 **/
static DFBSurfacePixelFormat
_cairo_to_dfb_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return DSPF_ARGB;
    case CAIRO_FORMAT_RGB24:
	return DSPF_RGB32;
    case CAIRO_FORMAT_A8:
	return DSPF_A8;
    case CAIRO_FORMAT_A1:
#ifdef WORDS_BIGENDIAN
	return DSPF_A1;
#else
#if DFB_NUM_PIXELFORMATS > 35
	return DSPF_A1_LSB;
#else
	return -1;
#endif
#endif
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB30:
    case CAIRO_FORMAT_INVALID:
    default:
	return -1;
    }
}

/**
 * _dfb_to_cairo_content:
 * @format: a #DFBSurfacePixelFormat
 *
 * Get the equivalent Cairo content for @format.
 *
 * Returns: A #cairo_content_t or 0 if no equivalent exists
 **/
static cairo_content_t
_dfb_to_cairo_content (DFBSurfacePixelFormat format)
{
    cairo_content_t content = 0;
    if (DFB_PIXELFORMAT_HAS_ALPHA (format)) {
	content |= CAIRO_CONTENT_ALPHA;
    }
    if (DFB_COLOR_BITS_PER_PIXEL (format)) {
	content |= CAIRO_CONTENT_COLOR_ALPHA;
    }
    return content;
}

/**
 * _dfb_to_pixman_format:
 * @format: a #DFBSurfacePixelFormat
 *
 * Get the equivalent Pixman format for @format.
 *
 * Returns: A #pixman_format_code_t or 0 if no equivalent exists
 **/
static pixman_format_code_t
_dfb_to_pixman_format (DFBSurfacePixelFormat format)
{
    if (DSPF_ARGB1555 == format) {
	return PIXMAN_a1r5g5b5;
    } else if (DSPF_RGB16 == format) {
	return PIXMAN_r5g6b5;
    } else if (DSPF_RGB24 == format) {
	return PIXMAN_r8g8b8;
    } else if (DSPF_RGB32 == format) {
	return PIXMAN_x8r8g8b8;
    } else if (DSPF_ARGB == format) {
	return PIXMAN_a8r8g8b8;
    } else if (DSPF_A8 == format) {
	return PIXMAN_a8;
    } else if (DSPF_YUY2 == format) {
	return PIXMAN_yuy2;
    } else if (DSPF_RGB332 == format) {
	return PIXMAN_r3g3b2;
    } else if (DSPF_YV12 == format) {
	return PIXMAN_yv12;
    } else if (DSPF_A1 == format) {
#ifdef WORDS_BIGENDIAN
	return PIXMAN_a1;
#else
	return 0;
#endif
    } else if (DSPF_ARGB4444 == format) {
	return PIXMAN_a4r4g4b4;
    } else if (DSPF_A4 == format) {
	return PIXMAN_a4;
    } else if (DSPF_RGB444 == format) {
	return PIXMAN_x4r4g4b4;
    } else if (DSPF_RGB555 == format) {
	return PIXMAN_x1r5g5b5;
#if DFB_NUM_PIXELFORMATS > 29
    } else if (DSPF_BGR555 == format) {
	return PIXMAN_x1b5g5r5;
#if DFB_NUM_PIXELFORMATS > 35
    } else if (DSPF_A1_LSB == format) {
#ifdef WORDS_BIGENDIAN
	return 0;
#else
	return PIXMAN_a1;
#endif
#endif
#endif
    } else {
	return 0;
    }
}

/**
 * _cairo_dfb_surface_release_image:
 * @self: a #cairo_dfb_surface_t
 *
 * Decreases the lock count for @self. If the count reaches zero then the
 * Pixman surface will be destroyed and the DirectFB surface will be unlocked.
 **/
static void
_cairo_dfb_surface_release_image (cairo_dfb_surface_t *self)
{
    if (! --self->locked) {
	if (likely (self->image && self->image->pixman_image)) {
	    pixman_image_unref (self->image->pixman_image);
	    (void) self->surface->Unlock (self->surface);
	    self->image->pixman_image = NULL;
	    self->image->data = NULL;
	}
    }
}

/**
 * _cairo_dfb_surface_acquire_image:
 * @self: a #cairo_dfb_surface_t
 *
 * Increases the lock count for @self and creates an image surface (or Pixman
 * image) for @self if one does not exist. Upon success the DirectFB surface
 * will be locked.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_status_t
_cairo_dfb_surface_acquire_image (cairo_dfb_surface_t *self)
{
    DFBSurfacePixelFormat pixelformat;
    pixman_format_code_t format;
    int width, height, stride;
    cairo_surface_t *image;
    DFBResult result;
    void *data;
    if (self->locked++) {
	return CAIRO_STATUS_SUCCESS;
    }
    if (likely (self->image)) {
	pixman_image_t *pixman_image;
	if (likely (self->image->pixman_image)) {
	    return CAIRO_STATUS_SUCCESS;
	}
	result = self->surface->Lock (self->surface, DSLF_READ | DSLF_WRITE,
				      &data, &stride);
	if (unlikely (DFB_OK != result)) {
	    return CAIRO_STATUS_NO_MEMORY;
	}
	pixman_image = pixman_image_create_bits (self->image->pixman_format,
						 self->image->width,
						 self->image->height,
						 data, stride);
	if (unlikely (! pixman_image)) {
	    (void) self->surface->Unlock (self->surface);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	_cairo_image_surface_init (self->image, pixman_image,
				   self->image->pixman_format);
	return CAIRO_STATUS_SUCCESS;
    }
    result = self->surface->GetPixelFormat (self->surface, &pixelformat);
    if (unlikely (DFB_OK != result)) {
	return CAIRO_STATUS_NO_MEMORY;
    }
    format = _dfb_to_pixman_format (pixelformat);
    if (unlikely (! format)) {
	return CAIRO_STATUS_INVALID_FORMAT;
    }
    if (unlikely (! pixman_format_supported_destination (format))) {
	return CAIRO_STATUS_INVALID_FORMAT;
    }
    result = self->surface->Lock (self->surface, DSLF_READ | DSLF_WRITE,
				  &data, &stride);
    if (unlikely (DFB_OK != result)) {
	return CAIRO_STATUS_NO_MEMORY;
    }
    (void) self->surface->GetSize (self->surface, &width, &height);
    image = _cairo_image_surface_create_with_pixman_format (data,
							    format,
							    width,
							    height,
							    stride);
    if (unlikely (CAIRO_STATUS_SUCCESS != image->status)) {
	cairo_status_t status = image->status;
	(void) self->surface->Unlock (self->surface);
	cairo_surface_destroy ((cairo_surface_t *) image);
	return status;
    }
    self->image = (cairo_image_surface_t *) image;
    return CAIRO_STATUS_SUCCESS;
}

/* high-level surface interface */

static cairo_status_t
_cairo_dfb_surface_finish (void *base)
{
    cairo_dfb_surface_t *self = base;
    if (likely (self)) {
	if (likely (self->surface)) {
	    (void) self->surface->Release (self->surface);
	}
	if (likely (self->dfb)) {
	    (void) self->dfb->Release (self->dfb);
	}
	if (self->image) {
	    cairo_surface_destroy ((cairo_surface_t *) self->image);
	}
    }
    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_dfb_surface_create_similar (void		       *base,
				   cairo_content_t	content,
				   int			width,
				   int			height)
{
    DFBResult result;
    cairo_surface_t *similar;
    IDirectFBSurface *surface;
    DFBSurfaceDescription dsc;
    DFBSurfaceCapabilities caps;
    DFBSurfacePixelFormat format;
    cairo_dfb_surface_t *self = base;
    if (unlikely (width <= 0 || height <= 0)) {
	return _cairo_image_surface_create_with_content (content,
							 width,
							 height);
    }
    format = _cairo_to_dfb_format (_cairo_format_from_content (content));
    if (unlikely (-1 == format)) {
	cairo_status_t error = CAIRO_STATUS_INVALID_FORMAT;
	return _cairo_surface_create_in_error (error);
    }
    (void) self->surface->GetCapabilities (self->surface, &caps);
    dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT |
		DSDESC_PIXELFORMAT | DSDESC_CAPS;
    dsc.width = width;
    dsc.height = height;
    dsc.pixelformat = format;
    dsc.caps = caps & DSCAPS_PREMULTIPLIED ? DSCAPS_PREMULTIPLIED : DSCAPS_NONE;
    result = self->dfb->CreateSurface (self->dfb, &dsc, &surface);
    if (unlikely (DFB_OK != result)) {
	cairo_status_t error = CAIRO_STATUS_NO_MEMORY;
	return _cairo_surface_create_in_error (error);
    }
    similar = cairo_directfb_surface_create (self->dfb, surface);
    (void) surface->Release (surface);
    return similar;
}

static cairo_image_surface_t *
_cairo_dfb_surface_map_to_image (void				*base,
				 const cairo_rectangle_int_t	*extents)
{
    cairo_dfb_surface_t *self = base;
    cairo_status_t status;
    status = _cairo_dfb_surface_acquire_image (self);
    if (unlikely (CAIRO_STATUS_SUCCESS != status)) {
	return _cairo_image_surface_create_in_error (status);
    }
    return _cairo_image_surface_map_to_image (self->image, extents);
}

static cairo_int_status_t
_cairo_dfb_surface_unmap_image (void			*base,
				cairo_image_surface_t	*image)
{
    cairo_dfb_surface_t *self = base;
    _cairo_dfb_surface_release_image (self);
    return _cairo_image_surface_unmap_image (self->image, image);
}

static cairo_status_t
_cairo_dfb_surface_acquire_source_image (void			 *base,
					 cairo_image_surface_t	**image,
					 void			**image_extra)
{
    cairo_dfb_surface_t *self = base;
    cairo_status_t status;
    status = _cairo_dfb_surface_acquire_image (self);
    if (unlikely (CAIRO_STATUS_SUCCESS != status)) {
	return status;
    }
    *image = self->image;
    *image_extra = NULL;
    return (*image)->base.status;
}

static void
_cairo_dfb_surface_release_source_image (void			*base,
					 cairo_image_surface_t	*image,
					 void			*image_extra)
{
    cairo_dfb_surface_t *self = base;
    _cairo_dfb_surface_release_image (self);
}

static cairo_bool_t
_cairo_dfb_surface_get_extents (void			*base,
				cairo_rectangle_int_t	*rectangle)
{
    cairo_dfb_surface_t *self = base;
    rectangle->x = rectangle->y = 0;
    (void) self->surface->GetSize (self->surface,
				   &rectangle->width,
				   &rectangle->height);
    return TRUE;
}

static cairo_int_status_t
_cairo_dfb_surface_paint (void			*base,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  const cairo_clip_t	*clip)
{
    cairo_dfb_surface_t *self = base;
    return _cairo_compositor_paint (&self->compositor,
				    &self->base,
				    op, source,
				    clip);
}

static cairo_int_status_t
_cairo_dfb_surface_mask (void			*base,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 const cairo_pattern_t	*mask,
			 const cairo_clip_t	*clip)
{
    cairo_dfb_surface_t *self = base;
    return _cairo_compositor_mask (&self->compositor,
				   &self->base,
				   op, source, mask,
				   clip);
}

static cairo_int_status_t
_cairo_dfb_surface_stroke (void				*base,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   const cairo_path_fixed_t	*path,
			   const cairo_stroke_style_t	*style,
			   const cairo_matrix_t		*ctm,
			   const cairo_matrix_t		*ctm_inverse,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   const cairo_clip_t		*clip)
{
    cairo_dfb_surface_t *self = base;
    return _cairo_compositor_stroke (&self->compositor,
				     &self->base,
				     op, source,
				     path, style, ctm, ctm_inverse,
				     tolerance, antialias,
				     clip);
}

static cairo_int_status_t
_cairo_dfb_surface_fill (void				*base,
			 cairo_operator_t		 op,
			 const cairo_pattern_t		*source,
			 const cairo_path_fixed_t	*path,
			 cairo_fill_rule_t		 fill_rule,
			 double				 tolerance,
			 cairo_antialias_t		 antialias,
			 const cairo_clip_t		*clip)
{
    cairo_dfb_surface_t *self = base;
    return _cairo_compositor_fill (&self->compositor,
				   &self->base,
				   op, source,
				   path, fill_rule, tolerance, antialias,
				   clip);
}

static cairo_int_status_t
_cairo_dfb_surface_show_glyphs (void				*base,
				cairo_operator_t		 op,
				const cairo_pattern_t		*source,
				cairo_glyph_t			*glyphs,
				int				 num_glyphs,
				cairo_scaled_font_t		*scaled_font,
				const cairo_clip_t		*clip)
{
    cairo_dfb_surface_t *self = base;
    return _cairo_compositor_glyphs (&self->compositor,
				     &self->base,
				     op, source,
				     glyphs, num_glyphs, scaled_font,
				     clip);
}

static cairo_surface_backend_t cairo_dfb_surface_backend = {
    .type = CAIRO_SURFACE_TYPE_DIRECTFB,
    .finish = _cairo_dfb_surface_finish,
    .create_context = _cairo_default_context_create,
    .create_similar = _cairo_dfb_surface_create_similar,
    .create_similar_image = NULL,
    .map_to_image = _cairo_dfb_surface_map_to_image,
    .unmap_image = _cairo_dfb_surface_unmap_image,
    .source = _cairo_surface_default_source,
    .acquire_source_image = _cairo_dfb_surface_acquire_source_image,
    .release_source_image = _cairo_dfb_surface_release_source_image,
    .snapshot = NULL,
    .copy_page = NULL,
    .show_page = NULL,
    .get_extents = _cairo_dfb_surface_get_extents,
    .get_font_options = _cairo_image_surface_get_font_options,
    .flush = NULL,
    .mark_dirty_rectangle = NULL,
    .paint = _cairo_dfb_surface_paint,
    .mask = _cairo_dfb_surface_mask,
    .stroke = _cairo_dfb_surface_stroke,
    .fill = _cairo_dfb_surface_fill,
    .fill_stroke = NULL,
    .show_glyphs = _cairo_dfb_surface_show_glyphs,
    .has_show_text_glyphs = NULL,
    .show_text_glyphs = NULL,
    .get_supported_mime_types = NULL,
};

/**
 * _cairo_surface_is_dfb:
 * @base: a #cairo_surface_t
 *
 * Checks if a @base is a #cairo_dfb_surface_t
 *
 * Returns: True if the surface is a DirectFB surface
 **/
static cairo_bool_t
_cairo_surface_is_dfb (cairo_surface_t *base)
{
    return base->backend == &cairo_dfb_surface_backend;
}

/* public interface */

/**
 * cairo_directfb_surface_create:
 * @dfb: a DirectFB context
 * @surface: a DirectFB surface
 *
 * Creates a DirectFB surface that draws to the given surface.
 *
 * Returns: the newly created surface
 *
 * Since: 1.2
 **/
cairo_surface_t *
cairo_directfb_surface_create (IDirectFB	*dfb,
			       IDirectFBSurface	*surface)
{
    DFBResult result;
    cairo_status_t error;
    cairo_content_t content = 0;
    DFBSurfacePixelFormat format;
    cairo_dfb_surface_t *self = NULL;
    result = surface->GetPixelFormat (surface, &format);
    if (unlikely (DFB_OK != result)) {
	error = CAIRO_STATUS_NO_MEMORY;
	goto error;
    }
    content = _dfb_to_cairo_content (format);
    if (unlikely (0 == content)) {
	error = CAIRO_STATUS_INVALID_FORMAT;
	goto error;
    }
    self = calloc (1, sizeof (cairo_dfb_surface_t));
    if (unlikely (! self)) {
	error = CAIRO_STATUS_NO_MEMORY;
	goto error;
    }
    _cairo_surface_init (&self->base,
                         &cairo_dfb_surface_backend,
			 NULL,
			 content);
    (void) dfb->AddRef (dfb);
    self->dfb = dfb;
    (void) surface->AddRef (surface);
    self->surface = surface;
    self->mask = DSBLIT_NOFX;
    self->locked = 0;
    _cairo_dfb_compositor_get (&self->compositor,
			       CAIRO_DIRECTFB_ACCELERATION_FLAG_PAINT |
			       CAIRO_DIRECTFB_ACCELERATION_FLAG_MASK |
			       CAIRO_DIRECTFB_ACCELERATION_FLAG_FILL |
			       CAIRO_DIRECTFB_ACCELERATION_FLAG_STROKE |
			       CAIRO_DIRECTFB_ACCELERATION_FLAG_GLYPHS);
    return &self->base;
error:
    (void) _cairo_dfb_surface_finish (self);
    free (self);
    return _cairo_surface_create_in_error (error);
}
slim_hidden_def(cairo_directfb_surface_create);

/**
 * cairo_directfb_surface_get_context:
 * @surface: a DirectFB surface
 *
 * Get the underlying DirectFB context used for the surface.
 *
 * Returns: the DirectFB context
 *
 * Since: 1.14
 **/
IDirectFB *
cairo_directfb_surface_get_context (cairo_surface_t *surface)
{
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) surface;
    if (unlikely (! _cairo_surface_is_dfb (surface))) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }
    return self->dfb;
}
slim_hidden_def(cairo_directfb_surface_get_context);

/**
 * cairo_directfb_surface_get_surface:
 * @surface: a DirectFB surface
 *
 * Get the underlying DirectFB surface used for the surface.
 *
 * Returns: the underlying DirectFB surface
 *
 * Since: 1.14
 **/
IDirectFBSurface *
cairo_directfb_surface_get_surface (cairo_surface_t *surface)
{
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) surface;
    if (unlikely (! _cairo_surface_is_dfb (surface))) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }
    return self->surface;
}
slim_hidden_def(cairo_directfb_surface_get_surface);

/**
 * cairo_directfb_surface_get_width:
 * @surface: a DirectFB surface
 *
 * Get the width of the DirectFB surface underlying the surface in pixels.
 *
 * Return value: the width of the surface in pixels.
 *
 * Since: 1.14
 **/
int
cairo_directfb_surface_get_width (cairo_surface_t *surface)
{
    int width;
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) surface;
    if (unlikely (! _cairo_surface_is_dfb (surface))) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }
    (void) self->surface->GetSize (self->surface, &width, NULL);
    return width;
}
slim_hidden_def(cairo_directfb_surface_get_width);

/**
 * cairo_directfb_surface_get_height:
 * @surface: a DirectFB surface
 *
 * Get the height of the DirectFB surface underlying the surface in pixels.
 *
 * Return value: the width of the surface in pixels.
 *
 * Since: 1.14
 **/
int
cairo_directfb_surface_get_height (cairo_surface_t *surface)
{
    int height;
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) surface;
    if (unlikely (! _cairo_surface_is_dfb (surface))) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }
    (void) self->surface->GetSize (self->surface, NULL, &height);
    return height;
}
slim_hidden_def(cairo_directfb_surface_get_height);

/**
 * cairo_directfb_surface_set_acceleration:
 * @surface: a DirectFB surface
 * @flags: new acceleration flags
 *
 * Set the acceleration flags for the surface. All operations are accelerated
 * by default.
 *
 * Since: 1.14
 **/
void
cairo_directfb_surface_set_acceleration (cairo_surface_t			*surface,
					 cairo_directfb_acceleration_flags_t	 flags)
{
    cairo_dfb_surface_t *self = (cairo_dfb_surface_t *) surface;
    if (unlikely (! _cairo_surface_is_dfb (surface))) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }
    _cairo_dfb_compositor_get (&self->compositor, flags);
}
slim_hidden_def(cairo_directfb_surface_set_acceleration);

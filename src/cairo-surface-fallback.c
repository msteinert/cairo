/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairo-surface-fallback-private.h"
#include "cairo-clip-private.h"

typedef struct {
    cairo_surface_t *dst;
    cairo_rectangle_t extents;
    cairo_image_surface_t *image;
    cairo_rectangle_t image_rect;
    void *image_extra;
} fallback_state_t;

/**
 * _fallback_init:
 * 
 * Acquire destination image surface needed for an image-based
 * fallback.
 * 
 * Return value: CAIRO_INT_STATUS_NOTHING_TO_DO if the extents are not
 * visible, CAIRO_STATUS_SUCCESS if some portion is visible and all
 * went well, or some error status otherwise.
 **/
static cairo_int_status_t
_fallback_init (fallback_state_t *state,
		cairo_surface_t  *dst,
		int               x,
		int               y,
		int               width,
		int               height)
{
    cairo_status_t status;

    state->extents.x = x;
    state->extents.y = y;
    state->extents.width = width;
    state->extents.height = height;
    
    state->dst = dst;

    status = _cairo_surface_acquire_dest_image (dst, &state->extents,
						&state->image, &state->image_rect,
						&state->image_extra);
    if (status)
	return status;

    /* XXX: This NULL value tucked away in state->image is a rather
     * ugly interface. Cleaner would be to push the
     * CAIRO_INT_STATUS_NOTHING_TO_DO value down into
     * _cairo_surface_acquire_dest_image and its backend
     * counterparts. */
    if (state->image == NULL)
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    return CAIRO_STATUS_SUCCESS;
}

static void
_fallback_fini (fallback_state_t *state)
{
    _cairo_surface_release_dest_image (state->dst, &state->extents,
				       state->image, &state->image_rect,
				       state->image_extra);
}

cairo_status_t
_cairo_surface_fallback_paint (cairo_surface_t	*surface,
			       cairo_operator_t	 op,
			       cairo_pattern_t	*source)
{
    cairo_status_t status;
    cairo_rectangle_t extents;
    cairo_box_t box;
    cairo_traps_t traps;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }
    
    status = _cairo_clip_intersect_to_rectangle (surface->clip, &extents);
    if (status)
	return status;

    box.p1.x = _cairo_fixed_from_int (extents.x);
    box.p1.y = _cairo_fixed_from_int (extents.y);
    box.p2.x = _cairo_fixed_from_int (extents.x + extents.width);
    box.p2.y = _cairo_fixed_from_int (extents.y + extents.height);

    status = _cairo_traps_init_box (&traps, &box);
    if (status)
	return status;
    
    _cairo_surface_clip_and_composite_trapezoids (source,
						  op,
						  surface,
						  &traps,
						  surface->clip,
						  CAIRO_ANTIALIAS_NONE);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_surface_mask_draw_func (void                    *closure,
			       cairo_operator_t         op,
			       cairo_pattern_t         *src,
			       cairo_surface_t         *dst,
			       int                      dst_x,
			       int                      dst_y,
			       const cairo_rectangle_t *extents)
{
    cairo_pattern_t *mask = closure;

    if (src)
	return _cairo_surface_composite (op,
					 src, mask, dst,
					 extents->x,         extents->y,
					 extents->x,         extents->y,
					 extents->x - dst_x, extents->y - dst_y,
					 extents->width,     extents->height);
    else
	return _cairo_surface_composite (op,
					 mask, NULL, dst,
					 extents->x,         extents->y,
					 0,                  0, /* unused */
					 extents->x - dst_x, extents->y - dst_y,
					 extents->width,     extents->height);
}

cairo_status_t
_cairo_surface_fallback_mask (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_pattern_t		*mask)
{
    cairo_status_t status;
    cairo_rectangle_t extents, source_extents, mask_extents;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }
    
    if (_cairo_operator_bounded_by_mask (op)) {
	status = _cairo_pattern_get_extents (mask, &mask_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &mask_extents);
    }

    status = _cairo_clip_intersect_to_rectangle (surface->clip, &extents);
    if (status)
	return status;

    status = _cairo_gstate_clip_and_composite (surface->clip, op,
					       source,
					       _cairo_surface_mask_draw_func,
					       mask,
					       surface,
					       &extents);

    return status;
}

cairo_status_t
_cairo_surface_fallback_stroke (cairo_surface_t		*surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				cairo_path_fixed_t	*path,
				cairo_stroke_style_t	*stroke_style,
				cairo_matrix_t		*ctm,
				cairo_matrix_t		*ctm_inverse,
				double			 tolerance,
				cairo_antialias_t	 antialias)
{
    cairo_status_t status;
    cairo_traps_t traps;
    
    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path,
						stroke_style,
						ctm, ctm_inverse,
						tolerance,
						&traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_surface_clip_and_composite_trapezoids (source,
						  op,
						  surface,
						  &traps,
						  surface->clip,
						  antialias);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_surface_fallback_fill (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias)
{
    cairo_status_t status;
    cairo_traps_t traps;

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path,
					      fill_rule,
					      tolerance,
					      &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    status = _cairo_surface_clip_and_composite_trapezoids (source,
							   op,
							   surface,
							   &traps,
							   surface->clip,
							   antialias);

    _cairo_traps_fini (&traps);

    return status;
}

typedef struct {
    cairo_scaled_font_t *font;
    const cairo_glyph_t *glyphs;
    int num_glyphs;
} cairo_show_glyphs_info_t;

static cairo_status_t
_cairo_surface_old_show_glyphs_draw_func (void                    *closure,
					  cairo_operator_t         op,
					  cairo_pattern_t         *src,
					  cairo_surface_t         *dst,
					  int                      dst_x,
					  int                      dst_y,
					  const cairo_rectangle_t *extents)
{
    cairo_show_glyphs_info_t *glyph_info = closure;
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    /* Modifying the glyph array is fine because we know that this function
     * will be called only once, and we've already made a copy of the
     * glyphs in the wrapper.
     */
    if (dst_x != 0 || dst_y != 0) {
	int i;
	
	for (i = 0; i < glyph_info->num_glyphs; ++i)
	{
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].x -= dst_x;
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].y -= dst_y;
	}
    }

    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    if (!src)
	src = &pattern.base;
    
    status = _cairo_surface_old_show_glyphs (glyph_info->font, op, src, 
					     dst,
					     extents->x, extents->y,
					     extents->x - dst_x,
					     extents->y - dst_y,
					     extents->width,
					     extents->height,
					     glyph_info->glyphs,
					     glyph_info->num_glyphs);

    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;
    
    status = _cairo_scaled_font_show_glyphs (glyph_info->font, 
					     op, 
					     src, dst,
					     extents->x,         extents->y,
					     extents->x - dst_x, extents->y - dst_y,
					     extents->width,     extents->height,
					     glyph_info->glyphs,
					     glyph_info->num_glyphs);

    if (src == &pattern.base)
	_cairo_pattern_fini (&pattern.base);

    return status;
}

cairo_status_t
_cairo_surface_fallback_show_glyphs (cairo_surface_t		*surface,
				     cairo_operator_t		 op,
				     cairo_pattern_t		*source,
				     const cairo_glyph_t	*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font)
{
    cairo_status_t status;
    cairo_rectangle_t extents, glyph_extents;
    cairo_show_glyphs_info_t glyph_info;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_mask (op)) {
	status = _cairo_scaled_font_glyph_device_extents (scaled_font,
							  glyphs, 
							  num_glyphs, 
							  &glyph_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &glyph_extents);
    }
    
    status = _cairo_clip_intersect_to_rectangle (surface->clip, &extents);
    if (status)
	return status;
    
    glyph_info.font = scaled_font;
    glyph_info.glyphs = glyphs;
    glyph_info.num_glyphs = num_glyphs;
    
    status = _cairo_gstate_clip_and_composite (surface->clip,
					       op,
					       source,
					       _cairo_surface_old_show_glyphs_draw_func,
					       &glyph_info,
					       surface,
					       &extents);
    
    return status;
}

cairo_surface_t *
_cairo_surface_fallback_snapshot (cairo_surface_t *surface)
{
    cairo_surface_t *snapshot;
    cairo_status_t status;
    cairo_pattern_union_t pattern;
    cairo_image_surface_t *image;
    void *image_extra;

    status = _cairo_surface_acquire_source_image (surface,
						  &image, &image_extra);
    if (status != CAIRO_STATUS_SUCCESS)
	return (cairo_surface_t *) &_cairo_surface_nil;

    snapshot = cairo_image_surface_create (image->format,
					   image->width,
					   image->height);
    if (cairo_surface_status (snapshot))
	return snapshot;

    _cairo_pattern_init_for_surface (&pattern.surface, &image->base);

    _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
			      &pattern.base,
			      NULL,
			      snapshot,
			      0, 0,
			      0, 0,
			      0, 0,
			      image->width,
			      image->height);

    _cairo_pattern_fini (&pattern.base);

    _cairo_surface_release_source_image (surface,
					 image, &image_extra);

    snapshot->is_snapshot = TRUE;

    return snapshot;
}

cairo_status_t
_cairo_surface_fallback_composite (cairo_operator_t	op,
				   cairo_pattern_t	*src,
				   cairo_pattern_t	*mask,
				   cairo_surface_t	*dst,
				   int			src_x,
				   int			src_y,
				   int			mask_x,
				   int			mask_y,
				   int			dst_x,
				   int			dst_y,
				   unsigned int		width,
				   unsigned int		height)
{
    fallback_state_t state;
    cairo_status_t status;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (status) {
	if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	    return CAIRO_STATUS_SUCCESS;
	return status;
    }

    status = state.image->base.backend->composite (op, src, mask,
						   &state.image->base,
						   src_x, src_y, mask_x, mask_y,
						   dst_x - state.image_rect.x,
						   dst_y - state.image_rect.y,
						   width, height);
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_surface_fallback_fill_rectangles (cairo_surface_t	*surface,
					 cairo_operator_t	 op,
					 const cairo_color_t	*color,
					 cairo_rectangle_t	*rects,
					 int			 num_rects)
{
    fallback_state_t state;
    cairo_rectangle_t *offset_rects = NULL;
    cairo_status_t status;
    int x1, y1, x2, y2;
    int i;

    assert (! surface->is_snapshot);

    if (num_rects <= 0)
	return CAIRO_STATUS_SUCCESS;

    /* Compute the bounds of the rectangles, so that we know what area of the
     * destination surface to fetch
     */
    x1 = rects[0].x;
    y1 = rects[0].y;
    x2 = rects[0].x + rects[0].width;
    y2 = rects[0].y + rects[0].height;
    
    for (i = 1; i < num_rects; i++) {
	if (rects[i].x < x1)
	    x1 = rects[i].x;
	if (rects[i].y < y1)
	    y1 = rects[i].y;

	if (rects[i].x + rects[i].width > x2)
	    x2 = rects[i].x + rects[i].width;
	if (rects[i].y + rects[i].height > y2)
	    y2 = rects[i].y + rects[i].height;
    }

    status = _fallback_init (&state, surface, x1, y1, x2 - x1, y2 - y1);
    if (status) {
	if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	    return CAIRO_STATUS_SUCCESS;
	return status;
    }

    /* If the fetched image isn't at 0,0, we need to offset the rectangles */
    
    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_rects = malloc (sizeof (cairo_rectangle_t) * num_rects);
	if (offset_rects == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto DONE;
	}

	for (i = 0; i < num_rects; i++) {
	    offset_rects[i].x = rects[i].x - state.image_rect.x;
	    offset_rects[i].y = rects[i].y - state.image_rect.y;
	    offset_rects[i].width = rects[i].width;
	    offset_rects[i].height = rects[i].height;
	}

	rects = offset_rects;
    }

    status = state.image->base.backend->fill_rectangles (&state.image->base,
							 op, color,
							 rects, num_rects);

    free (offset_rects);

 DONE:
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_surface_fallback_composite_trapezoids (cairo_operator_t		op,
					      cairo_pattern_t	       *pattern,
					      cairo_surface_t	       *dst,
					      cairo_antialias_t		antialias,
					      int			src_x,
					      int			src_y,
					      int			dst_x,
					      int			dst_y,
					      unsigned int		width,
					      unsigned int		height,
					      cairo_trapezoid_t	       *traps,
					      int			num_traps)
{
    fallback_state_t state;
    cairo_trapezoid_t *offset_traps = NULL;
    cairo_status_t status;
    int i;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (status) {
	if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	    return CAIRO_STATUS_SUCCESS;
	return status;
    }

    /* If the destination image isn't at 0,0, we need to offset the trapezoids */
    
    if (state.image_rect.x != 0 || state.image_rect.y != 0) {

	cairo_fixed_t xoff = _cairo_fixed_from_int (state.image_rect.x);
	cairo_fixed_t yoff = _cairo_fixed_from_int (state.image_rect.y);
	
	offset_traps = malloc (sizeof (cairo_trapezoid_t) * num_traps);
	if (!offset_traps) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto DONE;
	}

	for (i = 0; i < num_traps; i++) {
	    offset_traps[i].top = traps[i].top - yoff;
	    offset_traps[i].bottom = traps[i].bottom - yoff;
	    offset_traps[i].left.p1.x = traps[i].left.p1.x - xoff;
	    offset_traps[i].left.p1.y = traps[i].left.p1.y - yoff;
	    offset_traps[i].left.p2.x = traps[i].left.p2.x - xoff;
	    offset_traps[i].left.p2.y = traps[i].left.p2.y - yoff;
	    offset_traps[i].right.p1.x = traps[i].right.p1.x - xoff;
	    offset_traps[i].right.p1.y = traps[i].right.p1.y - yoff;
	    offset_traps[i].right.p2.x = traps[i].right.p2.x - xoff;
	    offset_traps[i].right.p2.y = traps[i].right.p2.y - yoff;
	}

	traps = offset_traps;
    }

    state.image->base.backend->composite_trapezoids (op, pattern,
						     &state.image->base,
						     antialias,
						     src_x, src_y,
						     dst_x - state.image_rect.x,
						     dst_y - state.image_rect.y,
						     width, height, traps, num_traps);
    if (offset_traps)
	free (offset_traps);

 DONE:
    _fallback_fini (&state);
    
    return status;
}

/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2008 Chris Wilson
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
 * The Initial Developer of the Original Code is Chris Wilson.
 */

#include "cairoint.h"

#include "cairo-sdl.h"

typedef struct _cairo_sdl_surface {
    cairo_surface_t base;

    SDL_Surface *sdl;
    cairo_image_surface_t *image;

    cairo_region_t update;
} cairo_sdl_surface_t;

static const cairo_surface_backend_t _cairo_sdl_surface_backend;

static cairo_surface_t *
_cairo_sdl_surface_create_internal (SDL_Surface *sdl,
				    cairo_surface_t *image)
{
    cairo_sdl_surface_t *surface;

    surface = malloc (sizeof (cairo_sdl_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_sdl_surface_backend,
			 image->content);

    surface->sdl = sdl;
    sdl->refcount++;
    surface->image = (cairo_image_surface_t *) cairo_surface_reference (image);

    _cairo_region_init (&surface->update);

    return &surface->base;
}

static cairo_surface_t *
_cairo_sdl_surface_create_similar (void	       *abstract_src,
				   cairo_content_t	content,
				   int		width,
				   int		height)
{
    return _cairo_image_surface_create_with_content (content, width, height);
}

static cairo_status_t
_cairo_sdl_surface_finish (void *abstract_surface)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    cairo_surface_destroy (&surface->image->base);
    SDL_FreeSurface (surface->sdl);

    _cairo_region_fini (&surface->update);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_sdl_surface_acquire_source_image (void                    *abstract_surface,
					 cairo_image_surface_t  **image_out,
					 void                   **image_extra)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    SDL_LockSurface (surface->sdl);

    *image_out = surface->image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_sdl_surface_release_source_image (void                   *abstract_surface,
					 cairo_image_surface_t  *image,
					 void                   *image_extra)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    SDL_UnlockSurface (surface->sdl);
}

static cairo_status_t
_cairo_sdl_surface_acquire_dest_image (void                    *abstract_surface,
				       cairo_rectangle_int_t   *interest_rect,
				       cairo_image_surface_t  **image_out,
				       cairo_rectangle_int_t   *image_rect_out,
				       void                   **image_extra)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    SDL_LockSurface (surface->sdl);

    image_rect_out->x = 0;
    image_rect_out->y = 0;
    image_rect_out->width  = surface->image->width;
    image_rect_out->height = surface->image->height;

    *image_out = surface->image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_sdl_surface_release_dest_image (void                    *abstract_surface,
				       cairo_rectangle_int_t   *interest_rect,
				       cairo_image_surface_t   *image,
				       cairo_rectangle_int_t   *image_rect,
				       void                    *image_extra)
{
    cairo_sdl_surface_t *surface = abstract_surface;
    cairo_status_t status;

    SDL_UnlockSurface (surface->sdl);

    status = _cairo_region_union_rect (&surface->update,
				       &surface->update,
				       interest_rect);
    status = _cairo_surface_set_error (&surface->base, status);
}

static cairo_status_t
_cairo_sdl_surface_clone_similar (void		*abstract_surface,
				  cairo_surface_t	*src,
				  int                  src_x,
				  int                  src_y,
				  int                  width,
				  int                  height,
				  int                 *clone_offset_x,
				  int                 *clone_offset_y,
				  cairo_surface_t    **clone_out)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    if (src->backend == surface->base.backend) {
	*clone_offset_x = *clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image = (cairo_image_surface_t *) src;
	cairo_format_masks_t masks;
	cairo_surface_t *clone;
	SDL_Surface *sdl;

	_pixman_format_to_masks (image->pixman_format, &masks);

	sdl = SDL_CreateRGBSurfaceFrom (image->data,
					image->width,
					image->height,
					masks.bpp,
					image->stride,
					masks.red_mask,
					masks.green_mask,
					masks.blue_mask,
					masks.alpha_mask);
	if (sdl == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	clone = _cairo_sdl_surface_create_internal (sdl, &image->base);
	SDL_FreeSurface (sdl);

	if (clone->status)
	    return clone->status;

	*clone_offset_x = *clone_offset_y = 0;
	*clone_out = clone;
	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_sdl_surface_composite (cairo_operator_t		 op,
			      const cairo_pattern_t	*src_pattern,
			      const cairo_pattern_t	*mask_pattern,
			      void			*abstract_dst,
			      int			 src_x,
			      int			 src_y,
			      int			 mask_x,
			      int			 mask_y,
			      int			 dst_x,
			      int			 dst_y,
			      unsigned int		 width,
			      unsigned int		 height)
{
    cairo_sdl_surface_t *dst = abstract_dst;
    cairo_sdl_surface_t *src;
    cairo_surface_attributes_t src_attr;
    cairo_bool_t is_integer_translation;
    int itx, ity;
    cairo_int_status_t status;

    /* under a few conditions we can perform a (hardware) blit...*/
    if (op != CAIRO_OPERATOR_SOURCE)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (mask_pattern)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (dst->base.current_clip_serial != 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_pattern_acquire_surface (src_pattern, &dst->base,
					     src_x, src_y, width, height,
					     (cairo_surface_t **) &src,
					     &src_attr);
    if (status)
	return status;

    is_integer_translation =
	_cairo_matrix_is_integer_translation (&src_attr.matrix, &itx, &ity);

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (is_integer_translation &&
	src_attr.extend == CAIRO_EXTEND_NONE &&
	src_attr.filter == CAIRO_FILTER_NEAREST)
    {
	SDL_Rect src_rect;
	SDL_Rect dst_rect;
	cairo_rectangle_int_t rect;

	src_rect.x = src_x + src_attr.x_offset + itx;
	src_rect.y = src_y + src_attr.y_offset + ity;
	src_rect.w = width;
	src_rect.h = height;

	dst_rect.x = dst_x;
	dst_rect.y = dst_y;
	dst_rect.w = width;
	dst_rect.h = height;

	SDL_BlitSurface (src->sdl, &src_rect, dst->sdl, &dst_rect);

	rect.x = dst_x;
	rect.y = dst_y;
	rect.width  = width;
	rect.height = height;
	status = _cairo_region_union_rect (&dst->update,
					   &dst->update,
					   &rect);
    }

    _cairo_pattern_release_surface (src_pattern, &src->base, &src_attr);
    return status;
}

static cairo_int_status_t
_cairo_sdl_surface_set_clip_region (void *abstract_surface,
				    cairo_region_t *region)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    return _cairo_surface_set_clip_region (&surface->image->base,
					   region,
					   surface->base.current_clip_serial);
}

static cairo_int_status_t
_cairo_sdl_surface_get_extents (void			  *abstract_surface,
				cairo_rectangle_int_t   *rectangle)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->image->width;
    rectangle->height = surface->image->height;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_sdl_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
    cairo_sdl_surface_t *surface = abstract_surface;

    cairo_surface_get_font_options (&surface->image->base, options);
}

static cairo_status_t
_cairo_sdl_surface_flush (void                  *abstract_surface)
{
    cairo_sdl_surface_t *surface = abstract_surface;
    cairo_box_int_t *boxes;
    int n_boxes, i;
    cairo_status_t status;

    n_boxes = 0;
    status = _cairo_region_get_boxes (&surface->update, &n_boxes, &boxes);
    if (status)
	return status;
    if (n_boxes == 0)
	return CAIRO_STATUS_SUCCESS;

    for (i = 0; i < n_boxes; i++) {
	SDL_UpdateRect (surface->sdl,
			boxes[i].p1.x,
			boxes[i].p1.y,
			boxes[i].p2.x - boxes[i].p1.x,
			boxes[i].p2.y - boxes[i].p1.y);
    }

    _cairo_region_boxes_fini (&surface->update, boxes);

    _cairo_region_fini (&surface->update);
    _cairo_region_init (&surface->update);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t _cairo_sdl_surface_backend = {
    CAIRO_SURFACE_TYPE_SDL,
    _cairo_sdl_surface_create_similar,
    _cairo_sdl_surface_finish,
    _cairo_sdl_surface_acquire_source_image,
    _cairo_sdl_surface_release_source_image,
    _cairo_sdl_surface_acquire_dest_image,
    _cairo_sdl_surface_release_dest_image,
    _cairo_sdl_surface_clone_similar,
    _cairo_sdl_surface_composite,
    NULL, /* fill rectangles */
    NULL, /* composite traps */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_sdl_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_sdl_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_sdl_surface_get_font_options,
    _cairo_sdl_surface_flush, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* font_fini */
    NULL, /* glyph_fini */

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* show_glyphs */
    NULL, /* snapshot */
    NULL, /* is_similar */

    NULL, /* reset */
};

static cairo_surface_t *
_cairo_image_surface_create_for_sdl (SDL_Surface *surface)
{
    cairo_format_masks_t masks;
    pixman_format_code_t format;

    masks.bpp = surface->format->BitsPerPixel;
    masks.alpha_mask = surface->format->Amask;
    masks.red_mask = surface->format->Rmask;
    masks.green_mask = surface->format->Gmask;
    masks.blue_mask = surface->format->Bmask;

    if (! _pixman_format_from_masks (&masks, &format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    return _cairo_image_surface_create_with_pixman_format (surface->pixels,
							   format,
							   surface->w,
							   surface->h,
							   surface->pitch);
}

cairo_surface_t *
cairo_sdl_surface_create (SDL_Surface *sdl)
{
    cairo_surface_t *image;
    cairo_surface_t *surface;

    image = _cairo_image_surface_create_for_sdl (sdl);
    if (image->status)
	return image;

    surface = _cairo_sdl_surface_create_internal (sdl, image);
    cairo_surface_destroy (image);

    return surface;
}

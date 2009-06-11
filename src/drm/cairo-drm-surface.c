/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
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

#include "cairo-drm-private.h"
#include "cairo-surface-fallback-private.h"

cairo_surface_t *
_cairo_drm_surface_create_similar (void			*abstract_surface,
			           cairo_content_t	 content,
				   int			 width,
				   int			 height)
{
    cairo_drm_surface_t *surface = abstract_surface;
    cairo_drm_device_t *device;

    if (surface->fallback != NULL)
	return _cairo_image_surface_create_with_content (content,
		                                         width, height);

    device = surface->device;
    if (width > device->max_surface_size || height > device->max_surface_size)
	return NULL;

    return device->surface.create (device, content, width, height);
}

void
_cairo_drm_surface_init (cairo_drm_surface_t *surface,
			 cairo_drm_device_t *device)
{
    surface->device = cairo_drm_device_reference (device);

    surface->bo = NULL;
    surface->width  = 0;
    surface->height = 0;
    surface->stride = 0;

    surface->fallback = NULL;
    surface->map_count = 0;
}

cairo_status_t
_cairo_drm_surface_finish (cairo_drm_surface_t *surface)
{
    if (surface->bo != NULL)
	cairo_drm_bo_destroy (surface->device, surface->bo);

    cairo_drm_device_destroy (surface->device);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_drm_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);
}

cairo_bool_t
_cairo_drm_surface_get_extents (void *abstract_surface,
			        cairo_rectangle_int_t *rectangle)
{
    cairo_drm_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return TRUE;
}

cairo_int_status_t
_cairo_drm_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  cairo_clip_t		*clip)
{
    cairo_drm_surface_t *surface = abstract_surface;

    if (surface->fallback != NULL)
	return _cairo_surface_paint (surface->fallback, op, source, clip);

    return _cairo_surface_fallback_paint (&surface->base, op, source, clip);
}

cairo_int_status_t
_cairo_drm_surface_mask (void			*abstract_surface,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  const cairo_pattern_t	*mask,
			  cairo_clip_t		*clip)
{
    cairo_drm_surface_t *surface = abstract_surface;

    if (surface->fallback != NULL) {
	return _cairo_surface_mask (surface->fallback,
				    op, source, mask,
				    clip);
    }

    return _cairo_surface_fallback_mask (&surface->base,
	                                 op, source, mask, clip);
}

cairo_int_status_t
_cairo_drm_surface_stroke (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   cairo_path_fixed_t		*path,
			   cairo_stroke_style_t		*style,
			   cairo_matrix_t		*ctm,
			   cairo_matrix_t		*ctm_inverse,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   cairo_clip_t			*clip)
{
    cairo_drm_surface_t *surface = abstract_surface;

    if (surface->fallback != NULL) {
	return _cairo_surface_stroke (surface->fallback,
				      op, source,
				      path, style,
				      ctm, ctm_inverse,
				      tolerance, antialias,
				      clip);
    }

    return _cairo_surface_fallback_stroke (&surface->base, op, source,
					   path, style,
					   ctm, ctm_inverse,
					   tolerance, antialias,
					   clip);
}

cairo_int_status_t
_cairo_drm_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias,
			 cairo_clip_t		*clip)
{
    cairo_drm_surface_t *surface = abstract_surface;

    if (surface->fallback != NULL) {
	return _cairo_surface_fill (surface->fallback,
				    op, source,
				    path, fill_rule,
				    tolerance, antialias,
				    clip);
    }

    return _cairo_surface_fallback_fill (&surface->base, op, source,
					 path, fill_rule,
					 tolerance, antialias,
					 clip);
}

cairo_int_status_t
_cairo_drm_surface_show_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				const cairo_pattern_t	*source,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font,
				cairo_clip_t		*clip,
				int			*remaining_glyphs)
{
    cairo_drm_surface_t *surface = abstract_surface;

    if (surface->fallback != NULL) {
	*remaining_glyphs = 0;
	return _cairo_surface_show_text_glyphs (surface->fallback,
						op, source,
						NULL, 0,
						glyphs, num_glyphs,
						NULL, 0, 0,
						scaled_font,
						clip);
    }

    return _cairo_surface_fallback_show_glyphs (&surface->base,
						op, source,
						glyphs, num_glyphs,
						scaled_font,
						clip);
}


cairo_surface_t *
cairo_drm_surface_create (cairo_drm_device_t *device,
			  cairo_content_t content,
			  int width, int height)
{
    cairo_surface_t *surface;

    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    if (device != NULL && device->status)
    {
	surface = _cairo_surface_create_in_error (device->status);
    }
    else if (device == NULL ||
	     device->surface.create == NULL ||
	     width == 0 || width > device->max_surface_size ||
	     height == 0 || height > device->max_surface_size)
    {
	surface = _cairo_image_surface_create_with_content (content,
							    width, height);
    }
    else
    {
	surface = device->surface.create (device, content, width, height);
    }

    return surface;
}

cairo_surface_t *
cairo_drm_surface_create_for_name (cairo_drm_device_t *device,
				   unsigned int name,
	                           cairo_format_t format,
				   int width, int height, int stride)
{
    cairo_surface_t *surface;

    if (! CAIRO_FORMAT_VALID (format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    if (device != NULL && device->status)
    {
	surface = _cairo_surface_create_in_error (device->status);
    }
    else if (device == NULL || device->surface.create_for_name == NULL)
    {
	/* XXX invalid device! */
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }
    else if (width == 0 || width > device->max_surface_size ||
	     height == 0 || height > device->max_surface_size)
    {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    }
    else
    {
	surface = device->surface.create_for_name (device,
	                                             name, format,
						     width, height, stride);
    }

    return surface;
}

cairo_surface_t *
cairo_drm_surface_create_from_cacheable_image (cairo_drm_device_t *dev,
	                                       cairo_surface_t *surface)
{
    if (surface->status) {
	surface = _cairo_surface_create_in_error (surface->status);
    } else if (dev != NULL && dev->status) {
	surface = _cairo_surface_create_in_error (dev->status);
    } else if (dev == NULL || dev->surface.create_from_cacheable_image == NULL) {
	/* XXX invalid device! */
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    } else {
	surface = dev->surface.create_from_cacheable_image (dev, surface);
    }

    return surface;
}

static cairo_drm_surface_t *
_cairo_surface_as_drm (cairo_surface_t *abstract_surface)
{
    if (unlikely (abstract_surface->status))
	return NULL;

    if (abstract_surface->type != CAIRO_SURFACE_TYPE_DRM)
	return NULL;

    return (cairo_drm_surface_t *) abstract_surface;
}

cairo_status_t
cairo_drm_surface_enable_scan_out (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);

    if (surface->device->surface.enable_scan_out == NULL)
	return CAIRO_STATUS_SUCCESS;

    return surface->device->surface.enable_scan_out (abstract_surface);
}

cairo_drm_device_t *
cairo_drm_surface_get_device (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    if (unlikely (abstract_surface->status))
	return _cairo_drm_device_create_in_error (abstract_surface->status);

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return surface->device;
}

unsigned int
cairo_drm_surface_get_handle (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->bo->handle;
}

cairo_int_status_t
_cairo_drm_surface_flink (void *abstract_surface)
{
    cairo_drm_surface_t *surface = abstract_surface;

    return _cairo_drm_bo_flink (surface->device, surface->bo);
}

unsigned int
cairo_drm_surface_get_name (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;
    cairo_status_t status;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    if (surface->bo->name)
	return surface->bo->name;

    if (surface->device->surface.flink == NULL)
	return 0;

    status = surface->device->surface.flink (abstract_surface);
    if (status) {
	if (_cairo_status_is_error (status))
	    status = _cairo_surface_set_error (abstract_surface, status);

	return 0;
    }

    return surface->bo->name;
}

cairo_format_t
cairo_drm_surface_get_format (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_format (abstract_surface);

    return surface->format;
}

int
cairo_drm_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_width (abstract_surface);

    return surface->width;
}

int
cairo_drm_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_height (abstract_surface);

    return surface->height;
}

int
cairo_drm_surface_get_stride (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_stride (abstract_surface);

    return surface->stride;
}

/* XXX drm or general surface layer? naming? */
cairo_surface_t *
cairo_drm_surface_map (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;
    cairo_rectangle_int_t roi;
    cairo_image_surface_t *image;
    cairo_status_t status;
    void *image_extra;

    if (unlikely (abstract_surface->status))
	return _cairo_surface_create_in_error (abstract_surface->status);

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	if (_cairo_surface_is_image (abstract_surface))
	    return cairo_surface_reference (abstract_surface);

	status = _cairo_surface_set_error (abstract_surface,
					   CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return _cairo_surface_create_in_error (status);
    }

    roi.x = roi.y = 0;
    roi.width = surface->width;
    roi.height = surface->height;

    status = _cairo_surface_acquire_dest_image (abstract_surface,
	                                        &roi,
						&image,
						&roi,
						&image_extra);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    assert (image_extra == NULL);

    surface->map_count++;

    return &image->base;
}

void
cairo_drm_surface_unmap (cairo_surface_t *abstract_surface,
	                 cairo_surface_t *image)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	if (_cairo_surface_is_image (abstract_surface))
	    cairo_surface_destroy (image);
	else
	    _cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    /* XXX assert image belongs to drm */
    //assert (image == drm->fallback);
    cairo_surface_destroy (image);

    assert (surface->map_count > 0);
    if (--surface->map_count == 0)
	cairo_surface_flush (&surface->base);
}

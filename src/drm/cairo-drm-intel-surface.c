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
 */

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-intel-private.h"

/* Basic generic/stub surface for intel chipsets */

#define MAX_SIZE 2048

typedef struct _intel_surface intel_surface_t;

struct _intel_surface {
    cairo_drm_surface_t base;
};

static inline intel_device_t *
to_intel_device (cairo_drm_device_t *device)
{
    return (intel_device_t *) device;
}

static inline intel_bo_t *
to_intel_bo (cairo_drm_bo_t *bo)
{
    return (intel_bo_t *) bo;
}

static cairo_status_t
intel_batch_flush (intel_device_t *device)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
intel_surface_batch_flush (intel_surface_t *surface)
{
    if (to_intel_bo (surface->base.bo)->write_domain)
	return intel_batch_flush (to_intel_device (surface->base.device));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
intel_surface_finish (void *abstract_surface)
{
    intel_surface_t *surface = abstract_surface;

    return _cairo_drm_surface_finish (&surface->base);
}

static cairo_status_t
intel_surface_acquire_source_image (void *abstract_surface,
				   cairo_image_surface_t **image_out,
				   void **image_extra)
{
    intel_surface_t *surface = abstract_surface;
    cairo_surface_t *image;
    cairo_status_t status;

    if (surface->base.fallback != NULL) {
	image = surface->base.fallback;
	goto DONE;
    }

    image = _cairo_surface_has_snapshot (&surface->base.base,
	                                 &_cairo_image_surface_backend,
					 surface->base.base.content);
    if (image != NULL)
	goto DONE;

    status = intel_surface_batch_flush (surface);
    if (unlikely (status))
	return status;

    image = intel_bo_get_image (to_intel_device (surface->base.device),
				to_intel_bo (surface->base.bo),
				&surface->base);
    status = image->status;
    if (unlikely (status))
	return status;

    status = _cairo_surface_attach_snapshot (&surface->base.base,
	                                     image,
	                                     cairo_surface_destroy);
    if (unlikely (status)) {
	cairo_surface_destroy (image);
	return status;
    }

DONE:
    *image_out = (cairo_image_surface_t *) cairo_surface_reference (image);
    *image_extra = NULL;
    return CAIRO_STATUS_SUCCESS;
}

static void
intel_surface_release_source_image (void *abstract_surface,
				    cairo_image_surface_t *image,
				    void *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_surface_t *
intel_surface_snapshot (void *abstract_surface)
{
    intel_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->base.fallback != NULL)
	return NULL;

    status = intel_surface_batch_flush (surface);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    return intel_bo_get_image (to_intel_device (surface->base.device),
	                       to_intel_bo (surface->base.bo),
			       &surface->base);
}

static cairo_status_t
intel_surface_acquire_dest_image (void *abstract_surface,
				  cairo_rectangle_int_t *interest_rect,
				  cairo_image_surface_t **image_out,
				  cairo_rectangle_int_t *image_rect_out,
				  void **image_extra)
{
    intel_surface_t *surface = abstract_surface;
    cairo_surface_t *image;
    cairo_status_t status;
    void *ptr;

    assert (surface->base.fallback == NULL);

    status = intel_surface_batch_flush (surface);
    if (unlikely (status))
	return status;

    /* Force a read barrier, as well as flushing writes above */
    if (to_intel_bo (surface->base.bo)->in_batch) {
	status = intel_batch_flush (to_intel_device (surface->base.device));
	if (unlikely (status))
	    return status;
    }

    ptr = intel_bo_map (to_intel_device (surface->base.device),
			to_intel_bo (surface->base.bo));
    if (unlikely (ptr == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    image = cairo_image_surface_create_for_data (ptr,
						 surface->base.format,
						 surface->base.width,
						 surface->base.height,
						 surface->base.stride);
    status = image->status;
    if (unlikely (status)) {
	intel_bo_unmap (to_intel_bo (surface->base.bo));
	return status;
    }

    surface->base.fallback = cairo_surface_reference (image);

    *image_out = (cairo_image_surface_t *) image;
    *image_extra = NULL;

    image_rect_out->x = 0;
    image_rect_out->y = 0;
    image_rect_out->width  = surface->base.width;
    image_rect_out->height = surface->base.height;

    return CAIRO_STATUS_SUCCESS;
}

static void
intel_surface_release_dest_image (void                    *abstract_surface,
				 cairo_rectangle_int_t   *interest_rect,
				 cairo_image_surface_t   *image,
				 cairo_rectangle_int_t   *image_rect,
				 void                    *image_extra)
{
    /* Keep the fallback until we flush, either explicitly or at the
     * end of this context. The idea is to avoid excess migration of
     * the buffer between GPU and CPU domains.
     */
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
intel_surface_flush (void *abstract_surface)
{
    intel_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->base.fallback == NULL)
	return intel_surface_batch_flush (surface);

    /* kill any outstanding maps */
    cairo_surface_finish (surface->base.fallback);

    status = cairo_surface_status (surface->base.fallback);
    cairo_surface_destroy (surface->base.fallback);
    surface->base.fallback = NULL;

    intel_bo_unmap (to_intel_bo (surface->base.bo));

    return status;
}

static const cairo_surface_backend_t intel_surface_backend = {
    CAIRO_SURFACE_TYPE_DRM,
    _cairo_drm_surface_create_similar,
    intel_surface_finish,

    intel_surface_acquire_source_image,
    intel_surface_release_source_image,
    intel_surface_acquire_dest_image,
    intel_surface_release_dest_image,

    NULL, //intel_surface_clone_similar,
    NULL, //intel_surface_composite,
    NULL, //intel_surface_fill_rectangles,
    NULL, //intel_surface_composite_trapezoids,
    NULL, //intel_surface_create_span_renderer,
    NULL, //intel_surface_check_span_renderer,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_drm_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_drm_surface_get_font_options,
    intel_surface_flush,
    NULL, /* mark_dirty_rectangle */
    NULL, //intel_surface_scaled_font_fini,
    NULL, //intel_surface_scaled_glyph_fini,

    _cairo_drm_surface_paint,
    _cairo_drm_surface_mask,
    _cairo_drm_surface_stroke,
    _cairo_drm_surface_fill,
    _cairo_drm_surface_show_glyphs,

    intel_surface_snapshot,

    NULL, /* is_similar */
};

static void
intel_surface_init (intel_surface_t *surface,
		    cairo_content_t content,
		    cairo_drm_device_t *device)
{
    _cairo_surface_init (&surface->base.base, &intel_surface_backend, content);
    _cairo_drm_surface_init (&surface->base, device);

    switch (content) {
    case CAIRO_CONTENT_ALPHA:
	surface->base.format = CAIRO_FORMAT_A8;
	break;
    case CAIRO_CONTENT_COLOR:
	surface->base.format = CAIRO_FORMAT_RGB24;
	break;
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_CONTENT_COLOR_ALPHA:
	surface->base.format = CAIRO_FORMAT_ARGB32;
	break;
    }
}

static cairo_surface_t *
intel_surface_create_internal (cairo_drm_device_t *device,
		              cairo_content_t content,
			      int width, int height)
{
    intel_surface_t *surface;
    cairo_status_t status;

    surface = malloc (sizeof (intel_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    intel_surface_init (surface, content, device);

    if (width && height) {
	surface->base.width  = width;
	surface->base.height = height;

	/* Vol I, p134: size restrictions for textures */
	width  = (width  + 3) & -4;
	height = (height + 1) & -2;
	surface->base.stride =
	    cairo_format_stride_for_width (surface->base.format, width);
	surface->base.bo = intel_bo_create (to_intel_device (device),
					    surface->base.stride * height,
					    TRUE);
	if (surface->base.bo == NULL) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static cairo_surface_t *
intel_surface_create (cairo_drm_device_t *device,
		      cairo_content_t content,
		      int width, int height)
{
    return intel_surface_create_internal (device, content, width, height);
}

static cairo_surface_t *
intel_surface_create_for_name (cairo_drm_device_t *device,
			       unsigned int name,
			       cairo_format_t format,
			       int width, int height, int stride)
{
    intel_surface_t *surface;
    cairo_content_t content;
    cairo_status_t status;

    switch (format) {
    default:
    case CAIRO_FORMAT_A1:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
	content = CAIRO_CONTENT_COLOR_ALPHA;
	break;
    case CAIRO_FORMAT_RGB24:
	content = CAIRO_CONTENT_COLOR;
	break;
    case CAIRO_FORMAT_A8:
	content = CAIRO_CONTENT_ALPHA;
	break;
    }

    if (stride < cairo_format_stride_for_width (format, width))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    surface = malloc (sizeof (intel_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    intel_surface_init (surface, content, device);

    if (width && height) {
	surface->base.width  = width;
	surface->base.height = height;
	surface->base.stride = stride;

	surface->base.bo = intel_bo_create_for_name (to_intel_device (device),
						     name);
	if (unlikely (surface->base.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error
						   (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static cairo_status_t
intel_surface_enable_scan_out (void *abstract_surface)
{
    intel_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (unlikely (surface->base.bo == NULL))
	return _cairo_error (CAIRO_STATUS_INVALID_SIZE);

    status = intel_surface_batch_flush (surface);
    if (unlikely (status))
	return status;

    if (to_intel_bo (surface->base.bo)->tiling == I915_TILING_Y) {
	intel_bo_set_tiling (to_intel_device (surface->base.device),
			     to_intel_bo (surface->base.bo),
			     I915_TILING_X, surface->base.stride);
    }

    if (unlikely (to_intel_bo (surface->base.bo)->tiling == I915_TILING_Y))
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT); /* XXX */

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
intel_device_throttle (cairo_drm_device_t *device)
{
    cairo_status_t status;

    status = intel_batch_flush (to_intel_device (device));
    if (unlikely (status))
	return status;

    intel_throttle (to_intel_device (device));
    return CAIRO_STATUS_SUCCESS;
}

static void
intel_device_destroy (void *data)
{
    intel_device_t *device = data;

    intel_device_fini (device);

    free (data);
}

cairo_drm_device_t *
_cairo_drm_intel_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    intel_device_t *device;
    cairo_status_t status;

    if (! intel_info (fd, NULL))
	return NULL;

    device = malloc (sizeof (intel_device_t));
    if (unlikely (device == NULL))
	return _cairo_drm_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = intel_device_init (device, fd);
    if (unlikely (status)) {
	free (device);
	return _cairo_drm_device_create_in_error (status);
    }

    device->base.bo.release = intel_bo_release;

    device->base.surface.create = intel_surface_create;
    device->base.surface.create_for_name = intel_surface_create_for_name;
    device->base.surface.create_from_cacheable_image = NULL;
    device->base.surface.flink = _cairo_drm_surface_flink;
    device->base.surface.enable_scan_out = intel_surface_enable_scan_out;

    device->base.device.throttle = intel_device_throttle;
    device->base.device.destroy = intel_device_destroy;

    return _cairo_drm_device_init (&device->base, fd, dev, MAX_SIZE);
}

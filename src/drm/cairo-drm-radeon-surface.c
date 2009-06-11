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
#include "cairo-drm-radeon-private.h"

/* Basic stub surface for radeon chipsets */

#define MAX_SIZE 2048

typedef struct _radeon_surface {
    cairo_drm_surface_t base;
} radeon_surface_t;

static inline radeon_device_t *
to_radeon_device (cairo_drm_device_t *device)
{
    return (radeon_device_t *) device;
}

static inline radeon_bo_t *
to_radeon_bo (cairo_drm_bo_t *bo)
{
    return (radeon_bo_t *) bo;
}

static cairo_status_t
radeon_batch_flush (radeon_device_t *device)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
radeon_surface_batch_flush (radeon_surface_t *surface)
{
    if (to_radeon_bo (surface->base.bo)->write_domain)
	return radeon_batch_flush (to_radeon_device (surface->base.device));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
radeon_surface_finish (void *abstract_surface)
{
    radeon_surface_t *surface = abstract_surface;

    return _cairo_drm_surface_finish (&surface->base);
}

static cairo_status_t
radeon_surface_acquire_source_image (void *abstract_surface,
				     cairo_image_surface_t **image_out,
				     void **image_extra)
{
    radeon_surface_t *surface = abstract_surface;
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

    status = radeon_surface_batch_flush (surface);
    if (unlikely (status))
	return status;

    image = radeon_bo_get_image (to_radeon_device (surface->base.device),
	                         to_radeon_bo (surface->base.bo),
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
radeon_surface_release_source_image (void  *abstract_surface,
				   cairo_image_surface_t *image,
				   void *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_surface_t *
radeon_surface_snapshot (void *abstract_surface)
{
    radeon_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->base.fallback != NULL)
	return NULL;

    status = radeon_surface_batch_flush (surface);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    return radeon_bo_get_image (to_radeon_device (surface->base.device),
	                        to_radeon_bo (surface->base.bo),
				&surface->base);
}

static cairo_status_t
radeon_surface_acquire_dest_image (void *abstract_surface,
				   cairo_rectangle_int_t *interest_rect,
				   cairo_image_surface_t **image_out,
				   cairo_rectangle_int_t *image_rect_out,
				   void **image_extra)
{
    radeon_surface_t *surface = abstract_surface;
    cairo_surface_t *image;
    cairo_status_t status;
    void *ptr;

    assert (surface->base.fallback == NULL);

    status = radeon_surface_batch_flush (surface);
    if (unlikely (status))
	return status;

    /* Force a read barrier, as well as flushing writes above */
    radeon_bo_wait (to_radeon_device (surface->base.device),
		    to_radeon_bo (surface->base.bo));

    ptr = radeon_bo_map (to_radeon_device (surface->base.device),
			 to_radeon_bo (surface->base.bo));
    if (unlikely (ptr == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    image = cairo_image_surface_create_for_data (ptr,
						 surface->base.format,
						 surface->base.width,
						 surface->base.height,
						 surface->base.stride);
    status = image->status;
    if (unlikely (status)) {
	radeon_bo_unmap (to_radeon_bo (surface->base.bo));
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
radeon_surface_release_dest_image (void                    *abstract_surface,
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
radeon_surface_flush (void *abstract_surface)
{
    radeon_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->base.fallback == NULL)
	return radeon_surface_batch_flush (surface);

    /* kill any outstanding maps */
    cairo_surface_finish (surface->base.fallback);

    status = cairo_surface_status (surface->base.fallback);
    cairo_surface_destroy (surface->base.fallback);
    surface->base.fallback = NULL;

    radeon_bo_unmap (to_radeon_bo (surface->base.bo));

    return status;
}

static const cairo_surface_backend_t radeon_surface_backend = {
    CAIRO_SURFACE_TYPE_DRM,
    _cairo_drm_surface_create_similar,
    radeon_surface_finish,

    radeon_surface_acquire_source_image,
    radeon_surface_release_source_image,
    radeon_surface_acquire_dest_image,
    radeon_surface_release_dest_image,

    NULL, //radeon_surface_clone_similar,
    NULL, //radeon_surface_composite,
    NULL, //radeon_surface_fill_rectangles,
    NULL, //radeon_surface_composite_trapezoids,
    NULL, //radeon_surface_create_span_renderer,
    NULL, //radeon_surface_check_span_renderer,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_drm_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_drm_surface_get_font_options,
    radeon_surface_flush,
    NULL, /* mark_dirty_rectangle */
    NULL, //radeon_surface_scaled_font_fini,
    NULL, //radeon_surface_scaled_glyph_fini,

    _cairo_drm_surface_paint,
    _cairo_drm_surface_mask,
    _cairo_drm_surface_stroke,
    _cairo_drm_surface_fill,
    _cairo_drm_surface_show_glyphs,

    radeon_surface_snapshot,

    NULL, /* is_similar */

    NULL, /* reset */
};

static void
radeon_surface_init (radeon_surface_t *surface,
	           cairo_content_t content,
		   cairo_drm_device_t *device)
{
    _cairo_surface_init (&surface->base.base, &radeon_surface_backend, content);
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
radeon_surface_create_internal (cairo_drm_device_t *device,
		              cairo_content_t content,
			      int width, int height)
{
    radeon_surface_t *surface;
    cairo_status_t status;

    surface = malloc (sizeof (radeon_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    radeon_surface_init (surface, content, device);

    if (width && height) {
	surface->base.width  = width;
	surface->base.height = height;

	surface->base.stride =
	    cairo_format_stride_for_width (surface->base.format, width);

	surface->base.bo = radeon_bo_create (to_radeon_device (device),
					     surface->base.stride * height,
					     RADEON_GEM_DOMAIN_GTT);

	if (unlikely (surface->base.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static cairo_surface_t *
radeon_surface_create (cairo_drm_device_t *device,
		     cairo_content_t content,
		     int width, int height)
{
    return radeon_surface_create_internal (device, content, width, height);
}

static cairo_surface_t *
radeon_surface_create_for_name (cairo_drm_device_t *device,
			      unsigned int name,
			      cairo_format_t format,
			      int width, int height, int stride)
{
    radeon_surface_t *surface;
    cairo_status_t status;
    cairo_content_t content;

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

    surface = malloc (sizeof (radeon_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    radeon_surface_init (surface, content, device);

    if (width && height) {
	surface->base.width  = width;
	surface->base.height = height;
	surface->base.stride = stride;

	surface->base.bo = radeon_bo_create_for_name (to_radeon_device (device),
						      name);

	if (unlikely (surface->base.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static void
radeon_device_destroy (void *data)
{
    radeon_device_t *device = data;

    radeon_device_fini (device);

    free (data);
}

cairo_drm_device_t *
_cairo_drm_radeon_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    radeon_device_t *device;
    uint64_t gart_size, vram_size;
    cairo_status_t status;

    if (! radeon_info (fd, &gart_size, &vram_size))
	return NULL;

    device = malloc (sizeof (radeon_device_t));
    if (device == NULL)
	return _cairo_drm_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = radeon_device_init (device, fd);
    if (unlikely (status)) {
	free (device);
	return _cairo_drm_device_create_in_error (status);
    }

    device->base.surface.create = radeon_surface_create;
    device->base.surface.create_for_name = radeon_surface_create_for_name;
    device->base.surface.create_from_cacheable_image = NULL;
    device->base.surface.flink = _cairo_drm_surface_flink;
    device->base.surface.enable_scan_out = NULL;

    device->base.device.throttle = NULL;
    device->base.device.destroy = radeon_device_destroy;

    device->base.bo.release = radeon_bo_release;

    device->vram_limit = vram_size;
    device->gart_limit = gart_size;

    return _cairo_drm_device_init (&device->base, dev, fd, MAX_SIZE);
}

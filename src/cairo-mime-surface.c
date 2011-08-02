/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
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
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

/**
 * SECTION:cairo-mime-surface
 * @Title: Callback Surfaces
 * @Short_Description: Allows the user to provide a callback to supply image
 * data upon demand
 * @See_Also: #cairo_surface_t
 *
 * The mime surfaces provide the ability to render from arbitrary sources
 * not necessarily resident nor immediately usable by Cairo. The user is
 * given the ability to insert a placeholder surface which can be used
 * with a pattern and then later supply the actual pixel data upon demand.
 * This deferred source is given both the sample region for the operation
 * along with the destination surface such that they may be taken into
 * account when creating the actual surface to use as the source of pixel
 * data.
 *
 * The reason why it is called the mime surface is two-fold. First it came
 * about as an extension of the mime-data property to handle deferred
 * image decoding when rendering to pixel buffers (as opposed to the pass-
 * through support in PDF and friends.) And then to further rationalise
 * the name, it is a surface that mimics a real source without ever
 * holding onto to any pixels of its own - a mime surface.
 *
 * The mime-surface callback interface consists of 4 functions. The principal
 * pair are the acquire/release callbacks which are called when pixel data
 * is required for an operation (along with the target surface and the sample
 * extents). The callee must supply a surface that covers the sample area and
 * set the actual extents of the returned surface in the output rectangle
 * parameter. The surface does not necessarily have to be an image surface,
 * but it is expected that an image surface is likely to be the most
 * convenient for uploading pixel data. (Use
 * cairo_surface_create_similar_image() to create an image surface that is
 * optimised for uploading to the target.) The release callback is
 * subsequently called when the returned surface is no longer needed (before
 * the operation completes, within the lifetime of the source).
 *
 * The other pair of functions are to aide with lifetime management of the
 * surface with respect to patterns and other users. The destroy callback
 * allows for the caller to cleanup the associated data when the last
 * reference to surface is destroyed. The snapshot callback is triggered
 * when there is an immutable surface pattern referencing the mime-surface
 * and the mime-surface will be be invalidated. (Since the mime-surface is
 * read-only and a reference will be held by the pattern, this can only be
 * triggered through an explicit cairo_surface_finish().) In this
 * circumstance, we need to clone the source in order to preserve the pixel
 * data for later use (i.e. we have recorded the pattern). The snapshot
 * callback provides an interface for the caller to clone the mime-surface
 * in an efficient manner.  The returned surface may be of any type so long
 * as it holds all pixel data and remains accessible.
 */

/**
 * CAIRO_HAS_MIME_SURFACE:
 *
 * Defined if the mime surface backend is available.
 * The mime surface backend is always built in.
 *
 * @Since: 1.12
 */

#include "cairoint.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"

typedef struct _cairo_mime_surface {
    cairo_surface_t base;

    cairo_rectangle_int_t extents;

    cairo_mime_surface_acquire_t acquire;
    cairo_mime_surface_release_t release;
    cairo_mime_surface_snapshot_t snapshot;
    cairo_mime_surface_destroy_t destroy;

    /* an explicit pre-allocated member in preference to the general user-data */
    void *user_data;
} cairo_mime_surface_t;

static cairo_status_t
_cairo_mime_surface_finish (void *abstract_surface)
{
    cairo_mime_surface_t *surface = abstract_surface;

    if (surface->destroy)
	surface->destroy (&surface->base, surface->user_data);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_mime_surface_get_extents (void			  *abstract_surface,
				 cairo_rectangle_int_t   *rectangle)
{
    cairo_mime_surface_t *surface = abstract_surface;

    *rectangle = surface->extents;
    return TRUE;
}

static cairo_status_t
_cairo_mime_surface_acquire_source_image (void                    *abstract_surface,
					  //cairo_surface_t	  *target,
					  cairo_image_surface_t  **image_out,
					  void                   **image_extra)
{
    cairo_mime_surface_t *mime = abstract_surface;
    cairo_surface_t *acquired;
    cairo_surface_t *dummy_target;
    cairo_rectangle_int_t extents;

    if (mime->acquire == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Force the callee to populate the extents rectangle */
    memset (&extents, 0, sizeof (extents));

    /* Masquerade for a flexible user-interface */
    dummy_target = _cairo_image_surface_create_with_content (mime->base.content, 0, 0);
    acquired = mime->acquire (&mime->base, mime->user_data,
			      dummy_target, &mime->extents, &extents);
    cairo_surface_destroy (dummy_target);

    if (acquired == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* The callee must have supplied us with all the image data */
    assert (extents.width == mime->extents.width && extents.height == mime->extents.height);

    if (! _cairo_surface_is_image (acquired)) {
	cairo_status_t status;
	void *extra = NULL;

	status = _cairo_surface_acquire_source_image (acquired, image_out, &extra);
	if (unlikely (status)) {
	    cairo_surface_destroy (acquired);
	    return status;
	}

	assert (extra == NULL);
	*image_extra = acquired;
    } else {
	*image_out = (cairo_image_surface_t *) acquired;
	*image_extra = NULL;
    }
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_mime_surface_release_source_image (void                   *abstract_surface,
					  cairo_image_surface_t  *image,
					  void                   *image_extra)
{
    cairo_mime_surface_t *mime = abstract_surface;

    if (image_extra) {
	cairo_surface_destroy (&image->base);
	image = image_extra;
    }

    if (mime->release)
	mime->release (&mime->base, mime->user_data, &image->base);
}

static cairo_surface_t *
_cairo_mime_surface_snapshot (void *abstract_surface)
{
    cairo_mime_surface_t *mime = abstract_surface;

    if (mime->snapshot == NULL)
	return NULL;

    return mime->snapshot (&mime->base, mime->user_data);
}

static const cairo_surface_backend_t cairo_mime_surface_backend = {
    CAIRO_SURFACE_TYPE_MIME,
    _cairo_mime_surface_finish,

    NULL, /* Read-only */

    NULL, /* create similar */
    NULL, /* create similar image */
    NULL, /* map to image */
    NULL, /* unmap image */

    _cairo_mime_surface_acquire_source_image,
    _cairo_mime_surface_release_source_image,
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_mime_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */


    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* glyphs */

    _cairo_mime_surface_snapshot,
};

cairo_surface_t *
cairo_mime_surface_create (void *data, cairo_content_t content, int width, int height)
{
    cairo_mime_surface_t *surface;

    if (width < 0 || height < 0)
	return _cairo_surface_create_in_error (CAIRO_STATUS_INVALID_SIZE);

    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (CAIRO_STATUS_INVALID_CONTENT);

    surface = calloc (1, sizeof (*surface));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_surface_init (&surface->base,
			 &cairo_mime_surface_backend,
			 NULL, /* device */
			 content);

    surface->extents.x = 0;
    surface->extents.y = 0;
    surface->extents.width  = width;
    surface->extents.height = height;

    surface->user_data = data;

    return &surface->base;
}

void
cairo_mime_surface_set_callback_data (cairo_surface_t *surface,
				      void *data)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return;

    if (surface->backend != &cairo_mime_surface_backend)
	return;

    mime = (cairo_mime_surface_t *)surface;
    mime->user_data = data;
}

void *
cairo_mime_surface_get_callback_data (cairo_surface_t *surface)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return NULL;

    if (surface->backend != &cairo_mime_surface_backend)
	return NULL;

    mime = (cairo_mime_surface_t *)surface;
    return mime->user_data;
}

void
cairo_mime_surface_set_acquire (cairo_surface_t *surface,
				cairo_mime_surface_acquire_t acquire,
				cairo_mime_surface_release_t release)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return;

    if (surface->backend != &cairo_mime_surface_backend)
	return;

    mime = (cairo_mime_surface_t *)surface;
    mime->acquire = acquire;
    mime->release = release;
}

void
cairo_mime_surface_get_acquire (cairo_surface_t *surface,
				cairo_mime_surface_acquire_t *acquire,
				cairo_mime_surface_release_t *release)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return;

    if (surface->backend != &cairo_mime_surface_backend)
	return;

    mime = (cairo_mime_surface_t *)surface;
    if (acquire)
	*acquire = mime->acquire;
    if (release)
	*release = mime->release;
}

void
cairo_mime_surface_set_snapshot (cairo_surface_t *surface,
				 cairo_mime_surface_snapshot_t snapshot)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return;

    if (surface->backend != &cairo_mime_surface_backend)
	return;

    mime = (cairo_mime_surface_t *)surface;
    mime->snapshot = snapshot;
}

cairo_mime_surface_snapshot_t
cairo_mime_surface_get_snapshot (cairo_surface_t *surface)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return NULL;

    if (surface->backend != &cairo_mime_surface_backend)
	return NULL;

    mime = (cairo_mime_surface_t *)surface;
    return mime->snapshot;
}

void
cairo_mime_surface_set_destroy (cairo_surface_t *surface,
				cairo_mime_surface_destroy_t destroy)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return;

    if (surface->backend != &cairo_mime_surface_backend)
	return;

    mime = (cairo_mime_surface_t *)surface;
    mime->destroy = destroy;
}

cairo_mime_surface_destroy_t
cairo_mime_surface_get_destroy (cairo_surface_t *surface)
{
    cairo_mime_surface_t *mime;

    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&surface->ref_count))
	return NULL;

    if (surface->backend != &cairo_mime_surface_backend)
	return NULL;

    mime = (cairo_mime_surface_t *)surface;
    return mime->destroy;
}

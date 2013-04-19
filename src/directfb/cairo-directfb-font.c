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

#include "cairo-image-surface-private.h"
#include "cairo-list-inline.h"
#include "directfb/cairo-directfb-private.h"
#include <directfb.h>

struct cairo_dfb_font {
    IDirectFB *dfb;
    IDirectFBSurface *surface;
    cairo_scaled_font_t *scaled_font;
    int width, height;
};

typedef struct cairo_dfb_glyph cairo_dfb_glyph_t;
typedef struct cairo_dfb_uncached_glyph cairo_dfb_uncached_glyph_t;

struct cairo_dfb_glyph {
    cairo_scaled_glyph_private_t base;
    int x, y, width, height;
};

struct cairo_dfb_uncached_glyph {
    cairo_dfb_uncached_glyph_t *next;
    cairo_dfb_glyph_t *glyph;
    cairo_scaled_glyph_t *scaled_glyph;
};

/**
 * _cairo_dfb_font_create:
 * @scaled_font: a #cairo_scaled_font_t
 * @dfb: a #IDirectFB
 *
 * Creates a new #cairo_dfb_font_t, which is used to cache scaled glyphs.
 *
 * Returns: A new #cairo_dfb_font_t
 **/
cairo_dfb_font_t *
_cairo_dfb_font_create (cairo_scaled_font_t	*scaled_font,
			IDirectFB		*dfb)
{
    cairo_dfb_font_t *self;
    self = calloc (1, sizeof (*self));
    if (unlikely (! self)) {
	goto error;
    }
    (void) dfb->AddRef (dfb);
    self->dfb = dfb;
    self->scaled_font = scaled_font;
    return self;
error:
    _cairo_dfb_font_destroy (self);
    return NULL;
}

/**
 * _cairo_dfb_font_destroy:
 * @self: a #cairo_dfb_font_t
 *
 * Destroys a #cairo_dfb_font_t.
 **/
void
_cairo_dfb_font_destroy (cairo_dfb_font_t *self)
{
    if (likely (self)) {
	if (likely (self->surface)) {
	    (void) self->surface->Release (self->surface);
	}
	if (likely (self->dfb)) {
	    (void) self->dfb->Release (self->dfb);
	}
	free (self);
    }
}

/**
 * _cairo_dfb_glyph_fini:
 * @glyph_private: a #cairo_scaled_glyph_private_t
 * @glyph: a #cairo_scaled_glyph_t
 * @font: a #cairo_dfb_glyph_t
 *
 * Destroys a #cairo_dfb_glyph_t. This function is a callback for
 * _cairo_scaled_glyph_attach_private().
 **/
static void
_cairo_dfb_glyph_fini (cairo_scaled_glyph_private_t	*glyph_private,
		       cairo_scaled_glyph_t		*glyph,
		       cairo_scaled_font_t		*font)
{
    free (glyph_private);
}

/**
 * _cairo_dfb_glyph_create:
 * @scaled_glyph: a #cairo_scaled_glyph_t
 *
 * Creates a new #cairo_dfb_glyph_t, which is used to cache the location of a
 * scaled glyph within a #cairo_dfb_font_t.
 *
 * Returns: A new #cairo_dfb_glyph_t
 **/
static cairo_dfb_glyph_t *
_cairo_dfb_glyph_create (cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_dfb_glyph_t *self;
    cairo_image_surface_t *image;
    self = calloc (1, sizeof (*self));
    if (unlikely (! self)) {
	goto error;
    }
    image = scaled_glyph->surface;
    self->width = image->width;
    self->height = image->height;
    return self;
error:
    free (self);
    return NULL;
}

/**
 * _cairo_dfb_uncached_glyph_destroy:
 * @self: a #cairo_dfb_glyph_t
 *
 * Destroys a #cairo_dfb_glyph_t.
 **/
static void
_cairo_dfb_uncached_glyph_destroy (cairo_dfb_uncached_glyph_t *self)
{
    if (self) {
	free (self->glyph);
	free (self);
    }
}

/**
 * _cairo_dfb_uncached_glyph_create:
 * @scaled_glyph: a #cairo_scaled_glyph_t
 *
 * Creates a new #cairo_dfb_uncached_glyph_t. This object is used to create a
 * list of #cairo_dfb_glyph_t that need to be cached.
 *
 * Returns: A new #cairo_dfb_uncached_glyph_t
 **/
static cairo_dfb_uncached_glyph_t *
_cairo_dfb_uncached_glyph_create (cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_dfb_uncached_glyph_t *self;
    self = calloc (1, sizeof (*self));
    if (unlikely (! self)) {
	goto error;
    }
    self->glyph = _cairo_dfb_glyph_create (scaled_glyph);
    if (unlikely (! self->glyph)) {
	goto error;
    }
    self->scaled_glyph = scaled_glyph;
    return self;
error:
    _cairo_dfb_uncached_glyph_destroy (self);
    return NULL;
}

/**
 * _cairo_dfb_font_resize:
 * @self: a #cairo_dfb_font_t
 * @width: new width in pixels
 * @height: new height in pixels
 *
 * Resizes the #cairo_dfb_font_t cache surface.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_int_status_t
_cairo_dfb_font_resize (cairo_dfb_font_t	*self,
			int			 width,
			int			 height)
{
    DFBSurfaceDescription dsc = {
	.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT,
	.width = width,
	.height = height,
	.pixelformat = DSPF_ARGB
    };
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
    IDirectFBSurface *surface = NULL;
    DFBResult result;
    result = self->dfb->CreateSurface (self->dfb, &dsc, &surface);
    if (unlikely (DFB_OK != result)) {
	status = CAIRO_INT_STATUS_NO_MEMORY;
	goto exit;
    }
    if (likely (self->surface)) {
	result = surface->Blit (surface, self->surface, NULL, 0, 0);
	if (unlikely (DFB_OK != result)) {
	    status = CAIRO_INT_STATUS_NO_MEMORY;
	    (void) surface->Release (surface);
	    goto exit;
	}
	(void) self->surface->Release (self->surface);
    }
    self->surface = surface;
    self->width = width;
    self->height = height;
exit:
    return status;
}

/**
 * _cairo_dfb_font_cache_A1:
 * @self: a #cairo_dfb_font_t
 * @glyph: a #cairo_dfb_glyph_t
 * @image: a #cairo_image_surface_t
 * @data: cache surface pixel bits
 * @stride: cache surface stride
 *
 * Caches the %CAIRO_FORMAT_A1 surface @image.
 **/
static void
_cairo_dfb_font_cache_A1 (cairo_dfb_font_t		*self,
			  cairo_dfb_glyph_t		*glyph,
			  cairo_image_surface_t		*image,
			  unsigned char			*data,
			  int				 stride)
{
    int i, j;
    unsigned char *src = image->data;
    data += glyph->y * stride + (glyph->x << 2);
    for (i = glyph->height; i; --i) {
	for (j = 0; j < glyph->width; ++j) {
	    ((uint32_t *) data)[j] =
		(src[j >> 3] & (1 << (j & 7))) ? 0xffffffff : 0;
	}
	src += image->stride;
	data += stride;
    }
}

/**
 * _cairo_dfb_font_cache_A8:
 * @self: a #cairo_dfb_font_t
 * @glyph: a #cairo_dfb_glyph_t
 * @image: a #cairo_image_surface_t
 * @data: cache surface pixel bits
 * @stride: cache surface stride
 *
 * Caches the %CAIRO_FORMAT_A8 surface @image.
 **/
static void
_cairo_dfb_font_cache_A8 (cairo_dfb_font_t		*self,
			  cairo_dfb_glyph_t		*glyph,
			  cairo_image_surface_t		*image,
			  unsigned char			*data,
			  int				 stride)
{
    int i, j;
    unsigned char *src = image->data;
    data += glyph->y * stride + (glyph->x << 2);
    for (i = glyph->height; i; --i) {
	for (j = 0; j < glyph->width; ++j) {
	    ((uint32_t *) data)[j] = src[j] * 0x01010101;
	}
	src += image->stride;
	data += stride;
    }
}

/**
 * _cairo_dfb_font_cache_ARGB32:
 * @self: a #cairo_dfb_font_t
 * @glyph: a #cairo_dfb_glyph_t
 * @image: a #cairo_image_surface_t
 * @data: cache surface pixel bits
 * @stride: cache surface stride
 *
 * Caches the %CAIRO_FORMAT_ARGB32 surface @image.
 **/
static void
_cairo_dfb_font_cache_ARGB32 (cairo_dfb_font_t		*self,
			      cairo_dfb_glyph_t		*glyph,
			      cairo_image_surface_t	*image,
			      unsigned char		*data,
			      int			 stride)
{
    int i;
    unsigned char *src = image->data;
    data += glyph->y * stride + (glyph->x << 2);
    for (i = glyph->height; i; --i) {
	(void) memcpy (data, src, glyph->width << 2);
	src += image->stride;
	data += stride;
    }
}

/**
 * _cairo_dfb_font_cache_glyphs:
 * @self: a #cairo_dfb_font_t
 * @list: a list of #cairo_dfb_uncached_glyph_t
 *
 * Caches the @list of glyphs on the internal cache surface.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
static cairo_int_status_t
_cairo_dfb_font_cache_glyphs (cairo_dfb_font_t			*self,
			      cairo_dfb_uncached_glyph_t	*list)
{
    int stride;
    DFBResult result;
    void *data = NULL;
    cairo_dfb_uncached_glyph_t *uncached_glyph;
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
    result = self->surface->Lock (self->surface, DSLF_WRITE,
				  (void *) &data, &stride);
    if (unlikely (DFB_OK != result)) {
	return CAIRO_INT_STATUS_NO_MEMORY;
    }
    for (uncached_glyph = list; uncached_glyph;
	 uncached_glyph = uncached_glyph->next) {
	switch (uncached_glyph->scaled_glyph->surface->format) {
	case CAIRO_FORMAT_A1:
	    _cairo_dfb_font_cache_A1 (self, uncached_glyph->glyph,
				      uncached_glyph->scaled_glyph->surface,
				      data, stride);
	    break;
	case CAIRO_FORMAT_A8:
	    _cairo_dfb_font_cache_A8 (self, uncached_glyph->glyph,
				      uncached_glyph->scaled_glyph->surface,
				      data, stride);
	    break;
	case CAIRO_FORMAT_ARGB32:
	    _cairo_dfb_font_cache_ARGB32 (self, uncached_glyph->glyph,
					  uncached_glyph->scaled_glyph->surface,
					  data, stride);
	    break;
	case CAIRO_FORMAT_RGB16_565:
	case CAIRO_FORMAT_RGB30:
	case CAIRO_FORMAT_RGB24:
	case CAIRO_FORMAT_INVALID:
	default:
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    goto exit;
	}
	_cairo_scaled_glyph_attach_private (uncached_glyph->scaled_glyph,
					    &uncached_glyph->glyph->base,
					    self, _cairo_dfb_glyph_fini);
	uncached_glyph->glyph = NULL;
    }
exit:
    if (likely (data)) {
	(void) self->surface->Unlock (self->surface);
    }
    return status;
}

/**
 * _cairo_dfb_font_get_rectangles:
 * @self: a #cairo_dfb_font_t
 * @glyphs: an array of #cairo_glyph_t
 * @num_glyphs: the number of @glyphs
 * @rects: output array of #DFBRectangle
 * @points: output array of #DFBPoint
 * @cache: output #IDirectFBSurface
 *
 * This function computes the location of the @glyphs on the @cache surface
 * and returns them in @rects. The points for the destination surface are
 * returned in @points. The glyph cache surface is returned in @cache. If any
 * of the @glyphs are not cached they will be added.
 *
 * Returns: %CAIRO_STATUS_SUCCESS if the operation succeeded
 **/
cairo_int_status_t
_cairo_dfb_font_get_rectangles (cairo_dfb_font_t	 *self,
				cairo_glyph_t		 *glyphs,
				int			  num_glyphs,
				DFBRectangle		**rects,
				DFBPoint		**points,
				IDirectFBSurface	**cache)
{
    int i, width = self->width, height = self->height;
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
    cairo_dfb_uncached_glyph_t *list = NULL;
    _cairo_scaled_font_freeze_cache (self->scaled_font);
    for (i = 0; i < num_glyphs; ++i) {
	cairo_dfb_glyph_t *glyph;
	cairo_scaled_glyph_t *scaled_glyph;
	status = _cairo_scaled_glyph_lookup (self->scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
	if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	    goto exit;
	}
	glyph = (cairo_dfb_glyph_t *)
	    _cairo_scaled_glyph_find_private (scaled_glyph, self);
	if (unlikely (! glyph)) {
	    cairo_dfb_uncached_glyph_t *uncached_glyph;
	    uncached_glyph = _cairo_dfb_uncached_glyph_create (scaled_glyph);
	    if (unlikely (! uncached_glyph)) {
		status = CAIRO_INT_STATUS_NO_MEMORY;
		goto exit;
	    }
	    if (likely (list)) {
		uncached_glyph->next = list;
	    }
	    list = uncached_glyph;
	    uncached_glyph->glyph->x = width;
	    uncached_glyph->glyph->y = 0;
	    width += scaled_glyph->surface->width;
	    if (scaled_glyph->surface->height > height) {
		height = scaled_glyph->surface->height;
	    }
	    glyph = uncached_glyph->glyph;
	}
	(*rects)[i].x = glyph->x;
	(*rects)[i].y = glyph->y;
	(*rects)[i].w = glyph->width;
	(*rects)[i].h = glyph->height;
	(*points)[i].x =
	    _cairo_lround (glyphs[i].x -
			   scaled_glyph->surface->base.device_transform.x0);
	(*points)[i].y =
	    _cairo_lround (glyphs[i].y -
			   scaled_glyph->surface->base.device_transform.y0);
    }
    if (width != self->width || height != self->height) {
	status = _cairo_dfb_font_resize (self, width, height);
	if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	    goto exit;
	}
	status = _cairo_dfb_font_cache_glyphs (self, list);
	if (unlikely (CAIRO_INT_STATUS_SUCCESS != status)) {
	    goto exit;
	}
    }
    *cache = self->surface;
exit:
    while (list) {
	cairo_dfb_uncached_glyph_t *next = list->next;
	_cairo_dfb_uncached_glyph_destroy (list);
	list = next;
    }
    _cairo_scaled_font_thaw_cache (self->scaled_font);
    return status;
}
